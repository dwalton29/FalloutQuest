#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
namespace fo3audio {
struct Sound {
  std::string path;
  float gain = 1;
  bool loop = false;
};
struct Object {
  uint32_t pickup = 0, open = 0, close = 0;
};
struct Cell {
  uint32_t music = 0, space = 0;
  uint8_t flags = 0;
  bool hasMusic = false;
};
struct Catalog {
  std::unordered_map<uint32_t, Sound> sounds;
  std::unordered_map<uint32_t, std::string> music;
  std::unordered_map<std::string, uint32_t> names;
  std::unordered_map<uint32_t, Object> objects;
  std::unordered_map<uint32_t, Cell> cells;
  std::unordered_map<uint32_t, uint32_t> spaces;
  std::string MusicForCell(uint32_t cell) const;
  uint32_t AmbientForCell(uint32_t cell) const;
};
bool SafePath(const std::string &path);
bool LoadCatalog(const std::string &esm, Catalog &out, std::string &error);
} // namespace fo3audio
