#pragma once

#include "fo3-authored-color-q1390.h"
#include "fo3-weather-imad-q1300.h"
#include "fo3-weather-light-q1320.h"
#include "fo3-weather-sky-q1330.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

// Q14.0: runtime time-of-day from Fallout3.esm.
//
// Data ownership stays Bethesda-authored:
//   CLMT TNAM -> sunrise/sunset spans
//   WTHR NAM0 -> Sunrise/Day/Sunset/Night colours
//   WTHR PNAM -> cloud-layer colours
//   WTHR FNAM -> day/night fog distances
//   WTHR 00IAD..03IAD -> weather ImageSpace modifiers
//   REFR XEMI -> REGN -> WTHR -> time-varying exterior emittance
//
// The only testing-only piece is the clock source: FalloutQuest starts frozen at
// 12:00 and the left trigger advances it. The colour values themselves are never
// hand-tinted. The continuous interpolation uses each CLMT transition span with
// the authored Sunrise/Sunset endpoint at the span midpoint, yielding a smooth
// Night->Sunrise->Day and Day->Sunset->Night bridge without inventing colours.

namespace fo3todq1400 {

constexpr const char* TAG = "FalloutQuest";
constexpr float PI = 3.14159265358979323846f;
constexpr float TEST_HOURS_PER_REAL_SECOND = 2.0f;
constexpr float TRIGGER_DEADZONE = 0.05f;

struct ClimateTimesQ1400 {
    bool valid = false;
    uint32_t formId = 0u;
    float sunriseBegin = 5.0f;
    float sunriseEnd = 9.0f;
    float sunsetBegin = 16.0f;
    float sunsetEnd = 20.0f;
};

struct WeatherTimeQ1400 {
    bool valid = false;
    uint32_t formId = 0u;
    std::string editorId;

    // [colour class][Sunrise, Day, Sunset, Night][RGBA]. Values here stay in
    // the authored 0..1 byte/display domain; sampling helpers decode RGB before
    // it enters the linear renderer established by Q13.9.
    float encoded[10][4][4]{};
    bool haveNam0 = false;

    // [cloud layer][Sunrise, Day, Sunset, Night][RGBA].
    float cloudEncoded[4][4][4]{};
    bool havePnam = false;

    float fogDayNear = 0.0f;
    float fogDayFar = 0.0f;
    float fogNightNear = 0.0f;
    float fogNightFar = 0.0f;
    bool haveFog = false;

    uint32_t imad[4]{0u, 0u, 0u, 0u};
};

struct TimeWeightsQ1400 {
    float w[4]{0.0f, 1.0f, 0.0f, 0.0f};
    const char* phase = "DAY";
};

struct RuntimeQ1400 {
    bool ready = false;
    uint32_t worldspaceFormId = 0u;
    uint32_t climateFormId = 0u;
    uint32_t weatherFormId = 0u;
    ClimateTimesQ1400 climate;
    WeatherTimeQ1400 weather;
    Fo3ImageSpaceQ1280 baseImage;
    std::array<Fo3ImageSpaceQ1280, 4> endpointImage{};
    std::unordered_map<uint32_t, WeatherTimeQ1400> emittanceWeather;
};

inline RuntimeQ1400 gRuntime;
inline float gTestHour = 12.0f;
inline int64_t gLastPredictedNs = 0;
inline int gLastLoggedHour = -1;
inline std::string gLastLoggedPhase;
inline bool gAppliedOnce = false;

inline float Clamp01(float v) {
    return std::clamp(v, 0.0f, 1.0f);
}

inline float Lerp(float a, float b, float t) {
    return a + (b - a) * Clamp01(t);
}

inline float WrapHour(float h) {
    if (!std::isfinite(h)) return 12.0f;
    h = std::fmod(h, 24.0f);
    if (h < 0.0f) h += 24.0f;
    return h;
}

inline float ByteTimeToHours(uint8_t value) {
    // Fallout CLMT stores these four time fields in ten-minute units.
    return static_cast<float>(value) / 6.0f;
}

inline bool LoadClimateTimes(uint32_t climateFormId, ClimateTimesQ1400& out) {
    using namespace fo3envq1000;
    out = {};
    out.formId = climateFormId;
    if (climateFormId == 0u) return false;
    std::vector<uint8_t> payload;
    if (!FindRecord("CLMT", climateFormId, payload)) return false;
    WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "TNAM", 4u) == 0 && size >= 4u) {
            out.sunriseBegin = ByteTimeToHours(bytes[0]);
            out.sunriseEnd = ByteTimeToHours(bytes[1]);
            out.sunsetBegin = ByteTimeToHours(bytes[2]);
            out.sunsetEnd = ByteTimeToHours(bytes[3]);
            out.valid = out.sunriseBegin < out.sunriseEnd &&
                        out.sunriseEnd <= out.sunsetBegin &&
                        out.sunsetBegin < out.sunsetEnd &&
                        out.sunsetEnd <= 24.0f;
        }
    });
    return out.valid;
}

