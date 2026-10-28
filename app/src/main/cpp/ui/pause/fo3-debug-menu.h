#pragma once
#include "player/fo3-player-state.h"
#include "dialogue/fo3-dialogue-panel.h"
#include <algorithm>
#include <cstdio>

namespace fo3debug {
inline std::string Hex(uint32_t id){char b[9];std::snprintf(b,sizeof b,"%08X",id);return b;}
inline std::string Time(float hour){char b[16];const int m=int(hour*60+.001f)%1440;std::snprintf(b,sizeof b,"%02d:%02d",m/60,m%60);return b;}
// Pure controller-driven menu model. The render thread owns admission and
// persistence; no highlighted row ever changes canonical player state.
class Menu {
public:
  enum class Page { Root,Debug,Time,XP,Categories,Items,Hex,Item,Points,Perks,Perk,Special };
  fo3dialogue::Panel panel;
  Page page=Page::Root;
  size_t selected=0,itemPage=0,perkPage=0;
  static constexpr size_t PageSize=10;
  fo3player::Player* owner=nullptr;
  std::vector<uint32_t> items,perks;
  std::array<uint8_t,7> special{};
  int hour=0,minute=0,points=0,category=0;
  uint32_t form=0,hex=0;
  float condition=1;
  bool ownedOnly=false;
  // Local renderer diagnostic. Not player state and never saved to FQPS.
  uint8_t perfHudMode=0; // 0=Off, 1=FPS, 2=FPS/CPU/NPC
  uint64_t serial=30000000;
  std::string feedback;
  static constexpr const char* SpecialNames[7]={"Strength","Perception","Endurance","Charisma","Intelligence","Agility","Luck"};
  void Open(fo3player::Player& player){owner=&player;panel.visible=true;Go(Page::Root);}
  void Close(){panel.visible=false;owner=nullptr;}
  void Go(Page p){page=p;selected=0;feedback.clear();Refresh();}
  void Reject(const char* message){feedback=message;Refresh();}
  void Feedback(bool ok,const char* success){feedback=ok?success:"Rejected: invalid, unsupported, resource limit or insufficient points";Refresh();}
  void Refresh(){
    if(!owner)return;
    panel.choices.clear();panel.title="DEBUG";panel.subtitle=feedback;
    panel.footer="Stick Navigate / Adjust    A Confirm    B Back";
    const auto& player=*owner;const auto& state=player.Snapshot();
    auto add=[&](std::string s){panel.choices.push_back(std::move(s));};
    switch(page){
      case Page::Root:panel.title="";panel.footer="A Select    B Continue";
        for(const char* key:{"sContinue","sSave","sLoad","sSettings","sQuit"}){
          const auto it=player.Definitions().strings.find(key);
          add(it==player.Definitions().strings.end()?std::string(key+1):it->second);
        }
        add("Debug");
        break;
      case Page::Debug:
        for(const char* s:{"Set Time","Add XP / Level","Add Items","Perk Points","Chosen Perks","SPECIAL Allocations"}) {add(s);}
        add(std::string("Performance HUD: ")+(perfHudMode==0?"Off":perfHudMode==1?"FPS":"Detailed"));
        add("Unlock all map locations");
        break;
      case Page::Time:panel.title="SET TIME";
        panel.subtitle="Current Time: "+Time(state.gameHour)+"\nSelected Time: "+Time(float(hour)+float(minute)/60)+"\n"+feedback;
        add("Hour: "+std::to_string(hour));add("Minute: "+std::to_string(minute));add("Apply Time");break;
      case Page::XP:panel.title="ADD XP / LEVEL";
        panel.subtitle="XP: "+std::to_string(state.experience)+"    Level: "+std::to_string(state.level)+"\n"+feedback;
        for(const char* s:{"Add 100 XP","Add 500 XP","Add 1,000 XP","Reach Next Level"}) {add(s);}
        break;
      case Page::Categories:panel.title="ADD ITEMS";
        for(const char* s:{"Weapons","Armour","Ammunition","Aid","Miscellaneous","Other","Enter hexadecimal FormID"}) {add(s);}
        break;
      case Page::Items:panel.title="ITEMS - PAGE "+std::to_string(itemPage+1);
        for(size_t i=itemPage*PageSize;i<items.size()&&i<(itemPage+1)*PageSize;++i){const auto& item=player.Definitions().items.at(items[i]);add(item.name+" ["+Hex(items[i])+"]");}
        add("Previous Page");add("Next Page");break;
      case Page::Hex:panel.title="FORMID: "+Hex(hex);panel.subtitle="Select a digit, adjust left/right, then Lookup.\n"+feedback;
        for(int i=0;i<8;++i) {add("Digit "+std::to_string(i+1)+": "+Hex(hex).substr(size_t(i),1));}
        add("Lookup FormID");break;
      case Page::Item:{panel.title="ADD ITEM";const auto found=player.Definitions().items.find(form);
        if(found==player.Definitions().items.end())break;
        panel.subtitle=found->second.name+" ["+Hex(form)+"]\n"+feedback;
        add("Add 1");add("Add 10");add("Add 100");
        if(found->second.maxCondition>0) {add("Condition: "+std::to_string(int(condition*100+.5f))+"%");}
        break;}
      case Page::Points:panel.title="PERK POINTS";
        panel.subtitle="Extra debug selection budget; normal level-ups still grant their perk.\n"+feedback;
        add("Selected points: "+std::to_string(points));add("Apply Perk Points");break;
      case Page::Perks:panel.title="CHOSEN PERKS - PAGE "+std::to_string(perkPage+1);
        panel.subtitle="Points: "+std::to_string(state.debugPerkPoints)+"\n"+feedback;
        for(size_t i=perkPage*PageSize;i<perks.size()&&i<(perkPage+1)*PageSize;++i){const auto id=perks[i];const auto& p=player.Definitions().pipboy.perks.at(id);auto it=state.pipboy.perks.find(id);
          add(p.name+" ["+Hex(id)+"] Rank "+std::to_string(it==state.pipboy.perks.end()?0:it->second)+(player.LevelUpPerkSupported(id)?"":" (unsupported)"));}
        add("Previous Page");add("Next Page");add(ownedOnly?"Show All Perks":"Show Chosen Perks");break;
      case Page::Perk:{panel.title="EDIT PERK";const auto& p=player.Definitions().pipboy.perks.at(form);const auto it=state.pipboy.perks.find(form);
        panel.subtitle=p.name+" ["+Hex(form)+"]\nRank "+std::to_string(it==state.pipboy.perks.end()?0:it->second)+" / "+std::to_string(p.ranks)+"  Points "+std::to_string(state.debugPerkPoints)+"\n"+feedback;
        add("Add One Rank (uses 1 point)");add("Remove One Rank (refunds 1 point)");
        panel.footer="A Confirm    B Back. Rank edits do not undo past skill/SPECIAL allocations.";break;}
      case Page::Special:panel.title="SPECIAL ALLOCATIONS";
        panel.subtitle="Edit base SPECIAL (1-10). Existing skill allocations are retained.\n"+feedback;
        for(size_t i=0;i<special.size();++i) {add(std::string(SpecialNames[i])+": "+std::to_string(special[i]));}
        add("Apply SPECIAL");break;
    }
    selected=std::min(selected,panel.choices.empty()?size_t(0):panel.choices.size()-1);
    panel.selected=selected;panel.revision=++serial;
  }
  void Move(int direction){if(panel.choices.empty())return;
    if(page==Page::Root){selected=selected==0?5:0;Refresh();return;}
    selected=size_t((int(selected)+direction+int(panel.choices.size()))%int(panel.choices.size()));Refresh();}
  void Adjust(int direction){
    if(page==Page::Time&&selected<2){int& v=selected?minute:hour;const int max=selected?60:24;v=(v+direction+max)%max;}
    else if(page==Page::Hex&&selected<8){const int shift=int(7-selected)*4;const auto digit=((hex>>shift)+direction)&15u;hex=(hex&~(15u<<shift))|(digit<<shift);}
    else if(page==Page::Points&&selected==0)points=std::clamp(points+direction,0,1000);
    else if(page==Page::Special&&selected<7)special[selected]=uint8_t(std::clamp(int(special[selected])+direction,1,10));
    else if(page==Page::Item&&selected==3)condition=float(std::clamp(int(condition*100+.5f)+direction*10,0,100))/100;
    Refresh();
  }
  void BuildItems(){items.clear();for(const auto& kv:owner->Definitions().items){const auto& item=kv.second;int c=5;
    switch(item.kind){case fo3player::ItemKind::Weapon:c=0;break;case fo3player::ItemKind::Armour:c=1;break;case fo3player::ItemKind::Ammo:c=2;break;case fo3player::ItemKind::Aid:case fo3player::ItemKind::Ingredient:c=3;break;case fo3player::ItemKind::Misc:c=4;break;default:break;}
    if(c==category&&!item.name.empty())items.push_back(kv.first);}
    std::sort(items.begin(),items.end(),[&](uint32_t a,uint32_t b){const auto& x=owner->Definitions().items.at(a).name;const auto& y=owner->Definitions().items.at(b).name;return x==y?a<b:x<y;});itemPage=0;}
  void BuildPerks(){perks.clear();for(const auto& kv:owner->Definitions().pipboy.perks)if((kv.second.playable&&!kv.second.hidden)||owner->Snapshot().pipboy.perks.count(kv.first))if(!ownedOnly||owner->Snapshot().pipboy.perks.count(kv.first))perks.push_back(kv.first);
    std::sort(perks.begin(),perks.end(),[&](uint32_t a,uint32_t b){const auto& x=owner->Definitions().pipboy.perks.at(a).name;const auto& y=owner->Definitions().pipboy.perks.at(b).name;return x==y?a<b:x<y;});perkPage=0;}
  // Caller checks safe mutation admission on EVERY confirm, including resume.
  bool Confirm(){
    if(!owner)return false;
    auto& p=*owner;const auto index=selected;bool changed=false;
    switch(page){
      case Page::Root:if(index==0)Close();else if(index==5)Go(Page::Debug);break;
      case Page::Debug:
        if(index==0){const int m=int(p.Snapshot().gameHour*60)%1440;hour=m/60;minute=m%60;Go(Page::Time);}
        else if(index==1)Go(Page::XP);else if(index==2)Go(Page::Categories);
        else if(index==3){points=p.Snapshot().debugPerkPoints;Go(Page::Points);}
        else if(index==4){BuildPerks();Go(Page::Perks);}
        else if(index==5){special=p.Snapshot().special;Go(Page::Special);}
        else if(index==7){const auto n=p.DebugUnlockLocations();changed=n!=0;feedback=std::to_string(n)+" locations unlocked";Refresh();}
        else if(index==6){perfHudMode=uint8_t((perfHudMode+1u)%3u);Refresh();}
        break;
      case Page::Time:if(index==2){changed=p.DebugSetTime(hour,minute);Feedback(changed,"Time applied");}else Adjust(1);break;
      case Page::XP:{
        if(index==3&&p.ExperienceProgress().ready){Reject("Level-up already pending. Close pause to allocate it.");break;}
        if(index==3&&p.Snapshot().level>=p.ExperienceProgress().maxLevel){Reject("Current level cap reached");break;}
        const uint32_t awards[]={100,500,1000};changed=index<3?p.RewardExperience(awards[index]):p.DebugReachNextLevel();Feedback(changed,"XP awarded; close pause to allocate pending levels");break;}
      case Page::Categories:if(index==6)Go(Page::Hex);else{category=int(index);BuildItems();Go(Page::Items);}break;
      case Page::Items:{const auto start=itemPage*PageSize;const auto count=std::min(PageSize,items.size()-std::min(start,items.size()));
        if(index<count){form=items[start+index];condition=1;Go(Page::Item);}else{if(index==count&&itemPage)--itemPage;else if(index==count+1&&(itemPage+1)*PageSize<items.size())++itemPage;selected=0;Refresh();}break;}
      case Page::Hex:if(index<8)Adjust(1);else if(p.Definitions().items.count(hex)){form=hex;condition=1;Go(Page::Item);}else Reject("Unknown item FormID");break;
      case Page::Item:if(index<3){const int amounts[]={1,10,100};changed=p.Add(form,amounts[index],condition);Feedback(changed,"Item added to inventory");}else Adjust(1);break;
      case Page::Points:if(index==1){changed=p.DebugSetPerkPoints(points);Feedback(changed,"Perk points applied");}else Adjust(1);break;
      case Page::Perks:{const auto start=perkPage*PageSize;const auto count=std::min(PageSize,perks.size()-std::min(start,perks.size()));
        if(index<count){form=perks[start+index];Go(Page::Perk);}else{if(index==count&&perkPage)--perkPage;else if(index==count+1&&(perkPage+1)*PageSize<perks.size())++perkPage;else if(index==count+2){ownedOnly=!ownedOnly;BuildPerks();}selected=0;Refresh();}break;}
      case Page::Perk:{const auto it=p.Snapshot().pipboy.perks.find(form);const int rank=it==p.Snapshot().pipboy.perks.end()?0:it->second;changed=p.DebugSetPerkRank(form,rank+(index==0?1:-1));Feedback(changed,"Perk rank applied");
        if(!changed&&index==0){
          const auto intense=p.Definitions().pipboy.formNames.find("intensetraining");
          if(intense!=p.Definitions().pipboy.formNames.end()&&form==intense->second)Reject("Use normal level-up for Intense Training, or edit SPECIAL directly.");
          else if(!p.LevelUpPerkSupported(form))Reject("This perk's effects are not implemented; rank was not granted.");
          else if(!p.Snapshot().debugPerkPoints)Reject("No perk points. Set a budget in Debug > Perk Points.");
          else Reject("Maximum original perk rank reached");
        }
        break;}
      case Page::Special:if(index==7){changed=p.SetPlayerAttributes(special,p.Snapshot().skills);Feedback(changed,"SPECIAL applied");}else Adjust(1);break;
    }return changed;
  }
  void Back(){switch(page){case Page::Root:Close();break;case Page::Debug:Go(Page::Root);break;case Page::Items:case Page::Hex:Go(Page::Categories);break;case Page::Item:Go(Page::Categories);break;case Page::Perk:Go(Page::Perks);break;default:Go(Page::Debug);break;}}
};
}
