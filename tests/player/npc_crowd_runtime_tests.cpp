#define FO3_NPC_CROWD_HOST_TEST
#define main PackageFixtureMain
#include "npc_package_runtime_tests.cpp"
#undef main
static void CongestionRoutes(){
  auto actor=Actor(12);actor.stateRestored=true;
  Q240NavigationGraph graph;auto mesh=std::make_shared<Fo3NpcNavMeshQ240>();mesh->formId=100;
  mesh->vertices={{0,0,20},{-400,-400,20},{400,-400,20},{400,400,20},{-400,400,20}};
  for(int i=0;i<4;++i){Fo3NpcNavTriangleQ240 t;t.vertex[0]=0;t.vertex[1]=uint16_t(i+1);t.vertex[2]=uint16_t((i+1)%4+1);
    t.neighbor[0]=(i+3)%4;t.neighbor[1]=-1;t.neighbor[2]=(i+1)%4;mesh->triangles.push_back(t);}
  graph.meshes={mesh};graph.byForm[100]=0;graph.triangleOffsets={0};graph.triangleCount=4;
  actor.runtime.position=Q240ScenePosition({0,-300,20});std::vector<std::array<float,3>> path;
  packageTargets={actor};assert(Q240ActorPath(actor,graph,{0,-300,20},{0,300,20},path)&&path.front()[0]>0);
  auto blocker=actor;blocker.source.refFormId=43;blocker.runtime.position=Q240ScenePosition({200,-200,20});
  packageTargets.push_back(blocker);
  assert(Q240ActorPath(actor,graph,{0,-300,20},{0,300,20},path)&&path.front()[0]<0&&path.back()==(std::array<float,3>{0,300,20}));
  packageTargets[1].runtime.offScene=true;
  assert(Q240ActorPath(actor,graph,{0,-300,20},{0,300,20},path)&&path.front()[0]>0);
  packageTargets[1].runtime.offScene=false;packageTargets[1].runtime.position=Q240ScenePosition({200,-200,220});
  assert(Q240ActorPath(actor,graph,{0,-300,20},{0,300,20},path)&&path.front()[0]>0); // Stacked walkways.
  packageTargets[1].runtime.position=Q240ScenePosition({-1000,-1000,20});
  packageTargets[1].aiPathGame={{200,-200,20}};packageTargets[1].aiPathIndex=0;
  assert(Q240ActorPath(actor,graph,{0,-300,20},{0,300,20},path)&&path.front()[0]<0); // Planned traffic.
  packageTargets[1].runtime.activity=fo3npc::Activity::Dead;
  assert(Q240ActorPath(actor,graph,{0,-300,20},{0,300,20},path)&&path.front()[0]>0);
  packageTargets[1].runtime.activity=fo3npc::Activity::Combat;packageTargets[1].runtime.position=Q240ScenePosition({1500,2500,20});
  packageTargets[1].aiPathGame.clear();
  assert(Q240ActorPath(actor,*Graph(),{1010,2010,20},{1900,2900,20},path)); // Sole passage remains usable.
}
int main(){
  CongestionRoutes();
  auto actor=Actor(12);actor.stateRestored=true;actor.runtime.activity=fo3npc::Activity::Package;
  auto catalog=gPlayerSession->player.Definitions();catalog.pipboy.packages[50].type=12;
  gPlayerSession=std::make_unique<Session>(catalog);actor.aiPackage=50;
  actor.runtime.position=Q240ScenePosition({1500,2300,20});
  auto follower=actor;follower.source.refFormId=43;follower.runtime.position=Q240ScenePosition({1350,2300,20});
  follower.runtime.yaw=-1.5707963f;follower.aiPathGame={{1500,2300,20}};follower.aiPathSurfaces={{0,0}};
  packageTargets={actor,follower};activePackageTargets=&packageTargets;
  for(int i=0;i<100&&!packageTargets[1].aiPathGame.empty();++i)Q240AdvancePath(packageTargets[1],.1f,i*.1);
  assert(packageTargets[1].aiPathGame.empty()&&packageTargets[1].runtime.hasLastRoamDestination);
  assert(packageTargets[1].runtime.procedure==fo3npc::Procedure::Waiting);
  auto initial=std::make_shared<fo3furniture::Scene>(),batch=std::make_shared<fo3furniture::Scene>();
  fo3furniture::State resident,arrival;resident.scene=initial;arrival.scene=batch;
  fo3furniture::JoinResidentActivities(arrival,resident);
  fo3furniture::Lease first,second;
  assert(first.Acquire(resident.Pool(),55,0,42));
  assert(!second.Acquire(arrival.Pool(),55,0,43));
  first.Release();assert(second.Acquire(arrival.Pool(),55,0,43));
  auto later=std::make_shared<fo3furniture::Scene>();fo3furniture::State third;third.scene=later;
  fo3furniture::JoinResidentActivities(third,arrival);
  fo3furniture::Lease another;assert(!another.Acquire(third.Pool(),55,0,44));
  std::cout<<"Production crowd stall recovery and cross-batch marker ownership passed\n";
}
