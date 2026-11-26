// Same production package executor as regular NPC tests, with the new
// resident dialogue director's start/eligibility seams explicitly injected.
static int chatRequests=0;
#define FO3_NPC_CHAT_AVAILABLE(actor,reference,package) (reference==60u||reference==0x14u)
#define FO3_NPC_CHAT_REQUEST(speaker,listener,now) (++chatRequests,true)
#define main ExistingPackageTestMain
#include "npc_package_runtime_tests.cpp"
#undef main

int main(){
  chatRequests=0;
  auto a=Actor(15);
  auto c=gPlayerSession->player.Definitions();
  auto& p=c.pipboy.packages[50];
  p.type=15;p.location.valid=false;p.target={0,60,0,true};
  c.pipboy.targets[60].base=61;c.weapons.actors[61].health=100;
  gPlayerSession=std::make_unique<Session>(std::move(c));
  Q230ActorVisual listener;
  listener.source.refFormId=60;listener.source.baseFormId=61;
  listener.runtime.position=Q240ScenePosition({1050,2050,20});
  packageTargets.push_back(listener);
  Q240UpdateNpcPackage(a,0);
  assert(a.aiPackage==50&&chatRequests==1);
  assert(a.runtime.procedure==fo3npc::Procedure::Waiting);
  // Dead or missing target must never trigger a fabricated voice.
  packageTargets[0].runtime.Die(1);
  uint32_t selected=0;std::array<float,3> anchor{};float radius=0;
  const auto* next=Q240SelectPackage(a,selected,anchor,radius);
  assert(!next||next->type!=15);
  assert(chatRequests==1);
  // The exterior marker lives in another grid CELL. Stage there while the
  // player is outside PLD2, then walk into PTDT range and initiate once.
  a=Actor(15);c=gPlayerSession->player.Definitions();
  auto& staged=c.pipboy.packages[50];
  staged.location={0,65,0,true};staged.location2={0,65,350,true};staged.target={0,0x14,200,true};
  auto& marker=c.pipboy.targets[65];marker.cell=88;marker.world=2;
  marker.x=1150;marker.y=2100;marker.z=20;
  gPlayerSession=std::make_unique<Session>(std::move(c));
  auto placePlayer=[&](float x,float z){
    gPlayerSession->player.SetScriptPlayerLocation(x,2100,z,1,2);
    gQ210Head=Q240ScenePosition({x,2100,z+100});
  };
  placePlayer(1650,20);chatRequests=0;
  for(int tick=0;tick<400;++tick)Q240UpdateNpcPackage(a,tick*.05);
  const std::array<float,3> staging{1150,2100,20};
  assert(a.aiPackage==50&&Q240PlanarDistance(Q240GamePosition(a),staging)<=8&&chatRequests==0);
  placePlayer(1450,600);
  for(int tick=400;tick<420;++tick)Q240UpdateNpcPackage(a,tick*.05);
  assert(chatRequests==0);
  placePlayer(1450,20);
  for(int tick=420;tick<700&&!chatRequests;++tick)Q240UpdateNpcPackage(a,tick*.05);
  assert(chatRequests==1&&Q240PlanarDistance(Q240GamePosition(a),{1450,2100,20})<=200);
  packageTargets.clear();
  std::cout<<"NPC Dialogue AI package: original actor target, approach, one voice request, dead-target rejection passed\n";
}
