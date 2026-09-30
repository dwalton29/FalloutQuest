#pragma once

#include "fo3-imagespace-q1280.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace fo3imadq1300 {

struct ScalarCurveQ1300 {
    bool present = false;
    float value = 0.0f;
    float time = 0.0f;
    uint32_t keys = 0u;
};

struct ColorCurveQ1300 {
    bool present = false;
    float rgba[4]{1.0f, 1.0f, 1.0f, 0.0f};
    float time = 0.0f;
    uint32_t keys = 0u;
};

struct WeatherImadQ1300 {
    bool valid = false;
    uint32_t formId = 0u;
    std::string editorId;
    uint32_t flags = 0u;
    float duration = 0.0f;

    ScalarCurveQ1300 eyeAdaptMult, eyeAdaptAdd;
    ScalarCurveQ1300 bloomRadiusMult, bloomRadiusAdd;
    ScalarCurveQ1300 bloomThresholdMult, bloomThresholdAdd;
    ScalarCurveQ1300 bloomScaleMult, bloomScaleAdd;
    ScalarCurveQ1300 targetLumMinMult, targetLumMinAdd;
    ScalarCurveQ1300 targetLumMaxMult, targetLumMaxAdd;
    ScalarCurveQ1300 sunlightScaleMult, sunlightScaleAdd;
    ScalarCurveQ1300 skyScaleMult, skyScaleAdd;
    ScalarCurveQ1300 saturationMult, saturationAdd;
    ScalarCurveQ1300 brightnessMult, brightnessAdd;
    ScalarCurveQ1300 contrastMult, contrastAdd;
    ColorCurveQ1300 tint;
};

inline bool BinaryTypeQ1300(const char* type, uint8_t first,
                            char second, char third, char fourth) {
    return static_cast<uint8_t>(type[0]) == first &&
           type[1] == second && type[2] == third && type[3] == fourth;
}

inline void ReadScalarCurveQ1300(const uint8_t* bytes, uint32_t size,
                                 ScalarCurveQ1300& out) {
    using namespace fo3envq1000;
    if (!bytes || size < 8u) return;
    float bestTime = -1.0e30f;
    float bestValue = 0.0f;
    uint32_t keys = 0u;
    for (uint32_t pos = 0u; pos + 8u <= size; pos += 8u) {
        const float time = ReadFloat(bytes + pos);
        const float value = ReadFloat(bytes + pos + 4u);
        if (!std::isfinite(time) || !std::isfinite(value)) continue;
        ++keys;
        if (time >= bestTime) {
            bestTime = time;
            bestValue = value;
        }
    }
    if (keys == 0u) return;
    out.present = true;
    out.value = bestValue;
    out.time = bestTime;
    out.keys = keys;
}

inline void ReadColorCurveQ1300(const uint8_t* bytes, uint32_t size,
                                ColorCurveQ1300& out) {
    using namespace fo3envq1000;
    if (!bytes || size < 20u) return;
    float bestTime = -1.0e30f;
    float best[4]{1.0f, 1.0f, 1.0f, 0.0f};
    uint32_t keys = 0u;
    for (uint32_t pos = 0u; pos + 20u <= size; pos += 20u) {
        const float time = ReadFloat(bytes + pos);
        float rgba[4]{
            ReadFloat(bytes + pos + 4u),
            ReadFloat(bytes + pos + 8u),
            ReadFloat(bytes + pos + 12u),
            ReadFloat(bytes + pos + 16u),
        };
        if (!std::isfinite(time) || !std::isfinite(rgba[0]) ||
            !std::isfinite(rgba[1]) || !std::isfinite(rgba[2]) ||
            !std::isfinite(rgba[3])) continue;
        ++keys;
        if (time >= bestTime) {
            bestTime = time;
            std::copy(rgba, rgba + 4, best);
        }
    }
    if (keys == 0u) return;
    out.present = true;
    out.time = bestTime;
    out.keys = keys;
    std::copy(best, best + 4, out.rgba);
}

