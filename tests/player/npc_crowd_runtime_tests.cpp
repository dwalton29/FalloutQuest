#define FO3_NPC_CROWD_HOST_TEST
#define main PackageFixtureMain
#include "npc_package_runtime_tests.cpp"
#undef main
int main(){
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
