#pragma once
#include <cstdint>
#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace fo3pipdata {
struct Condition {
  uint8_t flags = 0;
  uint16_t function = 0;
  float value = 0;
  uint32_t a = 0, b = 0, run = 0, reference = 0;
};
struct Point {
  float x = 0, y = 0;
};
struct World {
  std::string name, image;
  uint32_t parent = 0;
  uint16_t parentFlags = 0;
  int32_t width = 0, height = 0;
  int16_t nwX = 0, nwY = 0, seX = 0, seY = 0;
  float scale = 1, offsetX = 0, offsetY = 0;
  uint8_t flags = 0;
  bool valid = false;
  Point Project(float x, float y) const {
    return {(x / 4096.f * scale + offsetX - nwX) / (seX - nwX),
            (nwY - y / 4096.f * scale - offsetY) / (nwY - seY)};
  }
  Point Unproject(Point p) const {
    return {((nwX + p.x * (seX - nwX)) - offsetX) / scale * 4096,
            ((nwY - p.y * (nwY - seY)) - offsetY) / scale * 4096};
  }
};
struct Placement {
  uint32_t base = 0, cell = 0, world = 0, flags = 0, parent = 0;
  bool opposite = false;
  uint32_t linkedReference=0;
  float patrolWait=0;
  bool patrolAction=false;
  float x = 0, y = 0, z = 0;
};
struct Marker : Placement {
  std::string name;
  uint8_t type = 0, mapFlags = 0;
};
struct Objective {
  std::string text;
  std::vector<uint32_t> targets;
  bool conditionalTargets = false;
};
struct Stage {
  uint8_t flags = 0;
  std::vector<std::string> logs;
  bool scripted = false, conditional = false;
};
struct Quest {
  std::string name, editor, icon;
  uint32_t script = 0;
  uint8_t flags = 0, priority = 0;
  std::unordered_map<uint16_t, Stage> stages;
  std::unordered_map<uint32_t, Objective> objectives;
  std::vector<Condition> conditions;
};
struct PerkEffect {
  uint8_t type = 0, rank = 0, priority = 0, stage = 0, entryPoint = 0,
          function = 0, parameterType = 0;
  uint32_t form = 0;
  std::vector<uint8_t> parameters;
  std::unordered_map<uint8_t, std::vector<Condition>> conditions;
  bool scripted = false;
};
struct Perk {
  std::string name, description, icon;
  uint8_t ranks = 0, minLevel = 0;
  bool hidden = false;
  std::vector<PerkEffect> effects;
};
struct Note {
  std::string name, text, icon, image;
  uint8_t type = 0;
  uint32_t sound = 0, npc = 0, topic = 0;
};
struct Effect {
  uint32_t id = 0, magnitude = 0, area = 0, duration = 0, range = 0, av = 0;
  bool conditional = false;
};
struct Magic {
  uint32_t flags = 0, archetype = 0, av = 0, script = 0;
};
struct Ingestible {
  std::vector<Effect> effects;
  uint32_t flags = 0;
};
struct Topic {
  std::string editor, text;
  uint8_t type=0, flags=0;
  float priority=50;
  std::vector<uint32_t> quests;
};
struct ActorDefinition {
  std::string editor, name;
  uint32_t race=0, actorClass=0, voice=0, script=0, templateActor=0,combatStyle=0;
  float karma=0;int16_t disposition=0;
  uint16_t templateFlags=0;
  bool female=false;
  std::unordered_map<uint32_t,int8_t> factions;
  std::vector<uint32_t> packages;
  std::vector<uint8_t> aiData;
};
struct PackageLocation {
  uint32_t type=0xffffffffu,value=0;
  int32_t radius=0;
  bool valid=false;
};
struct PackageSchedule {
  int8_t month=-1,weekday=-1,hour=-1;
  uint8_t date=0;
  int32_t duration=0;
  bool valid=false;
};
struct PatrolPoint { uint32_t reference=0; Placement placement; };
struct PackageDefinition {
  std::string editor;
  uint32_t flags=0,combatStyle=0;
  bool combatStyleValid=true;
  uint8_t type=0xff;
  uint16_t behaviorFlags=0,typeFlags=0;
  PackageLocation location,location2,target,target2;
  PackageSchedule schedule;
  bool patrolRepeat=true,patrolCircular=false;
  uint32_t escortDistance=0;bool escortDistanceValid=false;
  std::vector<PatrolPoint> patrol;
  std::string patrolUnsupported;
  std::vector<Condition> conditions;
  bool scripted=false,procedureActions=false;
};
struct Response {
  Response()=default;
  Response(uint32_t s,uint8_t n,std::string t):sound(s),number(n),text(std::move(t)){}
  uint32_t sound = 0;
  uint8_t number = 0;
  std::string text;
  uint32_t emotion=0, speakerAnimation=0, listenerAnimation=0;
  int32_t emotionValue=0;
  uint8_t flags=0;
  std::string notes, edits;
};
struct ResultScript {
  std::string source;
  std::vector<uint8_t> compiled, header;
  std::vector<uint32_t> references;
};
struct Info {
  uint32_t id = 0, topic = 0, quest = 0, speaker = 0;
  uint8_t type = 0, flags = 0;
  std::vector<Condition> conditions;
  std::vector<Response> responses;
  std::vector<uint32_t> links;
  std::vector<std::string> scripts;
  bool compiledOnly = false, orderValid=true;
  uint32_t previous=0, challenge=0, challengeValue=0, recordFlags=0;
  uint8_t nextSpeaker=0, flags2=0;
  std::string prompt;
  std::vector<uint32_t> addedTopics, linksFrom;
  ResultScript begin, end;
};
struct Station {
  std::string name;
  uint32_t voice = 0, sound = 0;
};
struct Transmitter : Placement {
  float radius = 0, staticPercent = 0;
  uint32_t range = 0, position = 0;
};
struct Script {
  std::string source;
  std::unordered_map<uint32_t, std::string> variables;
};
struct RadiationStage {
  uint32_t threshold = 0, spell = 0;
};
struct Definitions {
  std::unordered_map<uint32_t, RadiationStage> radiationStages;
  std::unordered_map<uint32_t, World> worlds;
  std::unordered_set<uint32_t> doorBases;
  std::unordered_map<uint32_t, std::vector<Placement>> doors;
  std::unordered_map<uint32_t, Marker> markers;
  std::unordered_map<uint32_t, Placement> targets;
  std::unordered_map<uint32_t, Quest> quests;
  std::unordered_map<uint32_t, Perk> perks;
  std::unordered_map<uint32_t, Note> notes;
  std::unordered_map<uint32_t, Ingestible> aid;
  std::unordered_map<uint32_t, Magic> magic;
  std::unordered_map<uint32_t, Station> stations;
  std::unordered_map<uint32_t, Transmitter> transmitters;
  std::unordered_map<uint32_t, std::vector<Info>> topics;
  std::unordered_map<uint32_t, std::string> topicNames, voices, sounds;
  std::unordered_map<uint32_t, uint32_t> npcVoices, cellWorlds;
  std::unordered_map<uint32_t, Script> scripts;
  std::unordered_map<std::string, uint32_t> questNames;
  bool playerFemale = false;
  std::unordered_map<uint32_t,std::vector<uint32_t>> formLists;
  std::unordered_map<uint32_t,std::array<uint32_t,2>> raceVoices;
  std::unordered_map<uint32_t,Topic> dialogueTopics;
  std::unordered_map<uint32_t,ActorDefinition> dialogueActors;
  std::unordered_map<uint64_t,uint32_t> invariantActorCategories;
  std::unordered_map<uint32_t,PackageDefinition> packages;
  std::unordered_map<uint64_t,std::vector<PatrolPoint>> actorPatrols;
  std::unordered_map<uint64_t,std::string> actorPatrolUnsupported;
  std::unordered_set<uint64_t> actorPatrolCircular;
  std::unordered_map<uint32_t,std::pair<std::string,uint32_t>> referenceScripts;
  std::unordered_map<std::string,uint32_t> formNames;
  std::unordered_map<uint32_t,std::string> idleModels;
  std::vector<uint32_t> greetings, topLevelTopics;
  uint32_t radioHello = 0;
  float discoveryRadius = 0;
};
bool Relevant(const std::string &type);
void Decode(Definitions &, const std::string &, uint32_t id, uint32_t flags,
            const std::vector<uint8_t> &payload, uint32_t cell, uint32_t world,
            uint32_t topic);
void Finalize(Definitions &);
std::vector<std::string> NoteAudio(const Definitions &, uint32_t note);
} // namespace fo3pipdata

namespace fo3weapon {struct Definitions;}
namespace fo3pipdata {void FinalizeLevelledCategories(Definitions&,const fo3weapon::Definitions&);}

namespace fo3pipdata {
std::string VoicePath(const Definitions &, const Info &, const Response &, uint32_t voice);
uint32_t ActorVoice(const Definitions &,uint32_t base);
const ActorDefinition* ActorCategory(const Definitions&,uint32_t base,uint16_t category);
}
