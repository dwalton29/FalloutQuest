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
enum class Activity { Idle, Package, Dialogue, Combat, Dying, Dead, Unconscious };
enum class Animation { Idle, TurnLeft, TurnRight, Conversation, Walk, Speaking, Aim, Attack, Reload, Hit, Death, Count };
enum class CombatAction { Acquire, Pursue, Aim, Attack, Reload, Flee, Search };
inline float Angle(float a){return std::atan2(std::sin(a),std::cos(a));}
struct RuntimeState {
  uint32_t reference=0,package=0,combatTarget=0;
  Activity activity=Activity::Idle,suspended=Activity::Idle;
  Animation animation=Animation::Idle;
  std::array<float,3> position{},destination{};
  float yaw=0,authoredYaw=0,returnYaw=0,speed=0,headYaw=0,suspendedSpeed=0;
  bool dialogue=false,speaking=false,packageKnown=false;
  uint32_t suspendedPackage=0;
  CombatAction action=CombatAction::Acquire;
  uint64_t equippedWeapon=0,actionSerial=0;
  std::array<float,3> lastThreat{},pathDestinationGame{};
  double nextThink=0,nextPath=0,nextAttack=0,reloadUntil=0,lastSeen=0,deathAt=0,actionUntil=0;
  double actionStart=0,lastActionTime=0,pendingHit=0;
  bool pendingAttack=false;
  bool Alive() const {return activity!=Activity::Dying&&activity!=Activity::Dead&&activity!=Activity::Unconscious;}
  bool CanTalk() const {return Alive()&&activity!=Activity::Combat;}
  void BeginCombat(uint32_t target){
    if(!Alive())return;
    if(activity!=Activity::Combat){if(!dialogue){suspended=activity;suspendedPackage=package;}dialogue=false;speaking=false;}
    combatTarget=target;activity=Activity::Combat;speed=0;animation=Animation::Aim;action=CombatAction::Acquire;
  }
  void EndCombat(){if(activity!=Activity::Combat)return;combatTarget=0;activity=suspended;package=suspendedPackage;speed=0;animation=Animation::Idle;reloadUntil=0;}
  void Die(double now){if(activity==Activity::Dying||activity==Activity::Dead)return;dialogue=speaking=false;combatTarget=0;activity=Activity::Dying;speed=0;animation=Animation::Death;deathAt=now;++actionSerial;}
  void BeginDialogue(){if(dialogue||!CanTalk())return;suspended=activity;suspendedPackage=package;suspendedSpeed=speed;returnYaw=yaw;dialogue=true;activity=Activity::Dialogue;speed=0;}
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
