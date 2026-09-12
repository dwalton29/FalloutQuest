# Q16.12: authored dynamic load-door prompts + crash-safe HUD lifecycle.
#
# This pass fixes three concrete Q16.11 issues without inventing presentation:
#   1. Save caller GL state BEFORE first-time HUD resource initialization.
#   2. Render text_box.xml as one dynamically-sized interaction string.
#   3. Resolve door/destination nouns from Fallout3.esm FULL fields.
#
# The action grammar "Open <door> to <destination>" is engine behaviour; every
# noun/location comes from the user's ESM. No Megaton-specific prompt is stored.

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fo3-door-prompt-q1840.h")
    message(FATAL_ERROR "Q16.12 missing fo3-door-prompt-q1840.h")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fo3-interaction-hud-q1840.h")
    message(FATAL_ERROR "Q16.12 missing fo3-interaction-hud-q1840.h")
endif()

# -----------------------------------------------------------------------------
# 1. Transition TU: generic Fallout3.esm name/reference index + prompt cache.
# -----------------------------------------------------------------------------
if(NOT EXISTS "${Q720_CELL_SOURCE}")
    message(FATAL_ERROR "Q16.12 expected generated transition source at ${Q720_CELL_SOURCE}")
endif()
file(READ "${Q720_CELL_SOURCE}" Q1840_CELL_SOURCE)
string(PREPEND Q1840_CELL_SOURCE
       "#include \"fo3-door-prompt-q1840.h\"\n#include <algorithm>\n#include <cstring>\n#include <string>\n#include <unordered_map>\n#include <vector>\n")

set(Q1840_PROMPT_IMPL [==[
namespace Q1840Prompt {

struct GroupFrame {
    uint64_t end = 0u;
    uint32_t label = 0u;
    uint32_t type = 0u;
};

struct Metadata {
    uint32_t sourceBase = 0u;
    Fo3DoorTeleport teleport;
};

bool gIndexAttempted = false;
bool gIndexReady = false;
std::unordered_map<uint32_t, std::string> gFullNames;
std::unordered_map<uint32_t, uint32_t> gCellWorldspaces;
std::unordered_map<uint32_t, uint32_t> gRefBases;
std::unordered_map<uint32_t, Metadata> gDoorMetadata;
std::unordered_map<uint32_t, std::string> gPrompts;

std::string SubrecordString(const uint8_t* bytes, uint32_t size) {
    if (!bytes || size == 0u) return {};
    size_t n = 0u;
    while (n < size && bytes[n] != 0u) ++n;
    return std::string(reinterpret_cast<const char*>(bytes), n);
}

uint32_t CurrentWorldspace(const std::vector<GroupFrame>& groups) {
    for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
        if (it->type == 1u) return it->label;
    }
    return 0u;
}

bool EnsureIndex() {
    if (gIndexAttempted) return gIndexReady;
    gIndexAttempted = true;

    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) {
        Q71_LOGE("Q16.12 PROMPT INDEX FAILED: reason=open-esm");
        return false;
    }
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        Q71_LOGE("Q16.12 PROMPT INDEX FAILED: reason=bad-esm-size");
        return false;
    }

    std::vector<GroupFrame> groups;
    size_t doorNames = 0u;
    size_t cellNames = 0u;
    size_t worldNames = 0u;
    size_t refBases = 0u;

    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        while (!groups.empty() && offset >= groups.back().end) groups.pop_back();
        if (offset + HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[HEADER_SIZE]{};
        if (!ReadExact(file, header, sizeof(header))) break;
        const uint32_t sizeField = ReadLe32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < HEADER_SIZE ||
                offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            groups.push_back(GroupFrame{offset + sizeField,
                                        ReadLe32(header + 8u),
                                        ReadLe32(header + 12u)});
            continue;
        }

        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        const bool isDoor = std::memcmp(header, "DOOR", 4u) == 0;
        const bool isCell = std::memcmp(header, "CELL", 4u) == 0;
        const bool isWorld = std::memcmp(header, "WRLD", 4u) == 0;
        const bool isRef = std::memcmp(header, "REFR", 4u) == 0;
        if (!isDoor && !isCell && !isWorld && !isRef) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        const uint32_t formId = ReadLe32(header + 12u);
        const uint32_t recordFlags = ReadLe32(header + 8u);
        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, recordFlags, payload)) break;

        std::string full;
        uint32_t refBase = 0u;
        WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (!isRef && full.empty() &&
                std::memcmp(type, "FULL", 4u) == 0) {
                full = SubrecordString(bytes, size);
            } else if (isRef && refBase == 0u &&
                       std::memcmp(type, "NAME", 4u) == 0 && size >= 4u) {
                refBase = ReadLe32(bytes);
            }
        });

        if (!full.empty()) {
            gFullNames[formId] = full;
            if (isDoor) ++doorNames;
            else if (isCell) ++cellNames;
            else if (isWorld) ++worldNames;
        }
        if (isRef && refBase != 0u) {
            gRefBases[formId] = refBase;
            ++refBases;
        }
        if (isCell) {
            const uint32_t world = CurrentWorldspace(groups);
            if (world != 0u) gCellWorldspaces[formId] = world;
        }
    }

    std::fclose(file);
    gIndexReady = !gFullNames.empty() && !gRefBases.empty();
    Q71_LOGI("Q16.12 PROMPT INDEX READY: ready=%d fullNames=%zu door=%zu cell=%zu world=%zu refs=%zu cellWorldLinks=%zu source=Fallout3.esm",
             gIndexReady ? 1 : 0, gFullNames.size(), doorNames, cellNames,
             worldNames, refBases, gCellWorldspaces.size());
    return gIndexReady;
}

