#pragma once
#include "fo3-script-spatial.h"
namespace fo3script {
enum class ActionKind { PlayGroup, ImageModifier, DefaultActivate, SayTo, Look };
struct Action {
  ActionKind kind=ActionKind::PlayGroup;
  uint32_t reference=0,form=0,flags=0,info=0;
  std::string group;
  double started=0;
};
using AnimationRegistry=std::unordered_map<uint32_t,std::vector<AnimationSequence>>;
}
