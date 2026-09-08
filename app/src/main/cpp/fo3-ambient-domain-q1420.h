#pragma once

#include "fo3-time-of-day-q1400.h"

#include <algorithm>
#include <cmath>

// Q14.2: controlled legacy-ambient colour-domain A/B.
//
// Q13.9 decodes every WTHR RGB byte through sRGB before it enters the linear
// renderer. Xbox/vanilla comparison shows that this makes fully shadowed
// Megaton surfaces overwhelmingly cyan. This test changes ONE input only:
// WTHR Ambient (NAM0 colour class 3) is kept in its authored normalized-byte
// domain while sky/fog/sunlight/sun/clouds/LIGH/XEMI remain on the Q13.9 path.
// No hue shift, desaturation, brown tint, LUT, shadow-strength change or manual
// Fallout colour is applied here.

namespace fo3ambientq1420 {

constexpr const char* TAG = "FalloutQuest";
inline uint32_t gLoggedWeather = 0u;
inline int gLastLoggedHour = -1;

inline void SampleRawAmbient(const fo3todq1400::WeatherTimeQ1400& weather,
                             const fo3todq1400::TimeWeightsQ1400& weights,
                             float out[3]) {
    out[0] = out[1] = out[2] = 0.0f;
    if (!weather.haveNam0) return;
    for (int tod = 0; tod < 4; ++tod) {
        const float w = weights.w[tod];
        for (int c = 0; c < 3; ++c) {
            out[c] += weather.encoded[3][tod][c] * w;
        }
    }
}

} // namespace fo3ambientq1420

inline void ApplyFo3AmbientDomainQ1420() {
    using namespace fo3ambientq1420;
    using namespace fo3todq1400;

    if (!gRuntime.ready || !gRuntime.weather.haveNam0 ||
        !gFo3EnvironmentQ1000.valid) {
        return;
    }

    const TimeWeightsQ1400 weights = WeightsForHour(gTestHour, gRuntime.climate);
    float raw[3]{};
    float q139Linear[3]{};
    SampleRawAmbient(gRuntime.weather, weights, raw);
    SampleLinearRgb(gRuntime.weather, 3, weights, q139Linear);

    for (int c = 0; c < 3; ++c) {
        gFo3EnvironmentQ1000.ambient[c] = raw[c];
    }

    if (gLoggedWeather != gRuntime.weather.formId) {
        gLoggedWeather = gRuntime.weather.formId;
        gLastLoggedHour = -1;
        __android_log_print(
            ANDROID_LOG_INFO, TAG,
            "Q14.2 AMBIENT DOMAIN READY: weather=%08X EDID=%s source=Fallout3.esm NAM0class=Ambient activeDomain=raw-normalized-byte q139Comparison=sRGB-to-linear onlyAmbientChanged=1 filter=none tint=none",
            gRuntime.weather.formId,
            gRuntime.weather.editorId.empty() ? "<none>" : gRuntime.weather.editorId.c_str());
    }

    const int hourBucket = static_cast<int>(std::floor(gTestHour));
    if (hourBucket != gLastLoggedHour) {
        gLastLoggedHour = hourBucket;
        __android_log_print(
            ANDROID_LOG_INFO, TAG,
            "Q14.2 AMBIENT: hour=%05.2f phase=%s encoded=(%.3f %.3f %.3f) q139Linear=(%.3f %.3f %.3f) active=(%.3f %.3f %.3f) source=WTHR/NAM0 noOtherColorPathChanged=1",
            gTestHour, weights.phase,
            raw[0], raw[1], raw[2],
            q139Linear[0], q139Linear[1], q139Linear[2],
            gFo3EnvironmentQ1000.ambient[0],
            gFo3EnvironmentQ1000.ambient[1],
            gFo3EnvironmentQ1000.ambient[2]);
    }
}
