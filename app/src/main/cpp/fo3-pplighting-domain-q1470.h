#pragma once

// Q14.9 reuses Q14.7's proven LEFT Y action/state. Q14.8's raw-WTHR staging
// experiment is now dormant after device testing showed it only made the same
// blue-biased lighting brighter. LEFT Y now removes only the chroma from the
// current Q13.9/Q14.0 Ambient + Sunlight constants on statics and LAND while
// preserving each light's linear luminance. Shader arithmetic stays unchanged.

inline bool gFo3LegacyPpDiffuseDomainQ1470 = false;

inline bool GetFo3LegacyPpDiffuseDomainQ1470() {
    return gFo3LegacyPpDiffuseDomainQ1470;
}

inline void ToggleFo3LegacyPpDiffuseDomainQ1470() {
    gFo3LegacyPpDiffuseDomainQ1470 = !gFo3LegacyPpDiffuseDomainQ1470;
}

inline const char* GetFo3PpDiffuseDomainNameQ1470() {
    return gFo3LegacyPpDiffuseDomainQ1470
        ? "NEUTRAL_WORLD_LIGHT_CHROMA"
        : "AUTHORED_WORLD_LIGHT_CHROMA";
}
