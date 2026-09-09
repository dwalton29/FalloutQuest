#pragma once

// Q15.0 reuses Q14.7's proven LEFT Y action/state. Q14.8's raw-WTHR staging
// experiment is dormant, and Q14.9 proved that removing world-light chroma
// removes Megaton's cyan cast. Q15.0 narrows that successful isolation: LEFT Y
// neutralizes only Ambient chroma at the same linear luminance while leaving the
// authored Q13.9/Q14.0 Sunlight RGB untouched on statics and LAND. Shader
// arithmetic stays unchanged.

inline bool gFo3LegacyPpDiffuseDomainQ1470 = false;

inline bool GetFo3LegacyPpDiffuseDomainQ1470() {
    return gFo3LegacyPpDiffuseDomainQ1470;
}

inline void ToggleFo3LegacyPpDiffuseDomainQ1470() {
    gFo3LegacyPpDiffuseDomainQ1470 = !gFo3LegacyPpDiffuseDomainQ1470;
}

inline const char* GetFo3PpDiffuseDomainNameQ1470() {
    return gFo3LegacyPpDiffuseDomainQ1470
        ? "NEUTRAL_AMBIENT_AUTHORED_SUNLIGHT"
        : "AUTHORED_WORLD_LIGHT_CHROMA";
}