inline bool LoadWeatherImadQ1300(uint32_t formId, WeatherImadQ1300& out) {
    using namespace fo3envq1000;
    out = {};
    out.formId = formId;
    if (formId == 0u) return false;

    std::vector<uint8_t> payload;
    if (!FindRecord("IMAD", formId, payload)) {
        __android_log_print(ANDROID_LOG_WARN, TAG,
                            "Q13.0 WEATHER IMAD: form=%08X result=record-missing",
                            formId);
        return false;
    }

    WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "EDID", 4u) == 0 && out.editorId.empty()) {
            out.editorId = CString(bytes, size);
            return;
        }
        if (std::memcmp(type, "DNAM", 4u) == 0 && size >= 8u) {
            out.flags = Read32(bytes);
            out.duration = ReadFloat(bytes + 4u);
            return;
        }

        // Fallout 3 IMAD scalar curves are arrays of {time,value}. Binary
        // 00IAD..13IAD are multiplier channels; @IAD/AIAD/.../SIAD are adds.
        if (BinaryTypeQ1300(type, 0x00u, 'I', 'A', 'D')) ReadScalarCurveQ1300(bytes, size, out.eyeAdaptMult);
        else if (std::memcmp(type, "@IAD", 4u) == 0) ReadScalarCurveQ1300(bytes, size, out.eyeAdaptAdd);
        else if (BinaryTypeQ1300(type, 0x01u, 'I', 'A', 'D')) ReadScalarCurveQ1300(bytes, size, out.bloomRadiusMult);
        else if (std::memcmp(type, "AIAD", 4u) == 0) ReadScalarCurveQ1300(bytes, size, out.bloomRadiusAdd);
        else if (BinaryTypeQ1300(type, 0x02u, 'I', 'A', 'D')) ReadScalarCurveQ1300(bytes, size, out.bloomThresholdMult);
        else if (std::memcmp(type, "BIAD", 4u) == 0) ReadScalarCurveQ1300(bytes, size, out.bloomThresholdAdd);
        else if (BinaryTypeQ1300(type, 0x03u, 'I', 'A', 'D')) ReadScalarCurveQ1300(bytes, size, out.bloomScaleMult);
        else if (std::memcmp(type, "CIAD", 4u) == 0) ReadScalarCurveQ1300(bytes, size, out.bloomScaleAdd);
        else if (BinaryTypeQ1300(type, 0x04u, 'I', 'A', 'D')) ReadScalarCurveQ1300(bytes, size, out.targetLumMinMult);
        else if (std::memcmp(type, "DIAD", 4u) == 0) ReadScalarCurveQ1300(bytes, size, out.targetLumMinAdd);
        else if (BinaryTypeQ1300(type, 0x05u, 'I', 'A', 'D')) ReadScalarCurveQ1300(bytes, size, out.targetLumMaxMult);
        else if (std::memcmp(type, "EIAD", 4u) == 0) ReadScalarCurveQ1300(bytes, size, out.targetLumMaxAdd);
        else if (BinaryTypeQ1300(type, 0x06u, 'I', 'A', 'D')) ReadScalarCurveQ1300(bytes, size, out.sunlightScaleMult);
        else if (std::memcmp(type, "FIAD", 4u) == 0) ReadScalarCurveQ1300(bytes, size, out.sunlightScaleAdd);
        else if (BinaryTypeQ1300(type, 0x07u, 'I', 'A', 'D')) ReadScalarCurveQ1300(bytes, size, out.skyScaleMult);
        else if (std::memcmp(type, "GIAD", 4u) == 0) ReadScalarCurveQ1300(bytes, size, out.skyScaleAdd);
        else if (BinaryTypeQ1300(type, 0x11u, 'I', 'A', 'D')) ReadScalarCurveQ1300(bytes, size, out.saturationMult);
        else if (std::memcmp(type, "QIAD", 4u) == 0) ReadScalarCurveQ1300(bytes, size, out.saturationAdd);
        else if (BinaryTypeQ1300(type, 0x12u, 'I', 'A', 'D')) ReadScalarCurveQ1300(bytes, size, out.brightnessMult);
        else if (std::memcmp(type, "RIAD", 4u) == 0) ReadScalarCurveQ1300(bytes, size, out.brightnessAdd);
        else if (BinaryTypeQ1300(type, 0x13u, 'I', 'A', 'D')) ReadScalarCurveQ1300(bytes, size, out.contrastMult);
        else if (std::memcmp(type, "SIAD", 4u) == 0) ReadScalarCurveQ1300(bytes, size, out.contrastAdd);
        else if (std::memcmp(type, "TNAM", 4u) == 0) ReadColorCurveQ1300(bytes, size, out.tint);
    });

    out.valid = true;
    return true;
}

