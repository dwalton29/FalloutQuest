#pragma once
#include <array>
#include "fo3-furniture.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
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
enum class Animation { Idle, TurnLeft, TurnRight, Conversation, Walk, Speaking, Aim, Attack, Reload, Hit, Death, Run, StepLeft, StepRight, StepBack, Count };
enum class CombatAction { Acquire, Pursue, Aim, Attack, Reload, Flee, Search, Reposition, Cover, Wait };
enum class Procedure { None, Executing, Waiting, Completed, Blocked, InvalidTarget, RouteFailed, Unsupported, Interrupted };
// Standalone steering policy, not recovered Bethesda engine constants.
struct MovementPolicy {
  static constexpr float TurnSpeed=3.f,Acceleration=4.f,Braking=3.f;
  static constexpr float StandingTurn=2.1f,CrowdRadius=70.f,CrowdHeight=90.f;
};
inline float Angle(float a){return std::atan2(std::sin(a),std::cos(a));}
struct RuntimeState {
  fo3furniture::State furniture;
  uint32_t sandboxLastTarget=0,sandboxCycles=0;
  double sandboxNext=0;
  double roamWaitUntil=0;
  double crowdBlockedSince=-1,sandboxBlockedUntil=0;
  float crowdProgressDistance=0;
  std::array<float,3> crowdDestination{};
  uint32_t sandboxBlockedTarget=0;
  double locomotionTime=0;
  // Local-only roaming memory: original PACK schedules and saved progress are untouched.
  std::array<float,3> lastRoamDestination{};
  bool hasLastRoamDestination=false;
  uint32_t reference=0,package=0,combatTarget=0,navigationDoor=0;
  // One active XTEL approach. No player scene transition is ever queued.
  uint32_t xtelDoor=0,xtelCell=0;
  bool offScene=false;
  Activity activity=Activity::Idle,suspended=Activity::Idle;
  Animation animation=Animation::Idle;
  std::array<float,3> position{},destination{};
  float yaw=0,authoredYaw=0,returnYaw=0,speed=0,headYaw=0,suspendedSpeed=0;
  bool dialogue=false,speaking=false,packageKnown=false;
  uint32_t suspendedPackage=0;
  float suspendedPackageWait=0;
  CombatAction action=CombatAction::Acquire;
  CombatAction combatMovement=CombatAction::Acquire;
  double coverUntil=-1;
  double nextManoeuvre=0,manoeuvreUntil=0;
  uint64_t equippedWeapon=0,actionSerial=0;
  std::array<float,3> lastThreat{},pathDestinationGame{};
  double nextDoorQuery=0,nextThink=0,nextPath=0,nextAttack=0,reloadUntil=0,lastSeen=0,deathAt=0,actionUntil=0;
  double actionStart=0,lastActionTime=0,pendingHit=0,hitUntil=0; // Nonlethal authored IDLE reaction gate.
  bool pendingAttack=false;
  uint32_t hitSequence=0; // Per-actor original hit IDLE A/B/C rotation, transient.
  uint32_t fireSequence=0,tacticSequence=0;
  double nextTactic=0,fleeUntil=0,burstUntil=0,burstWaitUntil=0;
  bool moraleFlee=false;
  Procedure procedure=Procedure::None;
  double blockedSince=-1,nextPackageEvaluation=0;
  uint64_t packageRevision=UINT64_MAX;
  int packageMinute=-1;
  uint32_t evaluatedPackage=0;
  std::array<float,3> evaluatedAnchor{};
  float evaluatedRadius=0;
  // Ephemeral route backoff, owned per actor and per package. A failed
  // priority entry must let other authored entries run until its next retry.
  std::unordered_map<uint32_t,double> packageRetryAfter;
  bool Alive() const {return activity!=Activity::Dying&&activity!=Activity::Dead&&activity!=Activity::Unconscious;}
  bool CanTalk() const {return Alive()&&activity!=Activity::Combat;}
  void BeginCombat(uint32_t target){
    if(!Alive())return;
    if(activity!=Activity::Combat){if(!dialogue){suspended=activity;suspendedPackage=package;}dialogue=false;speaking=false;}
    furniture.RequestExit();combatTarget=target;activity=Activity::Combat;speed=0;animation=Animation::Aim;action=CombatAction::Acquire;
    combatMovement=CombatAction::Acquire;coverUntil=-1;nextManoeuvre=manoeuvreUntil=0;crowdBlockedSince=-1;moraleFlee=false;fleeUntil=nextTactic=burstUntil=burstWaitUntil=0;
  }
  void EndCombat(){if(activity!=Activity::Combat)return;combatTarget=0;activity=suspended;package=suspendedPackage;speed=0;animation=Animation::Idle;reloadUntil=0;combatMovement=CombatAction::Acquire;coverUntil=-1;nextManoeuvre=manoeuvreUntil=0;crowdBlockedSince=-1;moraleFlee=false;fleeUntil=nextTactic=burstUntil=burstWaitUntil=0;blockedSince=-1;procedure=Procedure::Interrupted;nextPackageEvaluation=0;}
  void Die(double now){if(activity==Activity::Dying||activity==Activity::Dead)return;furniture.Clear();dialogue=speaking=false;combatTarget=0;activity=Activity::Dying;speed=0;animation=Animation::Death;deathAt=now;++actionSerial;}
  void BeginDialogue(){if(dialogue||!CanTalk())return;furniture.RequestExit();suspended=activity;suspendedPackage=package;suspendedSpeed=speed;returnYaw=yaw;dialogue=true;activity=Activity::Dialogue;speed=0;}
  void EndDialogue(){if(!dialogue)return;dialogue=false;speaking=false;if(activity!=Activity::Combat){activity=suspended;package=suspendedPackage;speed=suspendedSpeed;}animation=Animation::Idle;blockedSince=-1;procedure=Procedure::Interrupted;nextPackageEvaluation=0;}
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
