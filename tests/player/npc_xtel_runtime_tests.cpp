// Production package executor with the v185 directed XTEL index injected
// through the same adapter used by the Android scene worker.
#include <memory>
#include "data/fo3-xtel-index.h"
static std::shared_ptr<const fo3xtel::Index> activeTestXtels;
#define FO3_NPC_XTEL_INDEX() activeTestXtels
#define main WorldXtelFixtureMain
#include "../world/xtel_index_tests.cpp"
#undef main
#define main PackageFixtureMain
#include "npc_package_runtime_tests.cpp"
#undef main

static void ActorCrossCellHandoff() {
  // The world-index fixture intentionally sets the destination door's
  // initially disabled flag. For the positive traversal case, clear only
  // that source-record flag; test the disabled case separately below.
  Bytes master=Master();bool patched=false;
  for(size_t at=0;at+24<=master.size();++at){
    if(std::memcmp(master.data()+at,"REFR",4)!=0)continue;
    const uint32_t id=uint32_t(master[at+12])|(uint32_t(master[at+13])<<8u)|
      (uint32_t(master[at+14])<<16u)|(uint32_t(master[at+15])<<24u);
    if(id==0x102){U32(master,at+8,0);patched=true;break;}
  }
  assert(patched);
  const std::string path=Write(master);
  fo3xtel::Index index;std::string error;
  assert(index.Build(path,error));std::remove(path.c_str());
  activeTestXtels=std::make_shared<const fo3xtel::Index>(std::move(index));
  auto actor=Actor(6);
  auto catalog=gPlayerSession->player.Definitions();
  auto& p=catalog.pipboy.packages[50];
  p.schedule.valid=true;p.schedule.hour=13;p.schedule.duration=1;
  p.location={0,777,0,true};
  catalog.pipboy.targets[777].cell=0x300;
  catalog.pipboy.targets[777].world=0x700;
  catalog.pipboy.targets[777].x=1500;
  catalog.pipboy.targets[777].y=2400;
  catalog.pipboy.targets[0x101].cell=0x200;
  catalog.pipboy.targets[0x101].world=0;
  catalog.pipboy.targets[0x101].base=0x1000;
  catalog.pipboy.targets[0x101].x=1900;
  catalog.pipboy.targets[0x101].y=2900;
  catalog.pipboy.targets[0x101].z=20;
  catalog.references[0x101].base=0x1000;
  catalog.references[0x101].cell=0x200;
  gPlayerSession=std::make_unique<Session>(std::move(catalog));
  gCurrentCellFormId=0x200;gExteriorWorldspaceQ1890=0;
  uint32_t chosenDoor=0;std::array<float,3> approach{};
  const bool eligibleDoor=Q240NpcRemoteDoor(actor,0x300,chosenDoor,approach);
  if(!eligibleDoor)std::cerr<<"No XTEL candidate: ref="<<std::hex<<chosenDoor
    <<" actor="<<actor.source.refFormId<<std::dec
    <<" health="<<gPlayerSession->player.ActorHealth(actor.source.refFormId)
    <<" doorAccess="<<gPlayerSession->player.CanActorOpenDoor(actor.source.refFormId,0x101)
    <<" graph="<<actor.navigationGraph->triangleCount<<"\n";
  assert(eligibleDoor&&chosenDoor==0x101);
  // No building transition is authorised before the original PACK hour.
  packageHour=12;
  Q240UpdateNpcPackage(actor,0);
  assert(!actor.runtime.offScene&&actor.aiPackage!=50);
  // Clock transition makes the same source-record door eligible.
  packageHour=13;
  bool handedOff=false;
  for(int i=0;i<600&&!handedOff;++i){
    Q240UpdateNpcPackage(actor,double(i)*.1);
    handedOff=actor.runtime.offScene;
  }
  if(!handedOff)std::cerr<<"No handoff: selected="<<std::hex<<actor.aiPackage
    <<" xtelDoor="<<actor.runtime.xtelDoor
    <<" xtelCell="<<actor.runtime.xtelCell<<std::dec
    <<" procedure="<<int(actor.runtime.procedure)
    <<" pathIndex="<<actor.aiPathIndex<<" pathCount="<<actor.aiPathGame.size()
    <<" savedRetry="<<actor.runtime.packageRetryAfter.size()<<"\n";
  assert(handedOff&&actor.aiPackage==50);
  const auto& saved=gPlayerSession->player.Snapshot().actors.at(actor.source.refFormId);
  assert(saved.cell==0x300&&saved.world==0x700);
  assert((saved.position==std::array<float,3>{11.f,12.f,13.f}));
  assert(saved.sequence==0&&saved.package==50);
  const auto before=saved.position;
  Q240UpdateNpcPackage(actor,90);
  assert(gPlayerSession->player.Snapshot().actors.at(actor.source.refFormId).position==before);

  // No player key can substitute for an NPC key; a locked load door blocks
  // XTEL package selection and never transfers canonical state.
  auto locked=gPlayerSession->player.Definitions();
  auto second=Actor(6);
  locked.references[0x101].locked=true;locked.references[0x101].key=0;
  gPlayerSession=std::make_unique<Session>(std::move(locked));
  Q240UpdateNpcPackage(second,0);
  assert(!second.runtime.offScene&&second.aiPackage!=50);
  activeTestXtels.reset();
  packageHour=12;
  gCurrentCellFormId=1;gExteriorWorldspaceQ1890=2;
  std::cout<<"NPC XTEL route/door permission/atomic actor CELL transfer passed\n";
}

