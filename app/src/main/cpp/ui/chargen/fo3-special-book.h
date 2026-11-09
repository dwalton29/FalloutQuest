#pragma once
#include "player/fo3-player-state.h"
#include "world/interaction/fo3-loot-cursor.h"
namespace fo3special {
constexpr unsigned IndexPage=8, BackCover=9;
inline constexpr std::array<const char*,BackCover> ForwardClips={
  "Forward","Backward","Left","Right","FastForward","FastBackward","FastLeft","FastRight","TurnLeft"};
inline constexpr std::array<const char*,IndexPage> ReverseClips={
  "Attack2","Attack1","AimUp","Aim","Holster","JumpLand","JumpLoop","JumpStart"};
struct BookTiming {
  std::array<float,ForwardClips.size()> forward{};
  std::array<float,ReverseClips.size()> reverse{};
};
// Controller presentation for the script-supplied SPECIAL total. The player
// owns the draft so scene retirement/reload cannot spend or discard points.
struct Model {
  fo3loot::Cursor cursor;
  uint32_t owner=0;uint64_t revision=0;
  bool aDown=false,bDown=false,neutral=false,xArmed=false;int direction=0;double repeat=0;
  bool authored=false,leftDown=false,rightDown=false,finishDown=false;
  unsigned page=0;
  bool forward=true;
  float turnTime=0,turnDuration=0;
  double turnStarted=0;
  enum class Result {None,Edited,Confirmed,Turned};
  Result Step(fo3player::Player& player,double now,float x,float y,bool a,bool b,bool focused,
              const BookTiming* book=nullptr,bool left=false,bool right=false,bool finish=false){
    const auto requested=player.Snapshot().specialMenuOwner;
    if(owner!=requested){*this={};owner=requested;authored=book!=nullptr;cursor.Target(owner,8);aDown=a;bDown=b;++revision;return Result::None;}
    if(!owner)return Result::None;
    const bool pressA=a&&!aDown,pressB=b&&!bDown;aDown=a;bDown=b;
    const bool previous=left&&!leftDown,next=right&&!rightDown,done=finish&&!finishDown;
    leftDown=left;rightDown=right;finishDown=finish;
    if(!focused||!std::isfinite(now)||!std::isfinite(x)||!std::isfinite(y)){neutral=false;return Result::None;}
    if(!neutral){neutral=!a&&!b&&!left&&!right&&!finish&&std::abs(x)<.3f&&std::abs(y)<.3f;return Result::None;}
    if(authored&&book){
      turnTime=std::clamp(float(now-turnStarted),0.f,turnDuration);
      if(turnTime<turnDuration)return Result::None;
      if(page==BackCover){
        if(player.ConfirmSpecialBook()){++revision;return Result::Confirmed;}
        return Result::None;
      }
      unsigned target=page;
      if(previous&&page>0)--target;
      else if((next||(pressA&&page==0))&&page<IndexPage)++target;
      else if(page==IndexPage&&player.SpecialBookRemaining()==0&&(done||(pressA&&cursor.selected==7)))target=BackCover;
      if(target!=page){
        forward=target>page;
        page=target;turnStarted=now;turnTime=0;
        turnDuration=forward?book->forward[page-1]:book->reverse[page];
        ++revision;return Result::Turned;
      }
      if(!page)return Result::None;
    }
    if(!authored||page==IndexPage){
      const auto selected=cursor.selected;cursor.Scroll(y,now,8);if(selected!=cursor.selected)++revision;
    }
    if(std::abs(x)<.3f){xArmed=true;direction=0;}
    int delta=0;
    if(xArmed&&std::abs(x)>.65f){const int next=x>0?1:-1;
      if(next!=direction||now>=repeat){delta=next;repeat=now+(next!=direction?.35:.12);direction=next;}
    }
    const auto selected=authored&&page<IndexPage?page-1:cursor.selected;
    if(selected==7){if(!authored&&pressA&&player.ConfirmSpecialBook()){++revision;return Result::Confirmed;}return Result::None;}
    if(pressA)delta=1;else if(pressB)delta=-1;
    if(delta&&player.AdjustSpecialBook(unsigned(selected),delta)){++revision;return Result::Edited;}
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
