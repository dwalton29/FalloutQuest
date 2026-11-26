#include "mq01_test_conversation.h"
#include "dialogue/fo3-dialogue-selection.h"
#include "npc/fo3-npc-state.h"
#include "npc/fo3-actor-animation.h"
#include "physics/fo3-nif-collision.h"
#include "rendering/mesh/fo3-burst-particles.h"
#include "scripting/fo3-script-master-match.h"
#include "ui/pause/fo3-debug-menu.h"
#include "ui/terminal/fo3-terminal-state.h"
bool LoadQuestObjectClips(const std::string&,std::vector<fo3script::AnimationSequence>&);
bool SitInQuestFurniture(const fo3pipdata::Definitions&,uint32_t,const std::string&,uint8_t&,std::array<float,3>&,std::array<float,3>&);
#include <cassert>
#include <iostream>
#include <unistd.h>

// Optional host checkpoint for the separate production NPC combat executor.
static const char* robotSecuritySave=nullptr;

// The original lamp has nine rigid bodies attached to nested chain bones.
static void CheckChandelierCollision(const std::string& path) {
  std::vector<uint8_t> bytes;
  fo3anim::Skeleton skeleton;
  std::vector<int> blockBones;
  std::vector<fo3anim::Clip> clips;
  assert(LoadFalloutMeshFile(path,bytes,nullptr));
  assert(fo3anim::DecodeObjectAnimation(bytes,skeleton,blockBones,clips,true));
  std::vector<Fo3NifCollisionShapeQ6F> shapes;
  assert(LoadFo3NifCollisionShapesComplete(path,shapes));
  unsigned links=0;
  for(const auto& shape:shapes) {
    if(shape.kind!=Fo3CollisionShapeKindQ6F::Capsule) continue;
    const int bone=blockBones.at(shape.targetBlock);
    assert(bone>=0 && !shape.bodyTransformApplied && !shape.nestedTransformApplied);
    // Compare the three basis vectors and translation with the independently
    // decoded visual rig. Both transforms must place the same capsule.
    const auto& bind=skeleton.bindGlobal.at(bone);
    for(int row=0;row<3;++row)
      for(int col=0;col<4;++col)
        assert(std::abs(shape.localToModel[row*4+col]-bind[col*4+row])<.002f);
    ++links;
  }
  assert(links==9);
}

static void TranquilityExit(fo3player::Player& run,const std::string& master,const std::string& meshRoot,bool invasion){
  const auto& data=run.Definitions().pipboy;std::string error;
  const auto exit=data.formNames.at("mq04exitdooractref"),pod=data.formNames.at("mq04playerpod");
  const auto& outside=data.targets.at(exit);
  run.SetScriptPlayerLocation(outside.x,outside.y,outside.z,outside.cell,outside.world);
  auto plan=run.PrepareActivationTransaction(exit,fo3player::PlayerRef,error);
  if(!plan)std::cerr<<"Simulation exit: "<<error<<std::endl;
  assert(plan);
  if(plan->HasReferenceChanges()){
    fo3script::ScriptMasterMatch match;std::atomic<bool> cancel{false};
    assert(fo3script::MatchScriptProgramsToMaster(master,plan->ExecutedScripts(),match,cancel,error));
  }
  assert(run.CommitScriptTransaction(*plan));
  const auto move=run.Snapshot().playerMove;assert(move.pending&&move.marker==pod);
  const auto& garage=data.targets.at(data.formNames.at("smithcaseygarageextdoor"));
  const auto& map=run.Snapshot().pipboy.lastExterior;
  assert(map.cell==garage.cell&&map.world==garage.world&&map.x==garage.x&&map.y==garage.y);
  for(int i=1;i<=7;++i)assert(run.ActorKnockedOut(data.formNames.at("mq04soldierref"+std::to_string(i)))==invasion);
  std::cout<<"Original exit moved to the pod and anchored the Pip-Boy map outside the garage"<<std::endl;
  const auto& ids=data.formNames;const auto locker=ids.at("mq04playercontainerref");
  const auto* stored=run.ContainerContents(locker);assert(stored&&!stored->empty());
  auto clips=std::make_shared<fo3script::AnimationRegistry>();
  for(const auto& [ref,file]:std::vector<std::pair<uint32_t,std::string>>{
      {ids.at("mq04playerpodshellref"),"furniture/tlpodplayer01.nif"},
      {ids.at("mq04dadpodshellref"),"furniture/tlpod01.nif"},
      {ids.at("vgeardoorr112ref"),"dungeons/vaultruined/doors/vgeardoorr112.nif"},
      {ids.at("vault112secrethatchref"),"dungeons/utility/rooms/utlrmstairshatch01.nif"}})
    assert(LoadQuestObjectClips(meshRoot+"/"+file,(*clips)[ref]));
  run.SetScriptAnimationRegistry(clips);run.SetScriptPlayerLocation(move.position[0],move.position[1],move.position[2],move.cell,move.world);
  run.AcknowledgePlayerMove();
  const auto key=fo3dialogue::VariableKey(data,"MQ04.DadGreet");
  const auto quest=ids.at("mq04");
  const auto original=data.objectReferenceScripts.at(locker).first;
  run.SetScriptEventDiagnostic([&](const std::string& line){if(line.find("SCPT "+std::to_string(original)+" ")!=std::string::npos)std::cerr<<line<<std::endl;});
  for(int i=0;i<40&&(!run.Snapshot().pipboy.dialogueVariables.count(key)||run.Snapshot().pipboy.quests.at(quest).stage<75);++i){
    run.SetScriptRuntimeClock(80+i*.25,.25);
    run.PumpWorldAwareGameMode(.25,{locker},[&](auto change){
      if(change->HasReferenceChanges()){
        fo3script::ScriptMasterMatch match;std::atomic<bool> cancel{false};
        if(!fo3script::MatchScriptProgramsToMaster(master,change->ExecutedScripts(),match,cancel,error)){
          std::cerr<<"Vault 112 return: "<<error<<std::endl;return false;
        }
      }
      return run.CommitScriptTransaction(*change);
    });
  }
  assert(run.Snapshot().pipboy.dialogueVariables.count(key)&&run.Snapshot().pipboy.dialogueVariables.at(key)==1);
  assert(run.ContainerContents(locker)->empty()&&run.Snapshot().pipboy.quests.at(quest).stage==75);
  for(const char* name:{"mq04playerpodshellref","mq04dadpodshellref"}){
    bool opening=false;assert(run.ScriptAnimationPlaying(ids.at(name),opening,"Forward")&&opening);
  }
  bool suit=false;assert(run.ScriptEquippedItem(fo3player::PlayerRef,ids.at("vaultsuit112"),suit)&&suit);
  std::cout<<"Vault 112 return restored equipment, reopened both pods and enabled James's greeting"<<std::endl;

  // The two goodbye topics decide whether James will escort the player.
  for(bool escort:{false,true}){
    fo3player::Player end(run);
    Conversation james(end,ids.at("mqdadref"));james.Start();
    uint32_t finish=ids.at(escort?"mq04dadletsgo":"mq04dadnotgoing");
    for(auto topic:{"mq04dadchoice1","mq04dadgoodthingiwasthere","mq04dadtakeabreak","mq04dadsoitsbacktorivetcity"})
      james.Choose(ids.at(topic));
    james.Choose(finish);
    const auto& status=end.Snapshot().pipboy.quests;
    assert(status.at(quest).stage==200&&status.at(ids.at("mq05")).stage==10);
    const auto escortKey=fo3dialogue::VariableKey(data,"MQ05.DadEscort");
    const auto flag=end.Snapshot().pipboy.dialogueVariables.find(escortKey);
    assert((flag!=end.Snapshot().pipboy.dialogueVariables.end()&&flag->second==1)==escort);
    assert(end.Snapshot().experience==run.Snapshot().experience+600&&end.Snapshot().achievements.count(7));
  }
  std::cout<<"James's two departure choices completed Tranquility Lane and opened The Waters of Life"<<std::endl;

}

// Host the production full-scene publication path. The native loader queue is
// represented by its requested cell and world, while the original transaction
// and published reference snapshot still come from the engine.
#include "world/fo3-scene-publication.h"
namespace simulationScene {
static uint32_t gCurrentCellFormId,gExteriorWorldspaceQ1890;
static float gSceneCenterXQ1730,gSceneCenterYQ1730,gSceneFloorZQ1730;
static std::shared_ptr<const fo3world::ReferenceStateSnapshot> gFo3ReferenceState;
static fo3scene::ReferencePublication gFo3ScenePublication;
static bool refreshRequested;
static bool CanDeferFo3DialogueResult(){return true;}
static bool QueueFo3WorldModelRefresh(uint32_t cell,uint32_t world,float,float,float){
  refreshRequested=cell==gCurrentCellFormId&&world==gExteriorWorldspaceQ1890;return refreshRequested;
}
#include "dialogue/fo3-dialogue-scene-refresh.inc"
static void Configure(Conversation& talk){
  const auto& origin=talk.context.speaker;
  gCurrentCellFormId=origin.cell;gExteriorWorldspaceQ1890=origin.world;
  gSceneCenterXQ1730=origin.x;gSceneCenterYQ1730=origin.y;gSceneFloorZQ1730=origin.z;
  refreshRequested=false;
  talk.session.previewReferences=[](const auto&,const auto&,auto&){return CanDeferFo3DialogueResult();};
  talk.session.publishReferences=PublishFo3DialogueSceneRefresh;
}
}