inline bool LoadWeatherTime(uint32_t weatherFormId, WeatherTimeQ1400& out) {
    using namespace fo3envq1000;
    out = {};
    out.formId = weatherFormId;
    if (weatherFormId == 0u) return false;

    std::vector<uint8_t> payload;
    if (!FindRecord("WTHR", weatherFormId, payload)) return false;
    WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "EDID", 4u) == 0 && out.editorId.empty()) {
            out.editorId = CString(bytes, size);
            return;
        }
        if (std::memcmp(type, "NAM0", 4u) == 0 && size >= 160u) {
            for (int cls = 0; cls < 10; ++cls) {
                for (int tod = 0; tod < 4; ++tod) {
                    const uint32_t p = static_cast<uint32_t>(cls * 16 + tod * 4);
                    for (int c = 0; c < 4; ++c) {
                        out.encoded[cls][tod][c] =
                            static_cast<float>(bytes[p + static_cast<uint32_t>(c)]) / 255.0f;
                    }
                }
            }
            out.haveNam0 = true;
            return;
        }
        if (std::memcmp(type, "PNAM", 4u) == 0 && size >= 64u) {
            for (int layer = 0; layer < 4; ++layer) {
                for (int tod = 0; tod < 4; ++tod) {
                    const uint32_t p = static_cast<uint32_t>(layer * 16 + tod * 4);
                    for (int c = 0; c < 4; ++c) {
                        out.cloudEncoded[layer][tod][c] =
                            static_cast<float>(bytes[p + static_cast<uint32_t>(c)]) / 255.0f;
                    }
                }
            }
            out.havePnam = true;
            return;
        }
        if (std::memcmp(type, "FNAM", 4u) == 0 && size >= 16u) {
            out.fogDayNear = ReadFloat(bytes + 0u);
            out.fogDayFar = ReadFloat(bytes + 4u);
            out.fogNightNear = ReadFloat(bytes + 8u);
            out.fogNightFar = ReadFloat(bytes + 12u);
            out.haveFog = std::isfinite(out.fogDayNear) && std::isfinite(out.fogDayFar) &&
                          std::isfinite(out.fogNightNear) && std::isfinite(out.fogNightFar);
            return;
        }
        const uint8_t first = static_cast<uint8_t>(type[0]);
        if (first <= 0x03u && type[1] == 'I' && type[2] == 'A' && type[3] == 'D' &&
            size >= 4u) {
            out.imad[first] = Read32(bytes);
        }
    });
    out.valid = out.haveNam0;
    return out.valid;
}

inline TimeWeightsQ1400 WeightsForHour(float hour, const ClimateTimesQ1400& c) {
    TimeWeightsQ1400 out;
    std::fill(std::begin(out.w), std::end(out.w), 0.0f);
    hour = WrapHour(hour);
    const float sunriseMid = 0.5f * (c.sunriseBegin + c.sunriseEnd);
    const float sunsetMid = 0.5f * (c.sunsetBegin + c.sunsetEnd);

    auto two = [&](int a, int b, float t, const char* phase) {
        out.w[a] = 1.0f - Clamp01(t);
        out.w[b] = Clamp01(t);
        out.phase = phase;
    };

    if (hour < c.sunriseBegin || hour >= c.sunsetEnd) {
        out.w[3] = 1.0f;
        out.phase = "NIGHT";
    } else if (hour < sunriseMid) {
        two(3, 0, (hour - c.sunriseBegin) /
                    std::max(sunriseMid - c.sunriseBegin, 0.001f),
            "NIGHT->SUNRISE");
    } else if (hour < c.sunriseEnd) {
        two(0, 1, (hour - sunriseMid) /
                    std::max(c.sunriseEnd - sunriseMid, 0.001f),
            "SUNRISE->DAY");
    } else if (hour < c.sunsetBegin) {
        out.w[1] = 1.0f;
        out.phase = "DAY";
    } else if (hour < sunsetMid) {
        two(1, 2, (hour - c.sunsetBegin) /
                    std::max(sunsetMid - c.sunsetBegin, 0.001f),
            "DAY->SUNSET");
    } else {
        two(2, 3, (hour - sunsetMid) /
                    std::max(c.sunsetEnd - sunsetMid, 0.001f),
            "SUNSET->NIGHT");
    }
    return out;
}

