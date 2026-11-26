// Native actor effect submission with the owner's original assets. GL material
// readiness and draw calls are recorded; no substitute texture is loaded.
#include "rendering/mesh/fo3-actor-effect-addons.h"
#include "rendering/mesh/fo3-static-nif.h"
#include "rendering/spacewarp/fo3-motion-identity.h"
#include "world/interaction/fo3-door-nif-animation.h"
#include "data/fo3-bsa-reader.h"
#include <cassert>
#include <iostream>
#include <unistd.h>
using Matrix=fo3anim::Matrix;
struct Fo3WorldPlacement {std::string modelPath,baseRecordType;float scale=1;};
struct CpuObject {Fo3StaticNifMesh mesh;};
struct GpuObject {
 std::string path;bool realDiffuse=false,scriptAnimated=false,zBufferTestQ1200=true,zBufferWriteQ1200=false;
 int q2401AnimationBone=-1,alphaSourceBlend=0,alphaDestBlend=0;
 float materialAlpha=1,q2400DoorTransform[16]{},scriptDiffuseRows[2][4]{};
};
struct Actor {
 struct {uint32_t refFormId=0;} source;
 struct {bool offScene=false;} runtime;
 struct Rig {Matrix scenePlacement=fo3anim::Identity();};
 std::vector<Rig> rigs{Rig{}};bool renderVisible=true;Matrix dialogueRoot=fo3anim::Identity();
 fo3anim::Skeleton skeleton;fo3anim::Pose pose;std::shared_ptr<const fo3player::CorpsePose> corpsePose;
};
std::vector<Actor> gQ230NpcActors;
std::unique_ptr<fo3player::Session> gPlayerSession;
float gQ210Head[4]={.5f,1.6f,-1.f};
constexpr float FO3_UNITS_PER_METRE=70;
constexpr int GL_TRUE=1,GL_FALSE=0,GL_BLEND=1,GL_DEPTH_TEST=2;
void glEnable(int){} void glDisable(int){} void glDepthMask(int){} void glBlendFunc(int,int){}
int Q1150BlendFactor(int i,bool){return i;}
namespace fo3hudrenderer {struct CachedStateGuard{~CachedStateGuard(){}};}
bool gQ230DetailedObjectPrevalidated=false,materialReady=false;
bool Fo3WorldReferenceHidden(uint32_t){return false;}
std::string root;
std::string Original(const std::string& path){
 auto file=fo3script::Lower(path);std::replace(file.begin(),file.end(),'\\','/');return root+"/"+file;
}
bool ReadOriginal(const std::string& path,std::vector<uint8_t>& bytes,std::string* resolved){
 return LoadFalloutMeshFile(Original(path),bytes,resolved);
}
void Q2016BuildPlacementMatrix(const Fo3WorldPlacement&,float,float,float,float* out){
 const Matrix m={1,0,0,0,0,0,-1,0,0,1,0,0,0,.25f,-.5f,1};std::copy(m.begin(),m.end(),out);
}
bool BuildCpuObjects(const Fo3WorldPlacement& p,std::vector<CpuObject>& out,bool strict){
 std::vector<Fo3StaticNifMesh> meshes;
 if(!LoadFo3StaticNifMeshes(Original(p.modelPath),meshes,strict,Fo3UsesPlacedReferenceRoot(p.baseRecordType)))return false;
 for(auto& mesh:meshes)out.push_back({std::move(mesh)});
 return true;
}
bool UploadCpuObject(CpuObject& part,float,float,float,GpuObject& gpu){
 assert(!part.mesh.indices.empty());gpu.path=part.mesh.modelPath;gpu.realDiffuse=materialReady;
 gpu.materialAlpha=part.mesh.alpha;return true;
}
void Q74DeleteGpuObjects(std::vector<GpuObject>& objects){objects.clear();}
std::map<uint64_t,std::pair<std::string,Matrix>> submitted;
void DrawSceneObject(const GpuObject& o,bool,fqspacewarp::Placement p){
 assert(p.kind==fqspacewarp::PlacementKind::ShaderAddon&&o.scriptAnimated);
 assert(std::isfinite(o.materialAlpha)&&o.materialAlpha>0);
 Matrix m;std::copy_n(o.q2400DoorTransform,16,m.begin());
 assert(submitted.emplace(p.id,std::make_pair(o.path,m)).second);
}
#define LoadFalloutMeshFile ReadOriginal
#include "rendering/mesh/fo3-actor-effect-render.inc"
#undef LoadFalloutMeshFile
int main(int argc,char** argv){
 if(argc!=4)return 2;
 root=argv[2];fo3player::Catalog catalog;std::string error;
 assert(fo3player::LoadCatalog(argv[1],catalog,error));
 gPlayerSession=std::make_unique<fo3player::Session>(std::move(catalog));
 auto& p=gPlayerSession->player;assert(p.Restore(argv[3],error));
 const auto& data=p.Definitions().pipboy;const auto mabel=data.formNames.at("mabelhendersonref");
 const auto flames=data.formNames.at("flames01"),smoke=data.formNames.at("smoke01");
 assert(p.ActorHealth(mabel)==0&&p.Snapshot().magicShaders.count({mabel,smoke}));
 auto playback=p.Snapshot().magicShaders.at({mabel,flames});assert(playback.elapsed>0&&playback.elapsed<5);
 const auto& fire=data.effectShaders.at(flames);
 assert(fire.addonFadeIn==.75f&&fire.addonStartScale==.5f&&fire.addonScaleIn==1.f);
 const auto& variants=data.debrisModels.at(fire.addon);
 Actor actor;actor.source.refFormId=mabel;
 std::vector<uint8_t> bytes;assert(ReadOriginal("characters/_male/skeleton.nif",bytes,nullptr));
 assert(fo3anim::DecodeSkeleton(bytes,actor.skeleton));
 actor.pose.global=actor.skeleton.bindGlobal;actor.rigs.front().scenePlacement[12]=19;
 gQ230NpcActors.push_back(std::move(actor));auto& resident=gQ230NpcActors.front();
 fo3addonrenderer::DrawAddons();assert(submitted.empty());fo3addonrenderer::Shutdown();
 // A recording material allows inspection of matrices that would be sent with a DDS.
 materialReady=true;fo3addonrenderer::DrawAddons();const auto before=submitted;
 assert(before.size()==18);
 char saved[]="/tmp/fq-addon-XXXXXX";int fd=mkstemp(saved);assert(fd>=0);close(fd);
 assert(p.Save(saved,error)&&p.Restore(saved,error));unlink(saved);
 assert(p.Snapshot().magicShaders.at({mabel,flames}).variantSeed==playback.variantSeed);
 submitted.clear();fo3addonrenderer::DrawAddons();assert(submitted==before);
 // Control: move one saved ragdoll node while leaving the live skeleton at bind.
 auto corpse=std::make_shared<fo3player::CorpsePose>();size_t moved=0;
 for(size_t i=0;i<resident.skeleton.bones.size();++i)if(resident.skeleton.bones[i].collisionOwner){
   corpse->bones.push_back({resident.skeleton.bones[i].name,resident.skeleton.bindGlobal[i],{},{}});
   moved=i;
 }
 corpse->bones.back().global[12]+=3;resident.corpsePose=corpse;
 submitted.clear();fo3addonrenderer::DrawAddons();
 const uint64_t movedId=(uint64_t(mabel)<<32)|(flames^uint32_t(moved*2654435761u));
 assert(submitted.size()==before.size()&&submitted.at(movedId)!=before.at(movedId));
 for(const auto& entry:before)if(entry.first!=movedId)assert(entry.second==submitted.at(entry.first));
 for(uint32_t seed=0;seed<16;++seed){
   playback.variantSeed=seed;size_t count=0;
   fo3actorfx::VisitAddons(resident.skeleton,resident.pose,corpse.get(),fo3anim::Identity(),fire,variants,playback,
     [&](size_t i,const auto&,const auto& placement,float alpha){
       assert(resident.skeleton.bones.at(i).collisionOwner&&alpha==1);
       const auto& original=corpse->bones.at(count++).global;
       for(int j=12;j<15;++j)assert(placement[j]==original[j]);
     });
   assert(count==18);
 }
 assert(p.AdvancePlacedEffects(5-playback.elapsed,error));submitted.clear();
 fo3addonrenderer::DrawAddons();assert(submitted.empty());fo3addonrenderer::Shutdown();
 std::cout<<"Original oven burn: 18 bone-bound addon submissions survive reload, follow corpse transforms and stop on expiry. Missing DDS suppressed all draws; positive material path recorded.\n";
}
