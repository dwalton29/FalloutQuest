#pragma once

#include <algorithm>

// Q14.8 bridge: render translation units only see this tiny cache/getter layer.
// The native world renderer defines FO3_DEFINE_WEATHER_REFRESH before including
// this header, after Q14.0's time-of-day header is already present. That keeps
// the large ESM/weather parser stack out of the generated LAND/CELL translation
// unit (where its helper names collide with cell-spawn helpers), while retaining
// the exact authored NAM0 endpoint samples and active Q14.0 TOD weights.
inline bool gFo3RawWeatherLightingReady = false;
inline float gFo3RawWeatherAmbient[3]{0.0f, 0.0f, 0.0f};
inline float gFo3RawWeatherSunlight[3]{0.0f, 0.0f, 0.0f};
inline float gFo3RawWeatherFog[3]{0.0f, 0.0f, 0.0f};
inline float gFo3LinearWeatherSunlight[3]{0.0f, 0.0f, 0.0f};

void RefreshFo3RawWeatherLighting();

#ifdef FO3_DEFINE_WEATHER_REFRESH
void RefreshFo3RawWeatherLighting() {
    const auto& runtime = fo3tod::gRuntime;
    if (!runtime.ready || !runtime.weather.valid || !runtime.weather.haveNam0) {
        gFo3RawWeatherLightingReady = false;
        return;
    }

    const fo3tod::TimeWeights weights =
        fo3tod::WeightsForHour(fo3tod::gTestHour, runtime.climate);

    for (int c = 0; c < 3; ++c) {
        float ambient = 0.0f;
        float sunlight = 0.0f;
        float fog = 0.0f;
        float linearSunlight = 0.0f;
        for (int tod = 0; tod < 4; ++tod) {
            const float w = weights.w[tod];
            ambient += runtime.weather.encoded[3][tod][c] * w;
            sunlight += runtime.weather.encoded[4][tod][c] * w;
            fog += runtime.weather.encoded[1][tod][c] * w;
            linearSunlight += fo3color::SrgbToLinear(
                runtime.weather.encoded[4][tod][c]) * w;
        }
        // PC D3D9 LAND captures prove AmbientColor, PSLightColor's authored
        // chroma, and FogColor are supplied from WTHR in the raw normalized
        // byte/display domain. Keep these staging values unscaled; callers that
        // need the existing Q14 sunlight scalar can recover it from the staged
        // linear sample and the live environment.
        gFo3RawWeatherAmbient[c] = ambient;
        gFo3RawWeatherSunlight[c] = sunlight;
        gFo3RawWeatherFog[c] = fog;
        gFo3LinearWeatherSunlight[c] = linearSunlight;
    }

    gFo3RawWeatherLightingReady = true;
}
#endif

inline bool GetFo3RawWeatherLighting(float outAmbient[3],
                                          float outSunlight[3]) {
    if (!outAmbient || !outSunlight || !gFo3RawWeatherLightingReady) {
        return false;
    }
    for (int c = 0; c < 3; ++c) {
        outAmbient[c] = gFo3RawWeatherAmbient[c];
        outSunlight[c] = gFo3RawWeatherSunlight[c];
    }
    return true;
}

inline bool GetFo3RawWeatherFog(float outFog[3]) {
    if (!outFog || !gFo3RawWeatherLightingReady) return false;
    for (int c = 0; c < 3; ++c) outFog[c] = gFo3RawWeatherFog[c];
    return true;
}

inline bool GetFo3LinearWeatherSunlight(float outSunlight[3]) {
    if (!outSunlight || !gFo3RawWeatherLightingReady) return false;
    for (int c = 0; c < 3; ++c) {
        outSunlight[c] = gFo3LinearWeatherSunlight[c];
    }
    return true;
}

