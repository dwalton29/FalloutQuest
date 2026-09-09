#pragma once

// Q15.3b reuses Q14.5's proven LEFT X state as a fog-path A/B selector.
// true/default = known-good Q15.2 per-fragment world/eye-distance fog.
// false        = Shader Package 17 projected-XYZ per-vertex/interpolated fog.
// WTHR fog RGB/near/far/power are identical in both modes.
//
// Legacy getter names stay intact so the existing renderer/OpenXR wiring remains
// unchanged; only the meaning of the false state changes from FOG_OFF to SP17.

inline bool gFo3FogEnabledQ1450 = true;

inline bool GetFo3FogEnabledQ1450() {
    return gFo3FogEnabledQ1450;
}

inline void ToggleFo3FogEnabledQ1450() {
    gFo3FogEnabledQ1450 = !gFo3FogEnabledQ1450;
}

inline const char* GetFo3FogModeNameQ1450() {
    return gFo3FogEnabledQ1450
        ? "QUEST_FRAGMENT_FOG"
        : "SP17_PROJECTED_VERTEX_FOG";
}