const std::string& FullName(uint32_t formId) {
    static const std::string empty;
    const auto it = gFullNames.find(formId);
    return it == gFullNames.end() ? empty : it->second;
}

std::string DestinationName(const Fo3DoorTeleport& teleport) {
    if (teleport.destinationCellFormId == 0u) return {};
    const std::string& cell = FullName(teleport.destinationCellFormId);
    if (!cell.empty()) return cell;
    const auto worldIt = gCellWorldspaces.find(teleport.destinationCellFormId);
    if (worldIt == gCellWorldspaces.end()) return {};
    return FullName(worldIt->second);
}

bool EnsureDoorMetadata(uint32_t sourceDoorRef) {
    if (gDoorMetadata.find(sourceDoorRef) != gDoorMetadata.end()) return true;
    if (!EnsureIndex()) return false;

    const auto baseIt = gRefBases.find(sourceDoorRef);
    if (baseIt == gRefBases.end() || baseIt->second == 0u) return false;

    Fo3DoorTeleport teleport;
    if (!ResolveFo3DoorTeleportQ1700(sourceDoorRef, &teleport) || !teleport.valid) {
        return false;
    }

    Metadata meta;
    meta.sourceBase = baseIt->second;
    meta.teleport = teleport;
    gDoorMetadata[sourceDoorRef] = meta;
    Q71_LOGI("Q16.12 PROMPT METADATA LAZY: sourceDoor=%08X sourceBase=%08X destinationDoor=%08X destinationCell=%08X source=Fallout3.esm",
             sourceDoorRef, meta.sourceBase,
             teleport.destinationDoorRefFormId,
             teleport.destinationCellFormId);
    return true;
}

