#define FO3_NPC_FURNITURE_HOST_TEST
#define main PackageFixtureMain
#include "npc_package_runtime_tests.cpp"
#undef main
#undef Q6H_LOGI
#include "../../app/src/main/cpp/rendering/mesh/fo3-static-nif.cpp"
#include <fstream>
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
  auto changed=gPlayerSession->player.Definitions();changed.pipboy.packages[50].type=6;gPlayerSession=std::make_unique<Session>(std::move(changed));a.runtime.nextPackageEvaluation=0;
  for(int i=480;i<580;++i)Q240UpdateNpcPackage(a,i*.1);assert(!a.runtime.furniture.Active()&&scene->reservations->owners.empty());
  std::cout<<"Furniture ownership, dialogue/combat exits and death cleanup passed\n";
}
static std::shared_ptr<Q240NavigationGraph> Navigation(uint32_t cell,uint32_t world,const char* esm){
 std::vector<Fo3NpcNavMeshQ240> raw;assert(LoadFo3NpcNavigationQ240(cell,world,raw,esm));auto graph=std::make_shared<Q240NavigationGraph>();
 for(auto& m:raw){auto mesh=std::make_shared<Fo3NpcNavMeshQ240>(std::move(m));graph->triangleOffsets.push_back(graph->triangleCount);graph->triangleCount+=mesh->triangles.size();graph->byForm[mesh->formId]=graph->meshes.size();graph->meshes.push_back(mesh);}return graph;
}
static void OriginalFurniture(const char* esm){
 fo3player::Catalog c;std::string error;assert(fo3player::LoadCatalog(esm,c,error));gPlayerSession=std::make_unique<Session>(std::move(c));
 const auto& d=gPlayerSession->player.Definitions().pipboy;
 for(uint32_t cell:{0x3a2au,0x3a33u}){
  gCurrentCellFormId=cell;gExteriorWorldspaceQ1890=0;
  auto scene=fo3furniture::BuildScene(d,cell,0,[](const auto& p,auto& bytes){return LoadFalloutMeshFile(p,bytes,nullptr);});
  std::cout<<"Cell "<<std::hex<<cell<<std::dec<<" verified bed slots="<<scene->slots.size()<<'\n';assert(!scene->slots.empty());
  auto graph=Navigation(cell,0,esm);std::vector<Fo3NpcActorQ230> residents;assert(LoadFo3CellActors(cell,residents,esm));
  bool exercised=false;
  for(const auto& source:residents){const auto* def=fo3pipdata::ActorCategory(d,source.baseFormId,16);if(!def)continue;
   for(uint32_t id:def->packages){const auto& p=d.packages.at(id);if(p.type!=4||p.location.type!=0)continue;
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
    std::cout<<source.fullName<<" original package "<<std::hex<<id<<" bed "<<original->reference<<std::dec<<" three sleep cycles without root drift passed\n";break;
   }if(exercised)break;
  }assert(exercised);
 }
}
int main(int argc,char** argv){SyntheticFurniture();if(argc>2){meshRoot=argv[2];OriginalFurniture(argv[1]);}std::cout<<"Furniture package runtime tests passed\n";}
