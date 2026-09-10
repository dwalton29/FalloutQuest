#pragma once

// Q15.4 reuses Q14.5's proven LEFT X one-bit state as a PPLighting-path
// selector. Fog is no longer controlled by this state: Q15.4 forces the same
// Q15.2 authored per-fragment fog in both modes.
//
// true/default = current FalloutQuest world-space mapped-normal Lambert path.
// false        = Shader Package 17 SLS1011 tangent-space LightData path.
//
// Legacy getter/function names stay intact so the established renderer,
// terrain bridge and OpenXR input wiring do not need another state system.

inline bool gFo3FogEnabledQ1450 = true;

inline bool GetFo3FogEnabledQ1450() {
    return gFo3FogEnabledQ1450;
}

inline void ToggleFo3FogEnabledQ1450() {
    gFo3FogEnabledQ1450 = !gFo3FogEnabledQ1450;
}

inline const char* GetFo3FogModeNameQ1450() {
    return gFo3FogEnabledQ1450
        ? "QUEST_WORLD_NORMAL_LIGHT"
        : "SP17_TANGENT_LIGHT";
}
