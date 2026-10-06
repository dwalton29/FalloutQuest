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
float GetFo3TimeOfDayHour(){return 12;}
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
#include "npc/fo3-npc-package-runtime.inc"
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
  assert(catalog.pipboy.packages.at(0x7e6dd).patrol.size()==4);
  assert(!catalog.pipboy.packages.at(0x7e6dd).patrolRepeat);
  assert(catalog.pipboy.packages.at(0x7e6dd).patrol.back().placement.patrolWait==20);
  gPlayerSession=std::make_unique<Session>(std::move(catalog));
  std::vector<Fo3NpcActorQ230> actors;assert(LoadFo3CellActors(0xa96,actors,path));
  std::vector<Fo3NpcNavMeshQ240> meshes;assert(LoadFo3NpcNavigationQ240(0xa96,0xa74,meshes,path));
  auto graph=std::make_shared<Q240NavigationGraph>();
  for(auto& mesh:meshes){graph->triangleOffsets.push_back(graph->triangleCount);graph->triangleCount+=mesh.triangles.size();graph->byForm[mesh.formId]=graph->meshes.size();graph->meshes.push_back(std::make_shared<Fo3NpcNavMeshQ240>(std::move(mesh)));}
  gCurrentCellFormId=0xa96;gExteriorWorldspaceQ1890=0xa74;
  std::cout<<"Original Megaton NAVM meshes="<<graph->meshes.size()<<" triangles="<<graph->triangleCount<<'\n';
  size_t edges=0,rejected=0;
  for(size_t m=0;m<graph->meshes.size();++m)for(size_t t=0;t<graph->meshes[m]->triangles.size();++t)Q240Neighbors(*graph,{m,t},[&](const Q240Node& to){++edges;std::array<float,3> p{};if(!Q240Portal(*graph,{m,t},to,p))++rejected;});
  std::cout<<"Original Megaton links="<<edges<<" rejected portals="<<rejected<<'\n';
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
  grounded.runtime.position=Q240ScenePosition({1250,2250,245});grounded.runtime.yaw=std::atan2(-150.f,50.f);grounded.aiLastUpdate=0;
  Q240UpdateNpcPackage(grounded,.1);
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
  if(argc>1)Original(argv[1]);
  std::cout<<"Production NPC package traversal, surface projection, shared portals, Travel and repathing passed\n";
  return 0;
}
