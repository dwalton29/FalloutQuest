#include "fo3-pipboy-data.h"
#include "data/fo3-esm-reader.h"
#include "weapons/fo3-weapon-data.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <queue>
#include <functional>
namespace fo3pipdata {
namespace {
struct Sub {
  std::string type;
  const uint8_t *p;
  uint32_t n;
};
using Subs = std::vector<Sub>;
const Sub *Find(const Subs &s, const char *t) {
  for (auto &v : s)
    if (v.type == t)
      return &v;
  return nullptr;
}
uint32_t U(const Sub *s, uint32_t offset = 0) {
  return s && s->n >= offset + 4 ? fo3esm::ReadU32(s->p + offset) : 0;
}
std::string Text(const Subs &s, const char *t) {
  auto v = Find(s, t);
  return v ? fo3esm::ZString(v->p, v->n) : "";
}
Condition Cond(const Sub &s) {
  Condition c;
  if (s.n == 20 || s.n == 24 || s.n >= 28) {
    c.flags = s.p[0];
    c.value = fo3esm::ReadF32(s.p + 4);
    c.function = fo3esm::ReadU16(s.p + 8);
    c.a = fo3esm::ReadU32(s.p + 12);
    c.b = fo3esm::ReadU32(s.p + 16);
    if (s.n >= 24) c.run = fo3esm::ReadU32(s.p + 20);
    if (s.n >= 28) c.reference = fo3esm::ReadU32(s.p + 24);
  }
  return c;
}
Placement Place(uint32_t flags, const Subs &s, uint32_t cell, uint32_t world) {
  Placement p;
  p.flags = flags;
  p.cell = cell;
  p.world = world;
  p.base = U(Find(s, "NAME"));
  auto d = Find(s, "DATA");
  if (d && d->n == 24) {
    p.x = fo3esm::ReadF32(d->p);
    p.y = fo3esm::ReadF32(d->p + 4);
    p.z = fo3esm::ReadF32(d->p + 8);
    p.rx=fo3esm::ReadF32(d->p+12);p.ry=fo3esm::ReadF32(d->p+16);p.rz=fo3esm::ReadF32(d->p+20);
  }
  if(auto scale=Find(s,"XSCL");scale&&scale->n==4)p.scale=fo3esm::ReadF32(scale->p);
  p.owner=U(Find(s,"XOWN"));
  auto e = Find(s, "XESP");
  if (e && e->n >= 5) {
    p.parent = U(e);
    p.opposite = e->p[4] & 1;
  }
  return p;
}
} // namespace
std::string VoicePath(const Definitions &d, const Info &i, const Response &r,
                      uint32_t voice) {
  auto v = d.voices.find(voice);
  if (v == d.voices.end())
    return {};
  char tail[40];
  std::snprintf(tail, sizeof(tail), "_%08x_%u", i.id, r.number);
  // Resolve by exact INFO/response identity in the installed voice directory.
  // Never guess/truncate the authored filename prefix or synthesize a playlist.
  return "@voice:" + v->second + ":" + tail;
}
bool Relevant(const std::string &t) {
  return t == "MESG" || t == "ANIO" || t == "IDLM" || t == "FURN" || t == "TERM" || t == "RADS" || t == "WRLD" || t == "PERK" || t == "QUST" ||
         t == "MGEF" || t == "TACT" || t == "INFO" || t == "DIAL" ||
         t == "RACE" || t == "FLST" || t == "IDLE" || t == "SOUN" || t == "VTYP" || t == "SCPT" || t == "NPC_" || t == "PACK";
}
void Decode(Definitions &d, const std::string &t, uint32_t id, uint32_t flags,
            const std::vector<uint8_t> &bytes, uint32_t cell, uint32_t world,
            uint32_t topic) {
  Subs s;
  fo3esm::WalkSubrecords(bytes,
                         [&](const char *tag, const uint8_t *p, uint32_t n) {
                           s.push_back({std::string(tag, 4), p, n});
                         });
  const auto editor=Text(s,"EDID");
  if(!editor.empty()) { auto key=editor; for(auto& c:key)c=char(std::tolower((unsigned char)c));d.formNames[key]=id; }
  if(t=="FLST") {for(auto& v:s)if(v.type=="LNAM"&&v.n==4)d.formLists[id].push_back(U(&v));}
  else if(t=="RACE"){auto voices=Find(s,"VTCK");if(voices&&voices->n>=8)d.raceVoices[id]={U(voices),U(voices,4)};auto data=Find(s,"DATA");if(data&&data->n==36&&(U(data,32)&4))d.childRaces.insert(id);}
  else if(t=="FURN") {auto& f=d.furniture[id];f.model=Text(s,"MODL");f.editor=editor;auto m=Find(s,"MNAM");f.markers=U(m);f.valid=m&&m->n==4&&!f.model.empty();}
  else if(t=="IDLE") { d.idleModels[id]=Text(s,"MODL");d.idleParents[id]=U(Find(s,"ANAM")); for(const auto& v:s)if(v.type=="CTDA")d.idleConditions[id].push_back(Cond(v)); }
  else if(t=="TERM") {d.terminalBases.insert(id);}
  else if(t=="ANIO") {const auto idle=U(Find(s,"DATA"));if(idle)d.idleAnimationObjects.insert(idle);}
  else if(t=="IDLM") {
    auto& m=d.idleMarkers[id];const auto f=Find(s,"IDLF"),c=Find(s,"IDLC"),timer=Find(s,"IDLT"),a=Find(s,"IDLA");
    if(f&&f->n==1&&c&&(c->n==1||c->n==4)&&timer&&timer->n==4&&a&&a->n==uint32_t(c->p[0])*4){
      m.flags=f->p[0];m.timer=fo3esm::ReadF32(timer->p);m.valid=std::isfinite(m.timer)&&m.timer>=0&&(m.flags&~5u)==0;
      for(uint32_t off=0;off<a->n;off+=4)m.animations.push_back(U(a,off));
    }
  }
  else if (t == "RADS") {
    const auto data = Find(s, "DATA");
    if (data && data->n == 8)
      d.radiationStages[id] = {U(data), U(data, 4)};
  } else if (t == "DOOR")
    d.doorBases.insert(id);
  else if (t == "WRLD") {
    World w;
    w.name = Text(s, "FULL");
    w.image = Text(s, "ICON");
    w.parent = U(Find(s, "WNAM"));
    auto f = Find(s, "PNAM");
    if (f && f->n >= 2)
      w.parentFlags = fo3esm::ReadU16(f->p);
    auto m = Find(s, "MNAM");
    if (m && m->n == 16) {
      w.width = int32_t(U(m));
      w.height = int32_t(U(m, 4));
      w.nwX = int16_t(fo3esm::ReadU16(m->p + 8));
      w.nwY = int16_t(fo3esm::ReadU16(m->p + 10));
      w.seX = int16_t(fo3esm::ReadU16(m->p + 12));
      w.seY = int16_t(fo3esm::ReadU16(m->p + 14));
      w.valid = w.seX > w.nwX && w.nwY > w.seY && w.width > 0 && w.height > 0;
    }
    auto o = Find(s, "ONAM");
    if (o && o->n == 12) {
      w.scale = fo3esm::ReadF32(o->p);
      w.offsetX = fo3esm::ReadF32(o->p + 4);
      w.offsetY = fo3esm::ReadF32(o->p + 8);
    }
    f = Find(s, "DATA");
    if (f && f->n)
      w.flags = f->p[0];
    w.valid = w.valid && std::isfinite(w.scale) && w.scale > 0;
    d.worlds[id] = std::move(w);
  } else if (t == "CELL")
    d.cellWorlds[id] = world;
  else if (t == "REFR" || t == "ACHR" || t == "ACRE") {
    auto p = Place(flags, s, cell, world);
    p.linkedReference=U(Find(s,"XLKR"));
    if(auto wait=Find(s,"XPRD");wait&&wait->n==4)p.patrolWait=fo3esm::ReadF32(wait->p);
    p.patrolAction=U(Find(s,"INAM"))||U(Find(s,"TNAM"));
    for(const auto& v:s)if((v.type=="SCDA"&&v.n)||(v.type=="SCTX"&&v.n>1))p.patrolAction=true;
    if(t=="ACHR") d.referenceScripts[id]={editor,p.base};
    if (p.base == 0x10 && Find(s, "XMRK")) {
      Marker m;
      static_cast<Placement &>(m) = p;
      m.name = Text(s, "FULL");
      auto f = Find(s, "FNAM");
      if (f && f->n)
        m.mapFlags = f->p[0];
      f = Find(s, "TNAM");
      if (f && f->n)
        m.type = f->p[0];
      d.markers[id] = std::move(m);
    }
    auto r = Find(s, "XRDO");
    if (r && r->n == 16) {
      Transmitter x;
      static_cast<Placement &>(x) = p;
      x.radius = fo3esm::ReadF32(r->p);
      x.range = U(r, 4);
      x.staticPercent = fo3esm::ReadF32(r->p + 8);
      x.position = U(r, 12);
      d.transmitters[id] = x;
    } // Retain only quest targets after finalization.
    d.targets[id] = p;
  } else if (t == "QUST") {
    Quest q;
    q.name = Text(s, "FULL");
    q.editor = Text(s, "EDID");
    q.icon = Text(s, "ICON");
    q.script = U(Find(s, "SCRI"));
    auto v = Find(s, "DATA");
    if (v && v->n >= 2) {
      q.flags = v->p[0];
      q.priority = v->p[1];
    }
    if(!v||v->n<2)q.diagnostics.push_back("QUST "+std::to_string(id)+" malformed DATA");
    Stage *stage = nullptr;
    StageItem *item=nullptr;
    Objective *obj = nullptr;
    ObjectiveTarget *target=nullptr;
    auto invalid=[&](const Sub& a){q.diagnostics.push_back("QUST "+std::to_string(id)+" malformed/orphan "+a.type);};
    for (auto &a : s) {
      if (a.type == "INDX") {
        if(a.n!=2){invalid(a);stage=nullptr;}else {
          auto index=fo3esm::ReadU16(a.p);
          if(q.stages.count(index))invalid(a);
          q.stageOrder.push_back(index);stage=&q.stages[index];
        }
        obj=nullptr;item=nullptr;target=nullptr;
      } else if (a.type == "QOBJ") {
        if(a.n!=4){invalid(a);obj=nullptr;}else {
          auto index=U(&a);if(q.objectives.count(index))invalid(a);
          q.objectiveOrder.push_back(index);obj=&q.objectives[index];
        }
        stage=nullptr;item=nullptr;target=nullptr;
      } else if (a.type == "QSDT") {
        if(!stage||a.n!=1){invalid(a);item=nullptr;}else {
          stage->items.push_back({});item=&stage->items.back();item->flags=a.p[0];stage->flags|=item->flags;
        }
      } else if (a.type == "NNAM" && obj) obj->text=fo3esm::ZString(a.p,a.n);
      else if (a.type == "QSTA") {
        if(!obj||a.n!=8){invalid(a);target=nullptr;}else {
          obj->targets.push_back(U(&a));obj->targetItems.push_back({});target=&obj->targetItems.back();
          target->reference=U(&a);target->metadata.assign(a.p+4,a.p+a.n);
        }
      } else if (a.type == "CNAM") {
        if(!item)invalid(a);else {item->log=fo3esm::ZString(a.p,a.n);stage->logs.push_back(item->log);}
      } else if(a.type=="CTDA") {
        if(a.n!=20&&a.n!=24&&a.n!=28){invalid(a);continue;}
        if(obj){if(!target)invalid(a);else {target->conditions.push_back(Cond(a));obj->conditionalTargets=true;}}
        else if(stage){if(!item)invalid(a);else {item->conditions.push_back(Cond(a));stage->conditional=true;}}
        else q.conditions.push_back(Cond(a));
      } else if(a.type=="SCHR"||a.type=="SCDA"||a.type=="SCTX"||a.type=="SCRO"||a.type=="SCRV"||a.type=="SLSD"||a.type=="SCVR") {
        if(!item){invalid(a);continue;}
        auto& r=item->result;
        if(a.type=="SCHR"){if(a.n!=20)invalid(a);r.header.assign(a.p,a.p+a.n);}
        else if(a.type=="SCDA"){r.compiled.assign(a.p,a.p+a.n);stage->scripted|=a.n!=0;}
        else if(a.type=="SCTX"){r.source=fo3esm::ZString(a.p,a.n);stage->scripted|=!r.source.empty();}
        else if(a.type=="SCRO"||a.type=="SCRV"){if(a.n!=4)invalid(a);else r.references.push_back(U(&a));}
        // Retain every script subrecord, including reference kind and local metadata.
        r.records.push_back({a.type,std::vector<uint8_t>(a.p,a.p+a.n)});
      }
    }
    d.questNames[q.editor] = id;
    d.quests[id] = std::move(q);
  } else if (t == "MESG") {
    MessageDefinition message;
    message.title=Text(s,"FULL");
    message.text=Text(s,"DESC");
    for(const auto& sub:s)if(sub.type=="ITXT")message.buttons.push_back(fo3esm::ZString(sub.p,sub.n));
    message.displayFlags=U(Find(s,"DNAM"));
    d.messages[id]=std::move(message);
  } else if (t == "PERK") {
    Perk p;
    p.name = Text(s, "FULL");
    p.description = Text(s, "DESC");
    p.icon = Text(s, "ICON");
    auto a = Find(s, "DATA");
    if (a && a->n >= 4) {
      p.minLevel = a->p[1];
      p.ranks = a->p[2];
      p.hidden = a->n > 4 && a->p[4];
    }
    PerkEffect *e = nullptr;
    uint8_t tab = 0;
    for (auto &v : s) {
      if (v.type == "PRKE" && v.n == 3) {
        p.effects.push_back({});
        e = &p.effects.back();
        e->type = v.p[0];
        e->rank = v.p[1];
        e->priority = v.p[2];
      } else if (v.type == "PRKF")
        e = nullptr;
      else if (e) {
        if (v.type == "DATA") {
          if (e->type == 0 && v.n == 8) {
            e->form = U(&v);
            e->stage = v.p[4];
          } else if (e->type == 1 && v.n == 4)
            e->form = U(&v);
          else if (e->type == 2 && v.n == 3) {
            e->entryPoint = v.p[0];
            e->function = v.p[1];
          }
        } else if (v.type == "PRKC" && v.n)
          tab = v.p[0];
        else if (v.type == "CTDA")
          e->conditions[tab].push_back(Cond(v));
        else if (v.type == "EPFT" && v.n)
          e->parameterType = v.p[0];
        else if (v.type == "EPFD")
          e->parameters.assign(v.p, v.p + v.n);
        else if (v.type == "SCDA" && v.n)
          e->scripted = true;
      }
    }
    d.perks[id] = std::move(p);
  } else if (t == "NOTE") {
    Note n;
    n.name = Text(s, "FULL");
    n.icon = Text(s, "ICON");
    n.image = Text(s, "XNAM");
    auto a = Find(s, "DATA");
    if (a && a->n)
      n.type = a->p[0];
    if (n.type == 1)
      n.text = Text(s, "TNAM");
    if (n.type == 0)
      n.sound = U(Find(s, "SNAM"));
    if (n.type == 3) {
      n.topic = U(Find(s, "TNAM"));
      n.npc = U(Find(s, "SNAM"));
    }
    d.notes[id] = std::move(n);
  } else if (t == "ALCH" || t == "INGR") {
    Ingestible a;
    a.flags = U(Find(s, "ENIT"), 8);
    Effect *e = nullptr;
    for (auto &v : s) {
      if (v.type == "EFID" && v.n == 4) {
        a.effects.push_back({});
        e = &a.effects.back();
        e->id = U(&v);
      } else if (v.type == "EFIT" && v.n == 20 && e) {
        e->magnitude = U(&v);
        e->area = U(&v, 4);
        e->duration = U(&v, 8);
        e->range = U(&v, 12);
        e->av = U(&v, 16);
      } else if (v.type == "CTDA" && e)
        e->conditional = true;
    }
    d.aid[id] = std::move(a);
  } else if (t == "MGEF") {
    auto a = Find(s, "DATA");
    if (a && a->n >= 72)
      d.magic[id] = {U(a), U(a, 64), U(a, 68), U(a, 8)};
  } else if (t == "TACT" && (flags & 0x20000)) {
    d.stations[id] = {Text(s, "FULL"), U(Find(s, "VNAM")), U(Find(s, "SNAM"))};
  } else if (t == "SOUN") {
    auto path = Text(s, "FNAM");
    std::replace(path.begin(), path.end(), '\\', '/');
    std::transform(path.begin(), path.end(), path.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    if (!path.empty() && path.rfind("sound/", 0) != 0)
      path = "sound/" + path;
    d.sounds[id] = std::move(path);
  } else if (t == "VTYP")
    d.voices[id] = Text(s, "EDID");
  else if (t == "NPC_") {
    d.npcVoices[id] = U(Find(s, "VTCK"));
    ActorDefinition actor;actor.editor=editor;actor.name=Text(s,"FULL");
    actor.race=U(Find(s,"RNAM"));actor.actorClass=U(Find(s,"CNAM"));actor.voice=d.npcVoices[id];
    actor.script=U(Find(s,"SCRI"));actor.templateActor=U(Find(s,"TPLT"));
    actor.female=(U(Find(s,"ACBS"))&1)!=0;
    auto acbs=Find(s,"ACBS");if(acbs&&acbs->n>=24){actor.templateFlags=fo3esm::ReadU16(acbs->p+22);actor.karma=fo3esm::ReadF32(acbs->p+16);actor.disposition=int16_t(fo3esm::ReadU16(acbs->p+20));}
    actor.combatStyle=U(Find(s,"ZNAM"));
    for(auto& v:s) {
      if(v.type=="SNAM"&&v.n>=5)actor.factions[U(&v)]=int8_t(v.p[4]);
      if(v.type=="PKID"&&v.n==4)actor.packages.push_back(U(&v));
      if(v.type=="AIDT")actor.aiData.assign(v.p,v.p+v.n);
    }
    d.dialogueActors[id]=std::move(actor);
    if (id == 7)
      d.playerFemale = (U(Find(s, "ACBS")) & 1) != 0;
  } else if (t == "PACK") {
    PackageDefinition package;
    package.editor=editor;
    if(auto data=Find(s,"PKDT");data&&data->n>=8) {
      package.flags=U(data);
      package.type=data->p[4];
      package.behaviorFlags=fo3esm::ReadU16(data->p+6);
      if(data->n>=10) package.typeFlags=fo3esm::ReadU16(data->p+8);
    }
    auto location=[&](const Sub* v,PackageLocation& out){
      if(!v||v->n<12)return;
      out.type=U(v);out.value=U(v,4);out.radius=static_cast<int32_t>(U(v,8));out.valid=true;
    };
    location(Find(s,"PLDT"),package.location);
    location(Find(s,"PLD2"),package.location2);
    location(Find(s,"PTDT"),package.target);
    location(Find(s,"PTD2"),package.target2);
    if(auto style=Find(s,"CNAM")){package.combatStyleValid=style->n==4;if(package.combatStyleValid)package.combatStyle=U(style);}
    if(auto repeat=Find(s,"PKPT");repeat&&repeat->n)package.patrolRepeat=repeat->p[0]!=0;
    if(auto distance=Find(s,"PKE2");distance&&distance->n==4){package.escortDistance=U(distance);package.escortDistanceValid=true;}
    if(auto schedule=Find(s,"PSDT");schedule&&schedule->n==8) {
      package.schedule.month=static_cast<int8_t>(schedule->p[0]);
      package.schedule.weekday=static_cast<int8_t>(schedule->p[1]);
      package.schedule.date=schedule->p[2];
      package.schedule.hour=static_cast<int8_t>(schedule->p[3]);
      package.schedule.duration=static_cast<int32_t>(U(schedule,4));
      package.schedule.valid=true;
    }
    // PACK POBA/POEA/POCA delimit OnBegin/OnEnd/OnChange. Never treat a
    // script in one event as permission to silently skip the others.
    int event=0;
    for(auto& v:s) {
      if(v.type=="POBA"){event=1;continue;}
      if(v.type=="POEA"){event=2;continue;}
      if(v.type=="POCA"){event=3;continue;}
      if(v.type=="CTDA")package.conditions.push_back(Cond(v));
      else if((v.type=="INAM"||v.type=="TNAM")&&v.n==4&&U(&v))package.procedureActions=true;
      else if(v.type=="SCDA"&&v.n){package.scripted=true;if(event!=1)package.otherProcedureScript=true;}
      else if(v.type=="SCTX"&&v.n>1) {
        package.scripted=true;
        if(event==1) {
          if(!package.onBeginScript.empty())package.onBeginScript+="\n";
          package.onBeginScript+=fo3esm::ZString(v.p,v.n);
        }else package.otherProcedureScript=true;
      }
      // Empty SCHR script headers are not executable actions.
    }
    d.packages[id]=std::move(package);
  } else if (t == "DIAL") {
    auto name = Text(s, "EDID");
    d.topicNames[id] = name;
    Topic entry;entry.editor=name;entry.text=Text(s,"FULL");
    auto data=Find(s,"DATA");if(data&&data->n)entry.type=data->p[0];
    if(data&&data->n>=2)entry.flags=data->p[1];
    auto priority=Find(s,"PNAM");if(priority&&priority->n==4)entry.priority=fo3esm::ReadF32(priority->p);
    for(auto& v:s)if(v.type=="QSTI"&&v.n==4)entry.quests.push_back(U(&v));
    d.dialogueTopics[id]=std::move(entry);
    if (name == "RadioHello")
      d.radioHello = id;
  } else if (t == "SCPT") {
    Script script;
    uint32_t index = 0;
    for (auto &a : s) {
      if(a.type=="SCTX")script.source=fo3esm::ZString(a.p,a.n);
      if(a.type=="SCDA")script.compiled.assign(a.p,a.p+a.n);
      if(a.type=="SCHR")script.header.assign(a.p,a.p+a.n);
      if((a.type=="SCRO"||a.type=="SCRV")&&a.n==4)script.references.push_back(U(&a));
      script.records.push_back({a.type,std::vector<uint8_t>(a.p,a.p+a.n)});
      if (a.type == "SLSD" && a.n >= 4)
        index = U(&a);
      if (a.type == "SCVR")
        script.variables[index] = fo3esm::ZString(a.p, a.n);
    }
    d.scripts[id] = std::move(script);
  } else if (t == "INFO") {
    Info i;
    i.id = id;
    i.topic = U(Find(s,"TPIC"));if(!i.topic)i.topic=topic;
    i.recordFlags=flags;i.previous=U(Find(s,"PNAM"));i.prompt=Text(s,"RNAM");
    i.challenge=U(Find(s,"KNAM"));i.challengeValue=U(Find(s,"DNAM"));
    i.quest = U(Find(s, "QSTI"));
    i.speaker = U(Find(s, "ANAM"));
    auto a = Find(s, "DATA");
    if (a && a->n >= 3) {
      i.type = a->p[0];
      i.flags = a->p[2];i.nextSpeaker=a->p[1];if(a->n>=4)i.flags2=a->p[3];
    }
    bool compiled = false, source = false;
    ResultScript* result=&i.begin;
    for (auto &v : s) {
      if (v.type == "TRDT" && v.n >= 16) {
        Response r;r.sound=U(&v,16);r.number=v.p[12];r.emotion=U(&v);r.emotionValue=int32_t(U(&v,4));
        if(v.n>=21)r.flags=v.p[20];
        i.responses.push_back(std::move(r));
      }
      else if (v.type == "NAM1" && !i.responses.empty())
        i.responses.back().text = fo3esm::ZString(v.p, v.n);
      else if (v.type == "CTDA")
        i.conditions.push_back(Cond(v));
      else if (v.type == "TCLT" && v.n == 4)
        i.links.push_back(U(&v));
      else if(v.type=="NAME"&&v.n==4)i.addedTopics.push_back(U(&v));
      else if(v.type=="TCLF"&&v.n==4)i.linksFrom.push_back(U(&v));
      else if(v.type=="NAM2"&&!i.responses.empty())i.responses.back().notes=fo3esm::ZString(v.p,v.n);
      else if(v.type=="NAM3"&&!i.responses.empty())i.responses.back().edits=fo3esm::ZString(v.p,v.n);
      else if(v.type=="SNAM"&&!i.responses.empty())i.responses.back().speakerAnimation=U(&v);
      else if(v.type=="LNAM"&&!i.responses.empty())i.responses.back().listenerAnimation=U(&v);
      else if(v.type=="NEXT")result=&i.end;
      else if(v.type=="SCHR")result->header.assign(v.p,v.p+v.n);
      else if(v.type=="SCRO"&&v.n==4)result->references.push_back(U(&v));
      else if (v.type == "SCDA" && v.n) {
        compiled = true;result->compiled.assign(v.p,v.p+v.n);
      }
      else if (v.type == "SCTX" && v.n) {
        source = true;
        i.scripts.push_back(fo3esm::ZString(v.p, v.n));result->source=i.scripts.back();
      }
    }
    i.compiledOnly = compiled && !source;
    i.compiledOnly=(!i.begin.compiled.empty()&&i.begin.source.empty())||(!i.end.compiled.empty()&&i.end.source.empty());
    d.topics[i.topic].push_back(std::move(i));
  }
}
void Finalize(Definitions &d) {
  // INFO editor ordering is a PNAM predecessor relation, not FormID order.
  // Prepare it once. Radio keeps its existing authored playlist ordering.
  for(auto& topic:d.topics) {
    auto type=d.dialogueTopics.find(topic.first);
    if(type==d.dialogueTopics.end()||(type->second.type!=0&&type->second.type!=3))continue;
    auto& infos=topic.second;std::unordered_map<uint32_t,size_t> index;
    for(size_t i=0;i<infos.size();++i)index[infos[i].id]=i;
    std::vector<std::vector<size_t>> children(infos.size());std::vector<size_t> indegree(infos.size());
    for(size_t i=0;i<infos.size();++i){auto prev=index.find(infos[i].previous);if(prev!=index.end()){children[prev->second].push_back(i);++indegree[i];}}
    std::priority_queue<size_t,std::vector<size_t>,std::greater<size_t>> ready;
    for(size_t i=0;i<infos.size();++i)if(!indegree[i])ready.push(i);
    std::vector<size_t> order;order.reserve(infos.size());
    while(!ready.empty()){auto i=ready.top();ready.pop();order.push_back(i);for(auto next:children[i])if(!--indegree[next])ready.push(next);}
    if(order.size()==infos.size()){std::vector<Info> sorted;sorted.reserve(infos.size());for(auto i:order)sorted.push_back(std::move(infos[i]));infos=std::move(sorted);}
    else infos.clear(); // Cyclic authored order is unusable, never guessed.
  }
  // PNAM is the authored Previous INFO relationship, not FormID order.
  // Keep the existing radio sequencing policy separate from normal dialogue.
  for(auto& topic:d.topics) {
    auto td=d.dialogueTopics.find(topic.first);if(td==d.dialogueTopics.end()||td->second.type==7)continue;
    auto& infos=topic.second;std::unordered_map<uint32_t,size_t> indices;
    std::unordered_map<uint32_t,std::vector<size_t>> following;
    for(size_t i=0;i<infos.size();++i)indices[infos[i].id]=i;
    for(size_t i=0;i<infos.size();++i)if(infos[i].previous&&indices.count(infos[i].previous))following[infos[i].previous].push_back(i);
    std::vector<size_t> order,stack;std::vector<bool> visited(infos.size(),false);
    auto chain=[&](size_t first){stack.push_back(first);while(!stack.empty()){
      const auto i=stack.back();stack.pop_back();if(visited[i])continue;visited[i]=true;order.push_back(i);
      auto next=following.find(infos[i].id);if(next!=following.end())for(auto n=next->second.rbegin();n!=next->second.rend();++n)stack.push_back(*n);
    }};
    for(size_t i=0;i<infos.size();++i)if(!infos[i].previous||!indices.count(infos[i].previous))chain(i);
    // Malformed cyclic ordering cannot participate in a canonical dialogue.
    for(size_t i=0;i<infos.size();++i)if(!visited[i])infos[i].orderValid=false;
    for(size_t i=0;i<infos.size();++i)if(!visited[i])chain(i);
    std::vector<Info> sorted;sorted.reserve(infos.size());for(auto i:order)sorted.push_back(std::move(infos[i]));infos=std::move(sorted);
  }
  d.greetings.clear();d.topLevelTopics.clear();
  for(auto& t:d.dialogueTopics) {
    if(t.second.editor=="GREETING")d.greetings.push_back(t.first);
    if(t.second.type==0&&(t.second.flags&2))d.topLevelTopics.push_back(t.first);
  }
  std::sort(d.topLevelTopics.begin(),d.topLevelTopics.end(),[&](auto a,auto b){
    if(d.dialogueTopics.at(a).priority!=d.dialogueTopics.at(b).priority)return d.dialogueTopics.at(a).priority>d.dialogueTopics.at(b).priority;
    return a<b;
  });
  // Compile bounded authored chains once, before reference pruning.
  const auto compile=[&](uint32_t start,std::vector<PatrolPoint>& points,bool& circular,std::string& error) {
    points.clear();circular=false;error.clear();uint32_t ref=start;std::unordered_set<uint32_t> visited;
    while(ref) {
      if(!visited.insert(ref).second){if(ref==start)circular=true;else error="Patrol chain loops into an intermediate marker";break;}
      if(visited.size()>256){error="Patrol chain exceeds bounded marker count";break;}
      const auto i=d.targets.find(ref);
      if(i==d.targets.end()){error="Patrol marker unavailable";break;}
      const auto& marker=i->second;
      if(marker.patrolAction||!std::isfinite(marker.patrolWait)||marker.patrolWait<0){error="Patrol marker action/script or invalid wait unsupported";break;}
      if(!points.empty()&&(marker.cell!=points.front().placement.cell||marker.world!=points.front().placement.world)){error="Patrol requires cell traversal";break;}
      points.push_back({ref,marker});ref=marker.linkedReference;
    }
    if(points.empty()&&error.empty())error="Patrol has no markers";
    if(!error.empty())points.clear();
  };
  d.actorPatrols.clear();d.actorPatrolUnsupported.clear();d.actorPatrolCircular.clear();
  for(auto& entry:d.packages) {
    auto& p=entry.second;if(p.type!=13)continue;
    p.patrol.clear();p.patrolCircular=false;p.patrolUnsupported.clear();
    if(p.location.valid&&p.location.type==0)compile(p.location.value,p.patrol,p.patrolCircular,p.patrolUnsupported);
    else if(p.location.valid&&p.location.type==6) {
      // Linked Ref is the actor's XLKR, not the unused PLDT value. Resolve
      // only actual actor PKID lists (including authored template inheritance).
      for(const auto& actor:d.referenceScripts) {
        const auto* def=ActorCategory(d,actor.second.second,16);
        if(!def||std::find(def->packages.begin(),def->packages.end(),entry.first)==def->packages.end())continue;
        const uint64_t key=(uint64_t(actor.first)<<32)|entry.first;bool circular=false;std::string error;
        const auto placement=d.targets.find(actor.first);
        compile(placement==d.targets.end()?0:placement->second.linkedReference,d.actorPatrols[key],circular,error);
        if(circular)d.actorPatrolCircular.insert(key);
        if(!error.empty())d.actorPatrolUnsupported[key]=error;
      }
    } else p.patrolUnsupported="Patrol starting location resolver unsupported";
  }
  std::unordered_set<uint32_t> retain;
  for(auto& actor:d.referenceScripts)retain.insert(actor.first);
  for(const auto& route:d.actorPatrols)for(const auto& point:route.second)retain.insert(point.reference);
  for(const auto& entry:d.packages) {
    const auto keep=[&](const PackageLocation& location) {
      if(location.valid&&(location.type==0u||location.type==6u)&&location.value)
        retain.insert(location.value);
    };
    keep(entry.second.location);keep(entry.second.location2);
    for(const auto& point:entry.second.patrol)retain.insert(point.reference);
    if(entry.second.target.valid&&entry.second.target.type==0)retain.insert(entry.second.target.value);
    if(entry.second.target2.valid&&entry.second.target2.type==0)retain.insert(entry.second.target2.value);
  }
  for (auto &p : d.targets)
    if(d.furniture.count(p.second.base)||d.idleMarkers.count(p.second.base))retain.insert(p.first);
  for (auto &p : d.targets)
    if (d.doorBases.count(p.second.base)) {
      d.doors[p.second.cell].push_back(p.second);
      // The XTEL graph and npc door permission checker both require this
      // authored REFR after quest-target pruning. A valid DOOR must never
      // become unreachable only because it is not a quest objective.
      retain.insert(p.first);
    }
  for (auto &q : d.quests)
    for (auto &o : q.second.objectives)
      for (auto id : o.second.targets)
        retain.insert(id);
  for (auto &t : d.transmitters)
    if (t.second.position)
      retain.insert(t.second.position);
  for (auto &t : d.targets)
    if (t.second.parent)
      retain.insert(t.second.parent);
  for (auto i = d.targets.begin(); i != d.targets.end();)
    if (!retain.count(i->first))
      i = d.targets.erase(i);
    else
      ++i;
}
std::vector<std::string> BroadcastAudio(const Definitions &d, const Info &i,
                                        uint32_t voice) {
  std::vector<std::string> paths;
  if (i.speaker) {
    auto v = d.npcVoices.find(i.speaker);
    if (v != d.npcVoices.end())
      voice = v->second;
  }
  for (auto &r : i.responses) {
    auto s = d.sounds.find(r.sound);
    auto path = s != d.sounds.end() ? s->second : VoicePath(d, i, r, voice);
    if (path.empty())
      return {};
    paths.push_back(path);
  }
  return paths;
}
std::vector<std::string> NoteAudio(const Definitions &d, uint32_t id) {
  std::vector<std::string> paths;
  auto n = d.notes.find(id);
  if (n == d.notes.end())
    return paths;
  auto sound = d.sounds.find(n->second.sound);
  if (n->second.type == 0 && sound != d.sounds.end())
    paths.push_back(sound->second);
  if (n->second.type == 3) {
    auto v = d.npcVoices.find(n->second.npc);
    auto infos = d.topics.find(n->second.topic);
    if (v == d.npcVoices.end() || infos == d.topics.end())
      return paths;
    for (auto &i : infos->second) {
      bool eligible = true;
      for (auto &c : i.conditions) {
        float value = 0;
        if (c.run != 0 || (c.flags & 5)) {
          eligible = false;
          break;
        }
        if (c.function == 72)
          value = n->second.npc == c.a;
        else if (c.function == 131)
          value = uint32_t(d.playerFemale) == c.a;
        else {
          eligible = false;
          break;
        }
        bool pass = false;
        switch (c.flags >> 5) {
        case 0:
          pass = value == c.value;
          break;
        case 1:
          pass = value != c.value;
          break;
        default:
          break;
        }
        if (!pass) {
          eligible = false;
          break;
        }
      }
      if (!eligible)
        continue; // Authored note topic is fixed; no world
                  // quest conditions are executed here.
      for (auto &r : i.responses) {
        auto s = d.sounds.find(r.sound);
        auto path =
            s != d.sounds.end() ? s->second : VoicePath(d, i, r, v->second);
        if (!path.empty())
          paths.push_back(path);
      }
    }
  }
  return paths;
}
} // namespace fo3pipdata

namespace fo3pipdata {
void FinalizeLevelledCategories(Definitions& d,const fo3weapon::Definitions& records) {
  d.invariantActorCategories.clear();
  // Only resolve categories needed by resident AI. Every variant must inherit
  // the SAME original source record, rather than picking a random variant.
  for(uint16_t category:{4,8,16})for(const auto& list:records.levelledActors) {
    std::unordered_set<uint32_t> visiting;unsigned budget=0;
    std::function<uint32_t(uint32_t,unsigned)> source=[&](uint32_t id,unsigned depth)->uint32_t {
      if(depth>=16||budget++>=65536||!visiting.insert(id).second)return 0;
      uint32_t result=0;const auto npc=d.dialogueActors.find(id);
      if(npc!=d.dialogueActors.end())result=npc->second.templateActor&&(npc->second.templateFlags&category)?source(npc->second.templateActor,depth+1):id;
      else {
        const auto l=records.levelledActors.find(id);
        if(l!=records.levelledActors.end()&&l->second.valid&&!l->second.chanceNone&&!l->second.chanceGlobal&&l->second.flags<=1) {
          for(const auto& entry:l->second.entries) {
            if(entry.level!=1||entry.count!=1){result=0;break;}
            const auto at=source(entry.actor,depth+1);
            if(!at||(result&&result!=at)){result=0;break;}
            result=at;
          }
        }
      }
      visiting.erase(id);return result;
    };
    if(const auto id=source(list.first,0))d.invariantActorCategories[(uint64_t(list.first)<<16)|category]=id;
  }
}
const ActorDefinition* ActorCategory(const Definitions& d,uint32_t base,uint16_t category) {
  std::array<uint32_t,16> seen{};size_t depth=0;
  while(base&&depth<seen.size()) {
    if(std::find(seen.begin(),seen.begin()+depth,base)!=seen.begin()+depth)return nullptr;
    seen[depth++]=base;
    auto actor=d.dialogueActors.find(base);if(actor==d.dialogueActors.end()){
      const auto shared=d.invariantActorCategories.find((uint64_t(base)<<16)|category);
      if(shared==d.invariantActorCategories.end())return nullptr;
      base=shared->second;continue;
    }
    if(actor->second.templateActor&&(actor->second.templateFlags&category))base=actor->second.templateActor;
    else return &actor->second;
  }
  return nullptr;
}
uint32_t ActorVoice(const Definitions& d,uint32_t base) {
  const auto* actor=ActorCategory(d,base,1);if(!actor)return 0;
  if(actor->voice)return actor->voice;
  auto race=d.raceVoices.find(actor->race);return race==d.raceVoices.end()?0:race->second[actor->female?1:0];
}
}
