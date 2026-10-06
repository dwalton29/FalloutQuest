#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace fo3npc {
// Spatial policy for VR, not Bethesda package or animation semantics.
struct DialoguePolicy {
  static constexpr float TalkRange=3.f,LeaveRange=5.f;
  static constexpr float PanelForward=.25f,PanelBelowHead=.28f;
  static constexpr float BodyThreshold=.55f,BodySpeed=1.15f,HeadLimit=.6f,HeadSpeed=1.8f;
  static constexpr float BlendSeconds=.3f,AnchorTime=.14f;
  static constexpr const char* SpeakingIdle="LooseTalkToPlayerLHCasualB100";
  static constexpr const char* ConversationIdle="LooseListenToPlayerRelaxedB";
};
enum class Activity { Idle, Package, Dialogue, Combat };
enum class Animation { Idle, TurnLeft, TurnRight, Conversation, Walk, Speaking, Count };
inline float Angle(float a){return std::atan2(std::sin(a),std::cos(a));}
struct RuntimeState {
  uint32_t reference=0,package=0,combatTarget=0;
  Activity activity=Activity::Idle,suspended=Activity::Idle;
  Animation animation=Animation::Idle;
  std::array<float,3> position{},destination{};
  float yaw=0,authoredYaw=0,returnYaw=0,speed=0,headYaw=0,suspendedSpeed=0;
  bool dialogue=false,speaking=false,packageKnown=false;
  uint32_t suspendedPackage=0;
  void BeginDialogue(){if(dialogue)return;suspended=activity;suspendedPackage=package;suspendedSpeed=speed;returnYaw=yaw;dialogue=true;activity=Activity::Dialogue;speed=0;}
  void EndDialogue(){if(!dialogue)return;dialogue=false;speaking=false;if(activity!=Activity::Combat){activity=suspended;package=suspendedPackage;speed=suspendedSpeed;}animation=Animation::Idle;}
  void Face(float desired,float dt){
    const float error=Angle((dialogue?desired:returnYaw)-yaw);
    const bool turn=dialogue?std::fabs(error)>DialoguePolicy::BodyThreshold:std::fabs(error)>.01f;
    if(turn)yaw=Angle(yaw+std::clamp(error,-DialoguePolicy::BodySpeed*dt,DialoguePolicy::BodySpeed*dt));
    const float look=dialogue?std::clamp(Angle(desired-yaw),-DialoguePolicy::HeadLimit,DialoguePolicy::HeadLimit):0;
    headYaw+=std::clamp(look-headYaw,-DialoguePolicy::HeadSpeed*dt,DialoguePolicy::HeadSpeed*dt);
    animation=dialogue?(turn?(error>0?Animation::TurnLeft:Animation::TurnRight):(speaking?Animation::Speaking:Animation::Conversation)):Animation::Idle;
  }
};
inline bool Interrupted(bool loading,bool resident,bool alive,bool combat,float distance){return loading||!resident||!alive||combat||!std::isfinite(distance)||distance>DialoguePolicy::LeaveRange;}
}
