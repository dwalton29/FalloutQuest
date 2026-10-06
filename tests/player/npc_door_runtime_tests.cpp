#define FO3_NPC_DOOR_HOST_TEST
#define main PackageFixtureMain
#include "npc_package_runtime_tests.cpp"
#undef main
#include "world/fo3-cell-traversal.h"
#include "world/interaction/fo3-interaction-ray.h"
struct Q2400InteriorDoorState {bool targetOpen=false,q2401NifAnimation=true,q2401Moving=false;float progress=0;uint64_t collisionSceneSerial=0;};
std::unordered_map<uint32_t,Q2400InteriorDoorState> gQ2400InteriorDoors;
uint64_t gQ2400DoorSceneSerial=1;
struct DoorObject {bool q2400SwingDoor=true;Fo3DoorTeleport teleport;uint32_t refFormId=20;float minX=.14f,maxX=.16f,minY=-1,maxY=2,minZ=-4,maxZ=0;};
std::vector<DoorObject> gObjects;
bool stepBlocked=true,wallBlocked=false,doorAvailable=true,loadDoor=false;int toggles=0,queries=0,openSounds=0;
void Q230UpdateActor(Q230ActorVisual&){}
bool Q230LiveBone(const Q230ActorVisual& actor,int,std::array<float,3>& out){out=actor.runtime.position;out[1]+=1;return true;}
bool HasFo3InteractionOccluder(float,float,float,float,float,float,float,uint32_t ignored){return ignored==20?wallBlocked:stepBlocked;}
static bool Q230QueryPathDoor(float,float,float,float,float,float,Fo3DoorAimQ1700* aim){++queries;aim->valid=doorAvailable;aim->sourceDoorRef=20;aim->localSwing=!loadDoor;aim->distance=.04f;return doorAvailable;}
bool Q2400ToggleInteriorDoor(uint32_t id,float,float){++toggles;auto& d=gQ2400InteriorDoors[id];d.targetOpen=!d.targetOpen;d.q2401Moving=true;d.collisionSceneSerial=gQ2400DoorSceneSerial;return true;}
namespace fo3audio {void Open(uint32_t){++openSounds;}}
#include "npc/fo3-npc-door-runtime.inc"
static Q230ActorVisual Prepare(bool locked=false,bool npcKey=false){
  auto actor=Actor(6);auto c=gPlayerSession->player.Definitions();c.references[42].base=43;c.references[20].base=200;c.references[20].locked=locked;c.references[20].key=300;
  fo3player::Item key;key.formId=300;key.kind=fo3player::ItemKind::Key;c.items[300]=key;
  c.actorInventories[43].entries.clear();if(npcKey)c.actorInventories[43].entries.push_back({300,0,1,1,1,false});
  gPlayerSession=std::make_unique<Session>(std::move(c));assert(gPlayerSession->player.PrepareActorInventory(42));
  gQ2400InteriorDoors.clear();++gQ2400DoorSceneSerial;gObjects={DoorObject{}};stepBlocked=true;wallBlocked=false;doorAvailable=true;loadDoor=false;toggles=queries=openSounds=0;
  actor.aiPathGame={{1015,2010,20}};actor.aiPathSurfaces={{0,0}};actor.aiPathIndex=0;
  return actor;
}
static void OriginalDoors(const char* path){
  fo3player::Catalog catalog;std::string error;assert(fo3player::LoadCatalog(path,catalog,error));gPlayerSession=std::make_unique<Session>(std::move(catalog));
  auto& player=gPlayerSession->player;const auto& c=player.Definitions();size_t checked=0;
  for(const auto& actor:c.references){
    const auto inventory=c.actorInventories.find(actor.second.base);if(inventory==c.actorInventories.end()||player.ActorHealth(actor.first)<=0)continue;
    for(const auto& item:inventory->second.entries){const auto definition=c.items.find(item.form);if(definition==c.items.end()||definition->second.kind!=fo3player::ItemKind::Key)continue;
      for(const auto& door:c.references){if(!c.pipboy.doorBases.count(door.second.base)||!door.second.locked||door.second.key!=item.form||c.scriptedBases.count(door.second.base))continue;
        if(player.PrepareActorInventory(actor.first)&&player.CanActorOpenDoor(actor.first,door.first)){std::cout<<"Original actor key access actor="<<std::hex<<actor.first<<" door="<<door.first<<" key="<<item.form<<std::dec<<'\n';++checked;break;}
      }
      if(checked>=3)break;
    }
    if(checked>=3)break;
  }
  assert(checked>0);
}
int main(int argc,char** argv){
  auto a=Prepare();auto before=a.runtime.position;Q240AdvancePath(a,.1f,1);
  assert(a.runtime.position==before&&toggles==1&&openSounds==1&&a.runtime.navigationDoor==20);
  stepBlocked=false;Q240AdvancePath(a,.1f,2);assert(a.runtime.position==before&&toggles==1&&queries==1); // Collision may disappear mid-animation; NPC still waits.
  gQ2400InteriorDoors[20].q2401Moving=false;Q240AdvancePath(a,.1f,3);
  assert(a.aiPathIndex==1&&a.runtime.navigationDoor==0&&toggles==1); // Short waypoint cannot bypass closed door.
  a=Prepare();wallBlocked=true;a.aiLastUpdate=1;assert(Q230PathBlocked(a,1,0,0,.1f,1)&&toggles==0);
  a.aiLastUpdate=1.1;assert(Q230PathBlocked(a,1,0,0,.1f,1)&&queries==1);
  a.aiLastUpdate=1.6;assert(Q230PathBlocked(a,1,0,0,.1f,1)&&queries==2); // Wall before door.
  a=Prepare();loadDoor=true;assert(Q230PathBlocked(a,1,0,0,.1f,1)&&toggles==0&&a.runtime.navigationDoor==0); // No player transition API exists in this fixture.
  a=Prepare(true);assert(gPlayerSession->player.Add(300,1));assert(gPlayerSession->player.CanOpenDoor(20));
  assert(Q230PathBlocked(a,1,0,0,.1f,1)&&toggles==0); // Player key is not NPC ownership.
  a=Prepare(true,true);assert(Q230PathBlocked(a,1,0,0,.1f,1)&&toggles==1);
  a=Prepare();auto c=gPlayerSession->player.Definitions();c.scriptedBases.insert(200);c.defaultActivationDoors.insert(200);
  gPlayerSession=std::make_unique<Session>(c);assert(gPlayerSession->player.CanOpenDoor(20));
  assert(!gPlayerSession->player.CanActorOpenDoor(42,20)&&Q230PathBlocked(a,1,0,0,.1f,1)&&toggles==0);
  a=Prepare();stepBlocked=false;gQ2400InteriorDoors[20].q2401Moving=true;gQ2400InteriorDoors[20].collisionSceneSerial=gQ2400DoorSceneSerial;
  before=a.runtime.position;Q240AdvancePath(a,.1f,4);assert(a.runtime.position==before&&toggles==0&&queries==0); // Another activator's closing door still blocks absent world collision.
  a=Prepare();Q240AdvancePath(a,.1f,5);stepBlocked=false;a.runtime.navigationDoor=0;before=a.runtime.position;Q240AdvancePath(a,.1f,6);assert(a.runtime.position==before&&toggles==1); // Replanning cannot bypass an opening animation.
  a=Prepare();gQ2400InteriorDoors[20].targetOpen=false;gQ2400InteriorDoors[20].q2401Moving=true;
  assert(Q230PathBlocked(a,1,0,0,.1f,1)&&toggles==0); // Closing sequence is never reversed mid-flight.
  if(argc>1)OriginalDoors(argv[1]);
  std::cout<<"NPC local door animation wait, collision, keys, script rejection and XTEL isolation passed\n";
}
