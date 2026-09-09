#pragma once

// Q15.1 reuses Q14.7's proven LEFT Y action/state. Q14.9 proved that removing
// world-light chroma removes Megaton's cyan cast, and Q15.0 narrowed the fix to
// neutral Ambient while preserving authored warm Sunlight. Q15.1 keeps that
// colour split and reduces only the neutral Ambient luminance to 65% on statics
// and LAND to test the remaining flat/washed appearance. Shader arithmetic and
// all other rendering paths stay unchanged.

inline bool gFo3LegacyPpDiffuseDomainQ1470 = false;

inline bool GetFo3LegacyPpDiffuseDomainQ1470() {
    return gFo3LegacyPpDiffuseDomainQ1470;
}

inline void ToggleFo3LegacyPpDiffuseDomainQ1470() {
    gFo3LegacyPpDiffuseDomainQ1470 = !gFo3LegacyPpDiffuseDomainQ1470;
}

inline const char* GetFo3PpDiffuseDomainNameQ1470() {
    return gFo3LegacyPpDiffuseDomainQ1470
        ? "NEUTRAL_AMBIENT_65PCT_AUTHORED_SUNLIGHT"
        : "AUTHORED_WORLD_LIGHT_CHROMA";
}
