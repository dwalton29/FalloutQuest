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
static void OriginalVaultArrivals(const char* path){
  fo3player::Catalog catalog;std::string error;assert(fo3player::LoadCatalog(path,catalog,error));
  const auto& defs=catalog.pipboy;
  const auto form=[&](const char* name){return defs.formNames.at(name);};
  const auto quest=form("cg02"),dad=form("cg02dadref"),overseer=form("cg02overseerref"),jonas=form("cg02jonasref");
  gCurrentCellFormId=defs.targets.at(dad).cell;gExteriorWorldspaceQ1890=0;
  std::vector<Fo3NpcNavMeshQ240> meshes;assert(LoadFo3NpcNavigationQ240(gCurrentCellFormId,0,meshes,path));
  auto graph=std::make_shared<Q240NavigationGraph>();
  for(auto& mesh:meshes){
    graph->triangleOffsets.push_back(graph->triangleCount);graph->triangleCount+=mesh.triangles.size();
    graph->byForm[mesh.formId]=graph->meshes.size();graph->meshes.push_back(std::make_shared<Fo3NpcNavMeshQ240>(std::move(mesh)));
  }
  auto place=[&](uint32_t ref,uint32_t start){
    const auto& placement=defs.targets.at(start);
    Q230ActorVisual actor;actor.source.refFormId=ref;actor.source.baseFormId=defs.targets.at(ref).base;
    actor.source.x=placement.x;actor.source.y=placement.y;actor.source.z=placement.z;actor.navigationGraph=graph;
    actor.runtime.position=Q240ScenePosition({placement.x,placement.y,placement.z});
    actor.runtime.yaw=-placement.rz;actor.stateRestored=true;return actor;
  };
  auto start=[&](uint16_t stage){
    catalog.initial.pipboy.quests[quest].stage=stage;catalog.initial.pipboy.quests[quest].running=true;
    catalog.initial.referenceEnabled[dad]=true;
    gPlayerSession=std::make_unique<Session>(catalog);gPlayerSession->player.RegisterOriginalActors();
  };
  auto reached=[&](const Q230ActorVisual& actor,uint32_t package){
    const auto& marker=defs.targets.at(defs.packages.at(package).location.value);
    return actor.aiPackage==package&&actor.runtime.procedure==fo3npc::Procedure::Completed&&
      Q240PlanarDistance(Q240GamePosition(actor),{marker.x,marker.y,marker.z})<8&&
      std::fabs(fo3npc::Angle(actor.runtime.yaw+marker.rz))<.02f;
  };
  start(6);
  const auto walk=form("cg02overseerwalkup"),speech=form("cg02overseergivespeech");
  packageTargets={place(overseer,overseer),place(dad,dad)};
  for(unsigned tick=0;tick<2400&&!reached(packageTargets[0],walk);++tick)Q240UpdateNpcPackage(packageTargets[0],tick*.05);
  assert(reached(packageTargets[0],walk));
  const auto& playerStart=defs.targets.at(form("cg02playerstartmarker"));
  start(7);
  gPlayerSession->player.SetScriptPlayerLocation(playerStart.x,playerStart.y,playerStart.z,playerStart.cell,0);
  packageTargets[0].aiLastUpdate=-1;
  for(unsigned tick=0;tick<2400&&!reached(packageTargets[0],speech);++tick)Q240UpdateNpcPackage(packageTargets[0],tick*.05);
  assert(reached(packageTargets[0],speech));
  start(80);
  const auto dadPhoto=form("cg02dadwaitforpicture"),jonasPhoto=form("cg02jonaswaitforpicture");
  const auto jonasStart=defs.packages.at(form("cg02jonaswaitinsidetargetrange")).location.value;
  packageTargets={place(dad,form("cg02dadtargetrangeinsidemarker")),place(jonas,jonasStart)};
  for(unsigned tick=0;tick<2400&&!(reached(packageTargets[0],dadPhoto)&&reached(packageTargets[1],jonasPhoto));++tick)
    for(auto& actor:packageTargets)Q240UpdateNpcPackage(actor,tick*.05);
  assert(reached(packageTargets[0],dadPhoto)&&reached(packageTargets[1],jonasPhoto));
  const auto& variables=gPlayerSession->player.Snapshot().pipboy.dialogueVariables;
  assert(variables.at(fo3dialogue::VariableKey(defs,"CG02.DadReady"))==1&&
    variables.at(fo3dialogue::VariableKey(defs,"CG02.JonasReady"))==1);
  std::cout<<"Original Vault NAVM routes reach Overseer and both photo markers and deliver readiness callbacks\n";
}
int main(int argc,char** argv){
  {
    auto mover=Actor(6);mover.stateRestored=true;mover.runtime.position=Q240ScenePosition({0,0,20});
    auto neighbor=mover;neighbor.source.refFormId=43;neighbor.runtime.position=Q240ScenePosition({50,0,20});
    std::vector<Q230ActorVisual> crowd{mover,neighbor};
    assert(Q240CrowdStepAllowed(mover,{1,0,20},crowd)); // Close staging is not physical overlap.
    crowd[1].runtime.position=Q240ScenePosition({30,0,20});
    assert(!Q240CrowdStepAllowed(mover,{1,0,20},crowd));
  }

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
  if(argc>1)OriginalVaultArrivals(argv[1]);
  std::cout<<"Production crowd stall recovery and cross-batch marker ownership passed\n";
}
