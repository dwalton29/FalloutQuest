#pragma once

#include "fo3-weather-imad-q1300.h"

#include <algorithm>
#include <cmath>

namespace fo3weatherq1320 {

inline float SafeScaleColourQ1320(float value, float scale) {
    if (!std::isfinite(value) || !std::isfinite(scale)) return 0.0f;
    return std::clamp(value * scale, 0.0f, 4.0f);
}

inline bool ApplyWeatherLightingQ1320(Fo3ImageSpaceQ1280& image) {
    using namespace fo3envq1000;
    using namespace fo3imadq1300;

    if (!image.valid || image.weatherDayImadFormId == 0u) return false;

    WeatherImadQ1300 modifier;
    if (!LoadWeatherImadQ1300(image.weatherDayImadFormId, modifier) || !modifier.valid) {
        return false;
    }

    // FO3's base IMGS carries HDR Sunlight Dimmer and LUM Ramp No Tex. The
    // weather IMAD modifies those two values using the documented 06IAD/FIAD
    // (sunlight scale) and 07IAD/GIAD (sky scale) multiply/add channels.
    const float baseSunlightDimmer = image.hdrSunlightDimmer;
    const float baseSkyScale = image.hdrLumRampNoTex;
    const float finalSunlightDimmer = ApplyScalarQ1300(
        baseSunlightDimmer,
        modifier.sunlightScaleMult,
        modifier.sunlightScaleAdd,
        0.0f, 4.0f);
    const float finalSkyScale = ApplyScalarQ1300(
        baseSkyScale,
        modifier.skyScaleMult,
        modifier.skyScaleAdd,
        0.0f, 4.0f);

    image.hdrSunlightDimmer = finalSunlightDimmer;
    image.hdrLumRampNoTex = finalSkyScale;

    Fo3EnvironmentQ1000& env = gFo3EnvironmentQ1000;
    if (!env.valid) {
        __android_log_print(ANDROID_LOG_WARN, TAG,
                            "Q13.2 WEATHER LIGHT: IMAD=%08X result=no-active-environment",
                            modifier.formId);
        return false;
    }

    float sunlightBefore[3]{env.sunlight[0], env.sunlight[1], env.sunlight[2]};
    float skyUpperBefore[3]{env.skyUpper[0], env.skyUpper[1], env.skyUpper[2]};
    float skyLowerBefore[3]{env.skyLower[0], env.skyLower[1], env.skyLower[2]};
    float horizonBefore[3]{env.horizon[0], env.horizon[1], env.horizon[2]};

    for (int c = 0; c < 3; ++c) {
        // Sunlight Dimmer affects the authored directional sunlight used by
        // statics and LAND. Ambient/local LIGH colours remain untouched.
        env.sunlight[c] = SafeScaleColourQ1320(env.sunlight[c], finalSunlightDimmer);

        // LUM Ramp No Tex / Sky Scale drives sky brightness. Our current sky is
        // the WTHR-authored gradient, so scale each gradient colour uniformly.
        // Fog is deliberately not scaled: it has its own authored WTHR colour.
        env.skyUpper[c] = SafeScaleColourQ1320(env.skyUpper[c], finalSkyScale);
        env.skyLower[c] = SafeScaleColourQ1320(env.skyLower[c], finalSkyScale);
        env.horizon[c] = SafeScaleColourQ1320(env.horizon[c], finalSkyScale);
    }

    __android_log_print(
        ANDROID_LOG_INFO, TAG,
        "Q13.2 WEATHER LIGHT APPLY: IMAD=%08X EDID=%s baseSunDimmer=%.3f sunMultPresent=%d sunMult=%.3f sunAddPresent=%d sunAdd=%.3f finalSunDimmer=%.3f baseSkyScale=%.3f skyMultPresent=%d skyMult=%.3f skyAddPresent=%d skyAdd=%.3f finalSkyScale=%.3f sunlight=(%.3f %.3f %.3f)->(%.3f %.3f %.3f) skyUpper=(%.3f %.3f %.3f)->(%.3f %.3f %.3f) skyLower=(%.3f %.3f %.3f)->(%.3f %.3f %.3f) horizon=(%.3f %.3f %.3f)->(%.3f %.3f %.3f)",
        modifier.formId,
        modifier.editorId.empty() ? "<none>" : modifier.editorId.c_str(),
        baseSunlightDimmer,
        modifier.sunlightScaleMult.present ? 1 : 0,
        modifier.sunlightScaleMult.present ? modifier.sunlightScaleMult.value : 1.0f,
        modifier.sunlightScaleAdd.present ? 1 : 0,
        modifier.sunlightScaleAdd.present ? modifier.sunlightScaleAdd.value : 0.0f,
        finalSunlightDimmer,
        baseSkyScale,
        modifier.skyScaleMult.present ? 1 : 0,
        modifier.skyScaleMult.present ? modifier.skyScaleMult.value : 1.0f,
        modifier.skyScaleAdd.present ? 1 : 0,
        modifier.skyScaleAdd.present ? modifier.skyScaleAdd.value : 0.0f,
        finalSkyScale,
        sunlightBefore[0], sunlightBefore[1], sunlightBefore[2],
        env.sunlight[0], env.sunlight[1], env.sunlight[2],
        skyUpperBefore[0], skyUpperBefore[1], skyUpperBefore[2],
        env.skyUpper[0], env.skyUpper[1], env.skyUpper[2],
        skyLowerBefore[0], skyLowerBefore[1], skyLowerBefore[2],
        env.skyLower[0], env.skyLower[1], env.skyLower[2],
        horizonBefore[0], horizonBefore[1], horizonBefore[2],
        env.horizon[0], env.horizon[1], env.horizon[2]);
    return true;
}

} // namespace fo3weatherq1320

inline bool LoadFo3ImageSpaceQ1320(uint32_t cellFormId, uint32_t worldspaceFormId) {
    const bool ready = LoadFo3ImageSpaceQ1300(cellFormId, worldspaceFormId);
    if (!ready) return false;
    fo3weatherq1320::ApplyWeatherLightingQ1320(gFo3ImageSpaceQ1280);
    return gFo3ImageSpaceQ1280.valid;
}
