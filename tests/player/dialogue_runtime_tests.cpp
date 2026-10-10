// Exercise the production dialogue bridge and package executor. Platform audio,
// input and skin queries are adapters; dialogue conditions/results are real.
#define main PackageFixtureMain
#include "npc_package_runtime_tests.cpp"
#undef main
#include "audio/fo3-dialogue-completion.h"
#include "audio/fo3-dialogue-face.h"
#include <unistd.h>
std::vector<Q230ActorVisual> gQ230NpcActors;
uint64_t gStereoFrame=0;
bool loading=false,pipboy=false;
int stopped=0,flushes=0;
bool IsFo3LoadingVisible(){return loading;}
bool Fo3PipboyFocus(){return pipboy;}
void FlushFo3PlayerState(){++flushes;}
void Q230UpdateActor(Q230ActorVisual&){}
bool Q230LiveBone(const Q230ActorVisual& actor,int,std::array<float,3>& p){p=actor.runtime.position;p[1]+=1.5f;return true;}
bool Q230LiveBounds(Q230ActorVisual&,std::array<float,3>&,std::array<float,3>&){return false;}
namespace fo3audio {
DialogueCompletionMailbox completion;
DialogueFaceMailbox face;
uint32_t published=0;
void Dialogue(const std::string&,uint32_t token){published=token;completion.Start(token);face.Start(token);}
void DialogueStop(){++stopped;published=0;completion.Stop();face.Start(0);}
uint64_t DialogueCompletion(){return completion.Take();}
DialogueFaceSample DialogueFace(uint32_t token){return face.Read(token);}
void NamedSound(const std::string&){}
void Scroll(){}
void DialogueGain(float){}
template<class T> void RadioState(const T&){}
}
#include "dialogue/fo3-dialogue-runtime.inc"
static double clockTime=0;
static void Tick(bool a=false,bool b=false){++gStereoFrame;clockTime+=.05;UpdateFo3Dialogue(clockTime,0,a,b,true);}
static void Speak(){
  for(int budget=0;gDialogue.phase==fo3dialogue::Phase::Speaking;++budget){
    assert(budget<32&&fo3audio::published==gDialogue.audioToken);
    assert(fo3audio::completion.Done(fo3audio::published,true));Tick();
  }
  Tick(); // Choice presentation becomes eligible on the subsequent frame.
}
static void Choose(size_t index){
  assert(gDialogue.phase==fo3dialogue::Phase::Choices&&index<gDialogue.choices.size());
  gDialogueCursor.selected=index;Tick(true);Tick(false);Speak();
}
static void Clean(){
  assert(!gDialogue.Active()&&!gDialoguePanel.visible&&gDialoguePanel.choices.empty()&&gDialoguePanel.subtitle.empty());
  assert(gDialoguePublishedToken==0&&gDialoguePanelInfo==0&&gDialoguePanelResponse==SIZE_MAX);
  assert(gDialogueChoicesVisibleFrame==0&&fo3audio::published==0&&gDialogueEligibility.empty());
  for(const auto& actor:gQ230NpcActors)assert(!actor.runtime.dialogue&&!actor.runtime.speaking);
}
static void PrepareSynthetic(){
  auto actor=Actor(6);auto c=gPlayerSession->player.Definitions();
  auto& d=c.pipboy;d.greetings={80};d.voices[90]="Voice";d.dialogueActors[43].voice=90;
  d.quests[81].flags=1;fo3pipdata::Info info;info.id=82;info.topic=80;info.quest=81;info.flags=1;
  info.responses={{0,1,"Original-format goodbye"}};d.topics[80]={info};
  gPlayerSession=std::make_unique<Session>(c);gQ230NpcActors={actor};gQ210Head=actor.runtime.position;
  Q240UpdateNpcPackage(gQ230NpcActors[0],clockTime);Tick();
}
static void SkipWithA(){
  PrepareSynthetic();
  auto catalog=gPlayerSession->player.Definitions();
  auto& greeting=catalog.pipboy.topics.at(80).front();
  greeting.flags=0; // Continue to one authored player option after the responses.
  greeting.responses.push_back({0,2,"Second authored spoken response"});
  greeting.links={91};
  catalog.pipboy.dialogueTopics[91].text="Authored follow-up";
  fo3pipdata::Info followup=greeting;
  followup.id=92;followup.topic=91;followup.flags=1;
  followup.responses.resize(1);followup.links.clear();
  catalog.pipboy.topics[91]={followup};
  gPlayerSession=std::make_unique<Session>(catalog);
  auto& actor=gQ230NpcActors[0];
  Tick(false);
  assert(StartFo3Dialogue(42));
  const auto first=gDialogue.audioToken;
  const int initialStops=stopped;
  Tick(true); // A stops the first response and publishes the second.
  assert(stopped==initialStops+1&&gDialogue.Active());
  assert(gDialogue.phase==fo3dialogue::Phase::Speaking);
  assert(gDialogue.info==82&&gDialogue.response==1&&gDialogue.audioToken!=first);
  assert(fo3audio::published==gDialogue.audioToken);
  assert(!fo3audio::completion.Done(first,true)); // old platform callback is stale
  const auto second=gDialogue.audioToken;
  Tick(true); // Holding A is not another button edge.
  assert(gDialogue.response==1&&gDialogue.audioToken==second);
  Tick(false);
  Tick(true); // Skip last response: do not also pick the first choice.
  assert(gDialogue.Active()&&gDialogue.phase==fo3dialogue::Phase::Choices);
  assert(gDialogue.choices.size()==1&&gDialogue.choices[0].topic==91);
  assert(gDialogue.info==82&&gDialogueChoicesVisibleFrame>gStereoFrame);
  assert(!fo3audio::completion.Done(second,true));
  Tick(true);
  assert(gDialogue.info==82&&gDialogue.phase==fo3dialogue::Phase::Choices);
  Tick(false); // Reveal options after the transition frame.
  assert(gDialoguePanel.choices.size()==1);
  Tick(true); // A in Choices should continue behaving as selection.
  assert(gDialogue.info==92&&gDialogue.phase==fo3dialogue::Phase::Speaking);
  Tick(false);
  const auto last=gDialogue.audioToken;
  Tick(true); // Skipping authored GOODBYE runs its normal end transition.
  assert(!gDialogue.Active()&&!fo3audio::completion.Done(last,true));
  Clean();assert(actor.runtime.activity==fo3npc::Activity::Package);
  Tick(false);
  // A and natural audio completion on the same frame must not double-skip.
  assert(StartFo3Dialogue(42));
  const auto natural=gDialogue.audioToken;
  assert(fo3audio::completion.Done(natural,true));
  Tick(true);
  assert(gDialogue.phase==fo3dialogue::Phase::Speaking);
  assert(gDialogue.response==1&&gDialogue.audioToken!=natural);
  Tick(false,true);Tick();Clean();
}
static void Synthetic(){
  PrepareSynthetic();auto& actor=gQ230NpcActors[0];
  uint32_t old=0;
  for(int repeat=0;repeat<4;++repeat){
    assert(DialogueCanStart(actor)&&StartFo3Dialogue(42));assert(actor.runtime.dialogue);
    assert(!StartFo3Dialogue(42)&&gDialogue.audioToken!=old);
    assert(!fo3audio::completion.Done(old,true));old=gDialogue.audioToken;
    auto lip=std::make_shared<fo3face::Lip>();lip->firstFrame=0;lip->frames.resize(30);lip->frames[0].speech[0]=1;lip->frames[0].modifiers[0]=.5f;
    fo3audio::face.Asset(old,lip);fo3audio::face.Position(old,0);Tick();
    assert(actor.facialWeights[0]==1&&actor.facialWeights[16]==.5f);
    Speak();Clean();assert(actor.runtime.activity==fo3npc::Activity::Package);
    for(int i=0;i<20;++i)Tick();for(float w:actor.facialWeights)assert(w==0);
    Q240UpdateNpcPackage(actor,clockTime);assert(actor.aiPackage==50&&actor.runtime.Alive());
  }
  assert(StartFo3Dialogue(42));loading=true;Tick();loading=false;Clean();
  assert(StartFo3Dialogue(42));actor.runtime.BeginCombat(0x14);Tick();Clean();
  actor.runtime.EndCombat();assert(actor.runtime.CanTalk()&&StartFo3Dialogue(42));Tick(false,true);Tick();Clean();
  assert(StartFo3Dialogue(42));pipboy=true;Tick();pipboy=false;Clean();
  assert(StartFo3Dialogue(42));gQ210Head[0]+=10;Tick();Clean();
}
static void OriginalBridge(const char* path){
  fo3player::Catalog c;std::string error;assert(fo3player::LoadCatalog(path,c,error));
  gPlayerSession=std::make_unique<Session>(c);gQ230NpcActors.clear();
  gCurrentCellFormId=0xa96;gExteriorWorldspaceQ1890=0xa74;packageHour=12;
  std::vector<Fo3NpcActorQ230> sources;assert(LoadFo3CellActors(0xa96,sources,path));
  std::vector<Fo3NpcNavMeshQ240> raw;assert(LoadFo3NpcNavigationQ240(0xa96,0xa74,raw,path));
  auto graph=std::make_shared<Q240NavigationGraph>();
  for(auto& mesh:raw){graph->triangleOffsets.push_back(graph->triangleCount);graph->triangleCount+=mesh.triangles.size();graph->byForm[mesh.formId]=graph->meshes.size();graph->meshes.push_back(std::make_shared<Fo3NpcNavMeshQ240>(std::move(mesh)));}
  for(const auto& source:sources)if(source.refFormId==0x3b46){Q230ActorVisual actor;actor.source=source;actor.navigationGraph=graph;actor.runtime.position=Q240ScenePosition({source.x,source.y,source.z});gQ230NpcActors.push_back(actor);}
  assert(gQ230NpcActors.size()==1);auto& actor=gQ230NpcActors[0];gQ210Head=actor.runtime.position;
  Q240UpdateNpcPackage(actor,clockTime);assert(actor.aiPackage==0x3dbce);Tick();
  assert(DialogueCanStart(actor)&&StartFo3Dialogue(actor.source.refFormId));assert(gDialogue.info==0x3da20);Speak();
  assert(gDialogue.choices.size()==3);Choose(0);assert(gDialogue.info==0x3b82);Choose(0);assert(gDialogue.info==0x3da07);
  Tick(false,true);Tick();Clean();
  const auto variables=gPlayerSession->player.Snapshot().pipboy.dialogueVariables;
  assert(!variables.empty()&&gPlayerSession->player.Snapshot().pipboy.talkedActors.count(0x3b46));
  Q240UpdateNpcPackage(actor,clockTime);assert(actor.aiPackage==0x55471);
  uint32_t previous=0;
  for(int repeat=0;repeat<3;++repeat){
    gQ210Head=actor.runtime.position;assert(DialogueCanStart(actor)&&StartFo3Dialogue(0x3b46));
    assert(gDialogue.info!=0x3da20&&gDialogue.audioToken!=previous);assert(!fo3audio::completion.Done(previous,true));previous=gDialogue.audioToken;
    Speak();Tick(false,true);Tick();Clean();
    assert(gPlayerSession->player.Snapshot().pipboy.dialogueVariables==variables);
    Q240UpdateNpcPackage(actor,clockTime);assert(actor.aiPackage==0x55471&&actor.runtime.activity==fo3npc::Activity::Package);
  }
  const auto initial=actor.runtime.position;
  for(int i=0;i<1500;++i){clockTime+=.016;Q240UpdateNpcPackage(actor,clockTime);}
  assert(actor.runtime.position!=initial);
  actor.runtime.BeginCombat(0x14);actor.runtime.EndCombat();gQ210Head=actor.runtime.position;
  assert(StartFo3Dialogue(0x3b46));Speak();Tick(false,true);Tick();Clean();
  std::cout<<"Original Lucas production bridge: handshake, three reentries, package movement and post-combat reentry passed\n";
}
int main(int argc,char** argv){Synthetic();SkipWithA();if(argc>1)OriginalBridge(argv[1]);std::cout<<"Dialogue bridge lifecycle tests passed\n";}
