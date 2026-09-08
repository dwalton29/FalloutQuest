#pragma once

#include "fo3-environment-q1000.h"
#include "fo3-visual-depth-q1010.h"
#include "fo3-external-emittance-q1380.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

// Q13.9: Fallout 3 stores weather/light colours as editor RGB bytes. The
// renderer now samples diffuse/LAND albedo through sRGB formats, so those
// authored colour bytes must enter the lighting equation in the same linear
// domain. This is a transfer-function correction only: no hue, saturation,
// Fallout-green filter, LUT, or hand-authored colour is introduced here.
namespace fo3colorq1390 {

constexpr const char* TAG = "FalloutQuest";

inline float SrgbToLinear(float value) {
    const float c = std::clamp(value, 0.0f, 1.0f);
    return c <= 0.04045f
        ? c / 12.92f
        : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

inline void Srgb3ToLinear(float color[3]) {
    color[0] = SrgbToLinear(color[0]);
    color[1] = SrgbToLinear(color[1]);
    color[2] = SrgbToLinear(color[2]);
}

inline uint32_t gEnvironmentWorld = 0u;
inline bool gEnvironmentConverted = false;
inline uint32_t gSkyWeather = 0u;
inline bool gSkyConverted = false;
inline bool gLoggedFixedEmittance = false;

inline void ResetState() {
    gEnvironmentWorld = 0u;
    gEnvironmentConverted = false;
    gSkyWeather = 0u;
    gSkyConverted = false;
    gLoggedFixedEmittance = false;
}

inline bool ReadFixedLightLinear(uint32_t lightFormId, float out[3]) {
    using namespace fo3envq1000;
    std::vector<uint8_t> payload;
    if (!FindRecord("LIGH", lightFormId, payload)) return false;

    bool haveColor = false;
    float encoded[3]{0.0f, 0.0f, 0.0f};
    float fade = 1.0f;
    WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "DATA", 4u) == 0 && size >= 12u) {
            encoded[0] = static_cast<float>(bytes[8u]) / 255.0f;
            encoded[1] = static_cast<float>(bytes[9u]) / 255.0f;
            encoded[2] = static_cast<float>(bytes[10u]) / 255.0f;
            haveColor = true;
        } else if (std::memcmp(type, "FNAM", 4u) == 0 && size >= 4u) {
            fade = ReadFloat(bytes);
        }
    });
    if (!haveColor) return false;
    if (!std::isfinite(fade)) fade = 1.0f;
    fade = std::clamp(fade, 0.0f, 16.0f);
    out[0] = SrgbToLinear(encoded[0]) * fade;
    out[1] = SrgbToLinear(encoded[1]) * fade;
    out[2] = SrgbToLinear(encoded[2]) * fade;
    return true;
}

} // namespace fo3colorq1390

inline bool LoadFo3EnvironmentQ1390(uint32_t worldspaceFormId) {
    using namespace fo3colorq1390;
    if (!LoadFo3EnvironmentQ1000(worldspaceFormId)) {
        gEnvironmentConverted = false;
        gEnvironmentWorld = 0u;
        return false;
    }
    if (gEnvironmentConverted && gEnvironmentWorld == worldspaceFormId) return true;

    Fo3EnvironmentQ1000& env = gFo3EnvironmentQ1000;
    const float rawSky[3]{env.skyUpper[0], env.skyUpper[1], env.skyUpper[2]};
    const float rawAmbient[3]{env.ambient[0], env.ambient[1], env.ambient[2]};
    const float rawSunlight[3]{env.sunlight[0], env.sunlight[1], env.sunlight[2]};
    const float rawFog[3]{env.fog[0], env.fog[1], env.fog[2]};

    Srgb3ToLinear(env.skyUpper);
    Srgb3ToLinear(env.skyLower);
    Srgb3ToLinear(env.horizon);
    Srgb3ToLinear(env.fog);
    Srgb3ToLinear(env.ambient);
    Srgb3ToLinear(env.sunlight);
    Srgb3ToLinear(env.sun);

    gEnvironmentWorld = worldspaceFormId;
    gEnvironmentConverted = true;
    __android_log_print(
        ANDROID_LOG_INFO, TAG,
        "Q13.9 AUTHORED COLOR READY: world=%08X weather=%08X transfer=sRGB-bytes-to-linear filter=none sky=(%.3f %.3f %.3f)->(%.3f %.3f %.3f) ambient=(%.3f %.3f %.3f)->(%.3f %.3f %.3f) sunlight=(%.3f %.3f %.3f)->(%.3f %.3f %.3f) fog=(%.3f %.3f %.3f)->(%.3f %.3f %.3f)",
        worldspaceFormId, env.weatherFormId,
        rawSky[0], rawSky[1], rawSky[2], env.skyUpper[0], env.skyUpper[1], env.skyUpper[2],
        rawAmbient[0], rawAmbient[1], rawAmbient[2], env.ambient[0], env.ambient[1], env.ambient[2],
        rawSunlight[0], rawSunlight[1], rawSunlight[2], env.sunlight[0], env.sunlight[1], env.sunlight[2],
        rawFog[0], rawFog[1], rawFog[2], env.fog[0], env.fog[1], env.fog[2]);
    return true;
}

