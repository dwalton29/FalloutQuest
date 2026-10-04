// Execute the production actor/render bridge with a recording GL adapter.
#include "../../app/src/main/cpp/rendering/mesh/fo3-static-nif.h"
#include "fo3-actor-animation.h"
#include "fo3-npc.h"
#include "fo3-npc-appearance.h"
#include "fo3-texture-bsa.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <unordered_map>
struct Vec3 {
  float x = 0, y = 0, z = 0;
};
struct Placement {
  float x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0, scale = 1;
  std::string modelPath;
};
using Fo3WorldPlacement = Placement;
struct CpuObject {
  Placement placement;
  Fo3StaticNifMesh mesh;
  std::vector<Vec3> positionsGame, normalsGame, tangentsGame, bitangentsGame;
  std::vector<float> q1960ExpandedVertices;
  bool q1960ExpandedReady = false;
};
using GLsizeiptr = ptrdiff_t;
struct GpuObject {
  unsigned vbo = 1;
  bool q230NpcActor = false, alphaBlend = false, zBufferTestQ1200 = true,
       zBufferWriteQ1200 = true;
  uint8_t alphaSourceBlend = 6, alphaDestBlend = 7;
};
struct Q230RigPart {
  CpuObject bind;
  std::vector<float> work;
  std::vector<std::array<float, 12>> posed;
  std::vector<fo3anim::Matrix> gameDeltas;
  std::vector<int> bones;
  fo3anim::Matrix placement{}, inversePlacement{};
  int rigidBone = -1;
  size_t gpuIndex = 0;
  float centerX = 0, centerY = 0, floorZ = 0;
};
struct Q230ActorVisual {
  Fo3NpcActorQ230 source;
  fo3anim::Skeleton skeleton;
  fo3anim::Clip clip;
  fo3anim::Pose pose;
  std::vector<CpuObject> parts;
  std::vector<GpuObject> objects;
  std::vector<Q230RigPart> rigs;
  std::unordered_map<std::string, Fo3RgbaTexture> generatedTextures;
  std::chrono::steady_clock::time_point animationStart{};
  uint64_t lastFrame = UINT64_MAX;
};
std::vector<Q230ActorVisual> gQ230NpcActors;
uint64_t gSceneLoadFrame = 1;
constexpr float FO3_UNITS_PER_METRE = 100, FLOOR_Y = 0, SCENE_FORWARD = 0;
constexpr int GL_ARRAY_BUFFER = 1, GL_DYNAMIC_DRAW = 2, GL_DEPTH_TEST = 3,
              GL_TRUE = 1, GL_FALSE = 0, GL_BLEND = 4;