static void RankedDoorsAndPersistentExterior() {
  Bytes master=Record("TES4",0);Append(master,Record("DOOR",0x1000));
  Bytes sources,destinations;
  for(uint32_t i=0;i<3;++i){
    Append(sources,Record("REFR",0x110+i,DoorRef(0x1000,0x210+i,false)));
    Append(destinations,Record("REFR",0x210+i,DoorRef(0x1000,0x110+i,false)));
  }
  Append(destinations,Record("REFR",0x310,DoorRef(0x1000,0x410,false)));
  Append(master,Record("CELL",0x300));Append(master,Group(0x300,6,destinations));
  Append(master,Record("CELL",0x400));Append(master,Group(0x400,6,Record("REFR",0x410,DoorRef(0x1000,0x310,false))));
  Bytes world=Record("CELL",0x201);Append(world,Group(0x201,6,sources));
  Append(master,Group(0xa74,1,world));
  auto path=Write(master);fo3xtel::Index index;std::string error;
  assert(index.Build(path,error));std::remove(path.c_str());
  activeTestXtels=std::make_shared<const fo3xtel::Index>(std::move(index));
  auto actor=Actor(6);auto catalog=gPlayerSession->player.Definitions();
  catalog.pipboy.cellWorlds[0x201]=0xa74;
  catalog.pipboy.targets[777].cell=0x300;
  catalog.pipboy.packages[50].location={0,777,0,true};
  for(uint32_t i=0;i<3;++i){
    auto& t=catalog.pipboy.targets[0x110+i];t.cell=0x201;t.world=0xa74;t.base=0x1000;
    t.x=1800+float(i)*50;t.y=2800;t.z=20;
    auto& ref=catalog.references[0x110+i];ref.base=0x1000;ref.cell=0x201;
  }
  gPlayerSession=std::make_unique<Session>(catalog);
  gCurrentCellFormId=0x200;gExteriorWorldspaceQ1890=0xa74;
  uint32_t chosen=0;std::array<float,3> point{};
  auto before=gQ240PathsPlanned;
  assert(Q240NpcRemoteDoor(actor,0x300,chosen,point)&&chosen==0x110);
  assert(gQ240PathsPlanned-before==0); // Eligibility does not build/discard any A* route.
  auto lockedCatalog=catalog;lockedCatalog.references[0x110].locked=true;
  gPlayerSession=std::make_unique<Session>(lockedCatalog);chosen=0;
  assert(Q240NpcRemoteDoor(actor,0x300,chosen,point)&&chosen==0x111);
  gPlayerSession=std::make_unique<Session>(catalog);
  // The nearest door can be on an unreachable stacked walkway. Ranking must
  // still try the next door, without accepting an unconnected NAVM component.
  auto graph=Graph();auto isolated=std::make_shared<Fo3NpcNavMeshQ240>();
  isolated->formId=999;isolated->vertices={{1000,2000,220},{1100,2000,220},{1000,2100,220}};
  Fo3NpcNavTriangleQ240 triangle;triangle.vertex[0]=0;triangle.vertex[1]=1;triangle.vertex[2]=2;
  isolated->triangles.push_back(triangle);graph->triangleOffsets.push_back(graph->triangleCount);
  graph->triangleCount++;graph->byForm[999]=graph->meshes.size();graph->meshes.push_back(isolated);
  actor.navigationGraph=graph;
  catalog.pipboy.targets[0x110].x=1010;catalog.pipboy.targets[0x110].y=2010;catalog.pipboy.targets[0x110].z=220;
  gPlayerSession=std::make_unique<Session>(catalog);before=gQ240PathsPlanned;chosen=0;
  assert(Q240NpcRemoteDoor(actor,0x300,chosen,point)&&chosen==0x111);
  assert(gQ240PathsPlanned-before==0);
  // A two-hop package retains the first XTEL destination separately from
  // the final PLDT cell; no fabricated direct transfer to the final target.
  catalog.pipboy.targets[777].cell=0x400;
  gPlayerSession=std::make_unique<Session>(catalog);
  uint32_t package=0;float radius=0;std::array<float,3> anchor{};
  assert(Q240SelectPackage(actor,package,anchor,radius)&&package==50);
  assert(actor.runtime.xtelCell==0x300&&actor.runtime.xtelDoor==0x111);
  // Door lives in a Megaton grid CELL, scene entered through persistent CELL.
  // The existing residency rule permits the authored handoff in this scene.
  actor.aiPackage=50;actor.runtime.xtelDoor=chosen;actor.runtime.xtelCell=0x300;
  actor.runtime.position=Q240ScenePosition(point);actor.aiPathGame={point};actor.aiPathIndex=1;
  Q240AdvanceNpcXtel(actor,.01f,1);
  assert(actor.runtime.offScene);
  assert(gPlayerSession->player.Snapshot().actors.at(42).cell==0x300);
  // A foreign world cannot use that ownership exception.
  actor.runtime.offScene=false;actor.runtime.xtelDoor=0x111;actor.runtime.xtelCell=0x300;
  gExteriorWorldspaceQ1890=0x3c;
  Q240AdvanceNpcXtel(actor,.01f,2);assert(!actor.runtime.offScene);
  activeTestXtels.reset();gCurrentCellFormId=1;gExteriorWorldspaceQ1890=2;
  std::cout<<"Ranked XTEL eligibility: 3->0 A* queries, including unreachable-best; persistent CELL handoff passed\n";
}
// Retained v279 uncached implementation is the choice-equivalence oracle.
static bool Q279ReferenceRemoteDoor(const Q230ActorVisual& actor,uint32_t targetCell,
    uint32_t& doorRef,std::array<float,3>& doorPosition,uint32_t* nextCell=nullptr){
  FQ_PERF_SCOPE(Path);
#ifndef FO3_NPC_XTEL_INDEX
  (void)actor;(void)targetCell;(void)doorRef;(void)doorPosition;(void)nextCell;return false;
#else
  const auto index=FO3_NPC_XTEL_INDEX();
  if(!index||!gPlayerSession||!actor.navigationGraph||!targetCell)return false;
  const auto& d=gPlayerSession->player.Definitions().pipboy;
  std::vector<uint32_t> doors;
  if(gExteriorWorldspaceQ1890==0x00000A74u){
    // The Megaton scene is loaded via its persistent WRLD CELL, but authored
    // XTEL doors actually belong to individual exterior grid CELLs.
    for(const auto& cell:d.cellWorlds)if(cell.second==gExteriorWorldspaceQ1890){
      if(const auto* exits=index->Outgoing(cell.first))
        doors.insert(doors.end(),exits->begin(),exits->end());
    }
  } else if(const auto* exits=index->Outgoing(gCurrentCellFormId)){
    doors=*exits;
  }
  if(doors.empty())return false;
  const auto here=Q240GamePosition(actor);
  struct Candidate {uint32_t ref;size_t legs;float distance;std::array<float,3> point;};
  std::vector<Candidate> candidates;candidates.reserve(doors.size());
  for(uint32_t ref:doors){
    const auto* edge=index->Find(ref);
    if(!edge||edge->sourceWorld!=gExteriorWorldspaceQ1890||
       (edge->sourceRecordFlags&0x820u)||(edge->destinationRecordFlags&0x820u))
      continue;
    const auto placement=d.targets.find(ref);
    if(placement==d.targets.end()||(placement->second.flags&0x820u)||
       placement->second.parent||!gPlayerSession->player.CanActorOpenDoor(actor.source.refFormId,ref))
      continue;
    std::vector<uint32_t> remainder;
    if(!index->CellRoute(edge->destinationCell,targetCell,remainder))continue;
    std::array<float,3> point{placement->second.x,placement->second.y,placement->second.z};
    candidates.push_back({ref,remainder.size()+1,Q240PlanarDistance(here,point),point});
  }
  // Rank using exactly the previous selection policy before doing NAVM work.
  // Stable ties retain the original iteration order. An inaccessible best door
  // still falls through to the next candidate; permission checks remain live.
  std::stable_sort(candidates.begin(),candidates.end(),[](const Candidate& a,const Candidate& b){
    return a.legs<b.legs||(a.legs==b.legs&&a.distance<b.distance);
  });
  if(candidates.empty())return false;
  // Selection only needs existence of an approach, not a discarded shortest
  // path. One directed traversal of the same validated NAVM portals answers
  // reachability for every candidate. Crowd penalties affect route shape, not
  // connectivity; actual movement still uses the unchanged crowd-aware A*.
  const auto& graph=*actor.navigationGraph;
  float startDistance=0;const auto start=Q240NearestNode(graph,here,&startDistance);
  const auto startFlat=Q240Flat(graph,start);
  if(!start.Valid()||startDistance>350.f||startFlat>=graph.triangleCount)return false;
  std::vector<uint8_t> reachable(graph.triangleCount,0);
  std::vector<Q240Node> frontier{start};reachable[startFlat]=1;
  for(size_t at=0;at<frontier.size();++at) {
    const auto current=frontier[at];
    Q240Neighbors(graph,current,[&](const Q240Node& next){
      const auto flat=Q240Flat(graph,next);std::array<float,3> portal{};
      if(flat<reachable.size()&&!reachable[flat]&&Q240Portal(graph,current,next,portal)) {
        reachable[flat]=1;frontier.push_back(next);
      }
    });
  }
  for(const auto& candidate:candidates) {
    float distance=0;const auto goal=Q240NearestNode(graph,candidate.point,&distance);
    const auto flat=Q240Flat(graph,goal);
    if(!goal.Valid()||distance>500.f||flat>=reachable.size()||!reachable[flat])continue;
    const auto endpoint=Q240GroundPoint(*graph.meshes[goal.mesh],goal.triangle,candidate.point);
    if(Q240PlanarDistance(endpoint,candidate.point)>96.f)continue;
    doorRef=candidate.ref;doorPosition=candidate.point;
    if(nextCell)*nextCell=index->Find(candidate.ref)->destinationCell;
    break;
  }
  return doorRef!=0;
#endif
}
static bool Q224ReferenceRemoteDoor(const Q230ActorVisual& actor,uint32_t targetCell,
    uint32_t& doorRef,std::array<float,3>& doorPosition){
#ifndef FO3_NPC_XTEL_INDEX
  (void)actor;(void)targetCell;(void)doorRef;(void)doorPosition;return false;
#else
  const auto index=FO3_NPC_XTEL_INDEX();
  if(!index||!gPlayerSession||!actor.navigationGraph||!targetCell)return false;
  const auto& d=gPlayerSession->player.Definitions().pipboy;
  std::vector<uint32_t> doors;
  if(gExteriorWorldspaceQ1890==0x00000A74u){
    // The Megaton scene is loaded via its persistent WRLD CELL, but authored
    // XTEL doors actually belong to individual exterior grid CELLs.
    for(const auto& cell:d.cellWorlds)if(cell.second==gExteriorWorldspaceQ1890){
      if(const auto* exits=index->Outgoing(cell.first))
        doors.insert(doors.end(),exits->begin(),exits->end());
    }
  } else if(const auto* exits=index->Outgoing(gCurrentCellFormId)){
    doors=*exits;
  }
  if(doors.empty())return false;
  const auto here=Q240GamePosition(actor);
  size_t minLegs=SIZE_MAX;float minDistance=INFINITY;
  for(uint32_t ref:doors){
    const auto* edge=index->Find(ref);
    if(!edge||edge->sourceWorld!=gExteriorWorldspaceQ1890||
       (edge->sourceRecordFlags&0x820u)||(edge->destinationRecordFlags&0x820u))
      continue;
    const auto placement=d.targets.find(ref);
    if(placement==d.targets.end()||(placement->second.flags&0x820u)||
       placement->second.parent||!gPlayerSession->player.CanActorOpenDoor(actor.source.refFormId,ref))
      continue;
    std::vector<uint32_t> remainder;
    if(!index->CellRoute(edge->destinationCell,targetCell,remainder))continue;
    std::array<float,3> point{placement->second.x,placement->second.y,placement->second.z};
    std::vector<std::array<float,3>> route;
    if(!Q240ActorPath(actor,*actor.navigationGraph,here,point,route)||route.empty()||
       Q240PlanarDistance(route.back(),point)>96.f)continue;
    const float distance=Q240PlanarDistance(here,point);
    if(remainder.size()+1<minLegs||(remainder.size()+1==minLegs&&distance<minDistance)){
      minLegs=remainder.size()+1;minDistance=distance;doorRef=ref;doorPosition=point;
    }
  }
  return doorRef!=0;
#endif
}

