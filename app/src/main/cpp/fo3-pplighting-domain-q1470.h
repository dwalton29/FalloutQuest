#pragma once

// Q15.5: authored Fallout world lighting only.
//
// LEFT Y was previously reused by a chain of renderer diagnostics (legacy
// encoded-domain diffuse, raw WTHR byte staging, neutral world-light chroma,
// neutral Ambient + authored Sunlight, then neutral Ambient at 65%). Those were
// useful to isolate the cyan carrier, but they are not vanilla lighting and must
// not remain selectable while we diagnose final fidelity.
//
// Keep the legacy symbol names so the existing CMake patch chain and OpenXR
// action plumbing continue to compile, but permanently pin the state to the
// authored path. LEFT Y is therefore a no-op from Q15.5 onward.

inline bool gFo3LegacyPpDiffuseDomainQ1470 = false;

inline bool GetFo3LegacyPpDiffuseDomainQ1470() {
    return false;
}

inline void ToggleFo3LegacyPpDiffuseDomainQ1470() {
    // Q15.5: intentionally disabled. Authored lighting is the only valid state.
    gFo3LegacyPpDiffuseDomainQ1470 = false;
}

inline const char* GetFo3PpDiffuseDomainNameQ1470() {
    return "AUTHORED_WORLD_LIGHT_ONLY";
}
