// Render-thread UI adapter. Player owns all pricing and durable inventories.
struct Fo3BarterUi {
  fo3player::BarterSession session;
  bool active=false,buying=true;
  std::vector<uint64_t> rows;
  fo3loot::Cursor cursor;
  double nextHorizontal=0;
  fo3dialogue::Panel playerPanel,merchantPanel;
  std::string status;
};
Fo3BarterUi gBarter;
void CloseFo3Barter(){
  gBarter.active=false;gBarter.session={};gBarter.rows.clear();gBarter.cursor.Target(0,0);
  gBarter.playerPanel.visible=gBarter.merchantPanel.visible=false;
  gDialoguePanelInfo=0;gDialoguePanelPhase=fo3dialogue::Phase::Inactive;
}
void BarterBuildPanels() {
  auto& p=gPlayerSession->player;auto& ui=gBarter;
  auto build=[&](bool buying,fo3dialogue::Panel& panel){
    const auto& inventory=buying?ui.session.merchant:ui.session.player;
    const auto previous=panel;const auto revision=panel.revision;panel=gDialoguePanel;panel.revision=revision;panel.visible=true;panel.subtitle.clear();panel.choices.clear();
    const auto ref=p.Definitions().references.find(ui.session.actor);
    const auto npc=ref==p.Definitions().references.end()?p.Definitions().pipboy.dialogueActors.end():p.Definitions().pipboy.dialogueActors.find(ref->second.base);
    const std::string name=npc==p.Definitions().pipboy.dialogueActors.end()?"Merchant":npc->second.name;
    int64_t caps=0;for(const auto& s:inventory)if(s.formId==0xf)caps+=s.count;
    panel.title=(buying?name:"You")+" | Caps "+std::to_string(caps);
    std::vector<uint64_t> ids;
    for(const auto& s:inventory){float price=0;if(!p.BarterItemAllowed(ui.session.actor,s)||!p.BarterPrice(s,buying,ui.session.discount,price))continue;
      int32_t pending=0;for(const auto& choice:ui.session.selections)if(choice.buying==buying&&choice.stack==s.id)pending=choice.count;
      const auto& item=p.Definitions().items.at(s.formId);
      panel.choices.push_back(item.name+" ("+std::to_string(s.count)+")  "+([](float value){char text[32];std::snprintf(text,sizeof(text),"%.1f",value);return std::string(text);})(price)+"c"+
        (item.maxCondition?" CND "+std::to_string(int(s.condition*100))+"%":"")+
        (pending?" ["+std::to_string(pending)+"]":""));ids.push_back(s.id);
    }
    panel.choices.push_back("Confirm transaction");
    if(buying==ui.buying){ui.rows=std::move(ids);ui.cursor.Target(ui.session.actor,panel.choices.size());panel.selected=ui.cursor.selected;}
    else panel.selected=SIZE_MAX;
    int64_t net=0;std::string reason;p.BarterBalance(ui.session,net,reason);
    panel.subtitle=(net>=0?"Pay ":"Receive ")+std::to_string(std::abs(net))+" caps | Weight "+std::to_string(int(p.InventoryWeight()))+"/"+std::to_string(int(p.CarryCapacity()))+
      (ui.status.empty()?(reason.empty()?"":"\n"+reason):"\n"+ui.status);
    panel.footer="Stick: scroll / switch side\nA Select   Trigger + stick: quantity\nB Return to dialogue";
    panel.revision=revision+(previous.title!=panel.title||previous.subtitle!=panel.subtitle||previous.choices!=panel.choices||previous.selected!=panel.selected);
  };
  build(false,ui.playerPanel);build(true,ui.merchantPanel);
}
void UpdateFo3Barter(double now,float stick,float horizontal,float trigger,bool a,bool b,bool focused) {
  if(!gBarter.active||!gPlayerSession)return;
  if(focused&&b){CloseFo3Barter();fo3audio::NamedSound("UIMenuCancel");return;}
  auto& ui=gBarter;auto& p=gPlayerSession->player;
  if(focused){ui.cursor.Scroll(stick,now,ui.rows.size()+1);
    if(std::fabs(horizontal)>.6f&&now>=ui.nextHorizontal){
      ui.nextHorizontal=now+.18;
      if(trigger>.5f&&ui.cursor.selected<ui.rows.size()){
        const auto id=ui.rows[ui.cursor.selected];const auto& inventory=ui.buying?ui.session.merchant:ui.session.player;
        const fo3player::Stack* stack=nullptr;for(const auto& s:inventory)if(s.id==id)stack=&s;
        auto found=std::find_if(ui.session.selections.begin(),ui.session.selections.end(),[&](const auto& s){return s.stack==id&&s.buying==ui.buying;});
        if(stack){int32_t count=found==ui.session.selections.end()?0:found->count;
          if(horizontal>0&&count<stack->count)++count;else if(horizontal<0&&count>0)--count;
          if(found!=ui.session.selections.end())ui.session.selections.erase(found);
          if(count)ui.session.selections.push_back({id,count,ui.buying});}
      }else{ui.buying=horizontal>0;ui.cursor.Target(0,0);}
      fo3audio::Scroll();
    }else if(std::fabs(horizontal)<.3f)ui.nextHorizontal=now;
    if(a){ui.status.clear();
      if(ui.cursor.selected>=ui.rows.size()){
        if(p.ConfirmBarter(ui.session,ui.status)){ui.status="Transaction complete";FlushFo3PlayerState();fo3audio::NamedSound("UIMenuOK");}
        else fo3audio::NamedSound("UIMenuCancel");
      }else{const auto id=ui.rows[ui.cursor.selected];
        auto choice=std::find_if(ui.session.selections.begin(),ui.session.selections.end(),[&](const auto& s){return s.stack==id&&s.buying==ui.buying;});
        if(choice==ui.session.selections.end())ui.session.selections.push_back({id,1,ui.buying});else ui.session.selections.erase(choice);
        fo3audio::NamedSound("UIMenuOK");}
    }
  }
  BarterBuildPanels();
}
