#define FO3_NPC_FURNITURE_HOST_TEST
bool furnitureLoading=false;
#define FO3_NPC_FURNITURE_LOADING furnitureLoading
#define main PackageFixtureMain
#include "npc_package_runtime_tests.cpp"
#undef main
#undef Q6H_LOGI
#include "../../app/src/main/cpp/rendering/mesh/fo3-static-nif.cpp"
#include <fstream>
#include <map>
std::string meshRoot;
bool LoadFalloutMeshFile(const std::string& path,std::vector<uint8_t>& bytes,std::string*) {
  std::string p=path;for(auto& c:p){if(c=='\\')c='/';else c=char(std::tolower((unsigned char)c));}
  std::ifstream f(meshRoot+"/"+p,std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(f),{});return !bytes.empty();
}
static void SyntheticFurniture(){
  auto a=Actor(4);auto scene=std::make_shared<fo3furniture::Scene>();
  auto program=std::make_shared<fo3furniture::Program>();program->enter.cycle=program->exit.cycle=2;program->enter.stop=program->exit.stop=1;
  fo3anim::Track t;t.bone="Bip01";t.hasTranslation=true;t.base.translation={0,20,0};
  program->enter.accumulationRoot=program->exit.accumulationRoot=program->loop.accumulationRoot="Bip01";program->enter.tracks={t};program->exit.tracks={t};program->loop.tracks={t};
  fo3furniture::Slot slot;slot.reference=55;slot.sleep=true;slot.program=program;slot.alignment=fo3anim::Identity();slot.alignment[12]=1000;slot.alignment[13]=2000;slot.alignment[14]=20;scene->slots={slot};
  auto c=gPlayerSession->player.Definitions();c.pipboy.packages[50].location.value=55;c.pipboy.targets[55].base=99;c.pipboy.furniture[99].valid=true;gPlayerSession=std::make_unique<Session>(std::move(c));
  a.runtime.furniture.scene=scene;Q240UpdateNpcPackage(a,0);
  for(int i=1;i<100;++i)Q240UpdateNpcPackage(a,i*.1);
  assert(a.runtime.furniture.phase==fo3furniture::Phase::Loop&&scene->reservations->owners.size()==1);
  a.runtime.BeginDialogue();for(int i=100;i<140;++i)Q240UpdateNpcPackage(a,i*.1);
  assert(!a.runtime.furniture.Active()&&scene->reservations->owners.empty());a.runtime.EndDialogue();
  for(int i=140;i<240;++i)Q240UpdateNpcPackage(a,i*.1);
  assert(a.runtime.furniture.phase==fo3furniture::Phase::Loop);a.runtime.BeginCombat(0x14);
  for(int i=240;i<280;++i)Q240UpdateNpcPackage(a,i*.1);
  assert(!a.runtime.furniture.Active()&&scene->reservations->owners.empty());a.runtime.EndCombat();
  for(int i=280;i<380;++i)Q240UpdateNpcPackage(a,i*.1);assert(a.runtime.furniture.Active());a.runtime.Die(38);assert(scene->reservations->owners.empty());
  a.runtime.activity=fo3npc::Activity::Package;
  for(int i=380;i<480;++i)Q240UpdateNpcPackage(a,i*.1);assert(a.runtime.furniture.phase==fo3furniture::Phase::Loop);
  furnitureLoading=true;Q240UpdateNpcPackage(a,48);assert(!a.runtime.furniture.Active()&&scene->reservations->owners.empty());furnitureLoading=false;
  for(int i=480;i<580;++i)Q240UpdateNpcPackage(a,i*.1);assert(a.runtime.furniture.phase==fo3furniture::Phase::Loop);
  auto changed=gPlayerSession->player.Definitions();changed.pipboy.packages[50].type=6;gPlayerSession=std::make_unique<Session>(std::move(changed));a.runtime.nextPackageEvaluation=0;
  for(int i=580;i<680;++i)Q240UpdateNpcPackage(a,i*.1);assert(!a.runtime.furniture.Active()&&scene->reservations->owners.empty());
  std::cout<<"Furniture ownership, dialogue/combat exits and death cleanup passed\n";
}
static std::shared_ptr<Q240NavigationGraph> Navigation(uint32_t cell,uint32_t world,const char* esm){
 std::vector<Fo3NpcNavMeshQ240> raw;assert(LoadFo3NpcNavigationQ240(cell,world,raw,esm));auto graph=std::make_shared<Q240NavigationGraph>();
 for(auto& m:raw){auto mesh=std::make_shared<Fo3NpcNavMeshQ240>(std::move(m));graph->triangleOffsets.push_back(graph->triangleCount);graph->triangleCount+=mesh->triangles.size();graph->byForm[mesh->formId]=graph->meshes.size();graph->meshes.push_back(mesh);}return graph;
}
static void OriginalFurniture(const char* esm,bool eat=false){
 fo3player::Catalog c;std::string error;assert(fo3player::LoadCatalog(esm,c,error));gPlayerSession=std::make_unique<Session>(std::move(c));
 const auto& d=gPlayerSession->player.Definitions().pipboy;
 for(uint32_t cell:{0x3a2au,0x3a33u}){
  gCurrentCellFormId=cell;gExteriorWorldspaceQ1890=0;
  auto scene=fo3furniture::BuildScene(d,cell,0,[](const auto& p,auto& bytes){return LoadFalloutMeshFile(p,bytes,nullptr);},eat);
  std::cout<<"Cell "<<std::hex<<cell<<std::dec<<" verified furniture slots="<<scene->slots.size()<<'\n';assert(!scene->slots.empty());
  auto graph=Navigation(cell,0,esm);std::vector<Fo3NpcActorQ230> residents;assert(LoadFo3CellActors(cell,residents,esm));
  bool exercised=false;
  for(const auto& source:residents){const auto* def=fo3pipdata::ActorCategory(d,source.baseFormId,16);if(!def)continue;
   for(uint32_t id:def->packages){const auto& p=d.packages.at(id);if(p.type!=(eat?3:4)||p.location.type!=0)continue;
    Q230ActorVisual a;a.source=source;a.runtime.reference=source.refFormId;a.navigationGraph=graph;a.runtime.position=Q240ScenePosition({source.x,source.y,source.z});a.runtime.furniture.scene=scene;
    const size_t target=Q240FurnitureTarget(a,p);if(target==SIZE_MAX)continue;
    const auto* original= &scene->slots[target];assert(original->reference==p.location.value);
    packageHour=p.schedule.hour+.5f;uint32_t selected=0;std::array<float,3> anchor{};float radius=0;
    assert(Q240SelectPackage(a,selected,anchor,radius)&&selected==id);
    Q240AdoptPackage(a,id,p,{original->alignment[12],original->alignment[13],original->alignment[14]});
    double now=0;for(int i=0;i<1500&&a.runtime.furniture.phase!=fo3furniture::Phase::Loop;++i){now+=.1;Q240AdvanceFurniture(a,&p,id,.1,now);}
    assert(a.runtime.furniture.phase==fo3furniture::Phase::Loop);
    const auto root=a.runtime.position;const auto yaw=a.runtime.yaw;
    for(int cycle=0;cycle<3;++cycle){
      for(int i=0;i<100;++i){now+=.1;Q240AdvanceFurniture(a,&p,id,.1,now);}assert(a.runtime.position==root);
      a.runtime.furniture.RequestExit();for(int i=0;i<100&&a.runtime.furniture.Active();++i){now+=.1;Q240AdvanceFurniture(a,nullptr,0,.1,now);}
      assert(!a.runtime.furniture.Active()&&scene->reservations->owners.empty());
      for(int i=0;i<1000&&a.runtime.furniture.phase!=fo3furniture::Phase::Loop;++i){now+=.1;Q240AdvanceFurniture(a,&p,id,.1,now);}assert(a.runtime.furniture.phase==fo3furniture::Phase::Loop&&a.runtime.position==root&&a.runtime.yaw==yaw);
    }
    a.runtime.furniture.Clear();assert(scene->reservations->owners.empty());exercised=true;
    std::cout<<source.fullName<<" original package "<<std::hex<<id<<" furniture "<<original->reference<<std::dec<<(eat?" three seated Eat":" three Sleep")<<" cycles without root drift passed\n";break;
   }if(exercised)break;
  }assert(exercised);
 }
}
static void Coverage(const char* esm){
  const auto& d=gPlayerSession->player.Definitions().pipboy;size_t assigned[2]{},completed[2]{};std::map<std::string,size_t> blockers;
  for(uint32_t cell:{0xa96u,0x3a29u,0x3a2au,0x3a2cu,0x3a2du,0x3a2eu,0x3a2fu,0x3a31u,0x3a32u,0x3a33u,0x3a34u,0x3a35u,0x4357u}){
    const uint32_t world=cell==0xa96?0xa74:0;gCurrentCellFormId=cell;gExteriorWorldspaceQ1890=world;
    auto scene=fo3furniture::BuildScene(d,cell,world,[](const auto& p,auto& bytes){return LoadFalloutMeshFile(p,bytes,nullptr);},true);
    std::vector<Fo3NpcActorQ230> actors;assert(LoadFo3CellActors(cell,actors,esm));auto nav=Navigation(cell,world,esm);
    for(const auto& source:actors){const auto* def=fo3pipdata::ActorCategory(d,source.baseFormId,16);if(!def)continue;
      for(auto id:def->packages){const auto& p=d.packages.at(id);if(p.type!=3&&p.type!=4)continue;const int kind=p.type==4?0:1;++assigned[kind];
        Q230ActorVisual a;a.source=source;a.runtime.reference=source.refFormId;a.navigationGraph=nav;a.runtime.position=Q240ScenePosition({source.x,source.y,source.z});a.runtime.furniture.scene=scene;
        const float hour=p.schedule.hour<0?12:std::fmod(p.schedule.hour+.5f,24.f);
        if(!Q240ScheduleActive(p.schedule,hour)||p.scripted){++blockers["schedule/script"];continue;}
        fo3dialogue::Context ctx;ctx.player=&gPlayerSession->player;ctx.speaker={source.refFormId,source.baseFormId,cell,world};ctx.target={0x14,7,cell,world};std::string error;
        const bool conditions=fo3dialogue::Conditions(p.conditions,ctx,error);if(!error.empty()||!conditions){++blockers["conditions unverified/false"];std::cout<<"Unverified "<<source.fullName<<" "<<std::hex<<id<<std::dec<<" condition="<<(error.empty()?"false in tested state":error)<<'\n';continue;}
        if(Q240FurnitureTarget(a,p)==SIZE_MAX){++blockers["no resident supported marker/ownership/child"];std::cout<<"Unverified "<<source.fullName<<" "<<std::hex<<id<<" location="<<p.location.type<<":"<<p.location.value<<std::dec<<" no available verified marker\n";continue;}
        a.aiPackage=id;a.runtime.package=id;a.runtime.packageKnown=true;double now=0;
        for(int i=0;i<1200&&a.runtime.furniture.phase!=fo3furniture::Phase::Loop;++i){now+=.1;if(a.runtime.packageRetryAfter.count(id))break;Q240AdvanceFurniture(a,&p,id,.1,now);}
        if(a.runtime.furniture.phase!=fo3furniture::Phase::Loop){++blockers["NAVM approach/timeout"];a.runtime.furniture.Clear();continue;}
        a.runtime.furniture.RequestExit();for(int i=0;i<200&&a.runtime.furniture.Active();++i){now+=.1;Q240AdvanceFurniture(a,nullptr,0,.1,now);}assert(!a.runtime.furniture.Active());++completed[kind];
      }
    }
    assert(scene->reservations->owners.empty());
    for(float hour:{12.f,20.f,1.f}){packageHour=hour;std::vector<Q230ActorVisual> population;
      for(const auto& source:actors){Q230ActorVisual a;a.source=source;a.runtime.reference=source.refFormId;a.runtime.position=Q240ScenePosition({source.x,source.y,source.z});a.navigationGraph=nav;a.runtime.furniture.scene=scene;population.push_back(std::move(a));}
      activePackageTargets=&population;for(int i=0;i<800;++i)for(auto& a:population)Q240UpdateNpcPackage(a,i*.1);
      size_t seated=0;for(const auto& a:population)seated+=a.runtime.furniture.phase==fo3furniture::Phase::Loop;
      std::cout<<"Schedule simulation cell="<<std::hex<<cell<<std::dec<<" hour="<<hour<<" population="<<population.size()<<" occupied="<<seated<<'\n';
    }
  }
  std::cout<<"Original-data lifecycle coverage Sleep="<<completed[0]<<"/"<<assigned[0]<<" Eat seating="<<completed[1]<<"/"<<assigned[1]<<'\n';
  for(const auto& b:blockers)std::cout<<"Coverage blocker "<<b.first<<"="<<b.second<<'\n';
  assert(assigned[0]==36&&assigned[1]==33);activePackageTargets=&packageTargets;
}
static void SandboxCoverage(const char* esm){
 const auto& d=gPlayerSession->player.Definitions().pipboy;size_t assigned=0,activities=0,wander=0,blocked=0;
 for(uint32_t cell:{0xa96u,0x3a29u,0x3a2au,0x3a2cu,0x3a2du,0x3a2eu,0x3a2fu,0x3a31u,0x3a32u,0x3a33u,0x3a34u,0x3a35u,0x4357u}){
  const uint32_t world=cell==0xa96?0xa74:0;gCurrentCellFormId=cell;gExteriorWorldspaceQ1890=world;auto nav=Navigation(cell,world,esm);
  auto scene=fo3furniture::BuildScene(d,cell,world,[](const auto& p,auto& bytes){return LoadFalloutMeshFile(p,bytes,nullptr);},true);
  std::vector<Fo3NpcActorQ230> actors;assert(LoadFo3CellActors(cell,actors,esm));
  for(const auto& source:actors){const auto* def=fo3pipdata::ActorCategory(d,source.baseFormId,16);if(!def)continue;
   for(auto id:def->packages){const auto& p=d.packages.at(id);if(p.type!=12)continue;++assigned;
    Q230ActorVisual a;a.source=source;a.runtime.reference=source.refFormId;a.navigationGraph=nav;a.runtime.position=Q240ScenePosition({source.x,source.y,source.z});a.runtime.furniture.scene=scene;a.aiPackage=id;
    const float hour=p.schedule.hour<0?12:std::fmod(p.schedule.hour+.5f,24.f);packageHour=hour;
    fo3dialogue::Context ctx;ctx.player=&gPlayerSession->player;ctx.speaker={source.refFormId,source.baseFormId,cell,world};ctx.target={0x14,7,cell,world};std::string error;
    bool supported=Q240ScheduleActive(p.schedule,hour)&&!p.scripted&&fo3dialogue::Conditions(p.conditions,ctx,error)&&error.empty();
    std::array<float,3> anchor{};float radius=float(std::max(0,p.location.radius));
    if(p.location.type==0){auto t=d.targets.find(p.location.value);if(t==d.targets.end()||t->second.world!=world||(!world&&t->second.cell!=cell))supported=false;else anchor={t->second.x,t->second.y,t->second.z};}
    else if(p.location.type==3)anchor={source.x,source.y,source.z};
    else if(p.location.type==1&&p.location.value==cell){anchor=Q240GamePosition(a);radius=std::numeric_limits<float>::max();}else supported=false;
    if(!supported){++blocked;continue;}
    a.runtime.evaluatedAnchor=anchor;a.runtime.evaluatedRadius=radius;
    const auto target=Q240SandboxTarget(a,p,anchor,radius);
    if(target!=SIZE_MAX){
      double now=0;for(int j=0;j<1400&&!a.runtime.packageRetryAfter.count(id)&&a.runtime.furniture.phase!=fo3furniture::Phase::Loop;++j){now+=.1;Q240AdvanceFurniture(a,&p,id,.1,now);}
      if(a.runtime.furniture.phase==fo3furniture::Phase::Loop){++activities;const auto* slot=a.runtime.furniture.Selected();std::cout<<"Sandbox activity "<<source.fullName<<" package="<<std::hex<<id<<" target="<<slot->reference<<std::dec<<" marker="<<slot->idleMarker<<'\n';
        for(int j=0;j<1000&&a.runtime.furniture.Active();++j){now+=.1;Q240AdvanceFurniture(a,&p,id,.1,now);}assert(!a.runtime.furniture.Active());continue;}
      a.runtime.furniture.Clear();
    }
    if(!(p.typeFlags&32u)&&Q240BuildPackagePath(a,p,anchor,radius)){++wander;}else ++blocked;
   }
  }
  assert(scene->reservations->owners.empty());
 }
 std::cout<<"Sandbox coverage assignments="<<assigned<<" activities="<<activities<<" wander-only="<<wander<<" blocked="<<blocked<<'\n';assert(assigned==90);
}
static void SandboxActivities(){
 SyntheticFurniture();auto a=Actor(12);auto scene=std::make_shared<fo3furniture::Scene>();
 auto program=std::make_shared<fo3furniture::Program>();program->enter.stop=program->exit.stop=1;program->enter.frequency=program->exit.frequency=1;
 fo3anim::Track t;t.bone="Bip01";program->enter.tracks=program->loop.tracks=program->exit.tracks={t};
 fo3furniture::Slot slot;slot.reference=55;slot.program=program;slot.alignment=fo3anim::Identity();slot.alignment[12]=1000;slot.alignment[13]=2000;slot.alignment[14]=20;slot.placement.x=1000;slot.placement.y=2000;slot.placement.z=20;
 scene->slots={slot};a.runtime.furniture.scene=scene;
 auto c=gPlayerSession->player.Definitions();c.pipboy.packages[50].typeFlags=32;gPlayerSession=std::make_unique<Session>(std::move(c));
 for(int i=0;i<1600;++i)Q240UpdateNpcPackage(a,i*.1);
 assert(a.runtime.sandboxCycles>=4);assert(a.aiPackage==50);assert(a.runtime.sandboxLastTarget==55);
 a.runtime.BeginDialogue();for(int i=1600;i<1640;++i)Q240UpdateNpcPackage(a,i*.1);assert(scene->reservations->owners.empty());a.runtime.EndDialogue();
 for(int i=1640;i<1800;++i)Q240UpdateNpcPackage(a,i*.1);assert(a.runtime.furniture.Active());
 a.runtime.BeginCombat(0x14);for(int i=1800;i<1840;++i)Q240UpdateNpcPackage(a,i*.1);assert(scene->reservations->owners.empty());a.runtime.EndCombat();
 auto restricted=gPlayerSession->player.Definitions();restricted.pipboy.packages[50].typeFlags=48;gPlayerSession=std::make_unique<Session>(std::move(restricted));a.runtime.nextPackageEvaluation=0;
 for(int i=1840;i<1900;++i)Q240UpdateNpcPackage(a,i*.1);assert(!a.runtime.furniture.Active());assert(a.aiPathGame.empty());
 // Marker shares the same lease and approach/orientation owner, with original clip duration.
 auto markerCatalog=gPlayerSession->player.Definitions();markerCatalog.pipboy.packages[50].typeFlags=48;gPlayerSession=std::make_unique<Session>(std::move(markerCatalog));
 fo3furniture::Slot marker=slot;marker.reference=56;marker.idleMarker=true;marker.program.reset();marker.idleTimer=2;
 fo3furniture::MarkerIdle idle;idle.form=70;idle.clip.stop=1;idle.clip.frequency=1;idle.clip.cycle=2;idle.clip.tracks={t};marker.idles={idle};scene->slots.push_back(marker);
 a.runtime.nextPackageEvaluation=0;for(int i=1900;i<2100;++i)Q240UpdateNpcPackage(a,i*.1);assert(a.runtime.sandboxLastTarget==56);
 a.runtime.Die(210);assert(scene->reservations->owners.empty());
 std::cout<<"Sandbox repeated chair activity, authored No Wandering/No Furniture, dialogue/combat release passed\n";
}
int main(int argc,char** argv){SandboxActivities();SyntheticFurniture();if(argc>2){meshRoot=argv[2];OriginalFurniture(argv[1]);OriginalFurniture(argv[1],true);Coverage(argv[1]);SandboxCoverage(argv[1]);}std::cout<<"Furniture package runtime tests passed\n";}
