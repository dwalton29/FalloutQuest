// Host-only inventory. All gameplay probes prepare disposable transactions on one baseline.
#include "dialogue/fo3-dialogue-results.h"
#include "dialogue/fo3-dialogue-selection.h"
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

namespace {
using namespace fo3player;
using fo3pipdata::ResultScript;
constexpr auto capture = ScriptProgramCapture::EnteredPrograms;
using Kind = ScriptProgramKind;
using Plan = std::unique_ptr<ScriptTransactionPlan>;

std::string key(const char* type, uint32_t id, const std::string& slot) {
    std::ostringstream s;
    s << type << ':' << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << id << ':' << slot;
    return s.str();
}

void row(const std::string& key, const std::string& kind, std::string value) {
    for (char& ch : value) if (ch == '\t' || ch == '\r' || ch == '\n') ch = ' ';
    std::cout << key << '\t' << kind << '\t' << value << '\n';
}

bool present(const ResultScript& s) { return !s.compiled.empty(); }

void compile(const std::string& k, const Player& player, const ResultScript& s,
             uint32_t owner, uint32_t scriptId = 0) {
    const auto& d = player.Definitions().pipboy;
    std::vector<fo3dialogue::Command> commands;
    std::string error;
    if (!fo3dialogue::CompileResult(d, s, commands, error, owner, scriptId)) {
        row(k, "compile", "rejected");
        row(k, "compiler_error", error);
        // The compiler stops at its first error. Recover other explicit unknown-command
        // diagnostics without interpreting isolated lines as valid control flow.
        std::istringstream source(s.source);
        std::string line;
        while (std::getline(source, line)) {
            ResultScript single;
            single.source = line;
            if (!fo3dialogue::CompileSourceResult(d, single, commands, error, owner, scriptId, nullptr) &&
                error.rfind("unsupported command: ", 0) == 0)
                row(k, "line_diagnostic", error);
        }
        return;
    }
    bool deferred = false;
    for (const auto& command : commands) {
        if (fo3dialogue::HasNumericExpression(command)) {
            fo3dialogue::CommandFormCapture bindings;
            bindings.limit = std::numeric_limits<size_t>::max();
            float value = 0;
            std::string why;
            const bool bound = fo3dialogue::BindNumericExpression(player, owner, scriptId, command.expression,
                                                                  bindings, value, why);
            row(k, "expression", (bound ? std::string("bound") : why) + "\x1f" + command.expression);
            if (bound) for (const auto& binding : bindings.bindings)
                row(k, "bound_form", std::to_string(binding.first));
        }
        if (command.type == fo3dialogue::CommandType::Unavailable) {
            deferred = true;
            row(k, "deferred_error", command.expression);
        }
    }
    row(k, "compile", deferred ? "with_deferred_failure" : "accepted");
}

void preview(const std::string& k, const Plan& plan, const std::string& error, Kind kind,
             uint32_t record, uint32_t item = 0, uint16_t stage = 0, unsigned line = 0) {
    if (!plan) {
        row(k, "preview", "rejected");
        row(k, "preview_error", error);
        return;
    }
    for (const auto& e : plan->ExecutedScripts()) {
        const auto& o = e.origin;
        if (o.kind == kind && o.record == record && o.item == item && o.stage == stage &&
            o.sourceLine == line && e.parent == ScriptExecution::NoParent) {
            row(k, "preview", "entered");
            return;
        }
    }
    row(k, "preview", "skipped");
}

void conditions(const std::string& k, const std::string& scope, const std::vector<fo3pipdata::Condition>& list,
                const Player& p, uint32_t actor) {
    fo3dialogue::Context ctx;
    ctx.player = &p;
    if (actor) fo3dialogue::ReferenceContext(p, actor, ctx.speaker);
    ctx.target.reference = PlayerRef;
    ctx.target.base = PlayerBase;
    ctx.randomPercent = 50;
    for (size_t i = 0; i < list.size(); ++i) {
        std::string error;
        const bool pass = fo3dialogue::Conditions({list[i]}, ctx, error);
        row(k, "condition", scope + ":" + std::to_string(i) + ":" + std::to_string(list[i].function) +
            ":" + (error.empty() ? (pass ? "true" : "false") : error));
    }
}

using Owners = std::map<uint32_t,uint32_t>;
void remember(Owners& owners, uint32_t id, uint32_t ref) {
    if (!id) return;
    auto entry = owners.emplace(id, ref);
    entry.first->second = std::min(entry.first->second, ref);
}

void survey(Player& player) {
    const auto& catalog = player.Definitions();
    const auto& d = catalog.pipboy;
    Owners placements, scriptOwners, questOwners, packageActors;
    for (const auto& [ref, r] : catalog.references) {
        if (!r.valid) continue;
        remember(placements, r.base, ref);
        auto script = catalog.eventBaseScripts.find(r.base);
        if (script != catalog.eventBaseScripts.end()) remember(scriptOwners, script->second, ref);
        auto actor = d.dialogueActors.find(r.base);
        if (actor != d.dialogueActors.end())
            for (auto package : actor->second.packages) remember(packageActors, package, ref);
    }
    for (const auto& [id, q] : d.quests) remember(questOwners, q.script, id);
    for (const auto& [id, s] : d.scripts) {
        if (!present(s)) continue;
        const auto k = key("SCPT", id, "script");
        const auto events = fo3script::ParseEvents(s);
        const auto quest = questOwners[id], ref = scriptOwners[id], owner = quest ? quest : ref;
        row(k, "owner", std::to_string(owner));
        if (!events.error.empty()) { row(k, "compile", "rejected"); row(k, "compiler_error", events.error); continue; }
        for (const auto& block : events.blocks) {
            // Preserve individual block evidence, even if several blocks use the same event.
            const auto b = k + "@" + std::to_string(block.sourceLine);
            row(b, "event", block.event);
            ResultScript body; body.source = block.body;
            compile(b, player, body, owner, id);
            if (!ref || quest) continue;
            uint32_t filter = 0;
            if (!block.filter.empty() && block.filter != "player") {
                const auto f = d.formNames.find(block.filter);
                if (f == d.formNames.end()) {row(b, "filter", "unresolved"); continue;}
                filter = f->second;
            }
            std::string error;
            const auto plan = player.PrepareReferenceEventTransaction(ref, block.event.c_str(), PlayerRef, error, capture, filter);
            preview(b, plan, error, Kind::ReferenceEvent, id, 0, 0, block.sourceLine);
        }
        if (events.blocks.empty()) row(k, "compile", "no_event_blocks");
    }
    for (const auto& [topic, infos] : d.topics) for (const auto& i : infos) {
        auto owner = placements[i.speaker];
        if (!owner) {
            // An unconditional GetIsID == 1 can supply the speaker base. Ignore OR
            // alternatives. Actor identity is still a sample when several are placed.
            bool followsOr = false;
            for (const auto& c : i.conditions) {
                if (!followsOr && !c.flags && c.run == 0 && c.function == 72 && c.value == 1)
                    owner = placements[c.a];
                followsOr = (c.flags & 1) != 0;
            }
        }
        for (unsigned phase = 0; phase < 2; ++phase) {
            const auto& s = phase ? i.end : i.begin;
            if (!present(s)) continue;
            const auto k = key("INFO", i.id, phase ? "end" : "begin");
            row(k, "owner", std::to_string(owner));
            compile(k, player, s, owner);
            conditions(k, "info", i.conditions, player, owner);
            const auto q = d.quests.find(i.quest);
            if (q != d.quests.end()) conditions(k, "parent_quest", q->second.conditions, player, owner);
            std::string error;
            const auto plan = player.PrepareDialogueInfoTransaction(topic, i.id, phase ? DialogueScriptPhase::End :
                DialogueScriptPhase::Begin, error, owner, capture);
            preview(k, plan, error, Kind::DialogueInfo, i.id, phase);
        }
    }
    for (const auto& [id, q] : d.quests) for (const auto& [stage, entry] : q.stages) {
        if (std::none_of(entry.items.begin(), entry.items.end(), [](const auto& i) { return present(i.result); })) continue;
        std::string error;
        const auto plan = player.PrepareQuestStageTransaction(id, stage, error, capture);
        for (size_t index = 0; index < entry.items.size(); ++index) {
            const auto& i = entry.items[index];
            if (!present(i.result)) continue;
            const auto k = key("QUST", id, std::to_string(stage) + "/" + std::to_string(index));
            row(k, "owner", std::to_string(id));
            compile(k, player, i.result, id);
            conditions(k, "item", i.conditions, player, 0);
            preview(k, plan, error, Kind::QuestStageItem, id, index, stage);
        }
    }
    for (const auto& [id, t] : d.terminals) for (size_t index = 0; index < t.entries.size(); ++index) {
        const auto& entry = t.entries[index];
        if (!present(entry.script)) continue;
        const auto k = key("TERM", id, std::to_string(index));
        const auto ref = placements[id];
        row(k, "owner", std::to_string(ref));
        compile(k, player, entry.script, ref);
        conditions(k, "entry", entry.conditions, player, ref);
        if (ref) {
            std::string error;
            const auto plan = player.PrepareTerminalTransaction(ref, id, index, error, capture);
            preview(k, plan, error, Kind::TerminalEntry, id, index);
        }
    }
    for (const auto& [id, p] : d.packages) for (unsigned i = 0; i < p.procedureScripts.size(); ++i) {
        const auto& s = p.procedureScripts[i];
        if (!present(s)) continue;
        const auto k = key("PACK", id, std::to_string(i));
        const auto actor = packageActors[id];
        row(k, "owner", std::to_string(actor));
        compile(k, player, s, actor);
        conditions(k, "package", p.conditions, player, actor);
        if (actor && i < 2) {
            std::string error;
            const auto plan = player.PreparePackageEventTransaction(actor, id, i == 1, error, capture);
            preview(k, plan, error, Kind::PackageResult, id, i);
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {std::cerr << "usage: script_fragment_probe Fallout3.esm\n"; return 2;}
    fo3player::Catalog catalog; std::string error;
    if (!fo3player::LoadCatalog(argv[1], catalog, error)) {std::cerr << error << '\n'; return 1;}
    catalog.initial.scriptRandom = 1; // Reproducible candidate RNG, including repeated runs.
    fo3player::Player player(std::move(catalog));
    player.RegisterOriginalActors();
    survey(player);
    return std::cout.good() ? 0 : 1;
}
