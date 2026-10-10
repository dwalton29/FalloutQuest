#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace fo3anim {
using Matrix =
    std::array<float, 16>; // column-major affine, game/NIF coordinates
struct Transform {
  std::array<float, 3> translation{0, 0, 0};
  std::array<float, 4> rotation{1, 0, 0, 0}; // Gamebryo quaternion: w,x,y,z
  float scale = 1;
};
struct Bone {
  std::string name;
  int parent = -1;
  Transform bind;
};
struct Skeleton {
  std::vector<Bone> bones;
  std::vector<Matrix> bindGlobal, inverseBind;
};
struct Key {
  float time = 0;
  std::array<float, 4> value{}, forward{}, backward{};
};
struct Channel {
  uint32_t interpolation = 1;
  int dimensions = 0;
  std::vector<Key> keys;
  std::vector<float> controls; // expanded signed-short or float control points
};
struct Track {
  std::string bone;
  Transform base;
  bool hasTranslation = false, hasRotation = false, hasScale = false;
  float start = 0, stop = 0;
  Channel translation, rotation, scale;
  std::array<Channel, 3> xyzRotation;
};
struct TextKey {
  float time = 0;
  std::string text;
};
struct Clip {
  std::string name, accumulationRoot;
  float start = 0, stop = 0, frequency = 1;
  uint32_t cycle = 0;
  std::vector<Track> tracks;
  std::vector<TextKey> textKeys;
  size_t compressedTracks = 0, ignoredControllers = 0;
};
struct SampleTimings { double clipUs=0,skeletonUs=0; };
struct Pose {
  int accumulation=-1;
  std::string accumulationRoot;
  std::vector<Transform> local;
  std::vector<Matrix> global, delta;
  std::vector<int> trackBones;
  std::vector<uint8_t> evaluated;
};
Matrix Identity();
Matrix Multiply(const Matrix &a, const Matrix &b);
Matrix ToMatrix(const Transform &t);
bool Inverse(const Matrix &a, Matrix &inverse);
std::array<float, 4> QuaternionFromMatrix(const float *rowMajor);
std::array<float, 3> Point(const Matrix &m, const std::array<float, 3> &p,
                           bool direction = false);
bool FinalizeSkeleton(Skeleton &skeleton);
// Compose independently authored upper/lower pose branches. Root/world motion
// stays with navigation; the source skeleton supplies the branch topology.
bool OverlayBranch(const Skeleton&,Pose& base,const Pose& overlay,int branch);
int FindBone(const Skeleton &skeleton, const std::string &name);
void BindClip(const Skeleton &skeleton, const Clip &clip, Pose &pose);
enum class RootPolicy { Locomotion, Furniture };
// Absolute sequence sampling; actor root motion is not applied to world
// placement.
bool Sample(const Skeleton &skeleton, const Clip &clip, double elapsed,
            Pose &pose, SampleTimings* timings=nullptr, RootPolicy rootPolicy=RootPolicy::Locomotion,
            const Transform* retainedRoot=nullptr);
// Rebuild after local animation blending; one canonical skeleton composition.
bool ComposePose(const Skeleton&,Pose&);
void LookYaw(const Skeleton&,Pose&,int bone,float radians);
bool DecodeSkeleton(const std::vector<uint8_t> &bytes, Skeleton &out);
// Static model AVObject hierarchy, including geometry block mapping. Unlike
// DecodeUiAnimation, no embedded controller sequence is required or evaluated.
bool DecodeModelHierarchy(const std::vector<uint8_t> &bytes, Skeleton &out,
                          std::vector<int> &blockBones);
bool DecodeClip(const std::vector<uint8_t> &bytes, Clip &out);
// UI NIFs embed multiple sequences and animate an AVObject hierarchy, including
// geometry. blockBones maps original NIF block IDs to that hierarchy.
bool DecodeUiAnimation(const std::vector<uint8_t> &bytes, Skeleton &hierarchy,
                       std::vector<int> &blockBones, std::vector<Clip> &clips);
} // namespace fo3anim
