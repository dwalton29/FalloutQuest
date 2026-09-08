#pragma once

// Q14.7: runtime A/B for the static BSShaderPPLighting world-light diffuse
// colour domain. Default A preserves the established FalloutQuest linear-light
// path. LEFT Y switches only BaseMap + WTHR Ambient/Sunlight diffuse arithmetic
// to a legacy encoded-domain emulation, then decodes that result back to the
// renderer's linear buffer before specular/local lights/emissive/fog/post.

inline bool gFo3LegacyPpDiffuseDomainQ1470 = false;

inline bool GetFo3LegacyPpDiffuseDomainQ1470() {
    return gFo3LegacyPpDiffuseDomainQ1470;
}

inline void ToggleFo3LegacyPpDiffuseDomainQ1470() {
    gFo3LegacyPpDiffuseDomainQ1470 = !gFo3LegacyPpDiffuseDomainQ1470;
}

inline const char* GetFo3PpDiffuseDomainNameQ1470() {
    return gFo3LegacyPpDiffuseDomainQ1470
        ? "LEGACY_ENCODED_DIFFUSE"
        : "CURRENT_LINEAR_DIFFUSE";
}