inline void ResetFo3EnvironmentQ1390() {
    ResetFo3EnvironmentQ1000();
    fo3colorq1390::gEnvironmentWorld = 0u;
    fo3colorq1390::gEnvironmentConverted = false;
    fo3colorq1390::gSkyWeather = 0u;
    fo3colorq1390::gSkyConverted = false;
}

inline bool LoadFo3PlacedLightsQ1390(uint32_t worldspaceFormId,
                                     float arrivalX, float arrivalY, float arrivalZ) {
    using namespace fo3colorq1390;
    const bool result = LoadFo3PlacedLightsQ1010(worldspaceFormId,
                                                  arrivalX, arrivalY, arrivalZ);
    for (Fo3PlacedLightQ1010& light : gFo3PlacedLightsQ1010) {
        const float fade = std::max(light.fade, 0.0f);
        for (int channel = 0; channel < 3; ++channel) {
            const float signedValue = light.color[channel];
            const float sign = signedValue < 0.0f ? -1.0f : 1.0f;
            const float encoded = fade > 1.0e-6f
                ? std::clamp(std::abs(signedValue) / fade, 0.0f, 1.0f)
                : 0.0f;
            light.color[channel] = sign * SrgbToLinear(encoded) * fade;
        }
    }
    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "Q13.9 LOCAL LIGHT COLOR: world=%08X lights=%zu transfer=sRGB-bytes-to-linear authoredFadePreserved=1",
                        worldspaceFormId, gFo3PlacedLightsQ1010.size());
    return result;
}

inline bool ResolveFo3ExternalEmittanceQ1390(uint32_t worldspaceFormId,
                                              uint32_t referenceFormId,
                                              Fo3ExternalEmittanceQ1380& out) {
    using namespace fo3colorq1390;
    if (!ResolveFo3ExternalEmittanceQ1380(worldspaceFormId, referenceFormId, out)) {
        return false;
    }

    const float raw[3]{out.color[0], out.color[1], out.color[2]};
    if (out.regionDriven) {
        // REGN exterior emittance is a WTHR colour endpoint and carries no LIGH
        // Fade multiplier, so the resolved normalized RGB can be decoded directly.
        Srgb3ToLinear(out.color);
    } else {
        // Fixed LIGH emittance applies FNAM Fade after colour selection. Re-read
        // the authored RGB bytes so transfer decoding happens before that scalar.
        float linear[3]{};
        if (ReadFixedLightLinear(out.emittanceFormId, linear)) {
            out.color[0] = linear[0];
            out.color[1] = linear[1];
            out.color[2] = linear[2];
        } else {
            Srgb3ToLinear(out.color);
        }
    }

    if (!gLoggedFixedEmittance) {
        gLoggedFixedEmittance = true;
        __android_log_print(
            ANDROID_LOG_INFO, TAG,
            "Q13.9 XEMI COLOR: ref=%08X emittance=%08X type=%s encoded=(%.3f %.3f %.3f) linear=(%.3f %.3f %.3f) filter=none",
            referenceFormId, out.emittanceFormId, out.regionDriven ? "REGN" : "LIGH",
            raw[0], raw[1], raw[2], out.color[0], out.color[1], out.color[2]);
    }
    return true;
}
