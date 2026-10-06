#include "../../app/src/main/cpp/rendering/actor-skinning.h"
#include <memory>
#include "../../app/src/main/cpp/npc/fo3-animation-bounds.h"
// Execute the production actor/render bridge with a recording GL adapter.
#include "../../app/src/main/cpp/rendering/mesh/fo3-static-nif.h"
#include "fo3-actor-animation.h"
#include "fo3-npc.h"
#include "../..//app/src/main/cpp/npc/fo3-npc-state.h"
#include "../..//app/src/main/cpp/pipboy/fo3-pipboy-data.h"
#define Q6H_LOGI(...) ((void)0)
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
  int vertexCount=0;
  bool renderVisible=true;
  float minX=0,maxX=0,minY=0,maxY=0,minZ=0,maxZ=0;
  unsigned vbo = 1;
  bool q230NpcActor = false, alphaBlend = false, zBufferTestQ1200 = true,
       zBufferWriteQ1200 = true;
  uint8_t alphaSourceBlend = 6, alphaDestBlend = 7;
};
struct QActorSkin { fqskin::Mapping mapping;std::vector<fqskin::PaletteRow> palette;std::vector<float> bind; };
struct Q230RigPart {
  QActorSkin skin;
  std::vector<int> bones;
  std::vector<std::array<float,3>> hitVertices;
  std::vector<std::pair<std::array<float,3>,std::array<float,3>>> interactionBounds;
  fo3anim::Matrix placement{}, inversePlacement{}, scenePlacement{}, inverseScenePlacement{};
  int rigidBone = -1;
  size_t gpuIndex = 0;
  float centerX = 0, centerY = 0, floorZ = 0;
};
struct Q240NavigationGraph {
  std::vector<std::shared_ptr<const Fo3NpcNavMeshQ240>> meshes;
  std::unordered_map<uint32_t,size_t> byForm;
  std::vector<size_t> triangleOffsets;
  size_t triangleCount=0u;
};
struct Q230ActorVisual {
  Fo3NpcActorQ230 source;
  fo3npc::RuntimeState runtime;
  std::array<fo3anim::Clip,size_t(fo3npc::Animation::Count)> animations;
  int activeAnimation=0,headBone=-1,chestBone=-1;
  std::vector<fo3anim::Transform> blendFrom;
  double blendStart=-1;
  fo3anim::Matrix dialogueRoot=fo3anim::Identity();
  fo3anim::Skeleton skeleton;
  fo3anim::Clip clip;
  fo3anim::Pose pose;
  std::vector<CpuObject> parts;
  std::vector<GpuObject> objects;
  std::vector<Q230RigPart> rigs;
  std::vector<fo3anim::Envelope> renderEnvelope;
  std::shared_ptr<const Fo3NpcNavMeshQ240> navigation;
  std::shared_ptr<const Q240NavigationGraph> navigationGraph;
  std::vector<std::array<float,3>> aiPathGame;
  size_t aiPathIndex=0u;
  uint32_t aiPackage=0u,aiSequence=0u;
  double aiLastUpdate=-1.0,aiRepathAt=0.0;
  GpuObject renderBounds;
  bool renderBoundsReady=false,renderVisible=true;
  std::unordered_map<std::string, Fo3RgbaTexture> generatedTextures;
  std::chrono::steady_clock::time_point animationStart{};
  uint64_t lastFrame = UINT64_MAX;
};
std::vector<Q230ActorVisual> gQ230NpcActors;
uint64_t gStereoFrame = 1;
namespace fqopaque {
using Clock=std::chrono::steady_clock;
double Micros(Clock::time_point p) {return std::chrono::duration<double,std::micro>(Clock::now()-p).count();}
}
namespace fqactor {
struct Cost { double pose=0,clip=0,bones=0,draw=0;int actors=0; };
Cost npc;
}
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
bool LoadFo3NpcNavigationQ240(uint32_t,uint32_t,
                             std::vector<Fo3NpcNavMeshQ240>&,
                             const std::string&) {
  return false;
}
bool Q230BuildNpcActor(const Fo3NpcActorQ230 &, Q230ActorVisual &) {
  return false;
}
bool PrepareExpandedVertexStreamQ1960(CpuObject &cpu, float centerX, float centerY, float floorZ) {
  cpu.q1960ExpandedVertices.assign(cpu.mesh.indices.size() * 18, 0);
  cpu.q1960ExpandedReady = true;
  for (size_t i = 0; i < cpu.mesh.indices.size(); ++i) {
    const auto vertex=cpu.positionsGame[cpu.mesh.indices[i]];
    cpu.q1960ExpandedVertices[i*18]=(vertex.x-centerX)/FO3_UNITS_PER_METRE;
    cpu.q1960ExpandedVertices[i*18+1]=FLOOR_Y+(vertex.z-floorZ)/FO3_UNITS_PER_METRE;
    cpu.q1960ExpandedVertices[i*18+2]=SCENE_FORWARD-(vertex.y-centerY)/FO3_UNITS_PER_METRE;
    cpu.q1960ExpandedVertices[i * 18 + 12] = 0.25f;
  }
  return true;
}
bool UploadCpuObject(CpuObject &cpu, float, float, float, GpuObject &gpu) {
  gpu = {};gpu.vertexCount=cpu.mesh.indices.size();
  cpu.q1960ExpandedVertices.clear(); // production upload consumes this stream
  cpu.q1960ExpandedReady=false;
  return true;
}
bool QActorCreateSkin(GpuObject&,QActorSkin& skin,const std::vector<uint16_t>& indices,const std::vector<float>& weights,size_t bones,bool player,const std::vector<float>& bind) {
  skin.mapping=fqskin::Map(indices,weights,bones,player);skin.palette.resize(skin.mapping.sources.size()+1);
  for(auto& row:skin.palette) fqskin::Pack(row,fqskin::Identity(),fqskin::Identity());
  assert(bind.size()==indices.size()/4*18);
  skin.bind=bind;return true;
}
void QActorUploadSkin(GpuObject&,QActorSkin& skin,fqactor::Cost&,bool player) {
  ++uploads;recorded=skin.bind; // recording GPU adapter evaluates submitted palette
  for(size_t v=0;v<recorded.size()/18;++v) {
    const auto row=fqskin::Blend(skin.mapping.attributes.data()+v*8,skin.palette,player);
    const auto p=fqskin::Transform(row.data(),skin.bind.data()+v*18,false);
    std::copy(p.begin(),p.end(),recorded.begin()+v*18);
  }
}
bool LoadFalloutMeshFile(const std::string&,std::vector<uint8_t>&){return false;}
namespace fo3anim {bool DecodeClip(const std::vector<uint8_t>&,Clip&){return false;}}
#define FO3_ACTOR_RUNTIME_HOST_TEST 1
#include "../../app/src/main/cpp/npc/fo3-npc-runtime.inc"
#undef FO3_ACTOR_RUNTIME_HOST_TEST
int main() {
  // FO3 rigid parts keep actor axes despite a rotated head bone.
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
    instance.rotation = {half, 0, 0, half};
    fo3anim::Matrix attachment;
    assert(fo3appearance::HeadBindTransform(skeleton, fo3anim::ToMatrix(instance), attachment));
    auto point = fo3anim::Point(attachment, {22, 30, 40});
    assert(std::fabs(point[0]-22)<1e-5f && std::fabs(point[1]-30)<1e-5f && std::fabs(point[2]-50)<1e-5f);
    auto normal = fo3anim::Point(attachment, {1, 0, 0}, true);
    assert(std::fabs(normal[0]-1)<1e-5f && std::fabs(normal[1])<1e-5f);
    assert(fo3appearance::HeadBindTransform(skeleton,fo3anim::Identity(),attachment,true));
    auto facePoint = fo3anim::Point(attachment,{1,0,0});
    assert(std::fabs(facePoint[0])<1e-5f && std::fabs(facePoint[1]-1)<1e-5f &&
           std::fabs(facePoint[2]-5)<1e-5f);
    Fo3NpcActorQ230 faceSource;
    faceSource.raceHeadModels.resize(8);
    faceSource.raceHeadModels[6]="Characters\\Head\\EyeLeftHuman.NIF";
    faceSource.raceHeadModels[3]="Characters\\Head\\TeethLowerHuman.NIF";
    assert(fo3appearance::BoneLocalFacePart(faceSource,"characters/head/eyelefthuman.nif"));
    assert(fo3appearance::BoneLocalFacePart(faceSource,"characters/head/teethlowerhuman.nif"));
    assert(!fo3appearance::BoneLocalFacePart(faceSource,"characters/hair/hairbase.nif"));
    assert(!fo3appearance::HeadBindTransform({}, fo3anim::Identity(), attachment));
    Fo3NpcActorQ230 source;
    Fo3NpcVisualItemQ230 hat;
    hat.recordType="ARMO"; hat.count=1; hat.bipedMask=0x600; hat.modelPath="Armor\\Hat.nif";
    source.inventory.push_back(hat);
    assert(fo3appearance::HeadPart(source, "armor/hat.NIF"));
    source.inventory[0].bipedMask=4;
    assert(!fo3appearance::HeadPart(source, "armor/hat.NIF"));
    assert(fo3appearance::RenderHairShape("Hat",0x600));
    assert(!fo3appearance::RenderHairShape("NoHat",0x600));
    assert(fo3appearance::RenderHairShape("NoHat",0));
    assert(!fo3appearance::RenderHairShape("Hat",0));
    assert(fo3appearance::HairMorphModel("HairBase.NIF","Hat")=="HairBasehat.NIF");
    assert(fo3appearance::HairMorphModel("HairBase.NIF","NoHat")=="HairBasenohat.NIF");
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
  ++gStereoFrame;
  Q230UpdateActor(actor);
  assert(uploads == 2);
  // A rigid mouth/eye follows the same head deformation and captured origin.
  part.mesh.skinned = false;
  part.placement.modelPath = "mouth.nif";
  actor.source.raceHeadModels = {"mouth.nif"};
  assert(Q230UploadActorPart(actor, part, 10, 30, 40));
  ++gStereoFrame;
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
  ++gStereoFrame;
  Q230UpdateActor(actor);
  assert(std::fabs(recorded[0] - 0.12f) < 1e-5f);
  std::array<float,3> lo{},hi{},anchor{};
  assert(Q230LiveBounds(actor,lo,hi));
  assert(lo[0]<=hi[0]&&lo[1]<=hi[1]&&lo[2]<=hi[2]);
  actor.headBone=0;
  assert(Q230LiveBone(actor,0,anchor));
  const auto oldAnchor=anchor;
  actor.runtime.BeginDialogue();actor.runtime.Face(actor.runtime.yaw+1.5f,.1f);
  ++gStereoFrame;Q230UpdateActor(actor);
  assert(Q230LiveBounds(actor,lo,hi)&&Q230LiveBone(actor,0,anchor));
  assert(std::isfinite(anchor[0])&&std::fabs(fo3npc::Angle(actor.runtime.yaw-actor.runtime.authoredYaw))<.12f);
  assert(!Q230LiveBone(actor,-1,anchor));
  actor.runtime.EndDialogue();
  assert(!actor.runtime.dialogue);
  (void)oldAnchor;
  gQ230NpcActors.push_back(std::move(actor));
  Q230RenderNpcActors(false);
  Q230RenderNpcActors(true);
  assert(draws == 3);
  std::cout << "Actor runtime bridge tests passed\n";
}
