#pragma once
#include "fo3-player-state.h"
namespace fo3player {
// VR substitutes for the original early character-generation first-person rig.
// The original control scripts restore presentation when Pip-Boy access returns.
inline bool EarlyFirstPerson(const Player& player){return player.Snapshot().inCharGen&&player.PlayerControlDisabled(1);}
}
