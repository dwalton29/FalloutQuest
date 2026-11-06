#pragma once
#include "player/fo3-player-state.h"
#include "player/fo3-tag-skills.h"
#include "world/interaction/fo3-loot-cursor.h"
namespace fo3tagmenu {
struct Model {
  fo3loot::Cursor cursor;uint32_t owner=0;uint64_t revision=0;bool aDown=false,bDown=false,neutral=false;
  enum class Result {None,Edited,Confirmed};
  static unsigned SkillAt(size_t row){return unsigned(row<12?row:13);}
  Result Step(fo3player::Player& player,double now,float y,bool a,bool b,bool focused){
    const auto requested=player.Snapshot().tagMenuOwner;
    if(owner!=requested){*this={};owner=requested;cursor.Target(owner,14);aDown=a;bDown=b;++revision;return Result::None;}
    if(!owner)return Result::None;
    const bool press=a&&!aDown;aDown=a;bDown=b;
    if(!focused||!std::isfinite(now)||!std::isfinite(y)){neutral=false;return Result::None;}
    if(!neutral){neutral=!a&&!b&&std::abs(y)<.3f;return Result::None;}
    const auto before=cursor.selected;cursor.Scroll(y,now,14);if(before!=cursor.selected)++revision;
    if(!press)return Result::None;
    if(cursor.selected==13){if(player.ConfirmTagSkills()){++revision;return Result::Confirmed;}}
    else if(player.ToggleTagSkill(SkillAt(cursor.selected))){++revision;return Result::Edited;}
    return Result::None;
  }
  std::vector<std::string> Choices(const fo3player::Player& player)const{
    static const char* fallback[]={"Barter","Big Guns","Energy Weapons","Explosives","Lockpick","Medicine","Melee Weapons","Repair","Science","Small Guns","Sneak","Speech","Throwing","Unarmed"};
    std::vector<std::string> rows;const auto& state=player.Snapshot();
    for(size_t row=0;row<13;++row){const auto skill=SkillAt(row);const bool selected=state.tagDraft&(1u<<skill);const auto& label=player.Definitions().pipboy.skills[skill].name;
      const int base=std::max(0,int(state.skills[skill])-int(state.tagBonuses[skill]));const int value=std::min(100,base+(selected?int(std::lround(player.Definitions().rules.tagSkillBonus)):0));
      rows.push_back(std::string(selected?"[+] ":"[ ] ")+(label.empty()?fallback[skill]:label)+"  "+std::to_string(value));
    }
    rows.push_back(player.TagSkillsRemaining()==0?"Done":"Select all tag skills to finish");return rows;
  }
};
}
