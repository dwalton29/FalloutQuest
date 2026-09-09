#pragma once

// Q14.8 reuses Q14.7's proven LEFT Y action/state, but the old legacy
// BaseMap re-encode experiment is no longer the active diagnostic. LEFT Y now
// switches only the WTHR Ambient/Sunlight constants supplied to both static
// BSShaderPPLighting and LAND between Q13.9 sRGB-decoded values and Fallout 3's
// raw normalized byte values. The shader arithmetic itself stays unchanged.

inline bool gFo3LegacyPpDiffuseDomainQ1470 = false;

inline bool GetFo3LegacyPpDiffuseDomainQ1470() {
    return gFo3LegacyPpDiffuseDomainQ1470;
}

inline void ToggleFo3LegacyPpDiffuseDomainQ1470() {
    gFo3LegacyPpDiffuseDomainQ1470 = !gFo3LegacyPpDiffuseDomainQ1470;
}

inline const char* GetFo3PpDiffuseDomainNameQ1470() {
    return gFo3LegacyPpDiffuseDomainQ1470
        ? "RAW_WTHR_BYTE_OVER_255"
        : "Q1390_SRGB_TO_LINEAR";
}
