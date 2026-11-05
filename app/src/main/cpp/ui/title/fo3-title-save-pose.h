#pragma once
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
namespace fo3title {
struct Pose {
  unsigned cell = 0, world = 0;
  float x = 0, y = 0, z = 0, yaw = 0;
  bool valid = false;
};
inline bool SaveIdentity(const std::string &path,
                         std::array<unsigned char, 20> &id) {
  FILE *f = std::fopen(path.c_str(), "rb");
  if (!f)
    return false;
  bool ok = std::fread(id.data(), 1, id.size(), f) == id.size() &&
            !std::memcmp(id.data(), "FQPS", 4);
  std::fclose(f);
  return ok;
}
inline bool WritePose(const std::string &path, const Pose &p) {
  if (!p.valid || !p.cell)
    return false;
  std::array<unsigned char, 20> id{};
  if (!SaveIdentity(path, id))
    return false;
  const auto dest = path + ".arrival", tmp = dest + ".tmp";
  FILE *f = std::fopen(tmp.c_str(), "wb");
  if (!f)
    return false;
  bool ok = std::fwrite(id.data(), 1, id.size(), f) == id.size();
  ok = std::fprintf(f, "\n%u %u %.9g %.9g %.9g %.9g\n", p.cell, p.world, p.x,
                    p.y, p.z, p.yaw) > 0 &&
       ok;
  if (std::fclose(f) != 0)
    ok = false;
  if (ok)
    ok = std::rename(tmp.c_str(), dest.c_str()) == 0;
  if (!ok)
    std::remove(tmp.c_str());
  return ok;
}
inline bool ReadPose(const std::string &path, Pose &p) {
  std::array<unsigned char, 20> id{}, stored{};
  if (!SaveIdentity(path, id))
    return false;
  FILE *f = std::fopen((path + ".arrival").c_str(), "rb");
  if (!f)
    return false;
  Pose next;
  bool ok = std::fread(stored.data(), 1, stored.size(), f) == stored.size() &&
            id == stored;
  ok = std::fscanf(f, " %u %u %f %f %f %f", &next.cell, &next.world, &next.x,
                   &next.y, &next.z, &next.yaw) == 6 &&
       ok;
  std::fclose(f);
  if (!ok || !next.cell || !std::isfinite(next.x) || !std::isfinite(next.y) ||
      !std::isfinite(next.z) || !std::isfinite(next.yaw))
    return false;
  next.valid = true;
  p = next;
  return true;
}
} // namespace fo3title