// Walk the placed puzzle and use the visible original terminal menu. Each
// object executes its own attached OnActivate and GameMode blocks.
static void TranquilityFailsafe(const fo3player::Player& arrival,const std::string& master,const std::string& meshRoot,const std::string& save){
  fo3player::Player run(arrival);
  const auto& data=run.Definitions().pipboy;
  const auto term=data.formNames.at("mq04failsafeterminalref");
  const auto effects=data.formNames.at("mq04failsafeeffectsref");
  const auto seq=fo3dialogue::VariableKey(data,"MQ04.FailSeq");
  const auto inv=fo3dialogue::VariableKey(data,"MQ04.Invasion");
  std::string error;
  auto disabled=[&](uint32_t ref){bool value=false;assert(run.ReferenceDisabled(ref,value,error));return value;};
  auto value=[&](uint64_t key){const auto& vars=run.Snapshot().pipboy.dialogueVariables;
    const auto found=vars.find(key);return found==vars.end()?0.f:found->second;};
  auto publish=[&](auto& plan){
    if(!plan){std::cerr<<"Tranquility Lane action: "<<error<<std::endl;return false;}
    if(plan->HasReferenceChanges()){
      fo3script::ScriptMasterMatch match;std::atomic<bool> cancel{false};
      if(!fo3script::MatchScriptProgramsToMaster(master,plan->ExecutedScripts(),match,cancel,error)){
        std::cerr<<"Tranquility Lane publication: "<<error<<std::endl;return false;
      }
    }
    return run.CommitScriptTransaction(*plan);
  };
  auto press=[&](uint32_t ref){
    const auto& at=data.targets.at(ref);
    run.SetScriptPlayerLocation(at.x,at.y,at.z,at.cell,at.world);
    auto plan=run.PrepareActivationTransaction(ref,fo3player::PlayerRef,error);assert(publish(plan));
  };
  // The five unnamed placements in the abandoned house, ordered by base record.
  const std::array<uint32_t,5> objects{0x2b7fc,0x2b7fe,0x2b800,0x2b802,0x2b804};
  assert(disabled(term)&&disabled(effects));
  press(objects[4]);assert(value(seq)==1);
  press(objects[4]);
  for(int i=0;i<12;++i){run.SetScriptRuntimeClock(50+i*.1,.1);
    run.PumpWorldAwareGameMode(.1,{objects[4]},[&](auto plan){return publish(plan);});}
  assert(value(seq)==0&&disabled(term));
  press(objects[4]);press(objects[2]);press(objects[3]);assert(value(seq)==3);
  assert(run.Save(save,error)&&run.Restore(save,error));unlink(save.c_str());
  for(int index:{2,1,3,0})press(objects[index]);
  assert(value(seq)==8&&!disabled(term)&&!disabled(effects));
  fo3terminal::Session terminal;terminal.Open(run,term,0);
  const auto submenu=data.formNames.at("mq04failsafeterminalinvasionsubmenu");
  assert(terminal.page==fo3terminal::Session::Page::Menu);
  const auto& entries=data.terminals.at(terminal.menu).entries;
  const auto link=std::find_if(terminal.entries.begin(),terminal.entries.end(),[&](size_t n){return entries[n].submenu==submenu;});
  assert(link!=terminal.entries.end());
  auto open=run.PrepareTerminalTransaction(term,terminal.menu,*link,error);assert(publish(open));
  terminal.parents.push_back(terminal.menu);terminal.menu=submenu;terminal.Refresh(run);
  auto chooseVisible=[&](size_t index){
    assert(std::find(terminal.entries.begin(),terminal.entries.end(),index)!=terminal.entries.end());
    auto chosen=run.PrepareTerminalTransaction(term,terminal.menu,index,error);assert(publish(chosen));
    terminal.Refresh(run);
  };
  // Entry 1 explains the lock. Reading entry 2 makes the real start entry
  // (0) visible via GetScriptVariable on the placed terminal.
  chooseVisible(1);assert(value(inv)==0);
  chooseVisible(2);
  chooseVisible(0);assert(value(inv)==1);
  std::cout<<"Tranquility Lane failsafe tones, failed sequence, saved solution and invasion terminal choice passed"<<std::endl;
  const auto quest=data.formNames.at("mq04"),script=data.quests.at(quest).script;
  const auto karma=run.Snapshot().karma;
  run.SetScriptEventDiagnostic([&](const std::string& line){
    if(line.find("SCPT "+std::to_string(script)+" ")!=std::string::npos)std::cerr<<line<<std::endl;
  });
  for(int frame=0;frame<30&&run.Snapshot().pipboy.quests.at(quest).stage<70;++frame){
    run.SetScriptRuntimeClock(60+frame*.1,.1);
    run.PumpWorldAwareGameMode(.1,{},[&](auto plan){return publish(plan);});
  }
  assert(run.Snapshot().pipboy.quests.at(quest).stage==70);
  assert(!disabled(data.formNames.at("mq04exitdooractref"))&&run.Snapshot().karma==std::min(karma+300.f,1000.f));
  for(int soldier=1;soldier<=7;++soldier)assert(!disabled(data.formNames.at("mq04soldierref"+std::to_string(soldier))));
  assert(!disabled(data.formNames.at("mqdadref"))&&run.Snapshot().pipboy.quests.at(data.formNames.at("mq05")).stage==1);
  const auto& marker=data.targets.at(data.formNames.at("mq04georgeinvasionmarker"));
  const auto& resident=run.Snapshot().actors.at(data.formNames.at("georgeneusbaumref"));
  assert(resident.cell==marker.cell&&resident.position[0]==marker.x&&resident.position[1]==marker.y&&resident.position[2]==marker.z);
  TranquilityExit(run,master,meshRoot,true);
  std::cout<<"Original simulation invasion relocated residents, enabled seven soldiers and the exit, and awarded Karma"<<std::endl;

}

// Select the original trap message buttons and let each attached script
// consume GetButtonPressed. The host supplies Mabel's arrival at the trigger.
static void HendersonTraps(const fo3player::Player& start,const std::string& meshRoot,const std::string& save){
  const auto& data=start.Definitions().pipboy;
  const auto& ids=data.formNames;
  const auto mabel=ids.at("mabelhendersonref"),oven=ids.at("mq04mabelovenref");
  const auto lamp=ids.at("mq04chandelierref"),volume=ids.at("mq04chandeliertriggerref");
  std::string error;
  const auto skate=ids.at("mq04rollerskateactivatorref");
  const auto& blast=start.Definitions().explosions.at(ids.at("mq04ovenexplosion"));
  std::string model=fo3script::Lower(blast.model);
  std::replace(model.begin(),model.end(),'\\','/');
  std::vector<uint8_t> bytes;
  assert(LoadFalloutMeshFile(meshRoot+"/"+model,bytes,nullptr));
  const auto layers=fo3burst::Decode(bytes);
  assert(layers.size()==4);
  std::map<std::string,float> expectedHeights{{"fxsmokesub.dds",1.486684f},{"sparkburst02.dds",.403545f},
      {"shrap01.dds",0.f},{"grenadeflash01.dds",0.f}};
  for(const auto& layer:layers){
    const auto name=fo3script::Lower(layer.texture.substr(layer.texture.find_last_of("/\\")+1));
    const auto& matrix=layer.emitterToModel;
    assert(layer.count && std::abs(matrix.m[3])<.001f && std::abs(matrix.m[7])<.001f);
    assert(std::abs(matrix.m[11]-expectedHeights.at(name))<.001f);
    expectedHeights.erase(name);
  }
  assert(expectedHeights.empty());
  for(auto ref:{oven,lamp,skate}){
    fo3player::Player player(start);
    auto clips=std::make_shared<fo3script::AnimationRegistry>();
    const auto chandelierPath=meshRoot+"/clutter/questitems/mq04chandellier01.nif";
    CheckChandelierCollision(chandelierPath);
    assert(LoadQuestObjectClips(chandelierPath,(*clips)[lamp]));
    assert(LoadQuestObjectClips(meshRoot+"/clutter/questitems/mq04rollerskatesanimated.nif",(*clips)[skate]));
    player.SetScriptAnimationRegistry(clips);
    const auto& point=data.targets.at(ref);
    player.SetScriptPlayerLocation(point.x,point.y,point.z,point.cell,point.world);
    auto apply=[&](auto plan){
      if(!plan)std::cerr<<"Henderson trap "<<std::hex<<ref<<std::dec<<": "<<error<<std::endl;
      assert(plan&&player.CommitScriptTransaction(*plan));
    };
    for(int button:{0,1}){
      apply(player.PrepareActivationTransaction(ref,fo3player::PlayerRef,error));
      fo3player::ScriptMessage message;
      const auto prompt=ids.at(ref==oven?"mq04mabelovenmessage":ref==skate?"mq04mabelrollerskatemessage":"mq04mabelchandeliermessage");
      while(player.PollScriptMessage(message)&&message.form!=prompt){}
      assert(message.form==prompt&&player.SubmitScriptMessageButton(ref,button));
      apply(player.PrepareReferenceEventTransaction(ref,"GameMode",0,error));
      const auto key=fo3dialogue::VariableKey(data,ref==oven?"MQ04MabelOvenRef.OvenRigged":ref==skate?"MabelHendersonRef.SkatePlaced":"MabelHendersonRef.ChandelierRigged");
      const auto& vars=player.Snapshot().pipboy.dialogueVariables;
      assert((vars.count(key)?vars.at(key):0.f)==button);
      if(ref==skate&&button==1){
        bool moving=false;assert(player.ScriptAnimationPlaying(skate,moving,"Forward")&&moving);
      }
    }
    assert(player.Save(save,error)&&player.Restore(save,error)&&unlink(save.c_str())==0);
    player.SetScriptAnimationRegistry(clips);
    if(ref==oven){
      Conversation talk(player,mabel);talk.Start();talk.Choose(ids.at("mq04mabeluseoven"));
      assert(player.Snapshot().pipboy.dialogueVariables.at(fo3dialogue::VariableKey(data,"MQ04MabelOvenRef.OvenRigged"))==2);
      const auto& cooking=data.packages.at(ids.at("mq04mabelblowupoven"));
      assert(cooking.type==13&&cooking.location.value==ids.at("mq04mabelovenidle"));
      assert(fo3dialogue::Conditions(cooking.conditions,talk.context,error));
      if(robotSecuritySave)assert(player.Save(std::string(robotSecuritySave)+".oven",error));
    }else if(ref==skate){
      if(robotSecuritySave)assert(player.Save(std::string(robotSecuritySave)+".skate",error));
    }else{
      if(robotSecuritySave)assert(player.Save(std::string(robotSecuritySave)+".lamp",error));
      const auto& trigger=data.targets.at(volume);
      assert(player.MoveActorTo(mabel,volume,{0,0,0}));
      auto crossings=player.ScriptActorTriggerChanges(mabel,trigger.cell,trigger.world,{trigger.x,trigger.y,trigger.z});
      assert(std::find(crossings.begin(),crossings.end(),std::make_pair(volume,true))!=crossings.end());
      apply(player.PrepareReferenceEventTransaction(volume,"OnTriggerEnter",mabel,error));
      player.AcknowledgeScriptTrigger(volume,true,mabel);
      bool falling=false;assert(player.ScriptAnimationPlaying(lamp,falling,"Unequip")&&falling);
    }
  }
  std::cout<<"Henderson trap prompts, cooking dialogue, original skate animation and chandelier drop animation passed"<<std::endl;
}

