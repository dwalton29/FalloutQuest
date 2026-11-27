// The render thread keeps the displayed quote until the player acts. The player
// rechecks every price and instance before publishing a purchase.
struct Fo3RepairUi {
  fo3player::RepairSession session;
  fo3dialogue::Panel panel;
  fo3loot::Cursor cursor;
  bool xDown=false;
};
Fo3RepairUi gRepair;
void CloseFo3Repair() {
  gRepair.session={};gRepair.panel.visible=false;gRepair.cursor.Target(0,0);
  gDialoguePanelInfo=0;gDialoguePanelPhase=fo3dialogue::Phase::Inactive;
}
bool RefreshFo3Repair(uint32_t actor,const std::string& status={}) {
  auto& ui=gRepair;auto& p=gPlayerSession->player;
  if(!p.OpenRepair(actor,ui.session)){CloseFo3Repair();return false;}
  auto percent=[](float ratio){return std::to_string(int(std::round(ratio*100)))+"%";};
  auto& panel=ui.panel;
  panel.visible=true;panel.title="Repair services";
  panel.subtitle=std::to_string(p.ItemCount(0xf))+" caps   |   Repair to "+percent(ui.session.target);
  if(!status.empty())panel.subtitle+="   |   "+status;
  panel.choices.clear();
  for(const auto& r:ui.session.offers){
    const auto& item=p.Definitions().items.at(r.form);
    panel.choices.push_back(item.name+(r.count>1?" (x"+std::to_string(r.count)+")":"")+
        "   "+percent(r.condition)+"   "+std::to_string(r.price)+" caps");
  }
  ui.cursor.Target(actor,panel.choices.size());panel.selected=ui.cursor.selected;
  if(panel.choices.empty())panel.subtitle+="   |   No items to repair";
  panel.footer=panel.choices.empty()?"B Back":"A Repair one    X Repair all ("+std::to_string(ui.session.total)+")    B Back";
  ++panel.revision;return true;
}
void UpdateFo3Repair(double now,float stick,bool a,bool b,bool x,bool focused) {
  auto& ui=gRepair;const bool all=x&&!ui.xDown;ui.xDown=x;
  if(!focused||!gPlayerSession||!ui.panel.visible)return;
  if(b){CloseFo3Repair();fo3audio::NamedSound("UIMenuCancel");return;}
  ui.cursor.Scroll(stick,now,ui.session.offers.size());ui.panel.selected=ui.cursor.selected;
  if((!a&&!all)||ui.session.offers.empty())return;
  const auto actor=ui.session.actor;std::string error;
  if(gPlayerSession->player.ConfirmRepair(ui.session,all?0:ui.session.offers[ui.cursor.selected].stack,error)){
    FlushFo3PlayerState();fo3audio::NamedSound("UIRepairWeapon");
  }else fo3audio::NamedSound("UIMenuCancel");
  RefreshFo3Repair(actor,error);
}
