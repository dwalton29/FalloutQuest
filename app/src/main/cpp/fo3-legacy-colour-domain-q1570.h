#pragma once

// Q15.7: exact legacy colour-domain A/B.
// Default keeps FalloutQuest's current linear-light pipeline.
// LEFT Y selects the legacy D3D9-style world-diffuse arithmetic inferred from
// Fallout3.exe + Shader Package 17:
//   authored WTHR RGB -> byte/255 (no sRGB decode)
//   authored BaseMap  -> encoded/display domain for the diffuse multiply
//   result             -> decoded back to linear only for Quest's scene buffer
//
// This is not a tint or replacement colour. LEFT X remains Q15.6's independent
// RAW -> FOG -> HDR -> FINAL stage isolator.

inline bool gFo3LegacyColourDomainQ1570 = false;

inline bool GetFo3LegacyColourDomainQ1570() {
    return gFo3LegacyColourDomainQ1570;
}

inline void ToggleFo3LegacyColourDomainQ1570() {
    gFo3LegacyColourDomainQ1570 = !gFo3LegacyColourDomainQ1570;
}

inline const char* GetFo3LegacyColourDomainNameQ1570() {
    return gFo3LegacyColourDomainQ1570
        ? "LEGACY_ENCODED_WORLD_DIFFUSE"
        : "CURRENT_LINEAR_WORLD_DIFFUSE";
}
