#include "dialogue/fo3-dialogue-session.h"
#include "scripting/fo3-nonresident-script.h"
#include <cassert>
#include <iostream>
#include <unistd.h>
#include <fstream>
#include <iterator>
#include <zlib.h>
using namespace fo3dialogue;
static void Finish(Session& s,Context& ctx,fo3player::Player& p){
  while(s.phase==Phase::Speaking){const auto token=s.audioToken;s.AudioDone(token,true,ctx,p);}
}
static void Original(const char* path){
  fo3player::Catalog c;std::string error;assert(fo3player::LoadCatalog(path,c,error));
  c.initial.skills[size_t(fo3player::Skill::Explosives)]=25;
  const auto bomb=c.pipboy.targets.at(0x14bc8);
  for(int bonus=0;bonus<=2;++bonus){
    fo3player::Player p(c);p.RegisterOriginalActors();
    p.SetScriptPlayerLocation(bomb.x,bomb.y,bomb.z,bomb.cell,bomb.world);
    Context ctx;ctx.player=&p;ctx.speaker={0x3b46,0xa60,bomb.cell,bomb.world};ctx.target={0x14,7,bomb.cell,bomb.world};
    Session s;s.diagnostic=[](const std::string& m){if(m.find("UNSUPPORTED SCRIPT")!=std::string::npos)std::cerr<<m<<'\n';};
    // Original Lucas introduction and acceptance path, including authored variables.
    assert(s.Start(ctx,p));Finish(s,ctx,p);
    auto choose=[&](uint32_t topic){
      auto i=std::find_if(s.choices.begin(),s.choices.end(),[&](const Choice& v){return v.topic==topic;});
      assert(i!=s.choices.end());assert(s.Choose(size_t(i-s.choices.begin()),ctx,p));Finish(s,ctx,p);
    };
    choose(0x3b80);assert(!s.choices.empty());choose(s.choices.front().topic);choose(0x3b77);choose(0x1e35d);choose(0x3d9e0);choose(0x3d9dd);choose(0x1e355);
    s.End("return to bomb");
    assert(p.Snapshot().pipboy.quests.at(0x14e9e).stage==10);
    // Branch variable isolates the three authored rewards, not a fabricated grant.
    assert(p.SetDialogueVariable(VariableKey(c.pipboy,"ms11.simmsbonus"),float(bonus)));
    assert(s.Start(ctx,p));assert(s.info!=0x1501d&&s.info!=0x95aa4&&s.info!=0x95aa5);s.End("before disarm");
    fo3player::ScriptMessage message;
    assert(p.DispatchReferenceEvent(0x14bc8,"OnActivate",0x14,error)==fo3player::ScriptEventResult::Executed);
    assert(p.PollScriptMessage(message)&&message.form==0x78cb2);
    assert(p.SubmitScriptMessageButton(0x14bc8,1));p.PumpReferenceGameMode(.25,{0x14bc8});
    assert(p.Snapshot().pipboy.quests.at(0x14e9e).stage==30);
    assert(p.Snapshot().pipboy.dialogueVariables.at(VariableKey(c.pipboy,"ms11.disarm"))==2);
    if(bonus==2){ // Exercise the original conditional off-scene removal branch.
      auto burke=p.Snapshot().actors.at(0x14f77);const auto gob=p.Snapshot().actors.at(0x3b3d);
      burke.cell=gob.cell;burke.world=gob.world;burke.position=gob.position;
      assert(p.UpdateActor(0x14f77,burke));
    }
    std::vector<uint32_t> houseDoors;
    for(const auto& ref:c.references)if(ref.second.key==0x151e4){
      houseDoors.push_back(ref.first);assert(!p.CanOpenDoor(ref.first));
    }
    assert(!houseDoors.empty());
    const int caps=p.ItemCount(0xf);const uint32_t xp=p.Snapshot().experience;
    const auto before=p.Revision();
    // The bounded host publication policy mirrors the production nonresident bridge.
    s.previewReferences=[&](const fo3player::Player& player,const fo3player::ScriptTransactionPlan& plan,std::string& e){
      return fo3script::NonresidentActorRemoval(player,plan,bomb.cell,bomb.world,{0x3b46},e);
    };
    s.publishReferences=[&](fo3player::Player& player,fo3player::ScriptTransactionPlan& plan,std::string& e){
      return s.previewReferences(player,plan,e)&&player.CommitScriptTransaction(plan);
    };
    assert(s.CanStart(ctx));assert(p.Revision()==before&&p.ItemCount(0xf)==caps&&p.Snapshot().experience==xp);
    assert(s.Start(ctx,p));assert(s.info==uint32_t(bonus?0x95aa4:0x95aa5));
    assert(s.Subtitle(c.pipboy).find("You disarmed that thing!")!=std::string::npos);
    assert(s.Audio(c.pipboy).find("@voice:MaleUniqueSimms:")==0);
    // Begin results do not grant rewards before the last authored response ends.
    assert(p.ItemCount(0xf)==caps&&p.Snapshot().experience==xp);
    Finish(s,ctx,p);
    const auto& q=p.Snapshot().pipboy.quests.at(0x14e9e);
    assert(q.stage==210&&q.stages.count(30)&&q.stages.count(210)&&!q.running&&q.status==fo3pipdata::Completion::Complete);
    for(const auto& objective:q.objectives)assert(objective.second.status==fo3pipdata::Completion::Complete);
    assert(p.ItemCount(0xf)==caps+(bonus==1?100:bonus==2?500:0));
    assert(p.Snapshot().experience==xp+300&&p.ItemCount(0x151e4)==1&&p.ItemCount(0xc402b)==1);
    assert(p.CellFullName(0x151e3)=="My Megaton House");
    for(auto door:houseDoors)assert(p.CanOpenDoor(door));
    assert(!c.cellOwnership.at(0x151e3).data.explicitOwner); // Source cell has no ownership rewrite.
    assert(p.Snapshot().pipboy.quests.at(0x40691).running&&p.Snapshot().pipboy.quests.at(0x7f47a).running);
    assert(p.Snapshot().actorDispositionModifiers.at(0x3b46)==75&&p.Snapshot().achievements.count(23));
    bool sameCell=false;assert(p.ScriptInSameCell(0x14f77,sameCell,error));
    bool disabled=false;assert(p.ReferenceDisabled(0x14f77,disabled,error)&&disabled==!sameCell);
    const auto save="/tmp/fq-ms11-completion-"+std::to_string(getpid());assert(p.Save(save,error));
    fo3player::Player restored(c);assert(restored.Restore(save,error));std::remove(save.c_str());
    assert(restored.ItemCount(0xf)==p.ItemCount(0xf)&&restored.Snapshot().experience==xp+300);
    assert(restored.CellFullName(0x151e3)=="My Megaton House"&&restored.ItemCount(0x151e4)==1&&restored.ItemCount(0xc402b)==1);
    assert(restored.Snapshot().achievements.count(23)&&restored.Snapshot().actorDispositionModifiers.at(0x3b46)==75);
    assert(restored.ReferenceDisabled(0x14f77,disabled,error)&&disabled==!sameCell);
    // Stage replay is a no-op even after restart; said-once and stopped quest gate greetings.
    assert(restored.ExecuteQuestStage(0x14e9e,210,error));assert(restored.Snapshot().experience==xp+300&&restored.ItemCount(0xf)==p.ItemCount(0xf));
    fo3player::QuestNotice repeatedNotice;fo3player::ScriptItemNotice repeatedItem;
    assert(!restored.PollQuestNotice(repeatedNotice)&&!restored.PollScriptItemNotice(repeatedItem));
    ctx.player=&restored;s.End("reload");assert(s.Start(ctx,restored));assert(s.info!=0x1501d&&s.info!=0x95aa4&&s.info!=0x95aa5);s.End("finished");
    std::cout<<"Original MS11 branch "<<bonus<<": complete, rewards and save/reload passed\n";
  }
  // Disarming before the first conversation selects the separate first-meeting INFO.
  fo3player::Player first(c);first.RegisterOriginalActors();
  first.SetScriptPlayerLocation(bomb.x,bomb.y,bomb.z,bomb.cell,bomb.world);
  fo3player::ScriptMessage message;
  assert(first.DispatchReferenceEvent(0x14bc8,"OnActivate",0x14,error)==fo3player::ScriptEventResult::Executed);
  assert(first.PollScriptMessage(message)&&message.form==0x78cb2);
  assert(first.SubmitScriptMessageButton(0x14bc8,1));first.PumpReferenceGameMode(.25,{0x14bc8});
  Context ctx;ctx.player=&first;ctx.speaker={0x3b46,0xa60,bomb.cell,bomb.world};ctx.target={0x14,7,bomb.cell,bomb.world};
  Session session;assert(session.Start(ctx,first)&&session.info==0x1501d);
  assert(first.Snapshot().pipboy.dialogueVariables.at(VariableKey(c.pipboy,"ms11.simmsbonus"))==1);
  Finish(session,ctx,first);assert(first.Snapshot().pipboy.quests.at(0x14e9e).stage==210&&first.ItemCount(0xf)==100);
  // Original Swift Learner entry point applies exactly the acquired rank.
  for(uint8_t rank=1;rank<=3;++rank){fo3player::Player perk(c);assert(perk.GrantPerk(0x31dd3,rank));
    fo3pipdata::ResultScript reward;reward.source="RewardXP 300";
    assert(perk.ExecuteDialogueResult(reward,error));assert(perk.Snapshot().experience==uint32_t(300+30*rank));
  }
  std::cout<<"Original first-meeting greeting and Swift Learner ranks passed\n";
}
static void Atomic(){
  fo3player::Catalog c;c.initial.special.fill(1);c.initial.baseHealth=100;
  c.pipboy.formNames={{"q",10},{"cell",20},{"name",30},{"offscene",100}};
  c.pipboy.quests[10].objectives[1].text="objective";
  c.cellOwnership[20].name="Empty";c.pipboy.messages[30].title="Authored";
  c.references[100].base=200;c.references[100].cell=21;c.actorInventories[200].valid=true;
  fo3player::Player p(c);std::string error;
  fo3pipdata::ResultScript script;script.source="RewardXP 300\nSetCellFullName cell name\nCompleteAllObjectives q\nAddAchievement 23\nOffscene.Disable";
  const auto revision=p.Revision();auto plan=p.PrepareDialogueTransaction(script,{},error);
  assert(plan&&plan->HasReferenceChanges()&&p.Revision()==revision&&p.Snapshot().experience==0&&p.CellFullName(20)=="Empty");
  assert(!p.ExecuteDialogueResult(script,error)&&p.Snapshot().experience==0);
  assert(!fo3script::NonresidentActorRemoval(p,*plan,21,0,{},error));
  assert(!fo3script::NonresidentActorRemoval(p,*plan,20,0,{100},error));
  assert(fo3script::NonresidentActorRemoval(p,*plan,20,0,{},error));
  assert(p.CommitScriptTransaction(*plan));assert(p.Snapshot().experience==300&&p.CellFullName(20)=="Authored");
  assert(p.RollbackScriptTransaction(*plan));assert(p.Snapshot().experience==0&&p.CellFullName(20)=="Empty"&&p.Snapshot().achievements.empty());
  fo3player::ExperienceNotice experienceNotice;
  assert(!p.PollExperienceNotice(experienceNotice)); // rolled back with the script
  script.source="RewardXP 300\nSetCellFullName cell missing";
  assert(!p.PrepareDialogueTransaction(script,{},error)&&p.Snapshot().experience==0);
  const auto legacy="/tmp/fq-ms11-legacy-"+std::to_string(getpid());
  const auto save="/tmp/fq-ms11-v17-"+std::to_string(getpid());
  assert(p.Save(legacy,error));
  script.source="RewardXP 300\nSetCellFullName cell name\nAddAchievement 23";
  assert(p.ExecuteDialogueResult(script,error)&&p.Save(save,error));
  assert(p.PollExperienceNotice(experienceNotice)&&
         experienceNotice.amount==300&&experienceNotice.total==300&&
         experienceNotice.levelUp); // level 2 threshold has been crossed
  assert(!p.PollExperienceNotice(experienceNotice));
  std::ifstream input(save,std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
  assert(bytes[4]==17);const auto before=p.Revision();
  auto put=[](std::vector<uint8_t>& data,size_t at,uint32_t value){for(unsigned i=0;i<4;++i)data[at+i]=uint8_t(value>>(8*i));};
  auto write=[&](std::vector<uint8_t> data){put(data,12,uint32_t(data.size()-20));put(data,16,uint32_t(crc32(0,data.data()+20,data.size()-20)));
    std::ofstream out(save,std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),data.size());};
  auto bad=bytes;put(bad,bad.size()-16,999);write(bad);assert(!p.Restore(save,error));
  bad=bytes;bad.pop_back();write(bad);assert(!p.Restore(save,error));
  assert(p.Revision()==before&&p.Snapshot().experience==300&&p.CellFullName(20)=="Authored");
  write(bytes);assert(p.Restore(save,error)&&p.CellFullName(20)=="Authored"&&p.Snapshot().achievements.count(23));
  assert(!p.PollExperienceNotice(experienceNotice)); // restore does not replay XP
  assert(p.Restore(legacy,error)&&p.CellFullName(20)=="Empty"&&p.Snapshot().achievements.empty()&&p.Snapshot().experience==0);
  std::remove(save.c_str());std::remove(legacy.c_str());
}
int main(int argc,char** argv){Atomic();if(argc>1)Original(argv[1]);std::cout<<"MS11 completion transactions passed\n";}
