#include "npc/fo3-npc-combat.h"
#include "mq01_test_conversation.h"
#include "pipboy/fo3-quest-route.h"
#include "dialogue/fo3-dialogue-session.h"
#include "dialogue/fo3-dialogue-selection.h"
#include "scripting/fo3-script-master-match.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include "physics/fo3-nif-trigger.h"
#include <unistd.h>

static void Check(bool ok,const std::string& error){if(!ok)std::cerr<<error<<std::endl;assert(ok);}
static void Original(const char* path,const std::string& meshRoot){
  fo3player::TriggerModelLoader loader;
  if(!meshRoot.empty())loader=[&](const std::string& model,fo3pipdata::TriggerPrimitive& shape){
    auto file=fo3script::Lower(model);std::replace(file.begin(),file.end(),'\\','/');
    std::ifstream stream(meshRoot+"/"+file,std::ios::binary);if(!stream)return false;
    std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(stream),{}};
    return fo3trigger::DecodePhantom(bytes,shape);
  };
  fo3player::Catalog c;std::string error;Check(fo3player::LoadCatalog(path,c,error,loader),error);
  const auto id=[&](const char* name){return c.pipboy.formNames.at(name);};
  const auto mq=id("mq01"),ld=id("mq01ld"),escape=id("cg04"),vaultDoor=id("vault101cavedoorinterior");
  // Start at the preceding quest's final door. MQ01 itself has no seeded stages.
  auto& prior=c.initial.pipboy.quests[escape];
  prior.stage=145;prior.running=true;prior.stages={0,4,10,33,120,145};
  c.initial.pipboy.selectedQuest=escape;c.initial.inCharGen=true;
  fo3player::Player player(c);player.RegisterOriginalActors();
  const auto& vault=c.pipboy.targets.at(vaultDoor);
  player.SetScriptPlayerLocation(vault.x,vault.y,vault.z,vault.cell,vault.world);
  auto openVault=player.PrepareActivationTransaction(vaultDoor,fo3player::PlayerRef,error);
  Check(bool(openVault),error);Check(player.CommitScriptTransaction(*openVault),"vault activation");
  double time=0;
  auto vaultTick=[&](){
    time+=.1;player.SetScriptRuntimeClock(time,.1);
    player.PumpWorldAwareGameMode(.1,{vaultDoor},[&](auto plan){return player.CommitScriptTransaction(*plan);});
  };
  fo3player::ScriptMessage message;
  for(unsigned i=0;i<30&&!message.form;++i){vaultTick();player.PollScriptMessage(message);}
  assert(message.form==id("cg04vaultexitmessage")&&player.SubmitScriptMessageButton(vaultDoor,4));
  for(unsigned i=0;i<30&&player.Snapshot().pipboy.quests.at(escape).stage<150;++i)vaultTick();
  const auto& outside=c.pipboy.targets.at(vault.teleportRef);
  player.SetScriptPlayerLocation(outside.x,outside.y,outside.z,outside.cell,outside.world);
  for(unsigned i=0;i<200&&!player.Snapshot().pipboy.quests.count(mq);++i)vaultTick();
  assert(player.Snapshot().pipboy.quests.at(mq).stage==10&&player.Snapshot().pipboy.selectedQuest==mq);
  // Supply the fare for Moriarty's paid lead, then follow his original INFO chain.
  assert(player.Add(0xf,100));
  {
    Conversation moriarty(player,id("colinmoriartyref"));moriarty.Start();
    for(auto topic:{0x3b7cu,0x14f3eu,0x151b6u})moriarty.Choose(topic);
    assert(player.Snapshot().pipboy.quests.at(mq).stage==40);
  }
  uint32_t trigger=0;const auto script=c.pipboy.formNames.at("mq01cityambushscript");
  for(const auto& ref:c.references){const auto owner=c.eventBaseScripts.find(ref.second.base);if(owner!=c.eventBaseScripts.end()&&owner->second==script){assert(!trigger);trigger=ref.first;}}
  assert(trigger);const auto& placed=c.pipboy.targets.at(trigger);
  player.SetScriptPlayerLocation(placed.x,placed.y,placed.z,placed.cell,placed.world);
  const auto changes=player.ScriptTriggerChanges();assert(std::find(changes.begin(),changes.end(),std::make_pair(trigger,true))!=changes.end());
  Check(player.DispatchReferenceEvent(trigger,"OnTriggerEnter",fo3player::PlayerRef,error)==fo3player::ScriptEventResult::Executed,error);
  player.AcknowledgeScriptTrigger(trigger,true);assert(player.Snapshot().pipboy.quests.at(ld).stage==45);
  const auto faction=c.pipboy.formNames.at("mq01brotherhooddcfaction");
  const auto sarah=c.pipboy.formNames.at("mq01sarahref"),mutant=c.pipboy.formNames.at("mq01citysupermutant01ref");
  assert(player.ActorFactionRank(sarah,faction)==-1);
  // The ambush timer must survive leaving the volume, a save/reload and a
  // missing render object. No trigger identity is injected into the scheduler.
  const float escapedX=placed.x+3200;
  player.SetScriptPlayerLocation(escapedX,placed.y,placed.z,placed.cell,placed.world);
  const auto left=player.ScriptTriggerChanges();
  assert(std::find(left.begin(),left.end(),std::make_pair(trigger,false))!=left.end());
  player.AcknowledgeScriptTrigger(trigger,false);
  auto tickScene=[](fo3player::Player& p){
    std::vector<uint32_t> refs;p.AppendResidentGameModeReferences(refs);
    p.PumpWorldAwareGameMode(.25,refs,[&](auto plan){return p.CommitScriptTransaction(*plan);});
  };
  for(int i=0;i<8;++i)tickScene(player);
  auto save="/tmp/fq-mq01-scene-"+std::to_string(getpid());Check(player.Save(save,error),error);
  fo3player::Player resumed(c);Check(resumed.Restore(save,error),error);resumed.SetScriptPlayerLocation(escapedX,placed.y,placed.z,placed.cell,placed.world);
  assert(resumed.Snapshot().pipboy.quests.at(ld).stage==45);
  for(int i=0;i<16;++i)tickScene(resumed);
  assert(resumed.Snapshot().pipboy.quests.at(ld).stage==50&&resumed.Snapshot().pipboy.quests.at(mq).stage==50);
  for(const auto* name:{"mq01sarahref","mq01vargasref","mq01reddinref","mq01colvinref"}){
    const auto actor=c.pipboy.formNames.at(name);float aggression=-1;assert(resumed.ActorFactionRank(actor,faction)==0&&resumed.ActorAIValue(actor,0,aggression)&&aggression==1);
  }
  assert(fo3npc::Relationship(resumed,sarah,mutant)==fo3npc::Reaction::Enemy);
  Check(resumed.Save(save,error),error);fo3player::Player returned(c);Check(returned.Restore(save,error),error);
  assert(returned.ActorFactionRank(sarah,faction)==0&&fo3npc::Relationship(returned,sarah,mutant)==fo3npc::Reaction::Enemy);
  const auto before=returned.Snapshot().actorFactions;
  Check(returned.DispatchReferenceEvent(trigger,"OnTriggerEnter",fo3player::PlayerRef,error)==fo3player::ScriptEventResult::Executed,error);
  for(int i=0;i<24;++i)returned.PumpReferenceGameMode(.25,{trigger});
  assert(returned.Snapshot().actorFactions==before&&returned.Snapshot().pipboy.quests.at(ld).stage==50);
  // Actual deaths run the shared mutant OnDeath script. The second starts
  // the quest's timed conversation; the GameMode script chooses its listener.
  {
    fo3player::Player battle(returned);
    battle.SetScriptPlayerLocation(placed.x,placed.y,placed.z,placed.cell,placed.world);
    const auto other=c.pipboy.formNames.at("mq01citysupermutant02ref");
    const auto pistol=c.pipboy.formNames.at("weap10mmpistol");
    for(const auto actor:{mutant,other}){
      Check(battle.ApplyAttack(fo3player::PlayerRef,pistol,actor,10000),"ambush mutant hit");
      assert(battle.ActorHealth(actor)==0);
    }
    Check(battle.Snapshot().pipboy.quests.at(mq).stage==55,"second ambush death should start Sarah's scene");
    const auto speaker=c.pipboy.formNames.at("mq01vargasref"),listener=sarah;
    const auto topic=c.pipboy.formNames.at("mq01lyonspridevargas");
    std::string diagnostic;battle.SetScriptEventDiagnostic([&](const std::string& msg){if(msg.find(std::to_string(c.pipboy.quests.at(mq).script))!=std::string::npos)diagnostic=msg;});
    const auto key=fo3dialogue::VariableKey(c.pipboy,"mq01.targetref");assert(key);
    auto finishLine=[&](const fo3script::Action& action){
      fo3dialogue::Context ctx;ctx.player=&battle;ctx.talking=true;
      Check(fo3dialogue::ReferenceContext(battle,action.reference,ctx.speaker)&&
            fo3dialogue::ReferenceContext(battle,action.flags,ctx.target),"scene speaker and listener");
      fo3dialogue::Session speech;
      Check(speech.StartSelectedTopic(action.form,action.info,ctx,battle),"start authored scene INFO");
      unsigned responses=0;
      while(speech.phase==fo3dialogue::Phase::Speaking){
        assert(++responses<32);speech.AudioDone(speech.audioToken,true,ctx,battle);
      }
      Check(speech.infoCompleted,"scene INFO completion");
      if(battle.HasReferenceEvent(action.reference,"SayToDone")){
        auto done=battle.PrepareReferenceEventTransaction(action.reference,"SayToDone",action.flags,error,
            fo3player::ScriptProgramCapture::EnteredPrograms,action.form);
        Check(bool(done),error);Check(battle.CommitScriptTransaction(*done),error);
      }
    };
    unsigned spoken=0;
    for(unsigned n=0;n<500&&battle.Snapshot().pipboy.quests.at(mq).stage==55;++n){
      battle.SetScriptRuntimeClock(.1*n,.1);
      battle.PumpWorldAwareGameMode(.1,{},[&](auto plan){return battle.CommitScriptTransaction(*plan);});
      fo3script::Action action;
      while(battle.PollScriptAction(action)){
        if(action.kind!=fo3script::ActionKind::SayTo)continue;
        if(!spoken)assert(action.reference==speaker&&action.form==topic&&action.flags==listener&&
                          action.forceSubtitle&&!action.noTargetLook&&action.info==0x1ecaf);
        ++spoken;finishLine(action);
        if(spoken==1){
          assert(battle.Snapshot().pipboy.referenceVariables.at(key)==listener);
          Check(battle.Save(save,error),error);Check(battle.Restore(save,error),error);
          assert(battle.Snapshot().pipboy.referenceVariables.at(key)==listener);
          battle.SetScriptPlayerLocation(placed.x,placed.y,placed.z,placed.cell,placed.world);
        }
      }
    }
    Check(spoken>=3&&battle.Snapshot().pipboy.quests.at(mq).stage==56,"Brotherhood exchange stalled after "+std::to_string(spoken)+" lines: "+diagnostic);
    const auto greet=c.pipboy.formNames.at("mq01sarahsceneagreetplayer");
    const auto& package=c.pipboy.packages.at(greet);
    const auto& marker=c.pipboy.targets.at(package.location.value);
    battle.SetScriptPlayerLocation(marker.x+20,marker.y,marker.z,marker.cell,marker.world);
    fo3dialogue::Context greeting;greeting.player=&battle;
    assert(fo3dialogue::ReferenceContext(battle,sarah,greeting.speaker));
    assert(fo3dialogue::ReferenceContext(battle,fo3player::PlayerRef,greeting.target));
    greeting.speaker.packageKnown=true;greeting.speaker.package=greet;
    greeting.speaker.x=marker.x;greeting.speaker.y=marker.y;greeting.speaker.z=marker.z;
    Check(fo3dialogue::Conditions(package.conditions,greeting,error),error);
    fo3dialogue::Session conversation;
    Check(conversation.CanActivate(greeting),"manual Talk should be available before the MQ11 briefing");
    Check(conversation.Start(greeting,battle,fo3dialogue::GreetingInitiator::Actor),"start Sarah's package greeting");
    assert(conversation.info==0x228a0);
    unsigned responseCount=0;
    while(conversation.phase==fo3dialogue::Phase::Speaking){
      assert(++responseCount<32);conversation.AudioDone(conversation.audioToken,true,greeting,battle);
    }
    assert(conversation.infoCompleted&&conversation.choices.size()==3&&battle.Snapshot().pipboy.quests.at(mq).stage==60);
    auto enterVolume=[&](const char* scriptName,uint32_t activator){
      const auto sc=c.pipboy.formNames.at(scriptName);
      uint32_t ref=0;
      for(const auto& item:c.references){auto association=c.eventBaseScripts.find(item.second.base);
        if(association!=c.eventBaseScripts.end()&&association->second==sc){
          assert(!ref);
          ref=item.first;
        }
      }
      assert(ref);const auto& pos=c.pipboy.targets.at(ref);
      std::vector<std::pair<uint32_t,bool>> crossed;
      if(activator==fo3player::PlayerRef){
        battle.SetScriptPlayerLocation(pos.x,pos.y,pos.z,pos.cell,pos.world);
        crossed=battle.ScriptTriggerChanges();
      }else crossed=battle.ScriptActorTriggerChanges(activator,pos.cell,pos.world,{pos.x,pos.y,pos.z});
      assert(std::find(crossed.begin(),crossed.end(),std::make_pair(ref,true))!=crossed.end());
      auto plan=battle.PrepareReferenceEventTransaction(ref,"OnTriggerEnter",activator,error,fo3player::ScriptProgramCapture::EnteredPrograms);
      Check(bool(plan),std::string(scriptName)+" "+error);
      fo3script::ScriptMasterMatch match;std::atomic<bool> cancel{false};
      Check(fo3script::MatchScriptProgramsToMaster(path,plan->ExecutedScripts(),match,cancel,error),error);
      Check(battle.CommitScriptTransaction(*plan),"commit trigger");
      battle.AcknowledgeScriptTrigger(ref,true,activator);
    };
    // Supply actor/player positions and combat hits. The original trigger
    // geometry, event filters, conditions, results and stage handoffs run.
    enterVolume("mq01ldstagetrig65script",fo3player::PlayerRef);
    enterVolume("mq01ldstagetrig65script",sarah);
    assert(battle.Snapshot().pipboy.quests.at(mq).stage==70&&battle.Snapshot().pipboy.quests.at(ld).stage==65);
    enterVolume("mq01ldstagetrig71script",fo3player::PlayerRef);
    assert(battle.Snapshot().pipboy.quests.at(ld).stage==71);
    if(loader){
      enterVolume("mq01ldstagetrig75script",fo3player::PlayerRef);
      assert(battle.Snapshot().pipboy.quests.at(ld).stage==75);
      for(const char* name:{"mq01schoolmutant01","mq01schoolmutant02","mq01schoolmutant03",
                            "mq01schoolmutant04","mq01schoolmutant05","mq01schoolmutant06"}){
        const auto ref=c.pipboy.formNames.at(name);bool disabled=true;
        assert(battle.ReferenceDisabled(ref,disabled,error)&&!disabled);
        Check(battle.RealizeActor(ref),name);
        Check(battle.ApplyAttack(fo3player::PlayerRef,pistol,ref,10000),name);
        assert(battle.ActorHealth(ref)==0);
      }
      assert(battle.Snapshot().pipboy.quests.at(ld).stage==90&&battle.Snapshot().pipboy.quests.at(mq).stage==80);
    }
    enterVolume("mq01gnnexplosions01script",fo3player::PlayerRef);
    for(const char* name:{"mq01gnrmutant01","mq01gnrmutant02","mq01gnrmutant03","mq01gnrmutant04"}){
      auto ref=c.pipboy.formNames.at(name);bool disabled=true;assert(battle.ReferenceDisabled(ref,disabled,error)&&!disabled);
      Check(battle.RealizeActor(ref),std::string("realize newly enabled actor ")+name);
      Check(battle.ApplyAttack(fo3player::PlayerRef,pistol,ref,10000),std::string("GNR mutant hit ")+name+" health="+std::to_string(battle.ActorHealth(ref)));
    }
    assert(battle.Snapshot().pipboy.quests.at(mq).stage==100);
    unsigned plazaLines=0;
    for(unsigned tick=1;tick<=600&&battle.Snapshot().pipboy.quests.at(mq).stage<102;++tick){
      battle.SetScriptRuntimeClock(100.+tick*.1,.1);
      battle.PumpWorldAwareGameMode(.1,{},[&](auto plan){return battle.CommitScriptTransaction(*plan);});
      fo3script::Action action;
      while(battle.PollScriptAction(action))if(action.kind==fo3script::ActionKind::SayTo){
        finishLine(action);++plazaLines;
      }
    }
    assert(plazaLines==7&&battle.Snapshot().pipboy.quests.at(mq).stage==102);
    const auto behemoth=c.pipboy.formNames.at("mq01behemothref");
    const auto bus=c.pipboy.formNames.at("gnrbus01"),otherBus=c.pipboy.formNames.at("gnrbus02");
    const auto mark=c.pipboy.targets.at(c.pipboy.formNames.at("mq01behemotharrivemarker"));
    battle.SetScriptPlayerLocation(mark.x+1200,mark.y,mark.z,mark.cell,mark.world);
    assert(battle.Snapshot().actors.count(behemoth));
    const auto step=fo3dialogue::VariableKey(c.pipboy,"MQ01BehemothRef.entryStage");assert(step);
    std::vector<std::pair<uint32_t,uint32_t>> explosions;
    for(unsigned tick=0;tick<1200;++tick){
      battle.SetScriptRuntimeClock(200.+tick*.1,.1);
      std::vector<uint32_t> refs;battle.AppendResidentGameModeReferences(refs);
      battle.PumpWorldAwareGameMode(.1,refs,[&](auto plan){return battle.CommitScriptTransaction(*plan);});
      fo3script::Action action;
      while(battle.PollScriptAction(action)){
        if(action.kind==fo3script::ActionKind::Explosion)explosions.emplace_back(action.reference,action.form);
        if(action.kind==fo3script::ActionKind::SayTo)finishLine(action);
      }
      const auto value=battle.Snapshot().pipboy.dialogueVariables.find(step);
      if(value!=battle.Snapshot().pipboy.dialogueVariables.end()&&value->second>=3&&battle.Snapshot().pipboy.quests.at(mq).stage>=120)break;
    }
    Check(battle.Snapshot().pipboy.quests.at(mq).stage==120&&explosions.size()==3,
      "Behemoth entrance stopped at MQ01 "+std::to_string(battle.Snapshot().pipboy.quests.at(mq).stage)+
      ", MQ01LD "+std::to_string(battle.Snapshot().pipboy.quests.at(ld).stage)+
      ", explosions "+std::to_string(explosions.size())+" "+diagnostic);
    assert(explosions[0].second==0x19f52);
    for(const auto ref:{bus,otherBus}){
      assert(std::count(explosions.begin(),explosions.end(),std::make_pair(ref,0x312feu))==1);
      assert(battle.Snapshot().objectDamage.at(ref)==2500);
      assert(battle.ObjectDamageStage(ref)&&battle.ObjectDamageStage(ref)->model==R"(Vehicles\BusHulk01.NIF)");
    }
    Check(battle.Save(save,error),error);Check(battle.Restore(save,error),error);
    assert(battle.ObjectDamageStage(bus)&&battle.ObjectDamageStage(otherBus));
    Check(battle.ApplyAttack(fo3player::PlayerRef,pistol,behemoth,100000),"Behemoth ending hit");
    assert(battle.ActorHealth(behemoth)==0&&battle.Snapshot().pipboy.quests.at(mq).stage==140);
    const auto door=c.pipboy.formNames.at("chevychasetognr01ref"),intercomBase=c.pipboy.formNames.at("gnrdoorintercom");
    const auto intercom=std::find_if(c.references.begin(),c.references.end(),[&](const auto& ref){return ref.second.base==intercomBase;});
    assert(intercom!=c.references.end()&&battle.GetLock(door).locked);
    const auto& at=c.pipboy.targets.at(intercom->first);
    battle.SetScriptPlayerLocation(at.x,at.y,at.z,at.cell,at.world);
    auto activation=battle.PrepareActivationTransaction(intercom->first,fo3player::PlayerRef,error);
    Check(bool(activation),"intercom activation: "+error);Check(battle.CommitScriptTransaction(*activation),"intercom publication");
    bool unlocked=false;fo3script::Action reply;
    while(battle.PollScriptAction(reply)){
      if(reply.kind!=fo3script::ActionKind::SayTo)continue;
      Check(reply.reference==intercom->first&&reply.info==0x6ec5d,"GNR unlock greeting");
      finishLine(reply);unlocked=true;
    }
    assert(unlocked&&battle.CanOpenDoor(door));
    const auto& threshold=c.doorTeleports.at(door);
    assert(threshold.valid&&threshold.destinationDoorRefFormId);
    const auto& inside=c.pipboy.targets.at(threshold.destinationDoorRefFormId);
    battle.SetScriptPlayerLocation(threshold.x,threshold.y,threshold.z,inside.cell,inside.world);
    const auto dog=c.pipboy.formNames.at("threedogref");
    fo3questroute::Navigator navigation;
    for(unsigned hop=0;hop<12;++hop){
      fo3pipdata::Location from;
      Check(battle.ScriptReferenceLocation(fo3player::PlayerRef,from),"GNR interior location");
      const auto route=navigation.Toward(battle,from,dog);
      Check(route.valid,"GNR doors must connect the lobby to the studio");
      if(!route.door)break;
      Check(battle.CanOpenDoor(route.door),"GNR interior door");
      const auto& link=c.doorTeleports.at(route.door);
      const auto& arrival=c.pipboy.targets.at(link.destinationDoorRefFormId);
      battle.SetScriptPlayerLocation(link.x,link.y,link.z,arrival.cell,arrival.world);
    }
    fo3pipdata::Location lobbyDestination;
    assert(battle.ScriptReferenceLocation(fo3player::PlayerRef,lobbyDestination)&&lobbyDestination.cell==c.pipboy.targets.at(dog).cell);
    Conversation studio(battle,dog);studio.Start();
    for(auto topic:{0x24170u,0x3e5a0u,0x2416cu,0x24164u,0x24162u,0x2415fu,0x26184u,0x3e57fu})studio.Choose(topic);
    const auto& ending=battle.Snapshot().pipboy.quests.at(mq);
    assert(ending.stage==200&&ending.status==fo3pipdata::Completion::Complete&&!ending.running);
    assert(battle.Snapshot().pipboy.quests.at(c.pipboy.formNames.at("mq02")).stage==10);
    Check(battle.Save(save,error),error);Check(battle.Restore(save,error),error);
    assert(battle.CanOpenDoor(door)&&battle.ObjectDamageStage(bus)&&battle.ItemCount(c.pipboy.formNames.at("mq02virgonote"))==1);
    std::cout<<"GNR ambush, Behemoth, persistent wrecks, working intercom, unlocked entrance and Three Dog handoff passed\n";
    std::cout<<"Original plaza exchange completed all seven lines at MQ01 stage 102\n";
    std::cout<<"Original checkpoint, schoolhouse and plaza triggers advanced to the four GNR deaths at MQ01 stage 100\n";
    std::cout<<"Original Sarah package greeting opened three replies and advanced MQ01 to stage 60\n";
    std::cout<<"Original Chevy Chase ambush deaths, "<<spoken<<" Brotherhood speech actions and stage 56 after mid-exchange reload passed\n";
  }
  // Original Behemoth weapon filter, through canonical attack/damage dispatch.
  // This isolates receipt of a hit; it does not simulate its scripted entrance,
  // explosion projectiles, bus destruction, or the full GNR battle.
  const auto behemoth=c.pipboy.formNames.at("mq01behemothref"),fatman=c.pipboy.formNames.at("weapfatman");
  const auto counter=fo3dialogue::VariableKey(c.pipboy,"MQ01BehemothRef.nukeCount");assert(counter);
  {
    auto captured=returned.PrepareReferenceEventTransaction(behemoth,"OnHitWith",fo3player::PlayerRef,error,fo3player::ScriptProgramCapture::EnteredPrograms,fatman);Check(bool(captured),error);
    fo3script::ScriptMasterMatch match;std::atomic<bool> cancelled{false};
    Check(fo3script::MatchScriptProgramsToMaster(path,captured->ExecutedScripts(),match,cancelled,error),error);
    assert(match.Matches(captured->ExecutedScripts()));
    auto wrong=captured->ExecutedScripts();wrong.front().origin.filterForm=c.pipboy.formNames.at("weap10mmpistol");
    assert(!match.Matches(wrong)&&!fo3script::MatchScriptProgramsToMaster(path,wrong,match,cancelled,error));
    assert(match.Matches(captured->ExecutedScripts()));
    auto arrived=returned.PreparePackageEventTransaction(c.pipboy.formNames.at("mq01colvinref"),c.pipboy.formNames.at("mq01colvinadvancefromcheckpoint"),true,error,fo3player::ScriptProgramCapture::EnteredPrograms);
    Check(bool(arrived),error);Check(fo3script::MatchScriptProgramsToMaster(path,arrived->ExecutedScripts(),match,cancelled,error),error);
    assert(returned.Snapshot().pipboy.quests.at(mq).stage==50); // preview did not publish a scene advance

  }

  assert(returned.ApplyAttack(fo3player::PlayerRef,c.pipboy.formNames.at("weap10mmpistol"),behemoth,1));
  assert(!returned.Snapshot().pipboy.dialogueVariables.count(counter));
  assert(returned.ApplyAttack(fo3player::PlayerRef,fatman,behemoth,1));assert(returned.Snapshot().pipboy.dialogueVariables.at(counter)==1);
  Check(returned.Save(save,error),error);fo3player::Player hitSaved(c);Check(hitSaved.Restore(save,error),error);unlink(save.c_str());
  assert(hitSaved.Snapshot().pipboy.dialogueVariables.at(counter)==1);
  // Exercise both idle-admission outcomes independently of the combat replay.
  const auto idle=c.pipboy.formNames.at("bmtstretch");
  const auto step=fo3dialogue::VariableKey(c.pipboy,"MQ01BehemothRef.entryStage");assert(step);
  for(bool alreadyPlaying:{false,true}){
    fo3player::Player entrance(c);entrance.RegisterOriginalActors();
    auto enable=entrance.PrepareReferenceEnablePlan({{behemoth,true}},error);Check(bool(enable),error);
    assert(entrance.CommitReferenceEnablePlan(*enable));
    auto detonation=entrance.PrepareQuestStageTransaction(ld,105,error);
    Check(bool(detonation),error);Check(entrance.CommitScriptTransaction(*detonation),"stage 105 publication");
    if(alreadyPlaying)entrance.SetScriptPlayingIdles({behemoth});
    for(unsigned tick=0;tick<20&&!entrance.Snapshot().pipboy.dialogueVariables.count(step);++tick){
      entrance.SetScriptRuntimeClock(tick*.1,.1);
      auto plan=entrance.PrepareReferenceEventTransaction(behemoth,"GameMode",0,error,fo3player::ScriptProgramCapture::EnteredPrograms);
      Check(bool(plan),error);
      if(!tick){
        fo3script::ScriptMasterMatch match;std::atomic<bool> cancelled{false};
        Check(fo3script::MatchScriptProgramsToMaster(path,plan->ExecutedScripts(),match,cancelled,error),error);
      }
      assert(entrance.CommitScriptTransaction(*plan));
    }
    assert(entrance.Snapshot().pipboy.dialogueVariables.at(step)==1);
    fo3script::Action action;unsigned count=0;
    while(entrance.PollScriptAction(action))if(action.kind==fo3script::ActionKind::PlayIdle){
      assert(action.reference==behemoth&&action.form==idle);++count;
    }
    assert(count==(alreadyPlaying?0u:1u));
  }
  std::cout<<"Original Behemoth selects BMTStretch once and skips it during an existing idle\n";
  std::cout<<"Original MQ01 ambush trigger -> five-second timer with mid-timer reload -> authored faction/aggression/objective changes; replay protection and Behemoth weapon filter passed. World traversal/battle not simulated.\n";
}
int main(int argc,char** argv){if(argc<2)return 77;Original(argv[1],argc>2?argv[2]:"");}
