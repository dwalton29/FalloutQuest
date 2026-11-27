#pragma once
#include "dialogue/fo3-dialogue-panel.h"
#include "player/fo3-stack-group.h"
#include <map>
#include <tuple>

namespace fo3transfer {
struct Row {std::vector<uint64_t> ids;int32_t count=0;bool allowed=false;};
struct Menu {
  uint32_t reference=0;
  bool depositing=false;
  int quantity=0;
  uint64_t revision=UINT64_MAX;
  fo3loot::Cursor cursor;
  fo3dialogue::Panel panel;
  std::vector<Row> rows;
  std::string name,status;

  void Refresh(const fo3player::Player& player){
    revision=player.Revision();rows.clear();panel.choices.clear();
    const auto* inventory=depositing?&player.Snapshot().inventory:player.ContainerContents(reference);
    std::map<fo3player::InventoryGroupKey,size_t> groups;
    if(inventory)for(const auto& s:*inventory){
      const auto& item=player.Definitions().items.at(s.formId);
      if(!item.playable||item.name.empty())continue;
      const bool allowed=depositing?player.CanStoreContainerStack(reference,s):player.CanTakeContainerStack(reference,s.id);
      const auto entry=groups.emplace(fo3player::InventoryGroup(s),rows.size());
      if(entry.second){
        rows.emplace_back().allowed=allowed;
        std::string label=item.name;
        if(item.maxCondition)label+="  "+std::to_string(int(std::round(s.condition*100)))+"%";
        if(s.equipped)label+="  [equipped]";
        if(!allowed)label+="  [unavailable]";
        else if(!depositing&&player.ContainerStackOwnership(reference,s.id).Theft())label+="  [steal]";
        panel.choices.push_back(label);
      }
      auto& row=rows[entry.first->second];row.ids.push_back(s.id);
      row.count+=std::min(INT32_MAX-row.count,s.count);
    }
    for(size_t i=0;i<rows.size();++i)if(rows[i].count>1)panel.choices[i]+=" ("+std::to_string(rows[i].count)+")";
    cursor.Target(reference,rows.size());panel.selected=cursor.selected;
    panel.title=depositing?"Your inventory":name;
    panel.subtitle=status.empty()?(rows.empty()?"Empty":""):status;
    panel.footer="X Switch inventory    A Transfer    B Close";++panel.revision;
  }
  bool Input(fo3player::Player& player,double time,float stick,bool a,bool b,bool x){
    if(!player.CanLootContainer(reference)){reference=0;return false;}
    if(revision!=player.Revision()){
      const auto previous=cursor.selected<rows.size()?rows[cursor.selected]:Row{};
      Refresh(player);
      if(rows.empty()||previous.ids!=rows[cursor.selected].ids||previous.count!=rows[cursor.selected].count){
        quantity=0;return false;
      }
      previousQuantity=0;
    }
    if(b){if(quantity){quantity=0;Refresh(player);}else reference=0;return false;}
    if(x&&!quantity){depositing=!depositing;status.clear();cursor={};Refresh(player);return false;}
    if(!quantity){cursor.Scroll(stick,time,rows.size());panel.selected=cursor.selected;}
    if(rows.empty())return false;
    const auto& row=rows[cursor.selected];
    if(quantity){
      fo3loot::Cursor& adjust=amountCursor;
      if(x)adjust.selected=adjust.selected?0:size_t(row.count-1);
      adjust.Scroll(-stick,time,size_t(row.count));quantity=int(adjust.selected)+1;
      panel.footer="Quantity "+std::to_string(quantity)+" / "+std::to_string(row.count)+"    X One / All    A Confirm    B Cancel";
      if(quantity!=previousQuantity){++panel.revision;previousQuantity=quantity;}
    }
    if(!a||!row.allowed)return false;
    if(!quantity&&row.count>1){
      quantity=row.count;amountCursor={};amountCursor.Target(reference,row.count);
      amountCursor.selected=row.count-1;previousQuantity=0;return false;
    }
    const bool done=player.TransferContainerItems(reference,depositing,row.ids,quantity?quantity:1);
    quantity=0;status=done?"":"Cannot transfer item";Refresh(player);return done;
  }
private:
  int previousQuantity=0;
  fo3loot::Cursor amountCursor;
};
}
