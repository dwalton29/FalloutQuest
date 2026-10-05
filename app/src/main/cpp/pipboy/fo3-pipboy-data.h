#pragma once
#include <cstdint>
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
struct Response {
  uint32_t sound = 0;
  uint8_t number = 0;
  std::string text;
};
struct Info {
  uint32_t id = 0, topic = 0, quest = 0, speaker = 0;
  uint8_t type = 0, flags = 0;
  std::vector<Condition> conditions;
  std::vector<Response> responses;
  std::vector<uint32_t> links;
  std::vector<std::string> scripts;
  bool compiledOnly = false;
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
  std::unordered_map<uint32_t, std::string> variables;
};
struct Definitions {
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
