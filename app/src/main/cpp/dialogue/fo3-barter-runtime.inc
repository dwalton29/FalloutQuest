#include "ui/menu/fo3-barter-controls.h"
// Render-thread UI adapter. Player owns all pricing and durable inventories.
struct Fo3BarterUi {
  fo3player::BarterSession session;
  bool active=false,buying=true;
  std::vector<uint64_t> rows;
  fo3loot::Cursor cursor;
  double nextHorizontal=0;
  fo3dialogue::Panel playerPanel,merchantPanel;
  std::string status;
  fo3barter::Controls controls;
};
Fo3BarterUi gBarter;
void CloseFo3Barter(){
  gBarter.controls={};gBarter.active=false;gBarter.session={};gBarter.rows.clear();gBarter.cursor.Target(0,0);
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
    for(const auto& s:inventory){if(!fo3barter::InCategory(p.Definitions().items.at(s.formId),ui.controls.filters[buying]))continue;float price=0;if(!p.BarterItemAllowed(ui.session.actor,s)||!p.BarterPrice(s,buying,ui.session.discount,price))continue;
      int32_t pending=0;for(const auto& choice:ui.session.selections)if(choice.buying==buying&&choice.stack==s.id)pending=choice.count;
      const auto& item=p.Definitions().items.at(s.formId);
      panel.choices.push_back(item.name+" ("+std::to_string(s.count)+")  "+([](float value){char text[32];std::snprintf(text,sizeof(text),"%.1f",value);return std::string(text);})(price)+"c"+
        (item.maxCondition?" CND "+std::to_string(int(s.condition*100))+"%":"")+
        (pending?" ["+std::to_string(pending)+"]":""));ids.push_back(s.id);
    }

    if(buying==ui.buying){ui.rows=std::move(ids);ui.cursor.Target(ui.session.actor,panel.choices.size());panel.selected=ui.cursor.selected;}
    else panel.selected=SIZE_MAX;
    int64_t net=0;std::string reason;p.BarterBalance(ui.session,net,reason);
    panel.subtitle=(net>=0?"Pay ":"Receive ")+std::to_string(std::abs(net))+" caps | Weight "+std::to_string(int(p.InventoryWeight()))+"/"+std::to_string(int(p.CarryCapacity()))+
      (ui.status.empty()?(reason.empty()?"":"\n"+reason):"\n"+ui.status);
    panel.footer.clear();
    panel.revision=revision+(previous.title!=panel.title||previous.subtitle!=panel.subtitle||previous.choices!=panel.choices||previous.selected!=panel.selected);
  };
  build(false,ui.playerPanel);build(true,ui.merchantPanel);
}
void UpdateFo3Barter(double now,float stick,float horizontal,float leftTrigger,float rightTrigger,bool x,bool a,bool b,bool focused) {
  if(!gBarter.active||!gPlayerSession)return;
  auto &ui=gBarter;auto &p=gPlayerSession->player;auto &c=ui.controls;
  const bool left=leftTrigger>.65f,right=rightTrigger>.65f;
  const bool pressLeft=left&&!c.leftDown,pressRight=right&&!c.rightDown,pressX=x&&!c.xDown;
  c.leftDown=left;c.rightDown=right;c.xDown=x;
  if(focused){
    if(b){if(c.Editing())c.CancelQuantity();else {CloseFo3Barter();fo3audio::NamedSound("UIMenuCancel");return;}}
    else if(c.Editing()){
      if(std::fabs(horizontal)>.6f&&now>=ui.nextHorizontal){c.Adjust(horizontal>0?1:-1);ui.nextHorizontal=now+.12;fo3audio::Scroll();}
      if(std::fabs(horizontal)<.3f)ui.nextHorizontal=now;
      if(a){c.Accept(ui.session);fo3audio::NamedSound("UIMenuOK");}
    }else{
      if(pressLeft||pressRight){ui.buying=pressRight;ui.cursor.Target(0,0);ui.status.clear();fo3audio::Scroll();}
      if(std::fabs(horizontal)>.6f&&now>=ui.nextHorizontal){
        auto &filter=c.filters[ui.buying];filter=(filter+1+(horizontal>0?1:5))%6-1;
        ui.cursor.Target(0,0);ui.nextHorizontal=now+.22;fo3audio::Scroll();
      }else if(std::fabs(horizontal)<.3f)ui.nextHorizontal=now;
      // Rebuild before selecting after a trigger/category edge.
      BarterBuildPanels();ui.cursor.Scroll(stick,now,ui.rows.size());
      if(pressX){
        ui.status.clear();
        if(!ui.session.selections.empty()){
          if(p.ConfirmBarter(ui.session,ui.status)){ui.status.clear();FlushFo3PlayerState();fo3audio::NamedSound("UIMenuOK");}
          else fo3audio::NamedSound("UIMenuCancel");
        }
      }else if(a&&ui.cursor.selected<ui.rows.size()){
        ui.status.clear();c.Begin(ui.session,ui.rows[ui.cursor.selected],ui.buying);fo3audio::NamedSound("UIMenuOK");
      }
    }
  }
  BarterBuildPanels();
}
