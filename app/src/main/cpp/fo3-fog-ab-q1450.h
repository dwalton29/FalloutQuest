#pragma once

// Q15.3 reuses Q14.5's proven LEFT X one-bit state as a fog-placement A/B.
// true  = FalloutQuest's current per-fragment nonlinear fog evaluation.
// false = Fallout 3 Shader Package 17-style per-vertex evaluation followed by
//         rasterizer interpolation. WTHR fog colour/near/far/power are identical
// in both modes; only evaluation placement changes.
//
// Legacy getter names are intentionally retained so the existing static/LAND
// bridge and OpenXR input wiring stay untouched.
inline bool gFo3FogEnabledQ1450 = true;

inline bool GetFo3FogEnabledQ1450() {
    return gFo3FogEnabledQ1450;
}

inline void ToggleFo3FogEnabledQ1450() {
    gFo3FogEnabledQ1450 = !gFo3FogEnabledQ1450;
}

inline const char* GetFo3FogModeNameQ1450() {
    return gFo3FogEnabledQ1450 ? "QUEST_FRAGMENT_FOG" : "VANILLA_VERTEX_FOG";
}