inline void SampleLinearRgb(const WeatherTimeQ1400& weather, int cls,
                            const TimeWeightsQ1400& weights, float out[3]) {
    using fo3colorq1390::SrgbToLinear;
    out[0] = out[1] = out[2] = 0.0f;
    if (!weather.haveNam0 || cls < 0 || cls >= 10) return;
    for (int tod = 0; tod < 4; ++tod) {
        const float w = weights.w[tod];
        for (int c = 0; c < 3; ++c) {
            out[c] += SrgbToLinear(weather.encoded[cls][tod][c]) * w;
        }
    }
}

inline float LinearToSrgb(float value) {
    const float c = std::max(value, 0.0f);
    return c <= 0.0031308f
        ? 12.92f * c
        : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

inline float BlendField(const std::array<Fo3ImageSpaceQ1280, 4>& endpoint,
                        const TimeWeightsQ1400& weights,
                        float Fo3ImageSpaceQ1280::*member) {
    float v = 0.0f;
    for (int i = 0; i < 4; ++i) v += endpoint[i].*member * weights.w[i];
    return v;
}

inline void BuildEndpointImages(RuntimeQ1400& runtime) {
    // Q12.8's loader is the clean base IMGS parser; Q13.0/Q13.2 then layer the
    // weather endpoint on top. Temporarily ask Q12.8 for the base and restore the
    // live global immediately afterwards so no frame can observe the temporary.
    const Fo3ImageSpaceQ1280 live = gFo3ImageSpaceQ1280;
    if (live.valid && live.cellFormId != 0u && live.worldspaceFormId != 0u &&
        LoadFo3ImageSpaceQ1280(live.cellFormId, live.worldspaceFormId)) {
        runtime.baseImage = gFo3ImageSpaceQ1280;
    } else {
        runtime.baseImage = live;
    }
    gFo3ImageSpaceQ1280 = live;

    using namespace fo3imadq1300;
    for (int i = 0; i < 4; ++i) {
        runtime.endpointImage[i] = runtime.baseImage;
        runtime.endpointImage[i].weatherDayImadFormId = runtime.weather.imad[i];
        if (runtime.weather.imad[i] == 0u) continue;

        WeatherImadQ1300 modifier;
        if (!LoadWeatherImadQ1300(runtime.weather.imad[i], modifier) || !modifier.valid) continue;
        ApplyWeatherImadQ1300(runtime.endpointImage[i]);

        // Q13.2 applies these two non-post channels separately. Build their
        // per-time endpoint values here so the same clock can interpolate them.
        runtime.endpointImage[i].hdrSunlightDimmer = ApplyScalarQ1300(
            runtime.baseImage.hdrSunlightDimmer,
            modifier.sunlightScaleMult, modifier.sunlightScaleAdd, 0.0f, 4.0f);
        runtime.endpointImage[i].hdrLumRampNoTex = ApplyScalarQ1300(
            runtime.baseImage.hdrLumRampNoTex,
            modifier.skyScaleMult, modifier.skyScaleAdd, 0.0f, 4.0f);
    }
}

inline Fo3ImageSpaceQ1280 BlendImage(const RuntimeQ1400& runtime,
                                     const TimeWeightsQ1400& weights) {
    Fo3ImageSpaceQ1280 out = runtime.baseImage;
#define Q1400_BLEND_FIELD(name) out.name = BlendField(runtime.endpointImage, weights, &Fo3ImageSpaceQ1280::name)
    Q1400_BLEND_FIELD(hdrEyeAdaptSpeed);
    Q1400_BLEND_FIELD(hdrBlurRadius);
    Q1400_BLEND_FIELD(hdrBlurPasses);
    Q1400_BLEND_FIELD(hdrEmissiveMultiplier);
    Q1400_BLEND_FIELD(hdrTargetLum);
    Q1400_BLEND_FIELD(hdrUpperLumClamp);
    Q1400_BLEND_FIELD(hdrBrightScale);
    Q1400_BLEND_FIELD(hdrBrightClamp);
    Q1400_BLEND_FIELD(hdrLumRampNoTex);
    Q1400_BLEND_FIELD(hdrLumRampMin);
    Q1400_BLEND_FIELD(hdrLumRampMax);
    Q1400_BLEND_FIELD(hdrSunlightDimmer);
    Q1400_BLEND_FIELD(hdrGrassDimmer);
    Q1400_BLEND_FIELD(hdrTreeDimmer);
    Q1400_BLEND_FIELD(hdrSkinDimmer);
    Q1400_BLEND_FIELD(bloomBlurRadius);
    Q1400_BLEND_FIELD(bloomAlphaInterior);
    Q1400_BLEND_FIELD(bloomAlphaExterior);
    Q1400_BLEND_FIELD(nightEyeBrightness);
    Q1400_BLEND_FIELD(cinematicSaturation);
    Q1400_BLEND_FIELD(cinematicContrastAvgLum);
    Q1400_BLEND_FIELD(cinematicContrast);
    Q1400_BLEND_FIELD(cinematicTintValue);
    Q1400_BLEND_FIELD(cinematicBrightness);
#undef Q1400_BLEND_FIELD

    for (int c = 0; c < 3; ++c) {
        out.nightEyeTint[c] = 0.0f;
        out.cinematicTint[c] = 0.0f;
        for (int i = 0; i < 4; ++i) {
            out.nightEyeTint[c] += runtime.endpointImage[i].nightEyeTint[c] * weights.w[i];
            out.cinematicTint[c] += runtime.endpointImage[i].cinematicTint[c] * weights.w[i];
        }
    }

    out.cinematicFlags = runtime.baseImage.cinematicFlags;
    int dominant = 1;
    for (int i = 0; i < 4; ++i) {
        if (weights.w[i] > 0.0001f) out.cinematicFlags |= runtime.endpointImage[i].cinematicFlags;
        if (weights.w[i] > weights.w[dominant]) dominant = i;
    }
    out.weatherDayImadFormId = runtime.weather.imad[dominant];
    out.valid = runtime.baseImage.valid;
    return out;
}

inline void UpdateSunDirection(Fo3EnvironmentQ1000& env, float hour,
                               const ClimateTimesQ1400& climate) {
    // Fallout's solar trajectory is engine behaviour rather than a WTHR colour.
    // For the Q14.0 test clock, derive a continuous daylight arc from the actual
    // CLMT sunrise/sunset limits and preserve Q10.0's established noon azimuth
    // and 0.85 noon elevation. This is explicitly a renderer bridge, not an ESM
    // colour/tint; later clock work can replace it with a byte-for-byte engine arc.
    const float start = climate.sunriseBegin;
    const float end = climate.sunsetEnd;
    const float h = WrapHour(hour);
    constexpr float noonY = 0.85f;
    const float noonAzimuth = std::atan2(0.40f, 0.35f);

    if (h >= start && h <= end) {
        const float t = Clamp01((h - start) / std::max(end - start, 0.001f));
        const float y = std::max(0.015f, noonY * std::sin(PI * t));
        const float horizontal = std::sqrt(std::max(1.0f - y * y, 0.0f));
        const float azimuth = noonAzimuth + (t - 0.5f) * PI;
        env.sunDirection[0] = horizontal * std::cos(azimuth);
        env.sunDirection[1] = y;
        env.sunDirection[2] = horizontal * std::sin(azimuth);
    } else {
        // Keep the Sun disc below the rendered hemisphere at night. Night WTHR
        // sunlight colours are still authoritative for the tiny directional fill.
        env.sunDirection[0] = -0.35f;
        env.sunDirection[1] = -0.25f;
        env.sunDirection[2] = -0.40f;
        const float len = std::sqrt(env.sunDirection[0] * env.sunDirection[0] +
                                    env.sunDirection[1] * env.sunDirection[1] +
                                    env.sunDirection[2] * env.sunDirection[2]);
        if (len > 1.0e-5f) {
            env.sunDirection[0] /= len;
            env.sunDirection[1] /= len;
            env.sunDirection[2] /= len;
        }
    }
}

inline void ApplyEnvironment(RuntimeQ1400& runtime, const TimeWeightsQ1400& weights) {
    Fo3EnvironmentQ1000& env = gFo3EnvironmentQ1000;
    if (!env.valid) return;

    float value[3]{};
    SampleLinearRgb(runtime.weather, 0, weights, value);
    for (int c = 0; c < 3; ++c) env.skyUpper[c] = value[c] * gFo3ImageSpaceQ1280.hdrLumRampNoTex;
    SampleLinearRgb(runtime.weather, 1, weights, value);
    for (int c = 0; c < 3; ++c) env.fog[c] = value[c];
    SampleLinearRgb(runtime.weather, 3, weights, value);
    for (int c = 0; c < 3; ++c) env.ambient[c] = value[c];
    SampleLinearRgb(runtime.weather, 4, weights, value);
    const float sunlightScale = fo3weatherq1320::EffectiveSunlightScaleQ1320(
        gFo3ImageSpaceQ1280.hdrSunlightDimmer);
    for (int c = 0; c < 3; ++c) env.sunlight[c] = value[c] * sunlightScale;
    SampleLinearRgb(runtime.weather, 5, weights, value);
    for (int c = 0; c < 3; ++c) env.sun[c] = value[c];
    SampleLinearRgb(runtime.weather, 7, weights, value);
    for (int c = 0; c < 3; ++c) env.skyLower[c] = value[c] * gFo3ImageSpaceQ1280.hdrLumRampNoTex;
    SampleLinearRgb(runtime.weather, 8, weights, value);
    for (int c = 0; c < 3; ++c) env.horizon[c] = value[c] * gFo3ImageSpaceQ1280.hdrLumRampNoTex;

    if (runtime.weather.haveFog) {
        // FNAM has only Day and Night distances. Sunrise/Sunset receive an equal
        // day/night bridge while NAM0 still uses its dedicated four RGB endpoints.
        const float nightAmount = Clamp01(weights.w[3] + 0.5f * (weights.w[0] + weights.w[2]));
        env.fogNear = Lerp(runtime.weather.fogDayNear, runtime.weather.fogNightNear, nightAmount);
        env.fogFar = Lerp(runtime.weather.fogDayFar, runtime.weather.fogNightFar, nightAmount);
    }
    UpdateSunDirection(env, gTestHour, runtime.climate);
}

inline void ApplySky(RuntimeQ1400& runtime, const TimeWeightsQ1400& weights) {
    using namespace fo3skyq1330;
    if (!EnsureWeatherQ1330() || gWeatherSkyQ1330.weatherFormId != runtime.weather.formId) return;

    float sun[3]{};
    SampleLinearRgb(runtime.weather, 5, weights, sun);
    for (int c = 0; c < 3; ++c) gWeatherSkyQ1330.sunColor[c] = sun[c];

    if (runtime.weather.havePnam) {
        for (int layer = 0; layer < 4; ++layer) {
            float rgb[3]{0.0f, 0.0f, 0.0f};
            float alpha = 0.0f;
            for (int tod = 0; tod < 4; ++tod) {
                for (int c = 0; c < 3; ++c) {
                    rgb[c] += fo3colorq1390::SrgbToLinear(
                        runtime.weather.cloudEncoded[layer][tod][c]) * weights.w[tod];
                }
                alpha += runtime.weather.cloudEncoded[layer][tod][3] * weights.w[tod];
            }
            for (int c = 0; c < 3; ++c) gWeatherSkyQ1330.clouds[layer].color[c] = rgb[c];
            gWeatherSkyQ1330.clouds[layer].color[3] = Clamp01(alpha);
        }
    }

    // We have directly written linear RGB, so prevent Q13.9's render wrapper
    // from decoding these already-linear dynamic values a second time.
    fo3colorq1390::gSkyWeather = runtime.weather.formId;
    fo3colorq1390::gSkyConverted = true;
}

inline void ApplyRegionEmittance(RuntimeQ1400& runtime,
                                 const TimeWeightsQ1400& weights) {
    for (auto& pair : gFo3ExternalEmittanceQ1380) {
        Fo3ExternalEmittanceQ1380& value = pair.second;
        if (!value.valid || !value.regionDriven || value.weatherFormId == 0u) continue;
        auto found = runtime.emittanceWeather.find(value.weatherFormId);
        if (found == runtime.emittanceWeather.end()) continue;
        float linear[3]{};
        SampleLinearRgb(found->second, 4, weights, linear); // Sunlight drives XEMI region emittance.
        // Q13.9's resolver expects the cached region colour in encoded form and
        // decodes it per draw. Re-encode the already interpolated linear result
        // so that decode returns the exact same linear interpolation.
        for (int c = 0; c < 3; ++c) value.color[c] = Clamp01(LinearToSrgb(linear[c]));
    }
}

inline bool BuildRuntime() {
    RuntimeQ1400 runtime;
    const Fo3EnvironmentQ1000& env = gFo3EnvironmentQ1000;
    if (!env.valid || env.worldspaceFormId == 0u || env.weatherFormId == 0u ||
        env.climateFormId == 0u || !gFo3ImageSpaceQ1280.valid) {
        return false;
    }

    runtime.worldspaceFormId = env.worldspaceFormId;
    runtime.climateFormId = env.climateFormId;
    runtime.weatherFormId = env.weatherFormId;
    if (!LoadClimateTimes(runtime.climateFormId, runtime.climate) ||
        !LoadWeatherTime(runtime.weatherFormId, runtime.weather)) {
        return false;
    }

    BuildEndpointImages(runtime);

    // Load the same XEMI map Q13.8/Q13.9 use, then cache the full four-endpoint
    // WTHR records for every region-driven external-emittance assignment.
    LoadFo3ExternalEmittanceQ1380(runtime.worldspaceFormId);
    for (const auto& pair : gFo3ExternalEmittanceQ1380) {
        const Fo3ExternalEmittanceQ1380& value = pair.second;
        if (!value.valid || !value.regionDriven || value.weatherFormId == 0u ||
            runtime.emittanceWeather.find(value.weatherFormId) != runtime.emittanceWeather.end()) {
            continue;
        }
        WeatherTimeQ1400 weather;
        if (LoadWeatherTime(value.weatherFormId, weather)) {
            runtime.emittanceWeather.emplace(value.weatherFormId, std::move(weather));
        }
    }

    runtime.ready = true;
    gRuntime = std::move(runtime);
    gAppliedOnce = false;
    gLastLoggedHour = -1;
    gLastLoggedPhase.clear();

    __android_log_print(
        ANDROID_LOG_INFO, TAG,
        "Q14.0 TIME OF DAY READY: world=%08X climate=%08X weather=%08X EDID=%s sunrise=%.2f..%.2f sunset=%.2f..%.2f IMADs=(%08X %08X %08X %08X) xemiWeather=%zu source=Fallout3.esm curve=CLMT-span-midpoint startHour=12.00 leftTriggerHoursPerSecond=%.1f filter=none",
        gRuntime.worldspaceFormId, gRuntime.climateFormId, gRuntime.weatherFormId,
        gRuntime.weather.editorId.empty() ? "<none>" : gRuntime.weather.editorId.c_str(),
        gRuntime.climate.sunriseBegin, gRuntime.climate.sunriseEnd,
        gRuntime.climate.sunsetBegin, gRuntime.climate.sunsetEnd,
        gRuntime.weather.imad[0], gRuntime.weather.imad[1],
        gRuntime.weather.imad[2], gRuntime.weather.imad[3],
        gRuntime.emittanceWeather.size(), TEST_HOURS_PER_REAL_SECOND);
    return true;
}

inline void ApplyCurrentTime(bool forceLog) {
    if (!gRuntime.ready) return;
    const TimeWeightsQ1400 weights = WeightsForHour(gTestHour, gRuntime.climate);

    gFo3ImageSpaceQ1280 = BlendImage(gRuntime, weights);
    ApplyEnvironment(gRuntime, weights);
    ApplySky(gRuntime, weights);
    ApplyRegionEmittance(gRuntime, weights);
    gAppliedOnce = true;

    const int hourBucket = static_cast<int>(std::floor(gTestHour));
    const bool phaseChanged = gLastLoggedPhase != weights.phase;
    if (forceLog || hourBucket != gLastLoggedHour || phaseChanged) {
        gLastLoggedHour = hourBucket;
        gLastLoggedPhase = weights.phase;
        const Fo3EnvironmentQ1000& env = gFo3EnvironmentQ1000;
        const float effectiveSunScale = fo3weatherq1320::EffectiveSunlightScaleQ1320(
            gFo3ImageSpaceQ1280.hdrSunlightDimmer);
        __android_log_print(
            ANDROID_LOG_INFO, TAG,
            "Q14.0 TIME: hour=%05.2f phase=%s weights=(sunrise=%.3f day=%.3f sunset=%.3f night=%.3f) ambient=(%.3f %.3f %.3f) sunlight=(%.3f %.3f %.3f) fog=(%.3f %.3f %.3f) fogNear=%.1f fogFar=%.1f sunDir=(%.3f %.3f %.3f) sunDimmer=%.3f effectiveSunScale=%.3f skyScale=%.3f activeIMAD=%08X bloomScale=%.3f bloomClamp=%.3f exposureTarget=%.3f",
            gTestHour, weights.phase,
            weights.w[0], weights.w[1], weights.w[2], weights.w[3],
            env.ambient[0], env.ambient[1], env.ambient[2],
            env.sunlight[0], env.sunlight[1], env.sunlight[2],
            env.fog[0], env.fog[1], env.fog[2], env.fogNear, env.fogFar,
            env.sunDirection[0], env.sunDirection[1], env.sunDirection[2],
            gFo3ImageSpaceQ1280.hdrSunlightDimmer,
            effectiveSunScale,
            gFo3ImageSpaceQ1280.hdrLumRampNoTex,
            gFo3ImageSpaceQ1280.weatherDayImadFormId,
            gFo3ImageSpaceQ1280.hdrBrightScale,
            gFo3ImageSpaceQ1280.hdrBrightClamp,
            gFo3ImageSpaceQ1280.hdrTargetLum);
    }
}

} // namespace fo3todq1400

