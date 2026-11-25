#pragma once
#include "fo3-script-spatial.h"
namespace fo3script {
enum class ActionKind { StopCombat, PipboyReset, RadioRefresh, PackageIdle, KillQuestUpdates, AutoSave, PlayGroup, ImageModifier, RemoveImageModifier, ScreenBlood, DefaultActivate, SayTo, Look, Conversation, WeaponHit, SoundAt, RadioConversation, UseWeapon, ActivationParents, PlayIdle, Explosion, DestructionModel };
struct Action {
  ActionKind kind=ActionKind::PlayGroup;
  uint32_t reference=0,form=0,flags=0,info=0,package=0;
  bool forceSubtitle=false,noTargetLook=false;
  uint8_t activationDepth=0;
  std::string group;
  double started=0;
  uint32_t sourceActor=0,targetActor=0;
  uint32_t cell=0,world=0;
  std::array<float,3> position{};
};
using AnimationRegistry=std::unordered_map<uint32_t,std::vector<AnimationSequence>>;
}
