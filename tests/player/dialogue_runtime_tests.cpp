// Exercise the production dialogue bridge and package executor. Platform audio,
// input and skin queries are adapters; dialogue conditions/results are real.
#define main PackageFixtureMain
#include "npc_package_runtime_tests.cpp"
#undef main
#include "audio/fo3-dialogue-completion.h"
#include "audio/fo3-dialogue-face.h"
#include "dialogue/fo3-dialogue-session.h"
#include <unistd.h>
std::vector<Q230ActorVisual> gQ230NpcActors;
struct StaticReference {uint32_t refFormId=0;bool q230NpcActor=false,physicalWeapon=false,q2400SwingDoor=false;std::string baseRecordType;};
std::array<StaticReference,2> gQ220Grab{};
std::unordered_set<uint32_t> gFo3ScriptRemovedItems;
std::unordered_map<uint32_t,int> gQ223DynamicBodies;
int retiredDisplays=0,collisionRefreshes=0;
void RetireFo3LooseBody(uint32_t){++retiredDisplays;}
void RefreshFo3RemovedCollisionRefs(){++collisionRefreshes;}

std::vector<StaticReference> gObjects;
uint64_t gStereoFrame=0;
bool loading=false,pipboy=false;
int stopped=0,flushes=0;
bool IsFo3LoadingVisible(){return loading;}
bool Fo3PipboyFocus(){return pipboy;}
void FlushFo3PlayerState(){++flushes;}
void Q230UpdateActor(Q230ActorVisual&){}
void Q230DialogueResponseAnimation(Q230ActorVisual&,const fo3dialogue::Session&){}
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
#include "world/fo3-scene-publication.h"
bool gSceneReady=true;
struct {bool active=false;} gSceneLoad,gQ230NpcArrival;
std::shared_ptr<const fo3world::ReferenceStateSnapshot> gFo3ReferenceState;
fo3scene::ReferencePublication gFo3ScenePublication;
bool Fo3ResidentAssetLaneBlocked(){return false;}
bool Fo3AcceptsScenePublication(fo3scene::PublicationStamp stamp){
  return gPlayerSession&&gFo3ScenePublication.Accepts(stamp,gPlayerSession->player.Snapshot().referenceStateRevision);
}
#include "dialogue/fo3-dialogue-runtime.inc"
static double clockTime=0;
static void Tick(bool a=false,bool b=false,bool x=false,float horizontal=0){++gStereoFrame;clockTime+=.05;UpdateFo3Dialogue(clockTime,0,a,b,true,horizontal,0,0,x);}
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
  {const uint32_t token=0x30000001u;assert(!gDialogue.Active());
    actor.runtime.scriptSpeechToken=token;gPlayerSession->player.SetScriptActorTalking(actor.source.refFormId,true);
    auto lip=std::make_shared<fo3face::Lip>();lip->frames.resize(30);lip->frames[0].speech[0]=.75f;
    fo3audio::face.Start(token);fo3audio::face.Asset(token,lip);fo3audio::face.Position(token,0);Tick();
    assert(actor.facialWeights[0]==.75f);
    actor.runtime.scriptSpeechToken=0;gPlayerSession->player.SetScriptActorTalking(actor.source.refFormId,false);fo3audio::face.Start(0);
    for(int i=0;i<20;++i)Tick();for(float weight:actor.facialWeights)assert(weight==0);
  }
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
static void BarterBridge(){
  PrepareSynthetic();auto c=gPlayerSession->player.Definitions();
  c.references[42].base=43;c.references[99].base=333;
  c.merchantContainers[42]=99;c.merchantServices[43]=0x400;c.pipboy.packages[50].flags&=~1u;
  c.barterSettings={{"fBarterBuyBase",1.55f},{"fBarterBuyMult",-.45f},{"fBarterSellBase",.45f},{"fBarterSellMult",.45f}};
  fo3player::Item caps;caps.formId=0xf;caps.name="Bottle Caps";caps.value=1;c.items[0xf]=caps;
  fo3player::Item item;item.formId=100;item.name="Original-format merchandise";item.value=100;c.items[100]=item;
  c.containers[333].entries={{100,0,10},{0xf,0,1000}};
  auto& greeting=c.pipboy.topics.at(80).front();greeting.flags=0;greeting.links={91};
  c.pipboy.dialogueTopics[91].text="Authored merchant topic";c.pipboy.topLevelTopics={91};
  auto trade=greeting;trade.id=92;trade.topic=91;trade.links.clear();trade.begin.source="ShowBarterMenu 50";
  c.pipboy.topics[91]={trade};gPlayerSession=std::make_unique<Session>(c);
  assert(gPlayerSession->player.Add(0xf,1000));
  fo3player::ActorState merchant;merchant.cell=gCurrentCellFormId;merchant.world=gExteriorWorldspaceQ1890;merchant.package=50;
  assert(gPlayerSession->player.UpdateActor(42,merchant));Tick();
  assert(StartFo3Dialogue(42));assert(!gBarter.active);Speak();
  assert(gDialogue.phase==fo3dialogue::Phase::Choices&&gDialogue.choices.size()==1);
  Tick(true);assert(gDialogue.phase==fo3dialogue::Phase::Speaking&&!gBarter.active);Tick(false);
  Speak();assert(gBarter.active&&gBarter.session.actor==42&&Fo3DialogueFocus());
  assert(gBarter.playerPanel.visible&&gBarter.merchantPanel.visible&&gBarter.rows.size()==1);
  Tick(true);Tick(false);
  assert(gBarter.controls.Editing()&&gBarter.controls.quantity==10&&gBarter.session.selections.empty());
  // X cannot skip quantity acceptance or commit a pending amount.
  Tick(false,false,true);Tick();assert(gPlayerSession->player.ItemCount(100)==0&&gBarter.controls.Editing());
  for(int i=0;i<9;++i){Tick(false,false,false,-1);Tick();}
  assert(gBarter.controls.quantity==1);
  Tick(true);Tick(false);assert(gBarter.session.selections.size()==1&&gBarter.session.selections[0].count==1);
  assert(gPlayerSession->player.ItemCount(100)==0);
  const auto flushesBefore=flushes;
  Tick(false,false,true);Tick(false,false,true);
  assert(flushes==flushesBefore+1&&gBarter.status.empty());Tick();
  assert(gPlayerSession->player.ItemCount(100)==1&&gPlayerSession->player.ItemCount(0xf)==900);
  assert(gBarter.session.selections.empty());
  Tick(false,true);Tick();assert(!gBarter.active&&gDialogue.Active());
  Choose(0);assert(gBarter.active);Tick(true);Tick(false);
  assert(gBarter.controls.Editing()&&gBarter.controls.maximum==9);
  Tick(false,true);Tick();assert(!gBarter.controls.Editing()&&gBarter.session.selections.empty()&&gBarter.active);
  Tick(true);Tick(false);Tick(true);Tick(false);assert(gBarter.session.selections.size()==1);
  const auto capsBefore=gPlayerSession->player.ItemCount(0xf);
  gQ230NpcActors[0].runtime.position[0]+=fo3npc::DialoguePolicy::LeaveRange+1;Tick();Clean();
  assert(!gBarter.active&&gPlayerSession->player.ItemCount(0xf)==capsBefore);
  std::cout<<"Barter bridge: authored voice phases, input focus, pending selection, commit, return and walk-away passed\n";
}
static void NonresidentPublication(){
  PrepareSynthetic();auto c=gPlayerSession->player.Definitions();
  c.pipboy.formNames["offscene"]=400;c.references[400].base=43;c.references[400].cell=1234;
  c.actorInventories[43].valid=true;c.cellOwnership[1234]={};
  gPlayerSession=std::make_unique<Session>(c);auto& player=gPlayerSession->player;
  gCurrentCellFormId=10;gExteriorWorldspaceQ1890=0;
  gFo3ReferenceState=std::make_shared<const fo3world::ReferenceStateSnapshot>(player.CaptureReferenceState());
  gFo3ScenePublication.Publish(gFo3ReferenceState);
  fo3pipdata::ResultScript script;script.source="RewardXP 7\nOffscene.Disable";std::string error;
  auto plan=player.PrepareDialogueTransaction(script,{},error);assert(plan&&plan->HasReferenceChanges());
  const auto stamp=gFo3ScenePublication.Stamp();
  assert(PreviewFo3DialogueReferences(player,*plan,error)&&player.Snapshot().experience==0);
  gSceneLoad.active=true;assert(!PublishFo3DialogueReferences(player,*plan,error)&&player.Snapshot().experience==0);gSceneLoad.active=false;
  assert(PublishFo3DialogueReferences(player,*plan,error)&&player.Snapshot().experience==7);
  assert(!gFo3ReferenceState->Enabled(400,true)&&gFo3ScenePublication.Stamp().scene!=stamp.scene);
  assert(Fo3AcceptsScenePublication(gFo3ScenePublication.Stamp()));
  std::cout<<"Production nonresident dialogue publication and busy-scene rejection passed\n";
}
static void OriginalMoiraInterview(const char* path){
  fo3player::Catalog catalog;std::string error;assert(fo3player::LoadCatalog(path,catalog,error));
  const auto ref=catalog.pipboy.formNames.at("moirabrownref"),display=catalog.pipboy.formNames.at("ms03armoredvaultsuitref"),gift=catalog.pipboy.formNames.at("ms03vaultsuit101armored");
  for(uint32_t answer:{0x5ecc5u,0x5ecc4u,0x5ecc3u,0x5ecc2u,0x5ecc1u}){
    gPlayerSession=std::make_unique<Session>(catalog);auto& p=gPlayerSession->player;p.RegisterOriginalActors();gFo3ScriptRemovedItems.clear();
    const auto& placed=catalog.pipboy.targets.at(ref);gCurrentCellFormId=placed.cell;gExteriorWorldspaceQ1890=placed.world;
    p.SetScriptPlayerLocation(placed.x,placed.y+40,placed.z,placed.cell,placed.world);
    gQ230NpcActors.clear();gObjects={{display,false,false,false,"ARMO"}};
    gFo3ReferenceState=std::make_shared<const fo3world::ReferenceStateSnapshot>(p.CaptureReferenceState());gFo3ScenePublication.Publish(gFo3ReferenceState);
    fo3dialogue::Context ctx;ctx.player=&p;ctx.speaker={ref,placed.base,placed.cell,placed.world,0,placed.x,placed.y,placed.z,false,false};ctx.target={fo3player::PlayerRef,7,placed.cell,placed.world,0,placed.x,placed.y+40,placed.z,false,false};
    fo3dialogue::Session talk;talk.previewReferences=PreviewFo3DialogueReferences;talk.publishReferences=PublishFo3DialogueReferences;
    const auto finish=[&](){unsigned count=0;while(talk.phase==fo3dialogue::Phase::Speaking){assert(++count<100);talk.AudioDone(talk.audioToken,true,ctx,p);}};
    const auto choose=[&](uint32_t topic){const auto i=std::find_if(talk.choices.begin(),talk.choices.end(),[&](const auto& c){return c.topic==topic;});assert(i!=talk.choices.end());assert(talk.Choose(size_t(i-talk.choices.begin()),ctx,p));finish();};
    assert(talk.Start(ctx,p));finish();choose(0x5ecc0);
    // The displayed ARMO is resident here, unlike the old data-only fixture.
    for(uint32_t option:{0x5ecc5u,0x5ecc4u,0x5ecc3u,0x5ecc2u,0x5ecc1u})assert(std::any_of(talk.choices.begin(),talk.choices.end(),[&](const auto& c){return c.topic==option;}));
    if(answer==0x5ecc5){
      const auto* info=talk.Resolve(answer,ctx);assert(info);auto plan=p.PrepareDialogueInfoTransaction(info->topic,info->id,fo3player::DialogueScriptPhase::Both,error,ref);assert(plan);
      gQ220Grab[0].refFormId=display;assert(!PreviewFo3DialogueReferences(p,*plan,error)&&!PublishFo3DialogueReferences(p,*plan,error)&&p.ItemCount(gift)==0);gQ220Grab[0].refFormId=0;
    }
    const int retired=retiredDisplays,collisions=collisionRefreshes;choose(answer);
    assert(p.ItemCount(gift)==1&&gFo3ScriptRemovedItems.count(display)&&!p.IsCollected(display));
    assert(retiredDisplays==retired+1&&collisionRefreshes==collisions+1&&!gFo3ReferenceState->Enabled(display,true));
    talk.End("leave shop");
  }
  std::cout<<"Original Moira five vault answers with resident display removal and real publication bridge passed\n";
}
int main(int argc,char** argv){Synthetic();SkipWithA();BarterBridge();NonresidentPublication();if(argc>1){OriginalBridge(argv[1]);OriginalMoiraInterview(argv[1]);}std::cout<<"Dialogue bridge lifecycle tests passed\n";}