// Exercise the robot terminal and the same hit delivery used by creature AI.
// The host supplies contact. Autonomous target acquisition is a separate check.
static void HendersonRobot(fo3player::Player& player,const std::string& save,const std::string& master,const std::string& meshRoot){
  const auto& data=player.Definitions().pipboy;
  const auto& ids=data.formNames;
  const auto terminal=ids.at("tlhendersonrobotterminalref"),robot=ids.at("tlhendersonrobotref");
  const auto door=ids.at("tlhendersonhousedoorref"),mabel=ids.at("mabelhendersonref"),quest=ids.at("mq04");
  const auto& at=data.targets.at(terminal);
  player.SetScriptPlayerLocation(at.x,at.y,at.z,at.cell,at.world);
  std::string error;
  auto commit=[&](auto plan){
    if(!plan)std::cerr<<"Henderson robot: "<<error<<std::endl;
    assert(plan);
    if(plan->HasReferenceChanges()||plan->HasProgramProvenance()){
      fo3script::ScriptMasterMatch matched;std::atomic<bool> cancel{false};
      if(!fo3script::MatchScriptProgramsToMaster(master,plan->ExecutedScripts(),matched,cancel,error)){
        std::cerr<<"Henderson provenance: "<<error<<std::endl;std::abort();
      }
    }
    assert(player.CommitScriptTransaction(*plan));
  };
  commit(player.PrepareReferenceEventTransaction(robot,"OnLoad",0,error));
  commit(player.PrepareActivationTransaction(terminal,fo3player::PlayerRef,error));
  fo3script::Action action;
  while(player.PollScriptAction(action)&&action.kind!=fo3script::ActionKind::DefaultActivate){}
  assert(action.reference==terminal&&action.flags==fo3player::PlayerRef);
  fo3terminal::Session menu;menu.Open(player,terminal,0);
  assert(menu.page==fo3terminal::Session::Page::Menu);
  auto choose=[&](size_t index){
    assert(std::find(menu.entries.begin(),menu.entries.end(),index)!=menu.entries.end());
    commit(player.PrepareTerminalTransaction(terminal,menu.menu,index,error));
    menu.Refresh(player);
  };
  choose(1);
  assert(player.GetLock(door).locked);
  choose(2);
  assert(!player.GetLock(door).locked);
  choose(1);choose(4);
  commit(player.PrepareReferenceEventTransaction(robot,"GameMode",0,error));
  float aggression=0;assert(player.ActorAIValue(robot,0,aggression)&&aggression==3);
  if(robotSecuritySave)assert(player.Save(robotSecuritySave,error));
  const auto weapon=ids.at("fists");
  const float damage=player.ActorUnarmedDamage(robot);
  assert(damage>0);
  const float karma=player.Snapshot().karma;
  for(int hit=0;player.ActorHealth(mabel)>0;++hit){
    assert(hit<200);
    assert(player.ApplyAttack(robot,weapon,mabel,damage));
  }
  assert(player.Snapshot().pipboy.quests.at(quest).stage==40);
  assert(player.Snapshot().pipboy.dialogueVariables.at(fo3dialogue::VariableKey(data,"MQ04.Task3Complete"))==4);
  assert(player.Snapshot().karma==std::max(-1000.f,karma-100));
  assert(player.Save(save,error)&&player.Restore(save,error)&&unlink(save.c_str())==0);
  float attributed=0;
  assert(player.IsActorKiller(mabel,robot,attributed)&&attributed==1);
  assert(player.IsActorKiller(mabel,fo3player::PlayerRef,attributed)&&attributed==0);
  fo3player::Player revived(player);
  assert(revived.ResurrectActor(mabel)&&revived.IsActorKiller(mabel,robot,attributed)&&attributed==0);
  {
    Conversation betty(player,ids.at("bettyref"));
    simulationScene::Configure(betty);betty.Start();
    betty.Choose(0x28694u);betty.Choose(0x28692u);
    assert(player.Snapshot().pipboy.quests.at(quest).stage==45);
  }
  const auto bettyRef=ids.at("bettyref");
  auto tickBetty=[&]{
    fo3npc::RuntimeState idle;
    player.SetScriptActorProcedures({{bettyRef,uint8_t(fo3npc::ObservedAIProcedure(idle,false))}});
    commit(player.PrepareReferenceEventTransaction(bettyRef,"GameMode",0,error));
  };
  for(unsigned i=0;i<4;++i)tickBetty();
  assert(player.ActorHealth(mabel)>0);
  const auto doghouse=ids.at("mq04doghouseref");
  const auto& target=data.targets.at(doghouse);
  player.SetScriptPlayerLocation(target.x,target.y,target.z,target.cell,target.world);
  commit(player.PrepareActivationTransaction(doghouse,fo3player::PlayerRef,error));
  for(const auto item:{ids.at("mq04slashermask"),ids.at("mq04slasherknife")}){
    bool equipped=false;
    assert(player.ItemCount(item)==1&&player.ScriptEquippedItem(fo3player::PlayerRef,item,equipped)&&equipped);
  }
  assert(player.ActorFactionRank(fo3player::PlayerRef,ids.at("mq04slasherfaction"))==0);
  commit(player.PrepareActivationTransaction(doghouse,fo3player::PlayerRef,error));
  assert(player.ItemCount(ids.at("mq04slasherknife"))==1);
  for(int tick=0;tick<30&&player.Snapshot().pipboy.quests.at(quest).stage==45;++tick){
    player.SetScriptRuntimeClock(90+tick*.25,.25);
    player.PumpWorldAwareGameMode(.25,{},[&](auto plan){return player.CommitScriptTransaction(*plan);});
  }
  assert(player.Snapshot().pipboy.quests.at(quest).stage==50);
  const auto knife=player.EquippedWeapon();
  assert(knife&&knife->formId==ids.at("mq04slasherknife"));
  const auto instance=knife->id;
  unsigned kills=0;
  for(const auto name:{"georgeneusbaumref","patneusbaumref","janetrockwellref","rogerrockwellref",
                       "marthasimpsonref","oldladydithersref","billfosterref","mabelhendersonref"}){
    const auto resident=ids.at(name);const auto& position=player.Snapshot().actors.at(resident);
    player.SetScriptPlayerLocation(position.position[0],position.position[1],position.position[2],position.cell,position.world);
    assert(player.ActorHealth(resident)>0);
    for(unsigned hit=0;player.ActorHealth(resident)>0;++hit){
      assert(hit<200&&player.WeaponHit(instance,resident));
    }
    ++kills;
    assert(player.Snapshot().pipboy.dialogueVariables.at(fo3dialogue::VariableKey(data,"MQ04.SlasherKills"))==kills);
    if(kills==4)assert(player.Save(save,error)&&player.Restore(save,error)&&unlink(save.c_str())==0);
  }
  tickBetty();
  assert(player.Snapshot().pipboy.quests.at(quest).stage==60);
  for(auto topic:{0x28928u,0x28929u}){
    fo3player::Player departing(player);
    Conversation finalBetty(departing,bettyRef);simulationScene::Configure(finalBetty);
    finalBetty.Start();finalBetty.Choose(topic);
    assert(departing.Snapshot().pipboy.quests.at(quest).stage==70);
    TranquilityExit(departing,master,meshRoot,false);
  }
  std::cout<<"Betty's final choices, simulation exit and James handoff passed"<<std::endl;
  std::cout<<"Original doghouse inventory events equipped the slasher items and opened stage 50"<<std::endl;
  std::cout<<"Original Henderson terminal and robot-attributed death reached stage 40 across reload"<<std::endl;
}

