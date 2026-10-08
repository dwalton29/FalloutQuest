// Same production package executor as regular NPC tests, with the new
// resident dialogue director's start/eligibility seams explicitly injected.
static int chatRequests=0;
#define FO3_NPC_CHAT_AVAILABLE(actor,reference) (reference==60u)
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
  packageTargets.clear();
  std::cout<<"NPC Dialogue AI package: original actor target, approach, one voice request, dead-target rejection passed\n";
}