bool BuildPrompt(uint32_t sourceDoorRef, std::string& out) {
    out.clear();
    if (!EnsureDoorMetadata(sourceDoorRef)) return false;
    if (!EnsureIndex()) return false;

    const Metadata& meta = gDoorMetadata[sourceDoorRef];
    const std::string doorName = FullName(meta.sourceBase);
    const std::string destination = DestinationName(meta.teleport);
    if (doorName.empty() && destination.empty()) {
        Q71_LOGE("Q16.12 AUTHORED DOOR PROMPT MISS: sourceDoor=%08X sourceBase=%08X destinationCell=%08X reason=no-FULL-fields",
                 sourceDoorRef, meta.sourceBase, meta.teleport.destinationCellFormId);
        return false;
    }

    out = "Open";
    if (!doorName.empty()) {
        out += " ";
        out += doorName;
    }
    if (!destination.empty()) {
        out += " to ";
        out += destination;
    }

    Q71_LOGI("Q16.12 AUTHORED DOOR PROMPT: sourceDoor=%08X sourceBase=%08X destinationDoor=%08X destinationCell=%08X door=\"%s\" destination=\"%s\" text=\"%s\"",
             sourceDoorRef, meta.sourceBase,
             meta.teleport.destinationDoorRefFormId,
             meta.teleport.destinationCellFormId,
             doorName.c_str(), destination.c_str(), out.c_str());
    return true;
}

} // namespace Q1840Prompt

void CacheFo3DoorPromptQ1840(uint32_t sourceDoorRef,
                             uint32_t sourceDoorBase,
                             const Fo3DoorTeleport& teleport) {
    if (sourceDoorRef == 0u || sourceDoorBase == 0u || !teleport.valid) return;
    Q1840Prompt::Metadata meta;
    meta.sourceBase = sourceDoorBase;
    meta.teleport = teleport;
    Q1840Prompt::gDoorMetadata[sourceDoorRef] = meta;
    Q1840Prompt::gPrompts.erase(sourceDoorRef);
    // Build the one-time authored-name index during scene preparation rather
    // than on the first visible HUD frame when possible.
    Q1840Prompt::EnsureIndex();
}

bool GetFo3DoorPromptQ1840(uint32_t sourceDoorRef,
                           char* outText,
                           size_t outCapacity) {
    if (!outText || outCapacity == 0u || sourceDoorRef == 0u) return false;
    outText[0] = '\0';

    auto cached = Q1840Prompt::gPrompts.find(sourceDoorRef);
    if (cached == Q1840Prompt::gPrompts.end()) {
        std::string prompt;
        if (!Q1840Prompt::BuildPrompt(sourceDoorRef, prompt) || prompt.empty()) {
            return false;
        }
        cached = Q1840Prompt::gPrompts.emplace(sourceDoorRef, std::move(prompt)).first;
    }

    const std::string& prompt = cached->second;
    const size_t count = std::min(prompt.size(), outCapacity - 1u);
    std::memcpy(outText, prompt.data(), count);
    outText[count] = '\0';
    return count > 0u;
}

]==])