static void OriginalDoorRanking(const char* esm) {
  fo3player::Catalog catalog;std::string error;assert(fo3player::LoadCatalog(esm,catalog,error));
  fo3xtel::Index index;assert(index.Build(esm,error));
  activeTestXtels=std::make_shared<const fo3xtel::Index>(std::move(index));
  gPlayerSession=std::make_unique<Session>(std::move(catalog));
  std::vector<Fo3NpcActorQ230> actors;assert(LoadFo3CellActors(0xa96,actors,esm));
  std::vector<Fo3NpcNavMeshQ240> meshes;assert(LoadFo3NpcNavigationQ240(0xa96,0xa74,meshes,esm));
  auto graph=std::make_shared<Q240NavigationGraph>();
  for(auto& mesh:meshes){graph->triangleOffsets.push_back(graph->triangleCount);graph->triangleCount+=mesh.triangles.size();graph->byForm[mesh.formId]=graph->meshes.size();graph->meshes.push_back(std::make_shared<Fo3NpcNavMeshQ240>(std::move(mesh)));}
  gCurrentCellFormId=0xa96;gExteriorWorldspaceQ1890=0xa74;
  const auto& d=gPlayerSession->player.Definitions().pipboy;
  std::unordered_set<uint32_t> destinations;
  for(const auto& cell:d.cellWorlds)if(cell.second==0xa74)if(const auto* exits=activeTestXtels->Outgoing(cell.first))
    for(auto ref:*exits)if(const auto* link=activeTestXtels->Find(ref))destinations.insert(link->destinationCell);
  uint32_t beforeQueries=0,afterQueries=0,matched=0,transfers=0;
  double beforeMs=0,afterMs=0;
  for(const auto& source:actors)for(uint32_t target:destinations){
    Q230ActorVisual actor;actor.source=source;actor.navigationGraph=graph;
    actor.runtime.position=Q240ScenePosition({source.x,source.y,source.z});
    uint32_t oldDoor=0,newDoor=0;std::array<float,3> oldPoint{},newPoint{};
    auto count=gQ240PathsPlanned;auto started=std::chrono::steady_clock::now();
    const bool oldResult=Q224ReferenceRemoteDoor(actor,target,oldDoor,oldPoint);
    beforeMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();beforeQueries+=gQ240PathsPlanned-count;
    count=gQ240PathsPlanned;started=std::chrono::steady_clock::now();
    const bool newResult=Q240NpcRemoteDoor(actor,target,newDoor,newPoint);
    afterMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();afterQueries+=gQ240PathsPlanned-count;
    assert(oldResult==newResult&&oldDoor==newDoor&&oldPoint==newPoint);++matched;
    // Original door ownership in the persistent Megaton scene must survive
    // traversal validation, not immediately retry the same original package.
    if(newResult){
      const auto* link=activeTestXtels->Find(newDoor);assert(link);
      actor.aiPackage=0;actor.runtime.xtelDoor=newDoor;actor.runtime.xtelCell=link->destinationCell;
      actor.runtime.position=Q240ScenePosition(newPoint);actor.aiPathGame={newPoint};actor.aiPathIndex=1;
      Q240AdvanceNpcXtel(actor,.01f,1);assert(actor.runtime.offScene);++transfers;
    }
  }
  assert(matched>0&&transfers>0&&afterQueries<beforeQueries);
  std::cout<<"Original NAVM/XTEL identical choices="<<matched<<" handoffs="<<transfers<<" oldQueries="<<beforeQueries<<" newQueries="<<afterQueries<<" oldHostMs="<<beforeMs<<" newHostMs="<<afterMs<<"\n";
}
static void DirectedDoorCacheAndPolicyReuse() {
  auto actor=Actor(6);auto graph=std::make_shared<Q240NavigationGraph>();
  auto mesh=std::make_shared<Fo3NpcNavMeshQ240>();mesh->formId=901;
  mesh->vertices={{0,0,0},{100,0,0},{0,100,0},{100,100,0}};
  Fo3NpcNavTriangleQ240 a,b;a.vertex[0]=0;a.vertex[1]=1;a.vertex[2]=2;
  b.vertex[0]=1;b.vertex[1]=3;b.vertex[2]=2;a.neighbor[1]=1;
  mesh->triangles={a,b};graph->triangleOffsets={0};graph->meshes={mesh};graph->byForm[901]=0;graph->triangleCount=2;
  const auto builds=gQ280DoorGraphBuilds,floods=gQ280DoorFloods,scans=gQ280DoorGoalScans;
  auto& cache=Q280DoorGraph(graph);
  assert(cache.Reaches(*graph,0,1)&&!cache.Reaches(*graph,1,0));
  assert(cache.Reaches(*graph,0,1)&&!cache.Reaches(*graph,1,0));
  assert(gQ280DoorGraphBuilds==builds+1&&gQ280DoorFloods==floods+2);
  assert(cache.Goal(*graph,123,{10,10,0}).flat==0);
  assert(cache.Goal(*graph,123,{10,10,0}).flat==0);
  assert(cache.Goal(*graph,123,{90,90,0}).flat==1); // Moved reference invalidates projection.
  assert(gQ280DoorGoalScans==scans+2);
  // A new graph publication with the same count is a different topology.
  auto replacement=std::make_shared<Q240NavigationGraph>(*graph);
  auto changed=std::make_shared<Fo3NpcNavMeshQ240>(*mesh);changed->triangles[0].neighbor[1]=-1;
  replacement->meshes={changed};assert(!Q280DoorGraph(replacement).Reaches(*replacement,0,1));
  uint32_t id=0;float radius=0;std::array<float,3> anchor{};
  actor.runtime.xtelDoor=88;actor.runtime.xtelCell=99;
  const auto evaluations=gQ280PackageEvaluations;
  assert(Q280EvaluatePackage(actor,1,id,anchor,radius)&&id==50);
  assert(actor.runtime.xtelDoor==88&&actor.runtime.xtelCell==99&&actor.aiLastUpdate==-1);
  // Reading policy then moving adopts the same evaluation, not another selector run.
  Q240UpdateNpcPackage(actor,1);
  assert(gQ280PackageEvaluations==evaluations+1&&actor.aiPackage==50);
  assert(Q280EvaluatePackage(actor,1.1,id,anchor,radius)&&gQ280PackageEvaluations==evaluations+1);
  Q280EvaluatePackage(actor,1.3,id,anchor,radius);assert(gQ280PackageEvaluations==evaluations+2);
  packageHour=13;Q280EvaluatePackage(actor,1.31,id,anchor,radius);
  assert(gQ280PackageEvaluations==evaluations+3);packageHour=12;
  actor.runtime.nextPackageEvaluation=0;Q280EvaluatePackage(actor,1.32,id,anchor,radius);
  assert(gQ280PackageEvaluations==evaluations+4); // Existing retry/interruption invalidation.
  actor.navigationGraph=Graph();Q280EvaluatePackage(actor,1.33,id,anchor,radius);
  assert(gQ280PackageEvaluations==evaluations+5);
  std::cout<<"Shared policy evaluation, directed door reuse, moved targets and graph replacement passed\n";
}
static void OriginalRivetDoorReuse(const char* esm) {
  fo3player::Catalog catalog;std::string error;assert(fo3player::LoadCatalog(esm,catalog,error));
  fo3xtel::Index index;assert(index.Build(esm,error));
  activeTestXtels=std::make_shared<const fo3xtel::Index>(std::move(index));
  gPlayerSession=std::make_unique<Session>(std::move(catalog));
  const auto& d=gPlayerSession->player.Definitions().pipboy;
  const auto& door=d.targets.at(0x1e984);
  // Persistent-cell XTEL ownership does not encode exterior grid (0,0).
  const std::array<int32_t,2> center{int32_t(std::floor(door.x/4096.f)),int32_t(std::floor(door.y/4096.f))};
  std::unordered_set<uint32_t> cells;
  for(const auto& c:d.cellGrids)if(d.cellWorlds.at(c.first)==0x3c&&
      std::abs(c.second[0]-center[0])<=2&&std::abs(c.second[1]-center[1])<=2)cells.insert(c.first);
  std::vector<Fo3NpcNavMeshQ240> meshes;
  assert(LoadFo3NpcNavigationQ240(0x2db4,0x3c,meshes,esm,&cells));
  auto graph=std::make_shared<Q240NavigationGraph>();
  for(auto& mesh:meshes){graph->triangleOffsets.push_back(graph->triangleCount);graph->triangleCount+=mesh.triangles.size();graph->byForm[mesh.formId]=graph->meshes.size();graph->meshes.push_back(std::make_shared<Fo3NpcNavMeshQ240>(std::move(mesh)));}
  gCurrentCellFormId=0x2db4;gExteriorWorldspaceQ1890=0x3c;
  const auto builds=gQ280DoorGraphBuilds,scans=gQ280DoorGoalScans,floods=gQ280DoorFloods,cellBuilds=gQ280CellRouteBuilds;
  double oldMs=0,newMs=0;size_t matched=0,successful=0;
  for(int repeat=0;repeat<4;++repeat)for(uint32_t ref:{0xc0cb4u,0xc0cb3u,0x239d7u,0x239d6u,0x1f1dcu,0x1f1ddu,0x1f1deu,0x1f1dfu,0x1f1e0u}) {
    const auto& placed=d.targets.at(ref);Q230ActorVisual actor;
    actor.source.refFormId=ref;actor.source.baseFormId=placed.base;actor.navigationGraph=graph;
    // Query origins come from the original exterior NAVM near the real door.
    // The ACHRs' authored starts are interior; this is not a saved-headset pose.
    const auto node=Q240NearestNode(*graph,{door.x,door.y,door.z});assert(node.Valid());
    const auto point=Q240Centroid(*graph,node);
    actor.runtime.position=Q240ScenePosition(point);
    uint32_t before=0,after=0;std::array<float,3> a{},b{};
    auto t=std::chrono::steady_clock::now();const bool old=Q279ReferenceRemoteDoor(actor,0x1d6a3,before,a);
    oldMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();
    t=std::chrono::steady_clock::now();const bool now=Q240NpcRemoteDoor(actor,0x1d6a3,after,b);
    newMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();
    assert(old==now&&before==after&&a==b);++matched;successful+=now;
  }
  assert(successful>0&&gQ280DoorGraphBuilds==builds+1&&gQ280DoorGoalScans>scans&&gQ280DoorFloods>floods&&gQ280CellRouteBuilds==cellBuilds+1);
  std::cout<<"Original Rivet City NAVM triangles="<<graph->triangleCount<<" identical choices="<<matched<<" reachable="<<successful
    <<" oldHostMs="<<oldMs<<" newHostMs="<<newMs<<" builds="<<gQ280DoorGraphBuilds-builds<<" goalScans="<<gQ280DoorGoalScans-scans<<" floods="<<gQ280DoorFloods-floods<<" cellRouteBuilds="<<gQ280CellRouteBuilds-cellBuilds<<"\n";
}
int main(int argc,char** argv){ActorCrossCellHandoff();RankedDoorsAndPersistentExterior();DirectedDoorCacheAndPolicyReuse();ReverseCellDistances();if(argc>1){OriginalDoorRanking(argv[1]);OriginalRivetDoorReuse(argv[1]);}}
