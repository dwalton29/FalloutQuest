#include "../../app/src/main/cpp/rendering/mesh/fo3-static-nif.cpp"
#include "fo3-actor-animation.h"
#include <cassert>
#include <fstream>
#include <iostream>
bool LoadFalloutMeshFile(const std::string &path, std::vector<uint8_t> &bytes,
                         std::string *resolved) {
  std::ifstream f(path, std::ios::binary);
  bytes.assign(std::istreambuf_iterator<char>(f), {});
  if (resolved)
    *resolved = path;
  return !bytes.empty();
}
static bool Near(float a, float b) { return std::fabs(a - b) < 1e-4f; }
static std::vector<uint8_t> Read(const char *path) {
  std::ifstream f(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
int main(int argc, char **argv) {
  using namespace fo3anim;
  Skeleton skeleton;
  Bone root;
  root.name = "root";
  root.bind.translation = {0, 0, 10};
  skeleton.bones.push_back(root);
  Bone child;
  child.name = "child";
  child.parent = 0;
  child.bind.translation = {0, 0, 5};
  skeleton.bones.push_back(child);
  assert(FinalizeSkeleton(skeleton));
  auto identity = Multiply(skeleton.bindGlobal[1], skeleton.inverseBind[1]);
  for (int i = 0; i < 16; ++i)
    assert(Near(identity[i], Identity()[i]));
  Clip clip;
  clip.start = 0;
  clip.stop = 1;
  clip.cycle = 0;
  Track track;
  track.bone = "child";
  track.hasTranslation = true;
  track.translation.dimensions = 3;
  Key a, b;
  a.time = 0;
  a.value = {0, 0, 5, 0};
  b.time = 1;
  b.value = {2, 0, 5, 0};
  track.translation.keys = {a, b};
  clip.tracks.push_back(track);
  Pose pose;
  assert(Sample(skeleton, clip, 0.5, pose));
  auto p = Point(pose.delta[1], {0, 0, 15});
  assert(Near(p[0], 1) && Near(p[2], 15));
  assert(Sample(skeleton, clip, 1.5, pose));
  assert(Near(pose.global[1][12], 1));
  clip.cycle = 2;
  assert(Sample(skeleton, clip, 5, pose));
  assert(Near(pose.global[1][12], 2));
  clip.cycle = 1;
  assert(Sample(skeleton, clip, 1.25, pose));
  assert(Near(pose.global[1][12], 1.5));
  // An open cubic spline with four control points is a Bezier curve.
  clip.cycle = 2;
  clip.tracks[0].translation.keys.clear();
  clip.tracks[0].translation.controls = {0, 0, 5, 0, 0, 5, 0, 0, 5, 8, 0, 5};
  clip.tracks[0].start = 0;
  clip.tracks[0].stop = 1;
  assert(Sample(skeleton, clip, 0.5, pose));
  assert(Near(pose.global[1][12], 1));
  assert(Sample(skeleton, clip, 1, pose));
  assert(Near(pose.global[1][12], 8));
  // Accumulation roots stay at identity even when the KF contains an
  // explicit root track. World locomotion consumes that motion separately.
  clip.accumulationRoot = "root";
  Track rootMotion;
  rootMotion.bone = "root";
  rootMotion.hasTranslation = true;
  rootMotion.base.translation = {100, 200, 300};
  rootMotion.hasRotation = true;
  rootMotion.base.rotation = {0.70710678f, 0, 0, 0.70710678f};
  clip.tracks.push_back(rootMotion);
  assert(Sample(skeleton, clip, 0, pose));
  assert(Near(pose.global[0][12], 0) && Near(pose.global[0][13], 0) &&
         Near(pose.global[0][14], 0));
  assert(Near(pose.global[1][14], 5));
  Skeleton cycle = skeleton;
  cycle.bones[0].parent = 1;
  assert(!FinalizeSkeleton(cycle));
  std::vector<uint8_t> bad(64, 0);
  Skeleton invalid;
  Clip invalidClip;
  assert(!DecodeSkeleton(bad, invalid));
  assert(!DecodeClip(bad, invalidClip));
  if (argc >= 3) {
    auto bytes = Read(argv[1]);
    Skeleton real;
    assert(DecodeSkeleton(bytes, real));
    assert(real.bones.size() == 66);
    for (int i = 2; i < argc; ++i) {
      auto original = Read(argv[i]);
      Clip originalClip;
      assert(DecodeClip(original, originalClip));
      assert(originalClip.compressedTracks > 0);
      Pose sampled;
      BindClip(real, originalClip, sampled);
      for (int bone : sampled.trackBones)
        assert(bone >= 0);
      for (int frame = 0; frame < 360; ++frame)
        assert(Sample(real, originalClip, frame / 72.0, sampled));
      const int head = FindBone(real, "Bip01 Head");
      assert(head >= 0);
      assert(Sample(real, originalClip, 0, sampled));
      assert(sampled.global[head][14] > 90 && sampled.global[head][14] < 130);
      if (originalClip.name == "Idle") {
        assert(originalClip.tracks.size() == 60 &&
               originalClip.compressedTracks == 52);
        Pose start, later;
        assert(Sample(real, originalClip, 0, start));
        assert(Sample(real, originalClip, 1, later));
        assert(std::fabs(start.global[head][13] - later.global[head][13]) >
               0.001f);
      }
      for (size_t n :
           {size_t(0), size_t(40), original.size() / 2, original.size() - 8}) {
        auto truncated = original;
        truncated.resize(n);
        assert(!DecodeClip(truncated, invalidClip));
      }
      std::cout << argv[i] << ": tracks=" << originalClip.tracks.size()
                << " compressed=" << originalClip.compressedTracks
                << " sampled=360 frames\n";
    }
  }
  std::cout << "Actor animation tests passed\n";
}
