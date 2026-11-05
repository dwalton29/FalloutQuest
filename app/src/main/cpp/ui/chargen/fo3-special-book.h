#pragma once
#include "player/fo3-player-state.h"
#include "world/interaction/fo3-loot-cursor.h"
namespace fo3special {
// Controller presentation for the script-supplied SPECIAL total. The player
// owns the draft so scene retirement/reload cannot spend or discard points.
struct Model {
  fo3loot::Cursor cursor;
  uint32_t owner=0;uint64_t revision=0;
  bool aDown=false,bDown=false,neutral=false,xArmed=false;int direction=0;double repeat=0;
  enum class Result {None,Edited,Confirmed};
  Result Step(fo3player::Player& player,double now,float x,float y,bool a,bool b,bool focused){
    const auto requested=player.Snapshot().specialMenuOwner;
    if(owner!=requested){*this={};owner=requested;cursor.Target(owner,8);aDown=a;bDown=b;++revision;return Result::None;}
    if(!owner)return Result::None;
    const bool pressA=a&&!aDown,pressB=b&&!bDown;aDown=a;bDown=b;
    if(!focused||!std::isfinite(now)||!std::isfinite(x)||!std::isfinite(y)){neutral=false;return Result::None;}
    if(!neutral){neutral=!a&&!b&&std::abs(x)<.3f&&std::abs(y)<.3f;return Result::None;}
    const auto selected=cursor.selected;cursor.Scroll(y,now,8);if(selected!=cursor.selected)++revision;
    if(std::abs(x)<.3f){xArmed=true;direction=0;}
    int delta=0;
    if(xArmed&&std::abs(x)>.65f){const int next=x>0?1:-1;
      if(next!=direction||now>=repeat){delta=next;repeat=now+(next!=direction?.35:.12);direction=next;}
    }
    if(cursor.selected==7){if(pressA&&player.ConfirmSpecialBook()){++revision;return Result::Confirmed;}return Result::None;}
    if(pressA)delta=1;else if(pressB)delta=-1;
    if(delta&&player.AdjustSpecialBook(unsigned(cursor.selected),delta)){++revision;return Result::Edited;}
    return Result::None;
  }
  std::vector<std::string> Choices(const fo3player::Player& player)const{
    static const char* fallback[]={"Strength","Perception","Endurance","Charisma","Intelligence","Agility","Luck"};
    std::vector<std::string> choices;
    for(unsigned i=0;i<7;++i){const auto& label=player.Definitions().pipboy.special[i].name;
      choices.push_back((label.empty()?fallback[i]:label)+std::string("  < ")+std::to_string(player.Snapshot().specialDraft[i])+" >");}
    choices.push_back(player.SpecialBookRemaining()==0?"Done":"Allocate all points to finish");return choices;
  }
};
}
