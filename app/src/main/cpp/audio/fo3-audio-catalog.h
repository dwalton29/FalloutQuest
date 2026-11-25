#pragma once
#include <cstdint>
#include <string>
#include <array>
#include <vector>
#include <memory>
#include "world/fo3-reference-state.h"
#include <unordered_map>
namespace fo3audio {
struct Sound {
  std::string path;
  float gain = 1;
  bool loop = false;
  float nearDistance=0,farDistance=0;
  uint8_t startTime=0,endTime=0;
  bool positional=true;
  float GainAt(float distance) const;
};
struct Object {
  uint32_t pickup = 0, open = 0, close = 0;
};
struct Cell {
  uint32_t music = 0, space = 0;
  uint8_t flags = 0;
  bool hasMusic = false;
};
struct RegionSound { uint32_t form,weatherMask,chance; };
struct SoundRegion {
  uint32_t world=0;
  uint8_t priority=0;
  bool overrideLower=false;
  std::vector<std::vector<std::array<float,2>>> areas;
  std::vector<RegionSound> sounds;
};
struct SoundPlacement { uint32_t reference,sound; float x,y,z; bool enabled; };
struct Listener {
  uint32_t cell=0,world=0,weather=0;
  float x=0,y=0,z=0,yaw=0,hour=0;
  std::shared_ptr<const fo3world::ReferenceStateSnapshot> states;
};
struct AmbientVoice { uint64_t key;uint32_t sound;float gain,pan;bool loop;uint32_t chance; };
struct Catalog {
  std::unordered_map<uint32_t, Sound> sounds;
  std::unordered_map<uint32_t, std::string> music;
  std::unordered_map<std::string, uint32_t> names;
  std::unordered_map<uint32_t, Object> objects;
  std::unordered_map<uint32_t, Cell> cells;
  std::unordered_map<uint32_t, uint32_t> spaces;
  std::unordered_map<uint32_t,uint32_t> spaceRegions,weatherMasks;
  std::unordered_map<uint32_t,std::vector<uint32_t>> weatherLoops;
  std::unordered_map<uint32_t,SoundRegion> regions;
  std::unordered_map<uint32_t,std::vector<SoundPlacement>> placements;
  std::vector<AmbientVoice> Environment(const Listener&) const;
  std::string MusicForCell(uint32_t cell) const;
  uint32_t AmbientForCell(uint32_t cell) const;
  uint32_t Footstep(uint32_t material, unsigned action) const;
};
bool SafePath(const std::string &path);
bool IsLoadingScreenEffect(const std::string& path);
bool LoadCatalog(const std::string &esm, Catalog &out, std::string &error);
} // namespace fo3audio
