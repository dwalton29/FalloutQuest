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
int main(){ActorCrossCellHandoff();}
