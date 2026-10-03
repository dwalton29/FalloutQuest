#pragma once

#include "rendering/environment/fo3-authored-color.h"
#include "rendering/environment/fo3-weather-sky.h"

namespace fo3skycolor {

inline bool PrepareFo3WeatherSkyColor() {
    using namespace fo3colorq1390;
    using namespace fo3sky;

    if (!EnsureWeather()) return false;
    const uint32_t weather = gWeatherSky.weatherFormId;
    if (gSkyConverted && gSkyWeather == weather) return true;

    const float rawSun[3]{gWeatherSky.sunColor[0],
                          gWeatherSky.sunColor[1],
                          gWeatherSky.sunColor[2]};

    Srgb3ToLinear(gWeatherSky.sunColor);
    for (CloudLayer& layer : gWeatherSky.clouds) {
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
        gWeatherSky.sunColor[0], gWeatherSky.sunColor[1],
        gWeatherSky.sunColor[2]);
    return true;
}

} // namespace fo3skycolor

inline void RenderFo3WeatherSkyColor(const float* mvp16) {
    // Q13.3 owns cloud/sun geometry and blending. Q13.9 only converts the
    // authored WTHR colour bytes before that renderer consumes them.
    fo3skycolor::PrepareFo3WeatherSkyColor();
    RenderFo3WeatherSky(mvp16);
}