set(Q1840_CELL_IMPL_MARKER [==[
bool ConsumeFo3CellTransitionRequestQ74(Fo3CellTransitionRequestQ74& outRequest) {
]==])
string(FIND "${Q1840_CELL_SOURCE}" "${Q1840_CELL_IMPL_MARKER}" Q1840_CELL_IMPL_POS)
if(Q1840_CELL_IMPL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.12 could not find transition implementation anchor")
endif()
string(REPLACE "${Q1840_CELL_IMPL_MARKER}"
       "${Q1840_PROMPT_IMPL}${Q1840_CELL_IMPL_MARKER}"
       Q1840_CELL_SOURCE "${Q1840_CELL_SOURCE}")
file(WRITE "${Q720_CELL_SOURCE}" "${Q1840_CELL_SOURCE}")

# -----------------------------------------------------------------------------
# 2. Final renderer: eagerly register rendered DOOR metadata when available.
#    Invisible/no-model XTEL anchors need no special patch: GetFo3DoorPromptQ1840
#    lazily resolves their REFR NAME base + XTEL from Fallout3.esm.
# -----------------------------------------------------------------------------
set(Q1840_NATIVE_FILE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp")
if(NOT EXISTS "${Q1840_NATIVE_FILE}")
    message(FATAL_ERROR "Q16.12 expected final q6h native source")
endif()
file(READ "${Q1840_NATIVE_FILE}" Q1840_NATIVE_SOURCE)
string(PREPEND Q1840_NATIVE_SOURCE "#include \"fo3-door-prompt-q1840.h\"\n")

set(Q1840_GPU_META_OLD [==[
    if (gpu.baseRecordType == "DOOR") {
        ResolveDoorTeleportCachedQ1698(gpu.refFormId, gpu.teleport);
    }
]==])
set(Q1840_GPU_META_NEW [==[
    if (gpu.baseRecordType == "DOOR") {
        ResolveDoorTeleportCachedQ1698(gpu.refFormId, gpu.teleport);
        if (gpu.teleport.valid) {
            CacheFo3DoorPromptQ1840(gpu.refFormId,
                                    cpu.placement.baseFormId,
                                    gpu.teleport);
        }
    }
]==])
string(FIND "${Q1840_NATIVE_SOURCE}" "${Q1840_GPU_META_OLD}" Q1840_GPU_META_POS)
if(Q1840_GPU_META_POS EQUAL -1)
    message(FATAL_ERROR "Q16.12 could not find live DOOR upload metadata block")
endif()
string(REPLACE "${Q1840_GPU_META_OLD}" "${Q1840_GPU_META_NEW}"
       Q1840_NATIVE_SOURCE "${Q1840_NATIVE_SOURCE}")
file(WRITE "${Q1840_NATIVE_FILE}" "${Q1840_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# 3. Final OpenXR host: stable aim show/hide + dynamic authored prompt string.
# -----------------------------------------------------------------------------
set(Q1840_Q4_FILE "${CMAKE_CURRENT_BINARY_DIR}/q1800-q4-generated.cpp")
if(NOT EXISTS "${Q1840_Q4_FILE}")
    message(FATAL_ERROR "Q16.12 expected final q1800 OpenXR source")
endif()
file(READ "${Q1840_Q4_FILE}" Q1840_Q4_SOURCE)

set(Q1840_HUD_INCLUDE_MARKER "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-interaction-hud-q1830.h\"")
set(Q1840_HUD_INCLUDE_NEW
    "${Q1840_HUD_INCLUDE_MARKER}\n#include \"fo3-door-prompt-q1840.h\"\n#include \"fo3-interaction-hud-q1840.h\"\n#include <string>")
string(FIND "${Q1840_Q4_SOURCE}" "${Q1840_HUD_INCLUDE_MARKER}" Q1840_HUD_INCLUDE_POS)
if(Q1840_HUD_INCLUDE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.12 could not find q1830 real-HUD include")
endif()
string(REPLACE "${Q1840_HUD_INCLUDE_MARKER}" "${Q1840_HUD_INCLUDE_NEW}"
       Q1840_Q4_SOURCE "${Q1840_Q4_SOURCE}")

set(Q1840_FIELD_MARKER [==[
    uint32_t doorAimDestinationQ1800_{0u};
]==])
set(Q1840_FIELD_NEW [==[
    uint32_t doorAimDestinationQ1800_{0u};
    uint32_t doorAimSourceQ1840_{0u};
    std::string doorPromptQ1840_;
]==])
string(FIND "${Q1840_Q4_SOURCE}" "${Q1840_FIELD_MARKER}" Q1840_FIELD_POS)
if(Q1840_FIELD_POS EQUAL -1)
    message(FATAL_ERROR "Q16.12 could not find q1800 door target fields")
endif()
string(REPLACE "${Q1840_FIELD_MARKER}" "${Q1840_FIELD_NEW}"
       Q1840_Q4_SOURCE "${Q1840_Q4_SOURCE}")

set(Q1840_AIM_OLD [==[
        Fo3DoorAimQ1700 aim;
        doorAimActiveQ1700_ = QueryFo3DoorAimQ1700(
            hand.position.x, hand.position.y, hand.position.z,
            -handMatrix.m[8], -handMatrix.m[9], -handMatrix.m[10], &aim) && aim.valid;
        if (doorAimActiveQ1700_) {
            doorAimYawQ1800_ = aim.rz;
            doorAimDestinationQ1800_ = aim.destinationDoorRef;
        }
]==])
set(Q1840_AIM_NEW [==[
        Fo3DoorAimQ1700 aim;
        const bool q1840WasActive = doorAimActiveQ1700_;
        const bool q1840NextActive = QueryFo3DoorAimQ1700(
            hand.position.x, hand.position.y, hand.position.z,
            -handMatrix.m[8], -handMatrix.m[9], -handMatrix.m[10], &aim) && aim.valid;

        if (q1840NextActive) {
            // Keep the authored destination/yaw cache for the post-A facing fix.
            doorAimYawQ1800_ = aim.rz;
            doorAimDestinationQ1800_ = aim.destinationDoorRef;

            if (doorAimSourceQ1840_ != aim.sourceDoorRef || doorPromptQ1840_.empty()) {
                char q1840Prompt[512]{};
                if (GetFo3DoorPromptQ1840(aim.sourceDoorRef,
                                          q1840Prompt,
                                          sizeof(q1840Prompt))) {
                    doorPromptQ1840_ = q1840Prompt;
                } else {
                    // No invented fallback wording. Missing authored data hides
                    // the text and leaves an explicit diagnostic.
                    doorPromptQ1840_.clear();
                    FQ_LOGE("Q16.12 HUD PROMPT MISS: sourceDoor=%08X destinationDoor=%08X",
                            aim.sourceDoorRef, aim.destinationDoorRef);
                }
                doorAimSourceQ1840_ = aim.sourceDoorRef;
            }

            if (!q1840WasActive) {
                FQ_LOGI("Q16.12 HUD TARGET SHOW: sourceDoor=%08X destinationDoor=%08X text=%s",
                        aim.sourceDoorRef, aim.destinationDoorRef,
                        doorPromptQ1840_.empty() ? "<missing>" : doorPromptQ1840_.c_str());
            }
        } else {
            if (q1840WasActive) {
                FQ_LOGI("Q16.12 HUD TARGET HIDE: sourceDoor=%08X resourcesRetained=1",
                        doorAimSourceQ1840_);
            }
            // Hide is state-only: do not destroy/reinitialize any GL resource.
            doorAimSourceQ1840_ = 0u;
            doorPromptQ1840_.clear();
        }
        doorAimActiveQ1700_ = q1840NextActive;
]==])
string(FIND "${Q1840_Q4_SOURCE}" "${Q1840_AIM_OLD}" Q1840_AIM_POS)
if(Q1840_AIM_POS EQUAL -1)
    message(FATAL_ERROR "Q16.12 could not find q1800 live door-aim block")
endif()
string(REPLACE "${Q1840_AIM_OLD}" "${Q1840_AIM_NEW}"
       Q1840_Q4_SOURCE "${Q1840_Q4_SOURCE}")

set(Q1840_DRAW_OLD "RenderFo3InteractionHudQ1790(mvp.m);")
set(Q1840_DRAW_NEW "RenderFo3InteractionHudQ1840(mvp.m, doorPromptQ1840_.c_str());")
string(FIND "${Q1840_Q4_SOURCE}" "${Q1840_DRAW_OLD}" Q1840_DRAW_POS)
if(Q1840_DRAW_POS EQUAL -1)
    message(FATAL_ERROR "Q16.12 could not find Q16.11 live HUD draw")
endif()
string(REPLACE "${Q1840_DRAW_OLD}" "${Q1840_DRAW_NEW}"
       Q1840_Q4_SOURCE "${Q1840_Q4_SOURCE}")

# Visible build proof: Q16.11 -> Q16.12.
set(Q1840_LABEL_OLD [==[
        q1600Digit(q1600X, 0x06u); // Q16.11: first 1 = B C
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x06u); // Q16.11: second 1 = B C
]==])
set(Q1840_LABEL_NEW [==[
        q1600Digit(q1600X, 0x06u); // Q16.12: 1 = B C
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x5Bu); // Q16.12: 2 = A B G E D
]==])
string(FIND "${Q1840_Q4_SOURCE}" "${Q1840_LABEL_OLD}" Q1840_LABEL_POS)
if(Q1840_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.12 could not find Q16.11 build-label digits")
endif()
string(REPLACE "${Q1840_LABEL_OLD}" "${Q1840_LABEL_NEW}"
       Q1840_Q4_SOURCE "${Q1840_Q4_SOURCE}")
string(REPLACE "Q16.11 BUILD LABEL:" "Q16.12 BUILD LABEL:"
       Q1840_Q4_SOURCE "${Q1840_Q4_SOURCE}")
string(REPLACE "text=Q16.11 anchor=left-hand" "text=Q16.12 anchor=left-hand"
       Q1840_Q4_SOURCE "${Q1840_Q4_SOURCE}")
string(REPLACE "Q16.11 AUTHORED DOOR FACING" "Q16.12 AUTHORED DOOR FACING"
       Q1840_Q4_SOURCE "${Q1840_Q4_SOURCE}")
file(WRITE "${Q1840_Q4_FILE}" "${Q1840_Q4_SOURCE}")

# -----------------------------------------------------------------------------
# 4. Configure-time proof: mature loader/environment survive; no hard-coded gate.
# -----------------------------------------------------------------------------
string(FIND "${Q1840_CELL_SOURCE}" "Q16.12 AUTHORED DOOR PROMPT:" Q1840_PROMPT_OK)
string(FIND "${Q1840_CELL_SOURCE}" "Q16.12 PROMPT METADATA LAZY:" Q1840_LAZY_OK)
string(FIND "${Q1840_NATIVE_SOURCE}" "CacheFo3DoorPromptQ1840(gpu.refFormId" Q1840_GPU_OK)
string(FIND "${Q1840_NATIVE_SOURCE}" "Q16.11 MATURE SCENE SWAP BEGIN:" Q1840_MATURE_OK)
string(FIND "${Q1840_NATIVE_SOURCE}" "LoadFo3CellEnvironmentQ1410(" Q1840_ENV_OK)
string(FIND "${Q1840_Q4_SOURCE}" "Q16.12 HUD TARGET HIDE:" Q1840_HIDE_OK)
string(FIND "${Q1840_Q4_SOURCE}" "RenderFo3InteractionHudQ1840" Q1840_DRAW_OK)
string(FIND "${Q1840_Q4_SOURCE}" "Q16.12: 2 = A B G E D" Q1840_LABEL_OK)
string(FIND "${Q1840_Q4_SOURCE}" "Open Gate to Wasteland" Q1840_HARDCODE_BAD)
if(Q1840_PROMPT_OK EQUAL -1 OR Q1840_LAZY_OK EQUAL -1 OR
   Q1840_GPU_OK EQUAL -1 OR Q1840_MATURE_OK EQUAL -1 OR
   Q1840_ENV_OK EQUAL -1 OR Q1840_HIDE_OK EQUAL -1 OR
   Q1840_DRAW_OK EQUAL -1 OR Q1840_LABEL_OK EQUAL -1 OR
   NOT Q1840_HARDCODE_BAD EQUAL -1)
    message(FATAL_ERROR "Q16.12 authored prompt / safe HUD verification failed")
endif()

message(STATUS "Q16.12 interaction enabled: ESM FULL door/destination prompt + single-string text_box.xml layout + GL-safe show/hide")