int uploads = 0, draws = 0;
std::vector<float> recorded;
void glBindBuffer(int, unsigned) {}
void glBufferData(int, GLsizeiptr, const void *, int) {}
void glBufferSubData(int, GLsizeiptr, GLsizeiptr bytes, const void *p) {
  ++uploads;
  recorded.assign(static_cast<const float *>(p),
                  static_cast<const float *>(p) + bytes / sizeof(float));
}
void glEnable(int) {}
void glDisable(int) {}
void glDepthMask(int) {}
void glBlendFunc(int, int) {}
int Q1150BlendFactor(uint8_t v, bool) { return v; }
void DrawSceneObject(const GpuObject &) { ++draws; }
Vec3 Normalize(Vec3 v) {
  float n = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
  return n > 0 ? Vec3{v.x / n, v.y / n, v.z / n} : Vec3{};
}
Vec3 GameDirectionToOpenXr(Vec3 v) { return {v.x, v.z, -v.y}; }
Vec3 ApplyEsmRotation(Vec3 v, const Placement &) { return v; }
bool LoadFo3CellActors(uint32_t, std::vector<Fo3NpcActorQ230> &,
                       const std::string &) {
  return false;
}
bool Q230BuildNpcActor(const Fo3NpcActorQ230 &, Q230ActorVisual &) {
  return false;
}
bool PrepareExpandedVertexStreamQ1960(CpuObject &cpu, float, float, float) {
  cpu.q1960ExpandedVertices.assign(cpu.mesh.indices.size() * 18, 0);
  cpu.q1960ExpandedReady = true;
  for (size_t i = 0; i < cpu.mesh.indices.size(); ++i)
    cpu.q1960ExpandedVertices[i * 18 + 12] = 0.25f;
  return true;
}
bool UploadCpuObject(CpuObject &, float, float, float, GpuObject &gpu) {
  gpu = {};
  return true;
}
#include "../../app/src/main/cpp/npc/fo3-npc-runtime.inc"
int main() {
  // Complete bind rotation/translation in a scaled, translated actor instance.
  {
    fo3anim::Skeleton skeleton;
    fo3anim::Bone head;
    head.name = "Bip01 Head";
    head.bind.translation = {0, 0, 5};
    const float half = std::sqrt(.5f);
    head.bind.rotation = {half, 0, 0, half};
    skeleton.bones.push_back(head);
    assert(fo3anim::FinalizeSkeleton(skeleton));
    fo3anim::Transform instance;
    instance.translation = {20, 30, 40}; instance.scale = 2;
    fo3anim::Matrix attachment;
    assert(fo3appearance::HeadBindTransform(skeleton, fo3anim::ToMatrix(instance), attachment));
    auto point = fo3anim::Point(attachment, {22, 30, 40});
    assert(std::fabs(point[0]-20)<1e-5f && std::fabs(point[1]-32)<1e-5f && std::fabs(point[2]-50)<1e-5f);
    auto normal = fo3anim::Point(attachment, {1, 0, 0}, true);
    assert(std::fabs(normal[0])<1e-5f && std::fabs(normal[1]-1)<1e-5f);
    assert(!fo3appearance::HeadBindTransform({}, fo3anim::Identity(), attachment));
    Fo3NpcActorQ230 source;
    Fo3NpcVisualItemQ230 hat;
    hat.recordType="ARMO"; hat.count=1; hat.bipedMask=2; hat.modelPath="Armor\\Hat.nif";
    source.inventory.push_back(hat);
    assert(fo3appearance::HeadPart(source, "armor/hat.NIF"));
    source.inventory[0].bipedMask=4;
    assert(!fo3appearance::HeadPart(source, "armor/hat.NIF"));
    assert(fo3appearance::SkinMaterial(14, 2));
    assert(fo3appearance::SkinMaterial(1, 0x400));
    assert(!fo3appearance::SkinMaterial(1, 2));
  }
  Q230ActorVisual actor;
  fo3anim::Bone bone;
  bone.name = "Bip01 Head";
  bone.bind.translation = {0, 0, 5};
  actor.skeleton.bones.push_back(bone);
  assert(fo3anim::FinalizeSkeleton(actor.skeleton));
  actor.clip.stop = 1;
  actor.clip.cycle = 2;
  fo3anim::Track track;
  track.bone = bone.name;
  track.hasTranslation = true;
  track.base.translation = {1, 0, 5};
  actor.clip.tracks.push_back(track);
  actor.animationStart = std::chrono::steady_clock::now();
  CpuObject part;
  part.placement.x = 20;
  part.placement.y = 30;
  part.placement.z = 40;
  part.placement.scale = 2;
  part.mesh.skinned = true;
  Fo3NifSkinBone skin;
  skin.name = bone.name;
  part.mesh.skinBones.push_back(skin);
  part.mesh.skinBoneIndices = {0, 0xffff, 0xffff, 0xffff};
  part.mesh.skinBoneWeights = {1, 0, 0, 0};
  part.mesh.indices = {0, 0, 0};
  part.positionsGame = {{20, 30, 50}};
  part.normalsGame = {{0, 0, 1}};
  part.tangentsGame = {{1, 0, 0}};
  part.bitangentsGame = {{0, 1, 0}};
  assert(Q230UploadActorPart(actor, part, 20, 30, 40));
  Q230UpdateActor(actor);
  assert(uploads == 1);
  for (size_t i = 0; i < 3; ++i) {
    assert(std::fabs(recorded[i * 18] - 0.02f) < 1e-5f);
    assert(std::fabs(recorded[i * 18 + 1] - 0.1f) < 1e-5f);
    assert(recorded[i * 18 + 12] == 0.25f);
  }
  Q230UpdateActor(actor);
  assert(uploads == 1);
  ++gSceneLoadFrame;
  Q230UpdateActor(actor);
  assert(uploads == 2);
  // A rigid mouth/eye follows the same head deformation and captured origin.
  part.mesh.skinned = false;
  part.placement.modelPath = "mouth.nif";
  actor.source.raceHeadModels = {"mouth.nif"};
  assert(Q230UploadActorPart(actor, part, 10, 30, 40));
  ++gSceneLoadFrame;
  Q230UpdateActor(actor);
  assert(std::fabs(recorded[0] - 0.12f) < 1e-5f);
  // Worn rigid headgear is animated through the same production head path.
  Fo3NpcVisualItemQ230 hat;
  hat.recordType = "ARMO"; hat.count = 1; hat.bipedMask = 2;
  hat.modelPath = "Armor\\Hat.nif";
  actor.source.inventory.push_back(hat);
  part.placement.modelPath = "armor/hat.NIF";
  assert(Q230UploadActorPart(actor, part, 10, 30, 40));
  assert(actor.rigs.back().rigidBone == 0);
  ++gSceneLoadFrame;
  Q230UpdateActor(actor);
  assert(std::fabs(recorded[0] - 0.12f) < 1e-5f);
  gQ230NpcActors.push_back(std::move(actor));
  Q230RenderNpcActors(false);
  Q230RenderNpcActors(true);
  assert(draws == 3);
  std::cout << "Actor runtime bridge tests passed\n";
}