inline void UpdateFo3TimeOfDayQ1400(float leftTriggerValue, int64_t predictedDisplayTimeNs) {
    using namespace fo3todq1400;
    const Fo3EnvironmentQ1000& env = gFo3EnvironmentQ1000;
    if (!env.valid || !gFo3ImageSpaceQ1280.valid) {
        gLastPredictedNs = predictedDisplayTimeNs;
        return;
    }

    if (!gRuntime.ready || gRuntime.worldspaceFormId != env.worldspaceFormId ||
        gRuntime.climateFormId != env.climateFormId ||
        gRuntime.weatherFormId != env.weatherFormId) {
        if (!BuildRuntime()) {
            gLastPredictedNs = predictedDisplayTimeNs;
            return;
        }
    }

    double dt = 0.0;
    if (gLastPredictedNs != 0 && predictedDisplayTimeNs > gLastPredictedNs) {
        dt = static_cast<double>(predictedDisplayTimeNs - gLastPredictedNs) * 1.0e-9;
        dt = std::clamp(dt, 0.0, 0.100);
    }
    gLastPredictedNs = predictedDisplayTimeNs;

    const float trigger = std::clamp(leftTriggerValue, 0.0f, 1.0f);
    bool moved = false;
    if (trigger > TRIGGER_DEADZONE && dt > 0.0) {
        const float normalized = (trigger - TRIGGER_DEADZONE) / (1.0f - TRIGGER_DEADZONE);
        gTestHour = WrapHour(gTestHour +
            static_cast<float>(dt) * TEST_HOURS_PER_REAL_SECOND * normalized);
        moved = true;
    }

    if (!gAppliedOnce || moved) ApplyCurrentTime(!gAppliedOnce);
}

inline float GetFo3TestHourQ1400() {
    return fo3todq1400::gTestHour;
}
