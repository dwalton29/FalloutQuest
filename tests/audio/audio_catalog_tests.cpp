#include "audio/fo3-audio-catalog.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include <vector>
using Bytes = std::vector<uint8_t>;
void check(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}
void u32(Bytes &b, uint32_t value) {
  for (int i = 0; i < 4; ++i)
    b.push_back(value >> (8 * i));
}
Bytes id(uint32_t value) {
  Bytes b;
  u32(b, value);
  return b;
}
Bytes text(std::string value) { return Bytes(value.begin(), value.end()); }
void sub(Bytes &b, const char *tag, const Bytes &data) {
  b.insert(b.end(), tag, tag + 4);
  b.push_back(data.size());
  b.push_back(data.size() >> 8);
  b.insert(b.end(), data.begin(), data.end());
}
void record(Bytes &b, const char *tag, uint32_t form, const Bytes &payload) {
  b.insert(b.end(), tag, tag + 4);
  u32(b, payload.size());
  u32(b, 0);
  u32(b, form);
  u32(b, 0);
  u32(b, 0);
  b.insert(b.end(), payload.begin(), payload.end());
}
int main(int argc, char **argv) {
  try {
    check(fo3audio::SafePath("fx/door.wav") && fo3audio::SafePath("explore/"),
          "valid authored path");
    for (auto p : {"../x.wav", "fx/../x.wav", "/absolute.wav", "C:/x.wav",
                   "fx//bad.wav", "x\n.wav"})
      check(!fo3audio::SafePath(p), "unsafe path rejected");
    Bytes bytes, p;
    sub(p, "EDID", text("OriginalSound"));
    sub(p, "FNAM", text("fx\\door.wav"));
    Bytes snd{20, 11, 0, 0};
    u32(snd, 16);
    snd.push_back(0xe8);
    snd.push_back(3);
    snd.push_back(0);
    snd.push_back(0);
    sub(p, "SNDX", snd);
    record(bytes, "SOUN", 100, p);
    p.clear();
    sub(p, "EDID", text("DefaultExplore"));
    sub(p, "FNAM", text("explore\\"));
    record(bytes, "MUSC", 200, p);
    p.clear();
    sub(p, "FNAM", text("public\\"));
    record(bytes, "MUSC", 201, p);
    p.clear();
    sub(p, "EDID", text("1NoMusic"));
    record(bytes, "MUSC", 202, p); // authored silence
    p.clear();
    sub(p, "SNAM", id(100));
    record(bytes, "ASPC", 300, p);
    p.clear();
    sub(p, "DATA", Bytes{0});
    sub(p, "XCAS", id(300));
    record(bytes, "CELL", 400, p);
    p.clear();
    sub(p, "DATA", Bytes{1});
    sub(p, "XCMO", id(201));
    record(bytes, "CELL", 401, p);
    p.clear();
    sub(p, "DATA", Bytes{1});
    record(bytes, "CELL", 402, p);
    p.clear();
    sub(p, "DATA", Bytes{0});
    sub(p, "XCMO", id(202));
    record(bytes, "CELL", 403, p);
    p.clear();
    sub(p, "SNAM", id(100));
    sub(p, "ANAM", id(101));
    record(bytes, "DOOR", 500, p);
    p.clear();
    sub(p, "SNAM", id(100));
    sub(p, "QNAM", id(102));
    record(bytes, "CONT", 501, p);
    p.clear();
    sub(p, "YNAM", id(100));
    record(bytes, "MISC", 502, p);
    const auto path =
        "/tmp/fq-audio-tests-" + std::to_string(getpid()) + ".esm";
    {
      std::ofstream f(path, std::ios::binary);
      f.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    }
    fo3audio::Catalog c;
    std::string error;
    check(fo3audio::LoadCatalog(path, c, error), "fixture decodes");
    check(c.sounds.at(100).path == "sound/fx/door.wav" &&
              c.sounds.at(100).loop &&
              std::abs(c.sounds.at(100).gain - .316227f) < .0001f,
          "original filename loop and attenuation");
    check(c.MusicForCell(400) == "music/explore/" &&
              c.AmbientForCell(400) == 100,
          "exterior music and authored acoustic loop");
    check(c.MusicForCell(401) == "music/public/", "explicit cell music");
    check(c.MusicForCell(402).empty() && c.MusicForCell(403).empty(),
          "no guessed interior music; silence overrides fallback");
    check(c.objects.at(500).open == 100 && c.objects.at(500).close == 101 &&
              c.objects.at(501).close == 102 && c.objects.at(502).pickup == 100,
          "original object sound links");
    {
      std::ofstream f(path, std::ios::binary);
      f.write(reinterpret_cast<const char *>(bytes.data()), bytes.size() - 1);
    }
    check(!fo3audio::LoadCatalog(path, c, error) &&
              c.objects.at(500).open == 100,
          "truncation leaves catalog unchanged");
    unlink(path.c_str());
    if (argc > 1) {
      check(fo3audio::LoadCatalog(argv[1], c, error),
            "original ESM audio decodes");
      check(c.sounds.size() == 1583 && c.music.size() == 31,
            "original catalog counts");
      std::cout << "Original ESM: " << c.sounds.size() << " sounds, "
                << c.music.size() << " music types, " << c.cells.size()
                << " cells\n";
    }
    std::cout << "Audio tests passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