inline float ApplyScalarQ1300(float base,
                              const ScalarCurveQ1300& mult,
                              const ScalarCurveQ1300& add,
                              float minValue, float maxValue) {
    float value = base;
    if (mult.present) value *= mult.value;
    if (add.present) value += add.value;
    if (!std::isfinite(value)) value = base;
    return std::clamp(value, minValue, maxValue);
}

inline float NormalizeCinematicTintQ1310(float authoredValue) {
    // GECK exposes Cinematic Tint R/G/B and Tint Alpha in the editor's
    // 0..255 colour/intensity domain. The post shader operates in 0..1.
    // Q13.0 incorrectly treated authored 1.0 as full-strength 1.0, turning
    // WastelandDayISFX (0,0,1,1) into an opaque blue monochrome frame.
    if (!std::isfinite(authoredValue)) return 0.0f;
    return std::clamp(authoredValue, 0.0f, 255.0f) / 255.0f;
}

inline bool ApplyWeatherImadQ1300(Fo3ImageSpaceQ1280& image) {
    using namespace fo3envq1000;
    if (!image.valid || image.weatherDayImadFormId == 0u) return false;

    WeatherImadQ1300 modifier;
    if (!LoadWeatherImadQ1300(image.weatherDayImadFormId, modifier) || !modifier.valid) {
        return false;
    }

    const float beforeBlur = image.hdrBlurRadius;
    const float beforeThreshold = image.hdrBrightClamp;
    const float beforeScale = image.hdrBrightScale;
    const float beforeSat = image.cinematicSaturation;
    const float beforeBrightness = image.cinematicBrightness;
    const float beforeContrast = image.cinematicContrast;
    const uint8_t beforeFlags = image.cinematicFlags;

    image.hdrEyeAdaptSpeed = ApplyScalarQ1300(
        image.hdrEyeAdaptSpeed, modifier.eyeAdaptMult, modifier.eyeAdaptAdd, 0.0f, 16.0f);
    image.hdrBlurRadius = ApplyScalarQ1300(
        image.hdrBlurRadius, modifier.bloomRadiusMult, modifier.bloomRadiusAdd, 0.0f, 16.0f);
    image.hdrBrightClamp = ApplyScalarQ1300(
        image.hdrBrightClamp, modifier.bloomThresholdMult, modifier.bloomThresholdAdd, 0.0f, 8.0f);
    image.hdrBrightScale = ApplyScalarQ1300(
        image.hdrBrightScale, modifier.bloomScaleMult, modifier.bloomScaleAdd, 0.0f, 8.0f);
    image.hdrTargetLum = ApplyScalarQ1300(
        image.hdrTargetLum, modifier.targetLumMinMult, modifier.targetLumMinAdd, 0.0f, 16.0f);
    image.hdrUpperLumClamp = ApplyScalarQ1300(
        image.hdrUpperLumClamp, modifier.targetLumMaxMult, modifier.targetLumMaxAdd, 0.0f, 16.0f);

    if (modifier.saturationMult.present || modifier.saturationAdd.present) {
        image.cinematicSaturation = ApplyScalarQ1300(
            image.cinematicSaturation, modifier.saturationMult, modifier.saturationAdd, -4.0f, 4.0f);
        image.cinematicFlags |= 0x01u;
    }
    if (modifier.contrastMult.present || modifier.contrastAdd.present) {
        image.cinematicContrast = ApplyScalarQ1300(
            image.cinematicContrast, modifier.contrastMult, modifier.contrastAdd, -4.0f, 4.0f);
        image.cinematicFlags |= 0x02u;
    }
    if (modifier.brightnessMult.present || modifier.brightnessAdd.present) {
        // IMAD brightness is a modifier channel. A base IMGS may leave the
        // cinematic brightness field zero while its enable flag is off; use a
        // neutral 1.0 base when the weather modifier activates that channel.
        const float brightnessBase = (beforeFlags & 0x08u) != 0u
            ? image.cinematicBrightness : 1.0f;
        image.cinematicBrightness = ApplyScalarQ1300(
            brightnessBase, modifier.brightnessMult, modifier.brightnessAdd, 0.0f, 4.0f);
        image.cinematicFlags |= 0x08u;
    }
    if (modifier.tint.present) {
        image.cinematicTint[0] = NormalizeCinematicTintQ1310(modifier.tint.rgba[0]);
        image.cinematicTint[1] = NormalizeCinematicTintQ1310(modifier.tint.rgba[1]);
        image.cinematicTint[2] = NormalizeCinematicTintQ1310(modifier.tint.rgba[2]);
        image.cinematicTintValue = NormalizeCinematicTintQ1310(modifier.tint.rgba[3]);
        image.cinematicFlags |= 0x04u;
    }

    __android_log_print(ANDROID_LOG_INFO, TAG,
        "Q13.1 WEATHER IMAD APPLY: IMAD=%08X EDID=%s flags=0x%08X duration=%.3f mode=day-full-strength endpointKeys=1 blur=%.3f->%.3f threshold=%.3f->%.3f scale=%.3f->%.3f cinematicFlags=0x%02X->0x%02X sat=%.3f->%.3f brightness=%.3f->%.3f contrast=%.3f->%.3f tintPresent=%d tintRaw=(%.3f %.3f %.3f %.3f) tintShader=(%.6f %.6f %.6f %.6f) tintDomain=0..255 sunlightMult=%s skyMult=%s eyeAdapt=deferred",
        modifier.formId,
        modifier.editorId.empty() ? "<none>" : modifier.editorId.c_str(),
        modifier.flags, modifier.duration,
        beforeBlur, image.hdrBlurRadius,
        beforeThreshold, image.hdrBrightClamp,
        beforeScale, image.hdrBrightScale,
        beforeFlags, image.cinematicFlags,
        beforeSat, image.cinematicSaturation,
        beforeBrightness, image.cinematicBrightness,
        beforeContrast, image.cinematicContrast,
        modifier.tint.present ? 1 : 0,
        modifier.tint.rgba[0], modifier.tint.rgba[1],
        modifier.tint.rgba[2], modifier.tint.rgba[3],
        image.cinematicTint[0], image.cinematicTint[1],
        image.cinematicTint[2], image.cinematicTintValue,
        modifier.sunlightScaleMult.present ? "present" : "none",
        modifier.skyScaleMult.present ? "present" : "none");
    return true;
}

} // namespace fo3imadq1300

inline bool LoadFo3ImageSpaceQ1300(uint32_t cellFormId, uint32_t worldspaceFormId) {
    const bool baseReady = LoadFo3ImageSpaceQ1280(cellFormId, worldspaceFormId);
    if (!baseReady) return false;
    fo3imadq1300::ApplyWeatherImadQ1300(gFo3ImageSpaceQ1280);
    return gFo3ImageSpaceQ1280.valid;
}