static unsigned RockwellConfrontation(fo3player::Player& restored,uint32_t speaker,uint32_t listener,uint32_t package){
  const auto& data=restored.Definitions().pipboy;
  const auto quest=data.formNames.at("mq04"),janet=data.formNames.at("janetrockwellref");
  const auto progress=fo3dialogue::VariableKey(data,"MQ04.RockwellConv");
  const auto& definition=data.packages.at(package);
  fo3dialogue::Context ctx;ctx.player=&restored;ctx.talking=true;
  assert(fo3dialogue::ReferenceContext(restored,speaker,ctx.speaker));
  assert(fo3dialogue::ReferenceContext(restored,listener,ctx.target));
  std::string error;
  assert(fo3dialogue::Conditions(definition.conditions,ctx,error)&&error.empty());
  ctx.speaker.package=package;ctx.speaker.packageKnown=true;
  assert(definition.type==15&&definition.target.value==listener);
  auto begin=restored.PreparePackageEventTransaction(speaker,package,false,error);
  assert(begin&&restored.CommitScriptTransaction(*begin));
  assert(restored.QueueScriptSpeech(speaker,definition.dialogueTopic,listener,true,false,package));
  fo3script::Action action;
  while(restored.PollScriptAction(action)&&action.kind!=fo3script::ActionKind::Conversation){}
  assert(action.package==package&&action.reference==speaker&&action.flags==listener);
  fo3dialogue::Session conversation;
  assert(conversation.StartTopic(action.form,ctx,restored,true));
  unsigned responses=0;
  while(conversation.phase==fo3dialogue::Phase::Speaking){
    if(ctx.speaker.reference!=conversation.actor)std::swap(ctx.speaker,ctx.target);
    assert(conversation.actor==(responses%2?listener:speaker)&&++responses<=32);
    conversation.AudioDone(conversation.audioToken,true,ctx,restored);
  }
  assert(responses>0&&restored.Snapshot().pipboy.dialogueVariables.at(progress)==-1);
  auto done=restored.PreparePackageEventTransaction(speaker,package,true,error);
  assert(done&&restored.CommitScriptTransaction(*done));
  fo3npc::RuntimeState actor;
  actor.procedure=fo3npc::Procedure::Waiting;
  for(bool linked:{true,false}){
    const int procedure=fo3npc::ObservedAIProcedure(actor,linked);
    assert(procedure==(linked?4:3));
    restored.SetScriptActorProcedures({{janet,uint8_t(procedure)}});
    auto tick=restored.PrepareReferenceEventTransaction(janet,"GameMode",0,error);
    if(!tick)std::cerr<<"Rockwell GameMode: "<<error<<std::endl;
    assert(tick&&restored.CommitScriptTransaction(*tick));
    assert(restored.Snapshot().pipboy.quests.at(quest).stage==(linked?25:30));
  }
  return responses;
}

static void RockwellDesk(const fo3player::Player& start,const std::string& save,const std::string& master){
  fo3player::Player player(start);
  const auto& data=player.Definitions().pipboy;
  const auto& ids=data.formNames;
  const auto janet=ids.at("janetrockwellref"),roger=ids.at("rogerrockwellref");
  const auto desk=ids.at("mq04rockwelldeskref"),item=ids.at("mq04undies");
  const auto key=fo3dialogue::VariableKey(data,"MQ04.UndiesPlaced");
  const auto& at=data.targets.at(desk);
  std::string error;
  auto apply=[&](auto plan){
    if(!plan)std::cerr<<"Rockwell basement: "<<error<<std::endl;
    assert(plan);
    if(plan->HasReferenceChanges()){
      fo3script::ScriptMasterMatch matched;std::atomic<bool> cancel{false};
      assert(fo3script::MatchScriptProgramsToMaster(master,plan->ExecutedScripts(),matched,cancel,error));
    }
    assert(player.CommitScriptTransaction(*plan));
  };
  auto press=[&]{
    player.SetScriptPlayerLocation(at.x,at.y,at.z,at.cell,at.world);
    apply(player.PrepareActivationTransaction(desk,fo3player::PlayerRef,error));
  };
  bool disabled=false;
  press();assert(player.ReferenceDisabled(ids.at("mq04undiesstaticref"),disabled,error)&&disabled);
  assert(player.Definitions().references.at(0x9ccba).base==item&&player.Pickup(0x9ccba));
  press();
  assert(player.ItemCount(item)==0&&player.Snapshot().pipboy.dialogueVariables.at(key)==1);
  assert(player.ReferenceDisabled(ids.at("mq04undiesstaticref"),disabled,error)&&!disabled);
  {
    Conversation talk(player,janet);talk.Start();
    talk.Choose(ids.at("mq04rockwellbasementcomewithme"));talk.Choose(ids.at("mq04rockwellbasementtrustme"));
  }
  assert(player.Save(save,error)&&player.Restore(save,error)&&unlink(save.c_str())==0);
  player.SetScriptPlayerLocation(at.x,at.y,at.z,at.cell,at.world);
  apply(player.PrepareReferenceEventTransaction(desk,"GameMode",0,error));
  const auto& moved=player.Snapshot().actors.at(janet);
  const auto& entry=data.targets.at(ids.at("mq04basementjanetentrymarker"));
  assert(moved.cell==entry.cell&&moved.position[0]==entry.x&&moved.position[1]==entry.y);
  // Supply the resident's arrival at the desk after the authored catch-up move.
  assert(player.MoveActorTo(janet,desk,{0,25,0}));
  apply(player.PrepareReferenceEventTransaction(janet,"GameMode",0,error));
  fo3script::Action line;
  while(player.PollScriptAction(line)&&line.kind!=fo3script::ActionKind::SayTo){}
  assert(line.reference==janet&&line.form==ids.at("mq04janetbasementline")&&line.flags==fo3player::PlayerRef);
  fo3dialogue::Context ctx;ctx.player=&player;
  assert(fo3dialogue::ReferenceContext(player,janet,ctx.speaker));
  ctx.target={fo3player::PlayerRef,7,at.cell,at.world,0,at.x,at.y,at.z,false,false};
  fo3dialogue::Session say;assert(say.StartSelectedTopic(line.form,line.info,ctx,player));
  say.AudioDone(say.audioToken,true,ctx,player);
  assert(player.Snapshot().pipboy.dialogueVariables.at(key)==2);
  for(bool wearing:{false,true}){
    fo3player::Player choice(player);
    {
      Conversation talk(choice,janet);talk.context.speaker=ctx.speaker;talk.context.target=ctx.target;
      choice.SetScriptPlayerLocation(at.x,at.y,at.z,at.cell,at.world);talk.Start();
      talk.Choose(ids.at("mq04janetundieslookslikewomensclothing"));
      talk.Choose(ids.at(wearing?"mq04janetundieswearingit":"mq04janetundiescheatingonyou"));
    }
    assert(choice.Snapshot().pipboy.dialogueVariables.at(fo3dialogue::VariableKey(data,"MQ04.Task2Complete"))==(wearing?5:2));
    RockwellConfrontation(choice,janet,roger,ids.at(wearing?"mq04janetconfrontrogerundiesw":"mq04janetconfrontrogerundies"));
  }
  std::cout<<"The desk, Janet's basement discovery, and both evidence interpretations reached stage 30"<<std::endl;
}

static void RockwellRollingPin(const fo3player::Player& start,const std::string& save){
  fo3player::Player run(start);
  const auto& data=run.Definitions().pipboy;
  const auto& ids=data.formNames;
  const auto pin=ids.at("mq04rollingpin"),martha=ids.at("marthasimpsonref"),roger=ids.at("rogerrockwellref");
  const auto placed=uint32_t{0x9ccb9};
  std::string error;
  assert(run.Definitions().references.at(placed).base==pin&&run.Pickup(placed)&&!run.Pickup(placed));
  const auto weapon=run.EquippedWeapon();assert(weapon&&weapon->formId==pin);
  const auto stack=weapon->id;
  const auto& resident=run.Snapshot().actors.at(martha);
  run.SetScriptPlayerLocation(resident.position[0],resident.position[1],resident.position[2],resident.cell,resident.world);
  const auto karma=run.Snapshot().karma;
  for(unsigned hits=0;run.ActorHealth(martha)>0;++hits)assert(hits<200&&run.WeaponHit(stack,martha));
  assert(run.Snapshot().pipboy.dialogueVariables.at(fo3dialogue::VariableKey(data,"MQ04.Task2Complete"))==4);
  assert(run.Snapshot().karma==std::max(-1000.f,karma-50));
  assert(run.Save(save,error)&&run.Restore(save,error)&&unlink(save.c_str())==0);
  for(auto response:{"mq04rogermarthadead","mq04rogerbloodyrollingpin","mq04rogerjanetsnapped"}){
    fo3player::Player choice(run);
    {
      Conversation talk(choice,roger);talk.Start();talk.Choose(ids.at("mq04rogerbadnewsaboutjanet"));talk.Choose(ids.at(response));
    }
    assert(choice.ItemCount(pin)==0);
    RockwellConfrontation(choice,roger,ids.at("janetrockwellref"),ids.at("mq04rogerconfrontjanetrollingpin"));
  }
  std::cout<<"The placed rolling pin, Martha's death and all three accusations reached the Rockwell turn-in"<<std::endl;
}

