#pragma once

#include "fo3-pplighting-domain-q1470.h"

#include <algorithm>

// Q14.8 bridge: render translation units only see this tiny cache/getter layer.
// The native world renderer defines FO3_Q1480_DEFINE_REFRESH before including
// this header, after Q14.0's time-of-day header is already present. That keeps
// the large ESM/weather parser stack out of the generated LAND/CELL translation
// unit (where its helper names collide with cell-spawn helpers), while retaining
// the exact authored NAM0 endpoint samples and active Q14.0 TOD weights.
inline bool gFo3RawWeatherLightingReadyQ1480 = false;
inline float gFo3RawWeatherAmbientQ1480[3]{0.0f, 0.0f, 0.0f};
inline float gFo3RawWeatherSunlightQ1480[3]{0.0f, 0.0f, 0.0f};

void RefreshFo3RawWeatherLightingQ1480();

#ifdef FO3_Q1480_DEFINE_REFRESH
void RefreshFo3RawWeatherLightingQ1480() {
    const auto& runtime = fo3todq1400::gRuntime;
    if (!runtime.ready || !runtime.weather.valid || !runtime.weather.haveNam0) {
        gFo3RawWeatherLightingReadyQ1480 = false;
        return;
    }

    const fo3todq1400::TimeWeightsQ1400 weights =
        fo3todq1400::WeightsForHour(fo3todq1400::gTestHour, runtime.climate);

    for (int c = 0; c < 3; ++c) {
        float ambient = 0.0f;
        float sunlight = 0.0f;
        for (int tod = 0; tod < 4; ++tod) {
            const float w = weights.w[tod];
            ambient += runtime.weather.encoded[3][tod][c] * w;
            sunlight += runtime.weather.encoded[4][tod][c] * w;
        }
        gFo3RawWeatherAmbientQ1480[c] = ambient;
        gFo3RawWeatherSunlightQ1480[c] = sunlight;
    }

    // Q14.0 applies ImageSpace/IMAD sunlight dimming after WTHR RGB sampling.
    // Preserve that exact scalar so LEFT Y changes only byte RGB transfer.
    const float sunlightDimmer = std::clamp(
        gFo3ImageSpaceQ1280.hdrSunlightDimmer, 0.0f, 4.0f);
    for (int c = 0; c < 3; ++c) {
        gFo3RawWeatherSunlightQ1480[c] *= sunlightDimmer;
    }

    gFo3RawWeatherLightingReadyQ1480 = true;
}
#endif

inline bool GetFo3RawWeatherLightingQ1480(float outAmbient[3],
                                          float outSunlight[3]) {
    if (!outAmbient || !outSunlight || !gFo3RawWeatherLightingReadyQ1480) {
        return false;
    }
    for (int c = 0; c < 3; ++c) {
        outAmbient[c] = gFo3RawWeatherAmbientQ1480[c];
        outSunlight[c] = gFo3RawWeatherSunlightQ1480[c];
    }
    return true;
}

inline bool UseFo3RawWeatherLightingQ1480() {
    return GetFo3LegacyPpDiffuseDomainQ1470();
}
