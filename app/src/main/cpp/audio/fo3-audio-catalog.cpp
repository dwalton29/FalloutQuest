#include "fo3-audio-catalog.h"
#include "data/fo3-esm-reader.h"
#include <algorithm>
#include <cmath>
#include <memory>
namespace fo3audio {
bool SafePath(const std::string &path) {
  if (path.empty() || path.front() == '/' ||
      path.find(':') != std::string::npos ||
      path.find('\n') != std::string::npos ||
      path.find('\r') != std::string::npos)
    return false;
  size_t at = 0;
  while (at < path.size()) {
    auto end = path.find('/', at);
    auto part = path.substr(at, end - at);
    if (part == ".." || part == "." || part.empty())
      return false;
    if (end == std::string::npos)
      break;
    at = end + 1;
  }
  return true;
}
std::string Catalog::MusicForCell(uint32_t id) const {
  auto cell = cells.find(id);
  if (cell == cells.end())
    return {};
  uint32_t musicId = cell->second.music;
  // Only the exterior exploration fallback is enabled in this milestone.
  // Interior music without an explicit XCMO is left silent rather than guessed.
  if (!cell->second.hasMusic && !(cell->second.flags & 1)) {
    auto name = names.find("DefaultExplore");
    if (name != names.end())
      musicId = name->second;
  }
  auto m = music.find(musicId);
  return m == music.end() ? std::string{} : m->second;
}
uint32_t Catalog::AmbientForCell(uint32_t id) const {
  auto c = cells.find(id);
  if (c == cells.end())
    return 0;
  auto s = spaces.find(c->second.space);
  return s == spaces.end() ? 0 : s->second;
}
bool LoadCatalog(const std::string &path, Catalog &out, std::string &error) {
  struct CloseFile {
    void operator()(FILE *f) const { fclose(f); }
  };
  std::unique_ptr<FILE, CloseFile> f(fopen(path.c_str(), "rb"));
  if (!f) {
    error = "audio ESM unavailable";
    return false;
  }
  const auto size = fo3esm::FileSize(f.get());
  if (size < 24) {
    error = "audio ESM truncated";
    return false;
  }
  Catalog catalog;
  std::function<bool(uint64_t, uint64_t, unsigned)> walk = [&](uint64_t at,
                                                               uint64_t end,
                                                               unsigned depth) {
    if (depth > 32)
      return false;
    while (at < end) {
      if (end - at < 24 || fseeko(f.get(), at, SEEK_SET))
        return false;
      uint8_t h[24];
      if (!fo3esm::ReadExact(f.get(), h, 24))
        return false;
      auto type = fo3esm::FourCC(h);
      auto n = fo3esm::ReadU32(h + 4), flags = fo3esm::ReadU32(h + 8),
           id = fo3esm::ReadU32(h + 12);
      if (type == "GRUP") {
        if (n < 24 || n > end - at || !walk(at + 24, at + n, depth + 1))
          return false;
        at += n;
        continue;
      }
      if (n > end - at - 24)
        return false;
      const bool selected =
          type == "SOUN" || type == "MUSC" || type == "ASPC" ||
          type == "CELL" || type == "DOOR" || type == "CONT" ||
          type == "WEAP" || type == "ARMO" || type == "AMMO" ||
          type == "MISC" || type == "ALCH" || type == "INGR" ||
          type == "BOOK" || type == "NOTE" || type == "KEYM";
      if (selected && !(flags & 0x20)) {
        std::vector<uint8_t> bytes;
        if (!fo3esm::ReadPayloadCurrent(f.get(), n, flags, bytes))
          return false;
        size_t pos = 0;
        uint32_t extended = 0;
        while (pos < bytes.size()) {
          if (bytes.size() - pos < 6)
            return false;
          const auto tag = fo3esm::FourCC(bytes.data() + pos);
          uint32_t length = fo3esm::ReadU16(bytes.data() + pos + 4);
          pos += 6;
          if (tag == "XXXX") {
            if (extended || length != 4 || bytes.size() - pos < 4)
              return false;
            extended = fo3esm::ReadU32(bytes.data() + pos);
            pos += 4;
            if (!extended)
              return false;
            continue;
          }
          length = extended ? extended : length;
          extended = 0;
          if (length > bytes.size() - pos)
            return false;
          pos += length;
        }
        if (extended)
          return false;
        Sound sound;
        Object object;
        Cell cell;
        std::string name, filename;
        uint32_t ambient = 0;
        fo3esm::WalkSubrecords(bytes, [&](const char *tag, const uint8_t *p,
                                          uint32_t len) {
          std::string t(tag, 4);
          if (t == "EDID")
            name = fo3esm::ZString(p, len);
          if (t == "FNAM" && (type == "SOUN" || type == "MUSC"))
            filename = fo3esm::ZString(p, len);
          if (type == "SOUN" && (t == "SNDX" || t == "SNDD") && len >= 12) {
            sound.loop = (fo3esm::ReadU32(p + 4) & 16) != 0;
            int16_t attenuation = static_cast<int16_t>(fo3esm::ReadU16(p + 8));
            sound.gain =
                std::clamp(std::pow(10.f, -attenuation / 2000.f), 0.f, 1.f);
          }
          if (len == 4) {
            auto form = fo3esm::ReadU32(p);
            if (t == "YNAM")
              object.pickup = form;
            if (t == "SNAM") {
              object.open = form;
              if (type == "ASPC")
                ambient = form;
            }
            if (t == "QNAM" || (type == "DOOR" && t == "ANAM"))
              object.close = form;
            if (t == "XCMO") {
              cell.music = form;
              cell.hasMusic = true;
            }
            if (t == "XCAS")
              cell.space = form;
          }
          if (type == "CELL" && t == "DATA" && len)
            cell.flags = p[0];
        });
        std::replace(filename.begin(), filename.end(), '\\', '/');
        if (type == "SOUN") {
          if (SafePath(filename))
            sound.path = "sound/" + filename;
          catalog.sounds[id] = sound;
          catalog.names[name] = id;
        } else if (type == "MUSC") {
          catalog.music[id] =
              SafePath(filename) ? "music/" + filename : std::string{};
          catalog.names[name] = id;
        } else if (type == "CELL")
          catalog.cells[id] = cell;
        else if (type == "ASPC")
          catalog.spaces[id] = ambient;
        else
          catalog.objects[id] = object;
      }
      at += 24 + n;
    }
    return true;
  };
  if (!walk(0, size, 0)) {
    error = "invalid audio ESM records";
    return false;
  }
  out = std::move(catalog);
  error.clear();
  return true;
}
} // namespace fo3audio
