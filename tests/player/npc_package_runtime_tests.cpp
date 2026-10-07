#include "npc/fo3-npc.h"
#include "npc/fo3-actor-animation.h"
#include "data/fo3-texture-bsa.h"
#include "npc/fo3-npc-state.h"
#include "player/fo3-player-state.h"
#include "dialogue/fo3-dialogue-conditions.h"
#include "dialogue/fo3-dialogue-session.h"
#include <memory>
#include <unordered_set>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>
bool LoadFalloutMeshFile(const std::string&,std::vector<uint8_t>&,std::string*) {return false;}
bool LoadFalloutTextureRgba(const std::string&,Fo3RgbaTexture&) {return false;}
bool originalLogs=false;
template<class... T> void TestLog(const char* fmt,T... args){if(originalLogs){std::printf(fmt,args...);std::puts("");}}
#define Q6H_LOGI(...) TestLog(__VA_ARGS__)
namespace fo3tod {float WrapHour(float h){return std::fmod(h+24.f,24.f);}}
float packageHour=12;
float GetFo3TimeOfDayHour(){return packageHour;}
float gSceneCenterXQ1730=1000,gSceneCenterYQ1730=2000,gSceneFloorZQ1730=20;
constexpr float FO3_UNITS_PER_METRE=100,FLOOR_Y=-1,SCENE_FORWARD=-3;
std::array<float,3> gQ210Head{};
uint32_t gCurrentCellFormId=1,gExteriorWorldspaceQ1890=2;
struct Session {bool saveBlocked=false;fo3player::Player player;explicit Session(fo3player::Catalog c):player(std::move(c)) {}};
std::unique_ptr<Session> gPlayerSession;
struct Q240NavigationGraph {
  std::vector<std::shared_ptr<const Fo3NpcNavMeshQ240>> meshes;
  std::unordered_map<uint32_t,size_t> byForm;
  std::vector<size_t> triangleOffsets;
  size_t triangleCount=0;
};
struct Q230CombatWeapon {fo3weapon::Definition definition;std::array<fo3anim::Clip,4> clips;std::vector<size_t> gpu;std::array<float,3> muzzle{};bool ready=false;};
struct Q230ActorVisual {
  std::unordered_map<uint32_t,Q230CombatWeapon> combatWeapons;
  std::array<fo3anim::Clip,size_t(fo3npc::Animation::Count)> animations;
  bool stateRestored=false;int headBone=0,chestBone=1;
  Fo3NpcActorQ230 source;
  fo3npc::RuntimeState runtime;
  std::vector<int> rigs{1};
  std::shared_ptr<const Q240NavigationGraph> navigationGraph;
  std::vector<std::array<float,3>> aiPathGame;
    std::vector<std::pair<size_t,size_t>> aiPathSurfaces;
    std::array<float,3> aiAnchorGame{};
  size_t aiPathIndex=0;
  uint32_t aiPackage=0,aiSequence=0;
  double aiLastUpdate=-1,aiRepathAt=0;
  uint64_t lastFrame=1;
};
std::vector<Q230ActorVisual> packageTargets;
#define FO3_NPC_PACKAGE_ACTOR_TARGET(reference,point) Q240ResidentPackageTarget(packageTargets,reference,point)
#ifdef FO3_NPC_DOOR_HOST_TEST
static bool Q230PathBlocked(Q230ActorVisual&,float,float,float,float,float);
#define FO3_NPC_PATH_BLOCKED Q230PathBlocked
#endif
#include "npc/fo3-npc-package-runtime.inc"
#undef FO3_NPC_PACKAGE_ACTOR_TARGET
#ifdef FO3_NPC_DOOR_HOST_TEST
#undef FO3_NPC_PATH_BLOCKED
#endif
static std::shared_ptr<Q240NavigationGraph> Graph(bool invalid=false) {
  auto graph=std::make_shared<Q240NavigationGraph>();
  // Two original-format triangles sharing edge 1 -> 2 on the first mesh.
  for(size_t i=0;i<2;++i) {
    auto mesh=std::make_shared<Fo3NpcNavMeshQ240>();mesh->formId=uint32_t(100+i);
    mesh->vertices=i==0?std::vector<std::array<float,3>>{{1000,2000,20},{2000,2000,20},{1000,3000,20}}:
      std::vector<std::array<float,3>>{{2000,2000,20},{2000,3000,20},{1000,3000,20}};
    Fo3NpcNavTriangleQ240 t;t.vertex[0]=0;t.vertex[1]=1;t.vertex[2]=2;
    const int edge=i==0?1:2;t.flags=1u<<edge;t.neighbor[edge]=0;
    mesh->external.push_back({uint32_t(i==0?(invalid?999:101):100),0});mesh->triangles.push_back(t);
    graph->triangleOffsets.push_back(graph->triangleCount++);graph->byForm[mesh->formId]=i;graph->meshes.push_back(mesh);
  }
  return graph;
}
static Q230ActorVisual Actor(uint8_t type) {
  packageTargets.clear();
  fo3player::Catalog c;c.initial.baseHealth=100;
  c.pipboy.targets[42].base=43;c.weapons.actors[43].health=100;
  auto& d=c.pipboy;d.dialogueActors[43].packages={51,50};
  d.packages[51].type=3; // Unsupported higher priority must not block the route.
  auto& p=d.packages[50];p.type=type;p.location={};p.location.valid=true;
  p.location.type=0;p.location.value=60;p.location.radius=2000;
  d.targets[60].x=1900;d.targets[60].y=2900;d.targets[60].z=20;d.targets[60].world=2;
  gPlayerSession=std::make_unique<Session>(std::move(c));
  Q230ActorVisual actor;actor.source.refFormId=42;actor.source.baseFormId=43;
  actor.runtime.position=Q240ScenePosition({1010,2010,20});actor.navigationGraph=Graph();return actor;
}
static void Original(const char* path) {
  originalLogs=true;
  fo3player::Catalog catalog;std::string error;assert(fo3player::LoadCatalog(path,catalog,error));
  size_t linkedRoutes=0;for(const auto& r:catalog.pipboy.actorPatrols)if(!r.second.empty())++linkedRoutes;
  std::cout<<"Original valid actor-linked Patrol routes="<<linkedRoutes<<'\n';assert(linkedRoutes>10);
  assert(catalog.pipboy.packages.at(0xc2f11).escortDistance==300&&catalog.pipboy.packages.at(0xc2f11).escortDistanceValid);
  assert(!catalog.pipboy.packages.at(0xc2f11).scripted&&catalog.pipboy.packages.at(0xc2f11).target.value==0x14);
  assert(catalog.pipboy.packages.at(0x7e6dd).patrol.size()==4);
  assert(!catalog.pipboy.packages.at(0x7e6dd).patrolRepeat);
  assert(catalog.pipboy.packages.at(0x7e6dd).patrol.back().placement.patrolWait==20);
  gPlayerSession=std::make_unique<Session>(std::move(catalog));
  std::vector<Fo3NpcActorQ230> actors;assert(LoadFo3CellActors(0xa96,actors,path));
  for(const auto& source:actors){Q230ActorVisual target;target.source=source;target.runtime.position=Q240ScenePosition({source.x,source.y,source.z});packageTargets.push_back(target);}
  size_t fleeFrom=0;for(const auto& entry:gPlayerSession->player.Definitions().pipboy.packages){const auto& p=entry.second;
    if(p.type==10&&!p.scripted&&!p.procedureActions&&!p.location.valid&&p.target.valid&&p.target.type==0&&p.target.radius>0)++fleeFrom;
  }
  assert(fleeFrom==4);std::cout<<"Original script-free Flee From definitions="<<fleeFrom<<'\n';
  const auto& originalFlee=gPlayerSession->player.Definitions().pipboy.packages.at(0x58a13);
  assert(originalFlee.type==10&&originalFlee.target.value==0x14&&originalFlee.target.radius==2000&&!originalFlee.scripted);
  const auto& npcFlee=gPlayerSession->player.Definitions().pipboy.packages.at(0x4e5de);
  assert(npcFlee.target.value==0x4e5c3&&npcFlee.target.radius==256&&!npcFlee.location.valid);
  std::vector<Fo3NpcNavMeshQ240> meshes;assert(LoadFo3NpcNavigationQ240(0xa96,0xa74,meshes,path));
  auto graph=std::make_shared<Q240NavigationGraph>();
  for(auto& mesh:meshes){graph->triangleOffsets.push_back(graph->triangleCount);graph->triangleCount+=mesh.triangles.size();graph->byForm[mesh.formId]=graph->meshes.size();graph->meshes.push_back(std::make_shared<Fo3NpcNavMeshQ240>(std::move(mesh)));}
  gCurrentCellFormId=0xa96;gExteriorWorldspaceQ1890=0xa74;
  std::cout<<"Original Megaton NAVM meshes="<<graph->meshes.size()<<" triangles="<<graph->triangleCount<<'\n';
  size_t edges=0,rejected=0;
  for(size_t m=0;m<graph->meshes.size();++m)for(size_t t=0;t<graph->meshes[m]->triangles.size();++t)Q240Neighbors(*graph,{m,t},[&](const Q240Node& to){++edges;std::array<float,3> p{};if(!Q240Portal(*graph,{m,t},to,p))++rejected;});
  std::cout<<"Original Megaton links="<<edges<<" rejected portals="<<rejected<<'\n';
  // Execute an original Flee From definition on the original resident NAVM,
  // independently of its quest-specific condition/PKID selection. No IDs enter
  // runtime behavior, and no authored conditions are bypassed in the game.
  assert(!actors.empty());Q230ActorVisual escaping;escaping.source=actors.front();escaping.navigationGraph=graph;
  escaping.runtime.position=Q240ScenePosition({escaping.source.x,escaping.source.y,escaping.source.z});escaping.aiPackage=0x58a13;
  const auto origin=Q240GamePosition(escaping);
  for(int i=0;i<5000;++i)Q240AdvanceFleePackage(escaping,originalFlee,origin,.016f,i*.016);
  assert(Q240PlanarDistance(Q240GamePosition(escaping),origin)>=originalFlee.target.radius);
  std::cout<<"Original Flee From minimum maintained on resident NAVM="<<originalFlee.target.radius<<'\n';
  for(const auto& source:actors){
    Q230ActorVisual actor;actor.source=source;actor.runtime.position=Q240ScenePosition({source.x,source.y,source.z});actor.navigationGraph=graph;
    uint32_t id=0;std::array<float,3> anchor{};float radius=0;
    const auto* p=Q240SelectPackage(actor,id,anchor,radius);
    std::cout<<source.editorId<<" selected="<<std::hex<<id<<std::dec<<" type="<<(p?int(p->type):-1)<<" radius="<<radius<<'\n';
    if(!p)continue;actor.aiPackage=id;
    assert(Q240BuildPackagePath(actor,*p,anchor,radius));
    actor.aiPackage=0;auto before=actor.runtime.position;for(int frame=0;frame<2400;++frame)Q240UpdateNpcPackage(actor,frame*.016);
    if(source.baseFormId==0xa60){
      assert(actor.aiPathIndex==actor.aiPathGame.size()); // Authored WaitForGreeting Travel.
      fo3dialogue::Context ctx;ctx.player=&gPlayerSession->player;ctx.speaker={source.refFormId,source.baseFormId};ctx.target={0x14,7};
      fo3dialogue::Session dialogue;assert(dialogue.Start(ctx,gPlayerSession->player));
      while(dialogue.phase==fo3dialogue::Phase::Speaking)dialogue.AudioDone(dialogue.audioToken,true,ctx,gPlayerSession->player);
      assert(dialogue.choices.size()==3);
      assert(dialogue.Choose(0,ctx,gPlayerSession->player));
      while(dialogue.phase==fo3dialogue::Phase::Speaking)dialogue.AudioDone(dialogue.audioToken,true,ctx,gPlayerSession->player);
      assert(dialogue.Choose(0,ctx,gPlayerSession->player));
      while(dialogue.phase==fo3dialogue::Phase::Speaking)dialogue.AudioDone(dialogue.audioToken,true,ctx,gPlayerSession->player);
      Q240UpdateNpcPackage(actor,40);
      assert(actor.aiPackage!=0x3dbce);
      const auto released=actor.runtime.position;
      for(int frame=1;frame<1200;++frame) {
        Q240UpdateNpcPackage(actor,40+frame*.016);
        if(actor.runtime.animation==fo3npc::Animation::Walk&&actor.aiPathIndex<actor.aiPathSurfaces.size()) {
          const auto face=actor.aiPathSurfaces[actor.aiPathIndex];const auto game=Q240GamePosition(actor);
          const auto onSurface=Q240GroundPoint(*graph->meshes[face.first],face.second,game);
          assert(std::fabs(game[2]-onSurface[2])<.01f);
        }
      }
      assert(actor.runtime.position!=released);
      std::cout<<"Lucas after greeting package="<<std::hex<<actor.aiPackage<<std::dec<<" sequence="<<actor.aiSequence<<'\n';
    }
    std::cout<<"Actor update complete; sequence="<<actor.aiSequence<<" waypoints="<<actor.aiPathGame.size()<<" next="<<actor.aiPathIndex<<" deltaY="<<actor.runtime.position[1]-before[1]<<'\n';
  }
}
int main(int argc,char** argv) {
  fo3pipdata::PackageSchedule schedule;schedule.valid=true;schedule.hour=22;schedule.duration=4;
  assert(Q240ScheduleActive(schedule,23)&&Q240ScheduleActive(schedule,1)&&!Q240ScheduleActive(schedule,12));
  auto graph=Graph();float distance=0;
  auto start=Q240NearestNode(*graph,{1010,2010,20},&distance);
  assert(start.mesh==0&&distance<.001f); // 457 units from centroid, on surface.
  std::vector<std::array<float,3>> path;
  assert(Q240Path(*graph,{1010,2010,20},{1900,2900,20},path));
  assert(path.size()==2&&path.back()[0]==1900&&path.back()[1]==2900);
  assert(path[0][0]==1500&&path[0][1]==2500); // Actual shared portal.
  assert(Q240Path(*graph,{1010,2010,20},{1050,2050,20},path));
  assert(path.size()==1&&path.back()[0]==1050); // Same-triangle exact target.
  assert(!Q240Path(*Graph(true),{1010,2010,20},{1900,2900,20},path));
  auto outOfRange=Graph();auto invalidTarget=std::make_shared<Fo3NpcNavMeshQ240>(*outOfRange->meshes[0]);
  invalidTarget->external[0].triangle=99;outOfRange->meshes[0]=invalidTarget;
  assert(!Q240Path(*outOfRange,{1010,2010,20},{1900,2900,20},path));
  auto disconnected=Graph();auto altered=std::make_shared<Fo3NpcNavMeshQ240>(*disconnected->meshes[1]);
  for(auto& v:altered->vertices)v[0]+=100;disconnected->meshes[1]=altered;
  assert(!Q240Path(*disconnected,{1010,2010,20},{2000,2900,20},path));
  // A sloped surface must determine the new root height immediately, even
  // when an authored actor placement starts 150 units above/below that surface.
  auto sloped=std::make_shared<Fo3NpcNavMeshQ240>(*graph->meshes[0]);
  sloped->vertices[0][2]=20;sloped->vertices[1][2]=120;sloped->vertices[2][2]=220;
  auto ground=Q240GroundPoint(*sloped,0,{1250,2250,170});
  assert(std::fabs(ground[2]-95)<.001f);
  ground=Q240GroundPoint(*sloped,0,{1250,2250,-170});assert(std::fabs(ground[2]-95)<.001f);
  auto grounded=Actor(6);
  auto single=std::make_shared<Q240NavigationGraph>();single->meshes={sloped};single->byForm[100]=0;single->triangleOffsets={0};single->triangleCount=1;
  grounded.navigationGraph=single;
  auto& target=gPlayerSession->player; (void)target;
  grounded.aiPackage=50;grounded.aiSequence=1;grounded.aiPathGame={{1400,2200,100}};grounded.aiPathSurfaces={{0,0}};
  grounded.runtime.position=Q240ScenePosition({1250,2250,245});grounded.runtime.yaw=std::atan2(-150.f,50.f)+3.14159265f;grounded.aiLastUpdate=0;
  Q240UpdateNpcPackage(grounded,.1);
  assert(grounded.runtime.animation==fo3npc::Animation::TurnLeft||grounded.runtime.animation==fo3npc::Animation::TurnRight);
  const auto actual=Q240GamePosition(grounded);
  const auto expected=Q240GroundPoint(*sloped,0,actual);assert(std::fabs(actual[2]-expected[2])<.001f);
  // Wander must keep walking within one large authored triangle.
  auto lone=Actor(5);lone.navigationGraph=single;
  std::array<float,3> anchor{1250,2250,95};
  assert(Q240BuildPackagePath(lone,gPlayerSession->player.Definitions().pipboy.packages.at(50),anchor,300));
  assert(lone.aiPathGame.size()==1&&Q240PlanarDistance(lone.aiPathGame.back(),anchor)<=300);
  auto actor=Actor(6);
  const auto game=Q240GamePosition(actor);assert(game[0]==1010&&game[1]==2010&&game[2]==20);
  Q240UpdateNpcPackage(actor,0);actor.lastFrame=1;
  Q240UpdateNpcPackage(actor,.1);assert(actor.lastFrame==UINT64_MAX); // Same-frame prior skin sample is invalidated.
  for(int i=2;i<1000;++i)Q240UpdateNpcPackage(actor,i*.1);
  assert(actor.aiPackage==50&&actor.aiSequence==1&&actor.aiPathIndex==actor.aiPathGame.size());
  auto final=Q240GamePosition(actor);assert(std::fabs(final[0]-1900)<.01&&std::fabs(final[1]-2900)<.01);
  Q240UpdateNpcPackage(actor,101);assert(actor.aiSequence==1&&actor.runtime.speed==0);
  for(uint8_t type:{5,12}) {
    actor=Actor(type);
    for(int i=0;i<1500;++i)Q240UpdateNpcPackage(actor,i*.1);
    assert(actor.aiSequence>3); // Production update traverses and builds more routes.
    actor.runtime.BeginDialogue();auto held=actor.runtime.position;
    Q240UpdateNpcPackage(actor,151);assert(actor.runtime.position==held);
    actor.runtime.EndDialogue();Q240UpdateNpcPackage(actor,152);assert(!actor.runtime.dialogue);
  }
  {
    auto guard=Actor(14);for(int i=0;i<1500;++i)Q240UpdateNpcPackage(guard,i*.1);assert(guard.aiPackage==50&&guard.aiSequence==1);
    const auto held=guard.runtime.position;Q240UpdateNpcPackage(guard,160);assert(guard.runtime.position==held&&guard.runtime.speed==0);
    auto ambush=Actor(9);Q240UpdateNpcPackage(ambush,1);assert(ambush.aiPackage==0);
  }
  {
    auto inherited=Actor(6);auto c=gPlayerSession->player.Definitions();
    c.pipboy.dialogueActors[44].templateActor=43;c.pipboy.dialogueActors[44].templateFlags=16;
    inherited.source.baseFormId=44;gPlayerSession=std::make_unique<Session>(std::move(c));
    Q240UpdateNpcPackage(inherited,1);assert(inherited.aiPackage==50);
  }
  {
    auto patrol=Actor(13);auto c=gPlayerSession->player.Definitions();
    c.pipboy.referenceScripts[42]={"actor",43};
    c.pipboy.packages[50].schedule.duration=24;c.pipboy.packages[50].patrolRepeat=false;
    c.pipboy.targets[60].linkedReference=61;c.pipboy.targets[60].patrolWait=2;
    c.pipboy.targets[61]=c.pipboy.targets[60];c.pipboy.targets[61].linkedReference=0;c.pipboy.targets[61].x=1100;c.pipboy.targets[61].y=2100;
    fo3pipdata::Finalize(c.pipboy);assert(c.pipboy.packages.at(50).patrol.size()==2);
    gPlayerSession=std::make_unique<Session>(c);
    for(int i=0;i<1600;++i)Q240UpdateNpcPackage(patrol,i*.1);
    assert(patrol.aiSequence==3&&Q240PlanarDistance(Q240GamePosition(patrol),{1100,2100,20})<.01f);
    // Saved leg progress must build a route to its next authored marker.
    patrol.aiSequence=2;patrol.aiPathGame.clear();patrol.aiRepathAt=0;Q240UpdateNpcPackage(patrol,170);
    assert(patrol.aiPathGame.back()[0]==1100);
    // Unsupported marker behavior rejects the whole high-priority route.
    c.pipboy.targets[60].patrolAction=true;fo3pipdata::Finalize(c.pipboy);
    assert(c.pipboy.packages.at(50).patrol.empty()&&!c.pipboy.packages.at(50).patrolUnsupported.empty());
    // Open repeat routes start at the closest marker, then reverse at endpoints.
    c.pipboy.targets[60].patrolAction=false;c.pipboy.targets[61]=c.pipboy.targets[60];c.pipboy.targets[61].linkedReference=0;c.pipboy.targets[61].x=1100;c.pipboy.targets[61].y=2100;c.pipboy.packages[50].patrolRepeat=true;fo3pipdata::Finalize(c.pipboy);
    gPlayerSession=std::make_unique<Session>(c);patrol.runtime.position=Q240ScenePosition({1100,2100,20});patrol.aiSequence=0;patrol.aiPathGame.clear();
    Q240UpdateNpcPackage(patrol,180);assert(patrol.aiSequence==2);
    Q240UpdateNpcPackage(patrol,180.1);assert(patrol.aiSequence==3);
    const auto waiting=patrol.runtime.position;Q240UpdateNpcPackage(patrol,181);assert(patrol.runtime.position==waiting);
    Q240UpdateNpcPackage(patrol,183);assert(patrol.aiPathGame.back()[0]==1900);
    c.pipboy.packages[50].location.type=6;c.pipboy.packages[50].location.value=0;c.pipboy.targets[42].linkedReference=60;
    fo3pipdata::Finalize(c.pipboy);assert(c.pipboy.actorPatrols.at((uint64_t(42)<<32)|50).size()==2);
    gPlayerSession=std::make_unique<Session>(c);uint32_t id=0;std::array<float,3> start{};float radius=0;
    assert(Q240SelectPackage(patrol,id,start,radius)&&id==50&&start[0]==1900);
    // Missing own linked reference invalidates this package rather than using
    // a fabricated destination or the unused PLDT value.
    c.pipboy.targets[42].linkedReference=0;fo3pipdata::Finalize(c.pipboy);
    gPlayerSession=std::make_unique<Session>(c);assert(!Q240SelectPackage(patrol,id,start,radius));
  }
  {
    auto escort=Actor(2);auto c=gPlayerSession->player.Definitions();auto& p=c.pipboy.packages[50];p.target={};p.target.valid=true;p.target.type=0;p.target.value=0x14;p.escortDistance=300;p.escortDistanceValid=true;
    gPlayerSession=std::make_unique<Session>(c);gQ210Head=Q240ScenePosition({1010,2010,20});
    Q240UpdateNpcPackage(escort,0);assert(escort.aiPackage==50&&escort.aiSequence==1&&!escort.aiPathGame.empty());
    gQ210Head=Q240ScenePosition({500,1500,20});const auto held=escort.runtime.position;Q240UpdateNpcPackage(escort,1);assert(escort.runtime.position==held&&escort.runtime.speed==0);
    const auto leadPoint=Q240ScenePosition(escort.aiPathGame.front());escort.runtime.yaw=std::atan2(-(leadPoint[0]-held[0]),-(leadPoint[2]-held[2]));gQ210Head=Q240ScenePosition({1800,2800,20});Q240UpdateNpcPackage(escort,2);assert(escort.runtime.position!=held); // Player ahead does not stall leader.
    for(int i=0;i<1300;++i){gQ210Head=escort.runtime.position;Q240UpdateNpcPackage(escort,3+i*.1);}
    assert(escort.aiSequence==2&&Q240PlanarDistance(Q240GamePosition(escort),{1900,2900,20})<.01f);
    c.pipboy.packages[50].target.value=60;gPlayerSession=std::make_unique<Session>(c);uint32_t id=0;std::array<float,3> point{};float radius=0;assert(!Q240SelectPackage(escort,id,point,radius));
    c.pipboy.packages[50].target.value=0x14;gPlayerSession=std::make_unique<Session>(c);escort.aiSequence=0;escort.aiPathGame.clear();escort.aiPathIndex=0;escort.aiRepathAt=0;escort.runtime.position=Q240ScenePosition({1010,2010,20});gQ210Head=Q240ScenePosition({1900,2900,20});
    Q240UpdateNpcPackage(escort,140);assert(escort.aiSequence==0&&!escort.aiPathGame.empty()); // First approach an out-of-range escorted player.
  }
  {
    // Real actor targets use the resident root, including before persistence.
    auto follower=Actor(1);auto c=gPlayerSession->player.Definitions();auto& p=c.pipboy.packages[50];p.target.valid=true;p.target.type=0;p.target.value=70;p.target.radius=100;
    c.pipboy.targets[70].base=71;c.weapons.actors[71].health=100;c.pipboy.targets[70].world=2;c.pipboy.targets[70].x=99999;
    c.pipboy.packages[52]=c.pipboy.packages[50];c.pipboy.packages[52].type=6;c.pipboy.dialogueActors[43].packages={50,52};
    gPlayerSession=std::make_unique<Session>(c);packageTargets.emplace_back();auto& leader=packageTargets.back();leader.source.refFormId=70;leader.source.baseFormId=71;leader.runtime.position=Q240ScenePosition({1900,2900,20});
    Q240UpdateNpcPackage(follower,0);assert(follower.aiPackage==50&&follower.aiPathGame.back()[0]==1900);
    leader.runtime.position=Q240ScenePosition({1400,2200,20});Q240UpdateNpcPackage(follower,2);assert(follower.aiPathGame.back()[0]==1400);
    fo3player::ActorState savedLeader;savedLeader.world=2;savedLeader.cell=1;savedLeader.position={1700,2700,20};assert(gPlayerSession->player.UpdateActor(70,savedLeader));
    Q240UpdateNpcPackage(follower,3);assert(follower.aiPathGame.back()[0]==1700); // Canonical restore precedes the leader's first simulation.
    leader.stateRestored=true;Q240UpdateNpcPackage(follower,4);assert(follower.aiPathGame.back()[0]==1400); // Live root wins over last persisted root.
    leader.runtime.activity=fo3npc::Activity::Dead;Q240UpdateNpcPackage(follower,5);assert(follower.aiPackage==52); // Lower priority valid route, never a corpse's spawn.
    leader.runtime.activity=fo3npc::Activity::Package;Q240UpdateNpcPackage(follower,6);assert(follower.aiPackage==50);
    c.pipboy.packages[50].type=10;c.pipboy.packages[50].location={};c.pipboy.packages[50].target.radius=500;gPlayerSession=std::make_unique<Session>(c);
    follower.aiPackage=0;follower.aiSequence=0;leader.runtime.position=follower.runtime.position;Q240UpdateNpcPackage(follower,7);assert(follower.aiPackage==50&&!follower.aiPathGame.empty());
    leader.rigs.clear();Q240UpdateNpcPackage(follower,8);assert(follower.aiPackage==52);leader.rigs={1};
    packageTargets.clear();Q240UpdateNpcPackage(follower,9);assert(follower.aiPackage==52);
  }
  {
    auto flee=Actor(10);auto c=gPlayerSession->player.Definitions();auto& p=c.pipboy.packages[50];p.location={};p.target.valid=true;p.target.type=0;p.target.value=0x14;p.target.radius=500;
    fo3pipdata::Condition gate;gate.function=72;gate.a=43;gate.value=1;p.conditions={gate};
    c.pipboy.packages[52]=c.pipboy.packages[50];c.pipboy.packages[52].type=6;c.pipboy.packages[52].conditions.clear();c.pipboy.packages[52].location.valid=true;c.pipboy.packages[52].location.type=0;c.pipboy.packages[52].location.value=60;
    c.pipboy.dialogueActors[43].packages={50,52};gPlayerSession=std::make_unique<Session>(c);gQ210Head=flee.runtime.position;
    Q240UpdateNpcPackage(flee,0);assert(flee.aiPackage==50&&flee.aiSequence==1&&flee.runtime.activity==fo3npc::Activity::Package&&!flee.aiPathGame.empty());
    assert(Q240PlanarDistance(flee.aiPathGame.back(),Q240GamePosition(flee))>=500);
    c.pipboy.packages[50].conditions[0].a=44;gPlayerSession=std::make_unique<Session>(c);Q240UpdateNpcPackage(flee,.1);assert(flee.aiPackage==50); // Valid false condition does not terminate an entered Flee.
    auto fresh=flee;fresh.aiPackage=0;fresh.aiSequence=0;Q240UpdateNpcPackage(fresh,.2);assert(fresh.aiPackage==52); // Same false condition prevents entry.
    for(int i=2;i<400;++i)Q240UpdateNpcPackage(flee,i*.1);
    const auto safe=flee.runtime.position;assert(Q240PlanarDistance(Q240GamePosition(flee),{1010,2010,20})>=500);Q240UpdateNpcPackage(flee,41);assert(flee.runtime.position==safe&&flee.runtime.speed==0&&flee.aiPathGame.empty());
    gQ210Head=flee.runtime.position;Q240UpdateNpcPackage(flee,42);assert(!flee.aiPathGame.empty()); // Threat approaches; resume avoidance.
    const auto route=flee.aiPathGame;gQ210Head=flee.runtime.position;gQ210Head[0]+=.5f;Q240UpdateNpcPackage(flee,42.1);assert(flee.aiPathGame==route); // No per-frame rebuild.
    flee.runtime.BeginDialogue();const auto held=flee.runtime.position;Q240UpdateNpcPackage(flee,43);assert(flee.runtime.position==held);flee.runtime.EndDialogue();
    flee.runtime.BeginCombat(0x14);Q240UpdateNpcPackage(flee,44);assert(flee.runtime.position==held&&flee.runtime.activity==fo3npc::Activity::Combat);flee.runtime.EndCombat();Q240UpdateNpcPackage(flee,45);assert(flee.aiPackage==50&&flee.aiSequence==1);
    flee.runtime.activity=fo3npc::Activity::Unconscious;const auto unconscious=flee.runtime.position;Q240UpdateNpcPackage(flee,45.1);assert(flee.runtime.position==unconscious&&flee.runtime.activity==fo3npc::Activity::Unconscious);flee.runtime.activity=fo3npc::Activity::Package;
    c.pipboy.packages[50].conditions[0].function=9999;gPlayerSession=std::make_unique<Session>(c);Q240UpdateNpcPackage(flee,46);assert(flee.aiPackage==52); // Unsupported condition never becomes success, even after entry.
    c.pipboy.packages[50].conditions.clear();c.pipboy.packages[50].location.valid=true;gPlayerSession=std::make_unique<Session>(c);Q240UpdateNpcPackage(flee,47);assert(flee.aiPackage==52); // Start-location/cower variant explicitly rejected.
    c.pipboy.packages[50].location={};c.pipboy.packages[50].schedule.valid=true;c.pipboy.packages[50].schedule.hour=12;c.pipboy.packages[50].schedule.duration=1;
    gPlayerSession=std::make_unique<Session>(c);Q240UpdateNpcPackage(flee,48);assert(flee.aiPackage==50);packageHour=14;Q240UpdateNpcPackage(flee,49);assert(flee.aiPackage==52);packageHour=12;
    c.pipboy.packages[50].location={};c.pipboy.packages[50].procedureActions=true;gPlayerSession=std::make_unique<Session>(c);Q240UpdateNpcPackage(flee,50);assert(flee.aiPackage==52);
    std::array<float,3> endpoint{};auto cut=Graph(true);assert(!Q240FleeDestination(*cut,Q240Centroid(*cut->meshes[0],0),{1010,2010,20},2000,endpoint)); // Disconnected safer mesh is unavailable.
    assert(!Q240FleeDestination(*Graph(),{1010,2010,2000},{1010,2010,20},500,endpoint)); // Off-surface actor cannot teleport onto NAVM.
  }
  {
    auto malformed=Actor(6);auto c=gPlayerSession->player.Definitions();c.pipboy.packages[50].combatStyleValid=false;c.pipboy.packages[52]=c.pipboy.packages[50];c.pipboy.packages[52].combatStyleValid=true;c.pipboy.dialogueActors[43].packages={50,52};gPlayerSession=std::make_unique<Session>(c);Q240UpdateNpcPackage(malformed,1);assert(malformed.aiPackage==52);
  }
  if(argc>1)Original(argv[1]);
  std::cout<<"Production NPC package traversal, surface projection, shared portals, Travel and repathing passed\n";
  return 0;
}
