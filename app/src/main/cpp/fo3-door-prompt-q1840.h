#pragma once

#include "fo3-cell-traversal-q1700.h"

#include <cstddef>
#include <cstdint>

// Q16.12 engine-compatibility surface for Fallout 3's load-door interaction
// string. The nouns come from the user's Fallout3.esm FULL fields; this cache is
// populated while authored DOORs are being prepared, never by an every-frame
// ESM scan.
void CacheFo3DoorPromptQ1840(uint32_t sourceDoorRef,
                             uint32_t sourceDoorBase,
                             const Fo3DoorTeleport& teleport);

bool GetFo3DoorPromptQ1840(uint32_t sourceDoorRef,
                           char* outText,
                           size_t outCapacity);