// Continue from Betty's actual stage-25 turn-in. The host signals voice
// completion and supplies resident procedure observations at the frame boundary.
static void RockwellSpeech(const fo3player::Player& start,const std::string& save,const std::string& master,const std::string& meshRoot){
  const auto& catalog=start.Definitions();
  const auto& data=catalog.pipboy;
  const auto& ids=data.formNames;
  const auto janet=ids.at("janetrockwellref"),roger=ids.at("rogerrockwellref"),quest=ids.at("mq04");
  const auto decision=fo3dialogue::VariableKey(data,"MQ04.Task2Complete");
  const auto package=ids.at("mq04janetconfrontrogerspeech");
  const auto& definition=data.packages.at(package);
  std::string error;
  RockwellDesk(start,save+".desk",master);
  RockwellRollingPin(start,save+".pin");
  for(bool success:{false,true}){
    fo3player::Player run(start);
    {
      Conversation talk(run,janet);talk.Start();
      talk.session.random.seed(success?7:0);
      talk.Choose(ids.at("mq04janetspeechchallenge"));
    }
    assert(run.Save(save,error));
    fo3player::Player restored(catalog);
    assert(restored.Restore(save,error)&&unlink(save.c_str())==0);
    auto value=[&](uint64_t key){
      const auto& vars=restored.Snapshot().pipboy.dialogueVariables;
      return vars.count(key)?vars.at(key):0.f;
    };
    assert(value(decision)==(success?1:0));
    fo3dialogue::Context ctx;ctx.player=&restored;ctx.talking=true;
    assert(fo3dialogue::ReferenceContext(restored,janet,ctx.speaker));
    assert(fo3dialogue::ReferenceContext(restored,roger,ctx.target));
    assert(fo3dialogue::Conditions(definition.conditions,ctx,error)==success&&error.empty());
    assert(restored.Snapshot().pipboy.quests.at(quest).stage==25);
    if(!success)continue;
    const auto karma=restored.Snapshot().karma;
    assert(RockwellConfrontation(restored,janet,roger,package)==7);
    assert(restored.Snapshot().karma==std::max(-1000.f,karma-50));
    assert(!restored.ActorGhost(janet)&&!restored.ActorGhost(roger));
    Conversation betty(restored,ids.at("bettyref"));betty.Start();
    betty.Choose(0x28041u);
    assert(restored.Snapshot().pipboy.quests.at(quest).stage==35);
    HendersonTraps(restored,meshRoot,save+".traps");
    HendersonRobot(restored,save+".robot",master,meshRoot);
    std::cout<<"Rockwell speech outcomes, seven-line confrontation, Karma and Mabel assignment passed"<<std::endl;
  }
}

// From the actual bench handoff, use the dialogue UI and its two speech-roll
// outcomes. The host provides actor positions and signals audio completion.
static void TranquilityOpening(const fo3player::Player& arrival, const std::string& save,const std::string& master,const std::string& meshRoot) {
  fo3player::Player invited(arrival);
  const auto& catalog = invited.Definitions();
  const auto& ids = catalog.pipboy.formNames;
  const uint32_t quest = ids.at("mq04");
  assert(invited.Snapshot().pipboy.quests.at(quest).stage == 5);
  {
    Conversation betty(invited, ids.at("bettyref"));
    betty.Start();
    for (const auto topic : {0x2581au, 0x25812u, 0x35e5cu, 0x25970u}) betty.Choose(topic);
    assert(invited.Snapshot().pipboy.quests.at(quest).stage == 15);
  }
  std::string error;
  assert(invited.Save(save, error));
  const auto priorXp = invited.Snapshot().experience;
  for (const bool success : {false, true}) {
    fo3player::Player run(catalog);
    assert(run.Restore(save, error));
    Conversation timmy(run, ids.at("timmyneusbaumref"));
    timmy.Start();
    // The RNG seed controls only the roll. Eligibility, reward and quest
    // stages still come from the original dialogue and its result scripts.
    timmy.session.random.seed(success ? 7 : 0);
    timmy.Choose(ids.at("mq04timmyspeechchallenge"));
    assert(run.Snapshot().pipboy.quests.at(quest).stage == 15);
    assert(run.Snapshot().experience == priorXp + (success ? 20 : 0));
    if (success) {
      assert(timmy.session.choices.size() == 3);
      timmy.Choose(0x25a00u);
      assert(run.Snapshot().pipboy.quests.at(quest).stage == 20);
    } else {
      assert(timmy.session.choices.empty());
    }
    const auto outcomeSave = save + ".speech";
    assert(run.Save(outcomeSave, error));
    fo3player::Player restored(catalog);
    assert(restored.Restore(outcomeSave, error));
    assert(restored.Snapshot().pipboy.quests.at(quest).stage == (success ? 20 : 15));
    assert(restored.Snapshot().experience == run.Snapshot().experience);
    if(success){
      Conversation report(restored,ids.at("bettyref"));
      simulationScene::Configure(report);report.Start();
      report.Choose(ids.at("mq04betty1qhowdoigetout"));
      report.Choose(ids.at("mq04betty1q1whatelse"));
      assert(simulationScene::refreshRequested&&restored.Snapshot().pipboy.quests.at(quest).stage==25);
      bool timmyDisabled=false,gnomeDisabled=true;
      assert(restored.ReferenceDisabled(ids.at("timmyneusbaumref"),timmyDisabled,error)&&timmyDisabled);
      assert(restored.ReferenceDisabled(ids.at("mq04gnomeref"),gnomeDisabled,error)&&!gnomeDisabled);
      assert(restored.ActorGhost(ids.at("janetrockwellref"))&&restored.ActorGhost(ids.at("rogerrockwellref")));
      RockwellSpeech(restored,save+".rockwells",master,meshRoot);
      std::cout<<"Betty accepted the first task, replaced Timmy's scene reference and started the Rockwell objective"<<std::endl;
    }
    assert(unlink(outcomeSave.c_str()) == 0);
  }
  assert(unlink(save.c_str()) == 0);
  std::cout << "Tranquility Lane greeting, first speech challenge and both saved outcomes passed\n";
}

