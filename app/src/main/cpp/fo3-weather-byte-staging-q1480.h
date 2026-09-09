#pragma once

#include "fo3-pplighting-domain-q1470.h"
#include "fo3-time-of-day-q1400.h"

#include <algorithm>

// Q14.8: expose the currently blended WTHR Ambient/Sunlight endpoints in the
// exact normalized-byte domain stored by Fallout3.esm. This deliberately does
// not inverse-transform the already-linear environment values: it samples the
// retained NAM0 endpoint bytes directly, preserving the active Q14.0 TOD weights.
// The existing ImageSpace/IMAD sunlight dimmer remains applied as the same scalar
// after endpoint blending, so LEFT Y isolates only RGB transfer/staging.
inline bool GetFo3RawWeatherLightingQ1480(float outAmbient[3],
                                          float outSunlight[3]) {
    using namespace fo3todq1400;
    if (!outAmbient || !outSunlight ||
        !gRuntime.ready || !gRuntime.weather.valid || !gRuntime.weather.haveNam0) {
        return false;
    }

    const TimeWeightsQ1400 weights = WeightsForHour(gTestHour, gRuntime.climate);
    for (int c = 0; c < 3; ++c) {
        outAmbient[c] = 0.0f;
        outSunlight[c] = 0.0f;
        for (int tod = 0; tod < 4; ++tod) {
            const float w = weights.w[tod];
            outAmbient[c] += gRuntime.weather.encoded[3][tod][c] * w;
            outSunlight[c] += gRuntime.weather.encoded[4][tod][c] * w;
        }
    }

    const float sunlightDimmer = std::clamp(
        gFo3ImageSpaceQ1280.hdrSunlightDimmer, 0.0f, 4.0f);
    for (float& c : *reinterpret_cast<float (*)[3]>(outSunlight)) {
        c *= sunlightDimmer;
    }
    return true;
}

inline bool UseFo3RawWeatherLightingQ1480() {
    return GetFo3LegacyPpDiffuseDomainQ1470();
}
