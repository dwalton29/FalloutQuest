#pragma once

// Q14.5: one-bit runtime state for the exterior fog A/B.
// Default preserves Q14.4/Q14.3 behaviour. LEFT X toggles only whether the
// already-authored WTHR fog contribution is blended into static + LAND pixels.
// No weather RGB, light, ImageSpace, exposure, material or terrain state changes.

inline bool gFo3FogEnabledQ1450 = true;

inline bool GetFo3FogEnabledQ1450() {
    return gFo3FogEnabledQ1450;
}

inline void ToggleFo3FogEnabledQ1450() {
    gFo3FogEnabledQ1450 = !gFo3FogEnabledQ1450;
}

inline const char* GetFo3FogModeNameQ1450() {
    return gFo3FogEnabledQ1450 ? "AUTHORED_FOG" : "FOG_OFF";
}