// Original-data checkpoint. The explicit developer entry action executes the
// original first stage. Locations substitute for world traversal, not quest
// gates; subsequent progression is owned by original INFO results.
static void Original(const char* path,const std::string& meshRoot){
  fo3player::Catalog catalog;std::string error;assert(fo3player::LoadCatalog(path,catalog,error));
  char savePath[]="/tmp/fq-mq01-progression-XXXXXX";
  const int fd=mkstemp(savePath);assert(fd>=0);close(fd);
  const std::string save(savePath);
  fo3player::Player first(catalog);first.RegisterOriginalActors();assert(first.Add(0xf,100));
  // A direct-world-spawn save has no Escape handoff. Exercise the actual
  // generic debug menu rather than synthesising a post-Escape quest snapshot.
  fo3debug::Menu entry;entry.Open(first);entry.BuildQuests();
  const auto mq=std::find(entry.quests.begin(),entry.quests.end(),0x14e87u);assert(mq!=entry.quests.end());
  entry.form=*mq;entry.Go(fo3debug::Menu::Page::Quest);const auto entryRevision=first.Revision();
  assert(first.Snapshot().pipboy.quests.count(*mq)==0&&first.Revision()==entryRevision);
  assert(entry.Confirm());const auto& started=first.Snapshot().pipboy.quests.at(*mq);
  assert(started.stage==10&&started.running&&started.stages.count(10)&&started.objectives.at(10).displayed);
  const auto startedRevision=first.Revision();assert(!entry.Confirm()&&first.Revision()==startedRevision);entry.Close();
  const auto xp=first.Snapshot().experience;
  {
    Conversation moriarty(first,catalog.pipboy.formNames.at("colinmoriartyref"));moriarty.Start();
    for(auto topic:{0x3b7cu,0x14f3eu,0x151b6u})moriarty.Choose(topic);
    assert(first.ItemCount(0xf)==0&&first.Snapshot().pipboy.quests.at(0x14e87).stage==40);
    assert(first.Snapshot().pipboy.quests.at(0x14e87).objectives.at(40).displayed);
  }
  assert(first.Save(save,error));fo3player::Player travelled(catalog);if(!travelled.Restore(save,error)){std::cerr<<"Failed to restore Moriarty save: "<<error<<std::endl;std::abort();}
  assert(travelled.Snapshot().pipboy.quests.at(0x14e87).stage==40&&travelled.ItemCount(0xf)==0);
  // Return to Megaton before continuing. Greeting/said-once state survives;
  // simply opening another conversation cannot charge Moriarty's fee again.
  {Conversation returned(travelled,catalog.pipboy.formNames.at("colinmoriartyref"));returned.Start();}
  assert(travelled.ItemCount(0xf)==0&&travelled.Snapshot().experience==xp);
  const auto sarah=catalog.pipboy.formNames.at("mq01sarahref");
  const auto destination=catalog.pipboy.targets.at(catalog.pipboy.formNames.at("citlyonspridequartersmarker"));
  {
    Conversation li(travelled,catalog.pipboy.formNames.at("doctorliref"));li.Start();
    for(auto topic:{0x426fau,0x5f9bau,0x43df7u,0x43df4u,0x43df3u})li.Choose(topic);
  }
  const auto& complete=travelled.Snapshot().pipboy.quests.at(0x14e87);
  assert(complete.stage==210&&complete.stages.count(40)&&complete.stages.count(210)&&!complete.running);
  assert(complete.status==fo3pipdata::Completion::Complete);
  for(const auto& objective:complete.objectives)assert(objective.second.status==fo3pipdata::Completion::Complete);
  assert(travelled.Snapshot().pipboy.quests.at(0x14e89).stage==30);
  assert(travelled.Snapshot().pipboy.quests.at(0x1d493).stage==150);
  // The authored early-skip outcome has no 300 XP / achievement-4 award.
  assert(travelled.Snapshot().experience==xp&&!travelled.Snapshot().achievements.count(4));
  const auto relocated=travelled.Snapshot().actors.at(sarah);
  assert(relocated.cell==destination.cell&&relocated.world==destination.world&&relocated.yaw==-destination.rz);
  assert((relocated.position==std::array<float,3>{destination.x,destination.y,destination.z}));
  bool disabled=false;
  assert(travelled.ReferenceDisabled(catalog.pipboy.formNames.at("mq01behemothref"),disabled,error)&&disabled);
  assert(travelled.ReferenceDisabled(catalog.pipboy.formNames.at("xmarkpostmq01mutantenabler"),disabled,error)&&!disabled);
  assert(travelled.ReferenceDisabled(catalog.pipboy.formNames.at("colpostmq01gnrcollision"),disabled,error)&&!disabled);
  assert(travelled.Save(save,error));fo3player::Player restored(catalog);assert(restored.Restore(save,error));unlink(save.c_str());
  const auto& saved=restored.Snapshot().pipboy.quests.at(0x14e87);
  assert(saved.stage==210&&!saved.running&&saved.status==complete.status&&saved.journal.size()==complete.journal.size());
  assert(restored.Snapshot().actors.at(sarah).position==relocated.position);
  assert(restored.Snapshot().referenceEnabled==travelled.Snapshot().referenceEnabled);
  assert(restored.Snapshot().experience==xp&&restored.ItemCount(0xf)==0);
  // After actual completion, replay of visited stages must remain a no-op.
  const auto revision=restored.Revision();
  assert(restored.ExecuteQuestStage(0x14e87,210,error)&&restored.ExecuteQuestStage(0x14e89,30,error));
  assert(restored.Revision()==revision&&!restored.ScriptActorMoveEpoch(sarah));
  {Conversation liAgain(restored,catalog.pipboy.formNames.at("doctorliref"));liAgain.Start();}
  assert(restored.Snapshot().experience==xp&&restored.ItemCount(0xf)==0);
  std::cout<<"Original MQ01 Moriarty payment -> early Rivet City dialogue -> authored stage 210; world state, return/reload and one-time effects passed\n";
  // Isolate the original ending conversation using the same stage-40 save.
  // Actor placement is a dialogue fixture: this does not bypass or certify
  // the physical GNR approach, locked entrance or Behemoth battle.
  assert(first.Save(save,error));fo3player::Player normal(catalog);assert(normal.Restore(save,error));unlink(save.c_str());
  const auto dog=catalog.pipboy.formNames.at("threedogref");
  {
    Conversation gnr(normal,dog);gnr.Start();
    for(auto topic:{0x24170u,0x3e5a0u,0x2416cu,0x24164u,0x24162u,0x2415fu,0x26184u,0x3e57fu})gnr.Choose(topic);
  }
  const auto& ending=normal.Snapshot().pipboy.quests.at(0x14e87);
  assert(ending.stage==200&&ending.status==fo3pipdata::Completion::Complete&&!ending.running);
  assert(normal.Snapshot().pipboy.quests.at(0x14e88).stage==10);
  const auto note=catalog.pipboy.formNames.at("mq02virgonote");
  assert(normal.Snapshot().experience==xp+300&&normal.Snapshot().achievements.count(4)&&normal.ItemCount(note)==1);
  assert(normal.Snapshot().pipboy.dialogueVariables.at(fo3dialogue::VariableKey(catalog.pipboy,"RadioGalaxyNewsQuest.ExploitReady"))==1);
  assert(normal.Snapshot().actors.at(sarah).position==relocated.position);
  assert(normal.Save(save,error));fo3player::Player rewarded(catalog);assert(rewarded.Restore(save,error));unlink(save.c_str());
  assert(rewarded.Snapshot().experience==xp+300&&rewarded.ItemCount(note)==1&&rewarded.Snapshot().achievements.count(4));
  const auto completedRevision=rewarded.Revision();assert(rewarded.ExecuteQuestStage(0x14e87,200,error));
  assert(rewarded.Revision()==completedRevision&&!rewarded.ScriptActorMoveEpoch(sarah));
  fo3player::ExperienceNotice notice;assert(!rewarded.PollExperienceNotice(notice));
  {Conversation gnrAgain(rewarded,dog);gnrAgain.Start();}
  assert(rewarded.Snapshot().experience==xp+300&&rewarded.ItemCount(note)==1);
  std::cout<<"Original Three Dog ending conversation: stage 200, MQ02, 300 XP, NOTE/radio/achievement, relocation and duplicate-reward protection passed; GNR world approach not simulated\n";

  const auto id=[&](const char* name){return catalog.pipboy.formNames.at(name);};
  const auto dishRef=id("mq02dishref"),dish=id("mq02virgoiidish"),relay=id("mq02relaywithoutdish");
  const auto quest=id("mq02"),station=id("radiogalaxynewsref02");
  // Same operands as the relay SCDA, with station and relay in slots 1 and 2.
  fo3pipdata::ResultScript bytecode;
  bytecode.references={station,relay};
  bytecode.compiled={
    0xa7,0x11,11,0,2,0,4,0,'t','u','n','e','r',1,0,
    0x1c,0,1,0,0xd6,0x11,2,0,0,0,
    0x1c,0,2,0,0x13,0x10,9,0,2,0,4,0,'n',1,0,0,0};
  std::vector<fo3dialogue::Command> commands;
  assert(fo3dialogue::CompileResult(catalog.pipboy,bytecode,commands,error));
  assert(commands.size()==3&&commands[0].type==fo3dialogue::CommandType::TuneRadio&&commands[0].form==station);
  assert(commands[1].type==fo3dialogue::CommandType::RadioConversation&&commands[1].form==station&&!commands[1].index);
  assert(commands[2].type==fo3dialogue::CommandType::PlayGroup&&commands[2].form==relay&&commands[2].expression=="backward");
  auto sequences=std::make_shared<fo3script::AnimationRegistry>();
  if(meshRoot.empty()){
    (*sequences)[relay]={{"Backward",1,false}};
    (*sequences)[id("monumentosc")]={{"Forward",1,false}};
  }else{
    const std::string monument="architecture/washingtonmonument/";
    const std::vector<std::pair<const char*,std::string>> models={
      {"mq02relaywithoutdish","clutter/questitems/mq02radiotower.nif"},
      {"monumentosc",monument+"oscilloscope.nif"},{"washmonumentgate01ref",monument+"washmonumentgate01.nif"},
      {"washingmonumentelevatorinsidepanel",monument+"elevatorcontrol01.nif"},
      {"washdoorbase01",monument+"washelevatordoor01.nif"},{"elevatorshaft",monument+"washingtonmonelevatorshaft01.nif"},
      {"washeshaftlight01ref",monument+"fxwashelvtrshaftlight01.nif"},
      {"washeshaftlight02ref",monument+"fxwashelvtrshaftlight01.nif"},
      {"washeshaftlight03ref",monument+"fxwashelvtrshaftlight01.nif"}
    };
    for(const auto& model:models){
      const auto path=meshRoot+"/"+model.second;
      if(!LoadQuestObjectClips(path,(*sequences)[id(model.first)])){
        std::cerr<<"Could not register the original controller sequences in "<<path<<std::endl;std::abort();
      }
    }
  }
  rewarded.SetScriptAnimationRegistry(sequences);
  auto runEvent=[&](uint32_t ref,const char* event,uint32_t actor){
    auto plan=rewarded.PrepareReferenceEventTransaction(ref,event,actor,error,fo3player::ScriptProgramCapture::EnteredPrograms);
    if(!plan)std::cerr<<event<<" "<<std::hex<<ref<<": "<<error<<std::endl;
    assert(plan&&rewarded.CommitScriptTransaction(*plan));
  };
  auto chooseButton=[&](uint32_t ref,uint32_t prompt,int button){
    runEvent(ref,"OnActivate",fo3player::PlayerRef);
    fo3player::ScriptMessage message;
    assert(rewarded.PollScriptMessage(message)&&message.form==prompt);
    assert(rewarded.SubmitScriptMessageButton(ref,button));
    runEvent(ref,"GameMode",0);
  };
  auto arrive=[&](uint32_t marker,uint16_t expected){
    const auto& location=catalog.pipboy.targets.at(marker);
    rewarded.SetScriptPlayerLocation(location.x,location.y,location.z,location.cell,location.world);
    for(unsigned tick=0;tick<60&&rewarded.Snapshot().pipboy.quests.at(quest).stage!=expected;++tick){
      rewarded.PumpWorldAwareGameMode(.25,{},[&](auto plan){return rewarded.CommitScriptTransaction(*plan);});
    }
    assert(rewarded.Snapshot().pipboy.quests.at(quest).stage==expected);
  };
  arrive(id("mq02museummarker"),20);
  chooseButton(dishRef,id("mq02dishmessage"),1);
  assert(!rewarded.ItemCount(dish)&&rewarded.Snapshot().pipboy.quests.at(quest).stage==20);
  chooseButton(dishRef,id("mq02dishmessage"),0);
  assert(rewarded.ItemCount(dish)==1&&rewarded.Snapshot().pipboy.quests.at(quest).stage==30);
  assert(rewarded.ReferenceDisabled(dishRef,disabled,error)&&disabled);
  if(!meshRoot.empty()){
    const auto terminalBase=id("mq02washingtonmonumentterminal"),gate=id("washmonumentgate01ref");
    const auto& gatePosition=catalog.pipboy.targets.at(gate);
    rewarded.SetScriptPlayerLocation(gatePosition.x,gatePosition.y,gatePosition.z,gatePosition.cell,gatePosition.world);
    uint32_t terminalRef=0;float best=INFINITY;
    for(const auto& placed:catalog.pipboy.targets){
      if(placed.second.base!=terminalBase||placed.second.world!=gatePosition.world)continue;
      const float distance=std::hypot(placed.second.x-gatePosition.x,placed.second.y-gatePosition.y);
      if(distance<best){best=distance;terminalRef=placed.first;}
    }
    assert(terminalRef&&rewarded.TerminalPassword(terminalRef));
    fo3terminal::Session terminal;terminal.Open(rewarded,terminalRef,0);
    assert(terminal.page==fo3terminal::Session::Page::Menu&&!terminal.entries.empty());
    auto open=rewarded.PrepareTerminalTransaction(terminalRef,terminal.menu,terminal.entries.front(),error);
    if(!open)std::cerr<<"Monument access terminal: "<<error<<std::endl;
    assert(open&&rewarded.CommitScriptTransaction(*open));
    bool opening=false;
    assert(rewarded.ScriptAnimationPlaying(gate,opening,"Forward")&&opening);
  }
  arrive(id("mq02elevatorentrance"),40);
  if(!meshRoot.empty()){
    const auto panel=id("washingmonumentelevatorinsidepanel"),destination=id("mq02elevatorexit");
    const auto& button=catalog.pipboy.targets.at(panel);
    rewarded.SetScriptPlayerLocation(button.x,button.y,button.z,button.cell,button.world);
    rewarded.SetScriptRuntimeClock(10.,.1);
    auto press=rewarded.PrepareActivationTransaction(panel,fo3player::PlayerRef,error);
    if(!press)std::cerr<<"Elevator control: "<<error<<std::endl;
    assert(press&&rewarded.CommitScriptTransaction(*press));
    bool shutting=false;assert(rewarded.ScriptAnimationPlaying(id("washdoorbase01"),shutting,"Backward")&&shutting);
    std::unordered_set<uint32_t> moving;
    for(unsigned tick=1;tick<=600&&!rewarded.Snapshot().playerMove.pending;++tick){
      rewarded.SetScriptRuntimeClock(10.+.1*tick,.1);
      runEvent(panel,"GameMode",0);
      fo3script::Action action;
      while(rewarded.PollScriptAction(action))if(action.kind==fo3script::ActionKind::PlayGroup)moving.insert(action.reference);
    }
    const auto move=rewarded.Snapshot().playerMove;
    assert(move.pending&&move.marker==destination&&moving.count(id("elevatorshaft"))&&moving.count(panel));
    for(const auto* light:{"washeshaftlight01ref","washeshaftlight02ref","washeshaftlight03ref"})assert(moving.count(id(light)));
    rewarded.SetScriptPlayerLocation(move.position[0],move.position[1],move.position[2],move.cell,move.world);
    rewarded.AcknowledgePlayerMove();
    std::cout<<"Monument password opened the gate and original elevator animation completed the MoveTo handoff\n";
  }
  const auto karma=rewarded.Snapshot().karma;
  chooseButton(relay,id("mq02relaybeforemessage"),0);
  assert(!rewarded.ItemCount(dish)&&rewarded.Snapshot().pipboy.quests.at(quest).stage==50);
  assert(rewarded.Snapshot().pipboy.tunedRadio==station&&rewarded.Snapshot().karma==karma+200);
  assert(rewarded.ReferenceDisabled(id("radiogalaxynewsref01"),disabled,error)&&disabled);
  assert(rewarded.ReferenceDisabled(station,disabled,error)&&!disabled);
  assert(rewarded.ReferenceDisabled(id("mq02dishdoneref"),disabled,error)&&!disabled);
  runEvent(relay,"GameMode",0);
  assert(rewarded.Snapshot().karma==karma+200);
  assert(rewarded.Save(save,error));
  fo3player::Player repaired(catalog);assert(repaired.Restore(save,error));unlink(save.c_str());
  assert(repaired.Snapshot().pipboy.tunedRadio==station&&repaired.Snapshot().pipboy.quests.at(quest).stage==50);
  std::cout<<"Original Museum dish and Washington Monument relay events passed through MQ02 stage 50\n";

  {
    // A player who finds Doctor Li before reporting the repair can take the
    // alternative reward instead of directions to Rivet City.
    fo3player::Player detour(repaired);
    Conversation li(detour,id("doctorliref"));li.Start();
    for(auto topic:{0x426fau,0x5f9bau,0x43df7u,0x43df4u,0x43df3u})li.Choose(topic);
    assert(detour.Snapshot().pipboy.quests.at(id("mq03")).stage>=30);
    Conversation deal(detour,dog);deal.Start();
    auto chooseAvailable=[&](std::initializer_list<uint32_t> topics){
      for(auto topic:topics){
        if(std::any_of(deal.session.choices.begin(),deal.session.choices.end(),[&](const auto& choice){return choice.topic==topic;})){
          deal.Choose(topic);return;
        }
      }
      for(const auto& choice:deal.session.choices)std::cerr<<std::hex<<choice.topic<<" ";
      std::cerr<<std::dec<<"missing GNR reward choice"<<std::endl;std::abort();
    };
    chooseAvailable({0x24450,0x2444f,0x3e578});
    chooseAvailable({0xbc710,0xbc711,0x58fa6,0x58fa7});
    assert(detour.Snapshot().pipboy.quests.at(quest).stage==200&&detour.ItemCount(id("mq02lootstashkey"))==1&&
           detour.ItemCount(id("mq02weaponscache"))==1);
    std::cout<<"Three Dog's alternate weapons cache reward unlocked after finding Doctor Li independently\n";
  }

  {Conversation report(repaired,dog);report.Start();
   for(auto topic:{0x24450u,0x2444cu,0x2444du})report.Choose(topic);}
  assert(repaired.Snapshot().pipboy.quests.at(quest).stage==200);
  assert(!repaired.Snapshot().pipboy.quests.at(quest).running);
  assert(repaired.Snapshot().pipboy.quests.at(id("mq03")).running);
  assert(repaired.ItemCount(id("mq02liphotonote"))==1&&repaired.Snapshot().achievements.count(5));
  std::cout<<"Three Dog turn-in completed Galaxy News Radio and started Scientific Pursuits\n";

  const auto pursuits=id("mq03"),panel=id("mqapanelref");
  auto visit=[&](uint32_t marker,uint16_t stage){
    const auto& location=catalog.pipboy.targets.at(marker);
    repaired.SetScriptPlayerLocation(location.x,location.y,location.z,location.cell,location.world);
    for(unsigned tick=0;tick<60&&repaired.Snapshot().pipboy.quests.at(pursuits).stage!=stage;++tick)
      repaired.PumpWorldAwareGameMode(.25,{},[&](auto plan){return repaired.CommitScriptTransaction(*plan);});
    assert(repaired.Snapshot().pipboy.quests.at(pursuits).stage==stage);
  };
  visit(id("sciencelabmarker"),20);
  {Conversation li(repaired,id("doctorliref"));li.Start();
   for(auto topic:{0x426fau,0x5f9bau,0x43df7u,0x43df4u,0x43df3u})li.Choose(topic);}
  assert(repaired.Snapshot().pipboy.quests.at(pursuits).stage==30);
  visit(id("mq05controlroommarker"),40);
  auto panelTick=[&]{
    auto plan=repaired.PrepareReferenceEventTransaction(panel,"GameMode",0,error,fo3player::ScriptProgramCapture::EnteredPrograms);
    if(!plan)std::cerr<<error<<std::endl;
    assert(plan&&repaired.CommitScriptTransaction(*plan));
  };
  panelTick();
  assert(repaired.Snapshot().pipboy.quests.at(pursuits).stage==40);
  assert(repaired.Pickup(id("mq03holo21ref")));
  panelTick();
  assert(repaired.Snapshot().pipboy.quests.at(pursuits).stage==50);
  assert(repaired.Snapshot().pipboy.quests.at(pursuits).objectives.at(50).displayed);
  std::cout<<"Scientific Pursuits reached the journal listening objective via Rivet City, Doctor Li and the placed holotape\n";

  const auto tape=id("mq03holo21");
  const auto segments=fo3pipdata::NoteSegments(catalog.pipboy,tape);
  assert(segments.size()==1&&segments.front().paths.size()==4);
  const auto& journal=catalog.pipboy.notes.at(tape);
  auto finished=repaired.PrepareDialogueInfoTransaction(journal.topic,segments.front().info,
      fo3player::DialogueScriptPhase::End,error,0,fo3player::ScriptProgramCapture::EnteredPrograms);
  if(!finished)std::cerr<<error<<std::endl;
  assert(finished&&repaired.CommitScriptTransaction(*finished));
  assert(repaired.Snapshot().pipboy.quests.at(pursuits).stage==60);
  assert(repaired.Snapshot().pipboy.quests.at(pursuits).objectives.at(60).displayed);
  const auto read=fo3dialogue::VariableKey(catalog.pipboy,"MQ03.HoloRead");
  assert(repaired.Snapshot().pipboy.dialogueVariables.at(read)==1);
  assert(repaired.Save(save,error));
  fo3player::Player listened(catalog);assert(listened.Restore(save,error));unlink(save.c_str());
  assert(listened.Snapshot().pipboy.quests.at(pursuits).stage==60);
  std::cout<<"Dad's journal end result displayed Vault 112 objective and survived reload\n";

  if(!meshRoot.empty()){
    const auto suit=id("vaultsuit112"),shell=id("mq04playerpodshellref"),chair=id("mq04playerpod");
    const auto& pod=catalog.pipboy.targets.at(shell);
    listened.SetScriptPlayerLocation(pod.x,pod.y,pod.z,pod.cell,pod.world);
    {
      Conversation robot(listened,id("mq03robobrain01"));robot.Start();
      assert(listened.ItemCount(suit)==1&&listened.Snapshot().pipboy.quests.at(pursuits).stage==65);
    }
    // Vault 112 can also be discovered before Three Dog or Doctor Li.
    fo3player::Player explorer(catalog);explorer.RegisterOriginalActors();
    {
      Conversation robot(explorer,id("mq03robobrain01"));robot.Start();
      assert(explorer.ItemCount(suit)==1&&explorer.Snapshot().pipboy.quests.at(pursuits).stage==65);
      const auto& gear=explorer.Snapshot().inventory;
      auto provided=std::find_if(gear.begin(),gear.end(),[&](const auto& stack){return stack.formId==suit;});
      assert(provided!=gear.end()&&explorer.Equip(provided->id)&&explorer.Snapshot().pipboy.quests.at(pursuits).stage==70);
    }
    auto animations=std::make_shared<fo3script::AnimationRegistry>();
    assert(LoadQuestObjectClips(meshRoot+"/furniture/tlpodplayer01.nif",(*animations)[shell]));
    listened.SetScriptAnimationRegistry(animations);
    auto activate=[&](uint32_t ref){
      auto plan=listened.PrepareActivationTransaction(ref,fo3player::PlayerRef,error);
      if(!plan)std::cerr<<"Pod activation "<<std::hex<<ref<<std::dec<<": "<<error<<std::endl;
      assert(plan&&listened.CommitScriptTransaction(*plan));
    };
    activate(shell);
    fo3player::ScriptMessage noSuit;
    assert(listened.PollScriptMessage(noSuit)&&noSuit.form==id("mq04podnosuitmessage"));
    const auto& inventory=listened.Snapshot().inventory;
    const auto clothes=std::find_if(inventory.begin(),inventory.end(),[&](const auto& stack){return stack.formId==suit;});
    assert(clothes!=inventory.end()&&listened.Equip(clothes->id));
    const auto suitOn=fo3dialogue::VariableKey(catalog.pipboy,"MQ03.SuitOn");
    assert(listened.Snapshot().pipboy.quests.at(pursuits).stage==70);
    assert(listened.Snapshot().pipboy.dialogueVariables.at(suitOn)==1);
    activate(shell);
    bool opening=false;assert(listened.ScriptAnimationPlaying(shell,opening,"Forward")&&opening);
    activate(chair);
    bool delegated=false;fo3script::Action action;
    while(listened.PollScriptAction(action))
      if(action.kind==fo3script::ActionKind::DefaultActivate&&action.reference==chair)delegated=true;
    assert(delegated);
    uint8_t seatMarker=0;std::array<float,3> seatedEye{},actorOrigin{};
    assert(SitInQuestFurniture(catalog.pipboy,chair,meshRoot,seatMarker,seatedEye,actorOrigin));
    const auto& placement=catalog.pipboy.targets.at(chair);
    listened.SetScriptPlayerLocation(seatedEye[0],seatedEye[1],(seatedEye[2]+placement.z)*.5f,placement.cell,placement.world,0,&actorOrigin);
    listened.SetFurnitureUse(chair,seatMarker,3);
    const auto chairState=fo3dialogue::VariableKey(catalog.pipboy,"MQ04.PlayerInChair");
    for(int tick=1;tick<=66;++tick){
      listened.SetScriptRuntimeClock(tick*.25,.25);
      auto plan=listened.PrepareReferenceEventTransaction(chair,"GameMode",0,error);
      if(!plan)std::cerr<<"Vault 112 chair tick: "<<error<<std::endl;
      assert(plan&&listened.CommitScriptTransaction(*plan));
      if(tick==1){
        bool closing=false;
        if(!listened.ScriptAnimationPlaying(shell,closing,"Backward")||!closing)
          std::cerr<<"Seated origin "<<actorOrigin[0]<<","<<actorOrigin[1]<<","<<actorOrigin[2]<<" eye "<<seatedEye[0]<<","<<seatedEye[1]<<","<<seatedEye[2]<<" chair "<<placement.x<<","<<placement.y<<","<<placement.z<<" chair var "<<(listened.Snapshot().pipboy.dialogueVariables.count(chairState)?listened.Snapshot().pipboy.dialogueVariables.at(chairState):0)<<std::endl;
        assert(closing);
        assert(listened.Snapshot().pipboy.dialogueVariables.at(chairState)==1);
      }
    }
    assert(listened.Snapshot().pipboy.dialogueVariables.at(chairState)==2);
    std::cout<<"Original pod chair marker, close animation, and 14-second countdown reached the simulation handoff\n";
    const auto simulation=id("mq04");
    const auto simScript=catalog.pipboy.quests.at(simulation).script;
    listened.SetScriptEventDiagnostic([&](const std::string& diagnostic){
      if(diagnostic.find("SCPT "+std::to_string(simScript)+" ")!=std::string::npos)std::cerr<<diagnostic<<std::endl;
    });
    auto runSimulation=[&](int tick){
      listened.SetScriptRuntimeClock(17+tick*.25,.25);
      listened.PumpWorldAwareGameMode(.25,{},[&](auto plan){
        fo3script::ScriptMasterMatch match;std::atomic<bool> cancel{false};
        if(!fo3script::MatchScriptProgramsToMaster(path,plan->ExecutedScripts(),match,cancel,error)){
          std::cerr<<"Vault 112 world publication: "<<error<<std::endl;return false;
        }
        return listened.CommitScriptTransaction(*plan);
      });
    };
    for(int tick=0;tick<4&&!listened.Snapshot().playerMove.pending;++tick)runSimulation(tick);
    const auto entry=listened.Snapshot().playerMove;
    assert(entry.pending&&entry.marker==id("mq04startbench")&&entry.world==id("tranquilitylane"));
    assert(listened.Snapshot().pipboy.dialogueVariables.at(suitOn)==0);
    const auto outfit=id("mq04playeroutfit");
    bool wearing=false;assert(listened.ScriptEquippedItem(fo3player::PlayerRef,outfit,wearing)&&wearing);
    listened.SetFurnitureUse(0,0,0);
    listened.SetScriptPlayerLocation(entry.position[0],entry.position[1],entry.position[2],entry.cell,entry.world);
    listened.AcknowledgePlayerMove();
    for(int tick=4;tick<48;++tick)runSimulation(tick);
    assert(listened.Snapshot().pipboy.quests.at(pursuits).stage==200&&listened.Snapshot().pipboy.quests.at(simulation).stage==5);
    std::cout<<"Original simulation entry equipped the outfit, completed Scientific Pursuits and started Tranquility Lane\n";
    TranquilityFailsafe(listened,path,meshRoot,save+".failsafe");
    TranquilityOpening(listened, save + ".mq04",path,meshRoot);
    // Returning belongings from the same original storage is independently exercised.
    fo3pipdata::ResultScript transfer;
    const auto locker=id("mq04playercontainerref");
    auto stored=listened.ContainerContents(locker);
    assert(stored&&!stored->empty());
    const auto belongings=*stored;
    assert(listened.Save(save,error));
    fo3player::Player returning(catalog);assert(returning.Restore(save,error));unlink(save.c_str());
    transfer.source="MQ04PlayerContainerRef.RemoveAllItems Player";
    assert(returning.ExecuteDialogueResult(transfer,error)&&returning.ContainerContents(locker)->empty());
    assert(returning.Snapshot().pipboy.quests.at(pursuits).stage==200&&returning.Snapshot().pipboy.quests.at(simulation).stage==5);
    const auto& returned=returning.Snapshot().inventory;
    for(const auto& stack:belongings){
      const auto it=std::find_if(returned.begin(),returned.end(),[&](const auto& current){return current.id==stack.id;});
      assert(it!=returned.end()&&it->formId==stack.formId&&it->count==stack.count&&it->condition==stack.condition&&
             it->loadedRounds==stack.loadedRounds&&it->needsAction==stack.needsAction&&!it->equipped);
    }
    std::cout<<"Vault 112 storage preserved inventory identities, condition and weapon state through save and return\n";
    std::cout<<"Vault 112 robot supplied the suit, shell checked equipped gear, and chair delegated native seating\n";
  }

}
int main(int argc,char** argv){if(argc==4)robotSecuritySave=argv[3];if(argc>1)Original(argv[1],argc>2?argv[2]:"");else {std::cout<<"Original MQ01 checkpoint requires Fallout3.esm; no original-data verification performed\n";return 77;}}
