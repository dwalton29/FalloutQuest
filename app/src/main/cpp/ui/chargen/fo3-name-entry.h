#pragma once
#include "player/fo3-player-state.h"
#include "world/interaction/fo3-loot-cursor.h"
#include <string_view>
namespace fo3name {
// A controller keyboard for the original TextEditMenu. The canonical draft is
// saved by Player; losing tracking or restarting the app cannot discard it.
struct Model {
  static constexpr std::string_view Alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -'.";
  fo3loot::Cursor cursor;
  uint32_t owner=0;size_t letter=0;uint64_t revision=0;
  bool aDown=false,bDown=false,neutral=false,xArmed=false;int xDirection=0;double xRepeat=0;
  enum class Result {None,Edited,Confirmed};
  Result Step(fo3player::Player& player,double now,float x,float y,bool a,bool b,bool focused){
    const auto requested=player.Snapshot().nameMenuOwner;
    if(owner!=requested){*this={};owner=requested;cursor.Target(owner,3);aDown=a;bDown=b;++revision;return Result::None;}
    if(!owner)return Result::None;
    const bool pressA=a&&!aDown,pressB=b&&!bDown;aDown=a;bDown=b;
    if(!focused||!std::isfinite(now)||!std::isfinite(x)||!std::isfinite(y)){neutral=false;return Result::None;}
    if(!neutral){neutral=!a&&!b&&std::abs(x)<.3f&&std::abs(y)<.3f;return Result::None;}
    const auto selected=cursor.selected;cursor.Scroll(y,now,3);if(selected!=cursor.selected)++revision;
    if(std::abs(x)<.3f){xArmed=true;xDirection=0;}
    if(cursor.selected==0&&xArmed&&std::abs(x)>.65f){const int direction=x>0?1:-1;
      if(direction!=xDirection||now>=xRepeat){letter=(letter+Alphabet.size()+direction)%Alphabet.size();++revision;xRepeat=now+(direction!=xDirection?.35:.12);xDirection=direction;}
    }
    auto draft=player.Snapshot().nameDraft;
    if(pressB||(pressA&&cursor.selected==1)){
      if(draft.empty())return Result::None;
      draft.pop_back();
    }else if(pressA&&cursor.selected==0){if(draft.size()>=32)return Result::None;draft.push_back(Alphabet[letter]);}
    else if(pressA&&cursor.selected==2){if(player.ConfirmPlayerName()){++revision;return Result::Confirmed;}return Result::None;}
    else return Result::None;
    if(player.EditPlayerName(draft)){++revision;return Result::Edited;}return Result::None;
  }
  std::vector<std::string> Choices()const{
    const std::string key=Alphabet[letter]==' '?"Space":std::string(1,Alphabet[letter]);
    return {"< "+key+" >", "Delete", "Done"};
  }
};
} // namespace fo3name
