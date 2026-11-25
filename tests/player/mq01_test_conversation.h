#pragma once
#include "dialogue/fo3-dialogue-session.h"
#include "scripting/fo3-nonresident-script.h"
#include <algorithm>
#include <cassert>

struct Conversation {
  fo3player::Player& player;
  fo3dialogue::Session session;
  fo3dialogue::Context context;
  explicit Conversation(fo3player::Player& p,uint32_t reference):player(p){
    const auto& placed=p.Definitions().pipboy.targets.at(reference);
    context.player=&p;
    context.speaker={reference,placed.base,placed.cell,placed.world,0,placed.x,placed.y,placed.z,false,false};
    context.target={fo3player::PlayerRef,7,placed.cell,placed.world,0,placed.x,placed.y+40,placed.z,false,false};
    p.SetScriptPlayerLocation(placed.x,placed.y+40,placed.z,placed.cell,placed.world);
    session.previewReferences=[this,reference](const auto& current,const auto& plan,std::string& error){
      return fo3script::NonresidentActorRemoval(current,plan,context.target.cell,context.target.world,{reference},error);
    };
    session.publishReferences=[this](auto& current,auto& plan,std::string& error){
      return session.previewReferences(current,plan,error)&&current.CommitScriptTransaction(plan);
    };
  }
  ~Conversation(){session.End("leave area");}
  void Finish(){
    unsigned responses=0;
    while(session.phase==fo3dialogue::Phase::Speaking){
      assert(++responses<100);const auto token=session.audioToken;
      (void)session.AudioDone(token,true,context,player);
    }
  }
  void Start(){assert(session.Start(context,player));Finish();}
  void Choose(uint32_t topic){
    const auto choice=std::find_if(session.choices.begin(),session.choices.end(),[&](const auto& row){return row.topic==topic;});
    assert(choice!=session.choices.end());
    assert(session.Choose(size_t(choice-session.choices.begin()),context,player));Finish();
  }
};
