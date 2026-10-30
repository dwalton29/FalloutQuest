#pragma once
#include "player/fo3-vr-body.h"
namespace fo3weapon {
// VR presentation/access policy, not Fallout data. Manual loading is the
// normal mode; the developer toggle permits the original animated reload.
inline bool& ManualReloadEnabled(){static bool enabled=true;return enabled;}
inline bool InsertionZone(fo3vr::V hand,fo3vr::V centre,fo3vr::V half){
  const float radius=fo3vr::Length(half)+.30f;
  return fo3vr::Finite(hand)&&fo3vr::Finite(centre)&&fo3vr::Finite(half)&&fo3vr::Length(hand-centre)<=radius;
}
}
