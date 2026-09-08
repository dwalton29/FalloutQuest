#pragma once

#include "fo3-authored-color-q1390.h"
#include "fo3-weather-sky-q1330.h"

namespace fo3skycolorq1390 {

inline bool PrepareFo3WeatherSkyColorQ1390() {
    using namespace fo3colorq1390;
    using namespace fo3skyq1330;

    if (!EnsureWeatherQ1330()) return false;
    const uint32_t weather = gWeatherSkyQ1330.weatherFormId;
    if (gSkyConverted && gSkyWeather == weather) return true;

    const float rawSun[3]{gWeatherSkyQ1330.sunColor[0],
                          gWeatherSkyQ1330.sunColor[1],
                          gWeatherSkyQ1330.sunColor[2]};

    Srgb3ToLinear(gWeatherSkyQ1330.sunColor);
    for (CloudLayerQ1330& layer : gWeatherSkyQ1330.clouds) {
        layer.color[0] = SrgbToLinear(layer.color[0]);
        layer.color[1] = SrgbToLinear(layer.color[1]);
        layer.color[2] = SrgbToLinear(layer.color[2]);
        // Alpha is coverage/opacity, not a colour channel. Keep it authored.
    }

    gSkyWeather = weather;
    gSkyConverted = true;
    __android_log_print(
        ANDROID_LOG_INFO, TAG,
        "Q13.9 SKY COLOR: weather=%08X sun=(%.3f %.3f %.3f)->(%.3f %.3f %.3f) transfer=sRGB-bytes-to-linear cloudRgbLinear=1 alphaUnchanged=1 cloudTextureTransfer=unchanged filter=none",
        weather,
        rawSun[0], rawSun[1], rawSun[2],
        gWeatherSkyQ1330.sunColor[0], gWeatherSkyQ1330.sunColor[1],
        gWeatherSkyQ1330.sunColor[2]);
    return true;
}

} // namespace fo3skycolorq1390

inline void RenderFo3SkyQ1390(const float* mvp16) {
    // Q13.3 owns cloud/sun geometry and blending. Q13.9 only converts the
    // authored WTHR colour bytes before that renderer consumes them.
    fo3skycolorq1390::PrepareFo3WeatherSkyColorQ1390();
    RenderFo3SkyQ1330(mvp16);
}
