#include "npc/fo3-npc.h"
#include "npc/fo3-npc-state.h"
#include "player/fo3-player-state.h"
#include "dialogue/fo3-dialogue-conditions.h"
#include <memory>
#include <unordered_set>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>
template<class... T> void TestLog(const char*,T...){ }
#define Q6H_LOGI(...) TestLog(__VA_ARGS__)
namespace fo3tod {float WrapHour(float h){return std::fmod(h+24.f,24.f);}}
float GetFo3TimeOfDayHour(){return 12;}
float gSceneCenterXQ1730=1000,gSceneCenterYQ1730=2000,gSceneFloorZQ1730=20;
constexpr float FO3_UNITS_PER_METRE=100,FLOOR_Y=-1,SCENE_FORWARD=-3;
std::array<float,3> gQ210Head{};
uint32_t gCurrentCellFormId=1,gExteriorWorldspaceQ1890=2;
struct Session {fo3player::Player player;explicit Session(fo3player::Catalog c):player(std::move(c)) {}};
std::unique_ptr<Session> gPlayerSession;
struct Q240NavigationGraph {
  std::vector<std::shared_ptr<const Fo3NpcNavMeshQ240>> meshes;
  std::unordered_map<uint32_t,size_t> byForm;
  std::vector<size_t> triangleOffsets;
  size_t triangleCount=0;
};
struct Q230ActorVisual {
  Fo3NpcActorQ230 source;
  fo3npc::RuntimeState runtime;
  std::vector<int> rigs{1};
  std::shared_ptr<const Q240NavigationGraph> navigationGraph;
  std::vector<std::array<float,3>> aiPathGame;
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
int main() {
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
  std::cout<<"Production NPC package traversal, surface projection, shared portals, Travel and repathing passed\n";
}
