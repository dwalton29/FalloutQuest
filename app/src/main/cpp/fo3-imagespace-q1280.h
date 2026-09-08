#pragma once

#include "fo3-environment-q1000.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

struct Fo3ImageSpaceQ1280 {
    bool valid = false;
    uint32_t cellFormId = 0u;
    uint32_t worldspaceFormId = 0u;
    uint32_t imageSpaceFormId = 0u;
    std::string editorId;

    float hdrEyeAdaptSpeed = 0.5f;
    float hdrBlurRadius = 1.0f;
    float hdrBlurPasses = 1.0f;
    float hdrEmissiveMultiplier = 1.0f;
    float hdrTargetLum = 1.0f;
    float hdrUpperLumClamp = 1.0f;
    float hdrBrightScale = 1.0f;
    float hdrBrightClamp = 1.0f;
    float hdrLumRampNoTex = 1.0f;
    float hdrLumRampMin = 1.0f;
    float hdrLumRampMax = 1.0f;
    float hdrSunlightDimmer = 1.0f;
    float hdrGrassDimmer = 1.0f;
    float hdrTreeDimmer = 1.0f;
    float hdrSkinDimmer = 1.0f;

    float bloomBlurRadius = 1.0f;
    float bloomAlphaInterior = 0.0f;
    float bloomAlphaExterior = 0.0f;

    float nightEyeTint[3]{1.0f, 1.0f, 1.0f};
    float nightEyeBrightness = 1.0f;

    float cinematicSaturation = 1.0f;
    float cinematicContrastAvgLum = 0.5f;
    float cinematicContrast = 1.0f;
    float cinematicTint[3]{1.0f, 1.0f, 1.0f};
    float cinematicTintValue = 0.0f;
    float cinematicBrightness = 1.0f;
    uint8_t cinematicFlags = 0u;
};

inline Fo3ImageSpaceQ1280 gFo3ImageSpaceQ1280;

inline const Fo3ImageSpaceQ1280& GetFo3ImageSpaceQ1280() {
    return gFo3ImageSpaceQ1280;
}

inline void ResetFo3ImageSpaceQ1280() {
    gFo3ImageSpaceQ1280 = {};
}

inline bool LoadFo3ImageSpaceQ1280(uint32_t cellFormId, uint32_t worldspaceFormId) {
    using namespace fo3envq1000;
    ResetFo3ImageSpaceQ1280();

    std::vector<uint8_t> cellPayload;
    if (!FindRecord("CELL", cellFormId, cellPayload)) {
        __android_log_print(ANDROID_LOG_WARN, TAG,
                            "Q12.8 IMAGE SPACE: cell=%08X world=%08X result=cell-record-missing",
                            cellFormId, worldspaceFormId);
        return false;
    }

    uint32_t imageSpaceFormId = 0u;
    WalkSubrecords(cellPayload.data(), cellPayload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "XCIM", 4u) == 0 && size >= 4u) {
            imageSpaceFormId = Read32(bytes);
        }
    });

    if (imageSpaceFormId == 0u) {
        __android_log_print(ANDROID_LOG_INFO, TAG,
                            "Q12.8 IMAGE SPACE: cell=%08X world=%08X XCIM=00000000 result=neutral",
                            cellFormId, worldspaceFormId);
        return false;
    }

    std::vector<uint8_t> imagePayload;
    if (!FindRecord("IMGS", imageSpaceFormId, imagePayload)) {
        __android_log_print(ANDROID_LOG_WARN, TAG,
                            "Q12.8 IMAGE SPACE: cell=%08X world=%08X XCIM=%08X result=imgs-record-missing",
                            cellFormId, worldspaceFormId, imageSpaceFormId);
        return false;
    }

    Fo3ImageSpaceQ1280 image;
    image.cellFormId = cellFormId;
    image.worldspaceFormId = worldspaceFormId;
    image.imageSpaceFormId = imageSpaceFormId;
    bool haveDnam = false;

    WalkSubrecords(imagePayload.data(), imagePayload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "EDID", 4u) == 0 && image.editorId.empty()) {
            image.editorId = CString(bytes, size);
            return;
        }
        if (std::memcmp(type, "DNAM", 4u) != 0 || size < 148u) return;

        auto f = [&](uint32_t offset) { return ReadFloat(bytes + offset); };
        image.hdrEyeAdaptSpeed = f(0u);
        image.hdrBlurRadius = f(4u);
        image.hdrBlurPasses = f(8u);
        image.hdrEmissiveMultiplier = f(12u);
        image.hdrTargetLum = f(16u);
        image.hdrUpperLumClamp = f(20u);
        image.hdrBrightScale = f(24u);
        image.hdrBrightClamp = f(28u);
        image.hdrLumRampNoTex = f(32u);
        image.hdrLumRampMin = f(36u);
        image.hdrLumRampMax = f(40u);
        image.hdrSunlightDimmer = f(44u);
        image.hdrGrassDimmer = f(48u);
        image.hdrTreeDimmer = f(52u);
        image.hdrSkinDimmer = f(56u);

        image.bloomBlurRadius = f(60u);
        image.bloomAlphaInterior = f(64u);
        image.bloomAlphaExterior = f(68u);

        image.nightEyeTint[0] = f(84u);
        image.nightEyeTint[1] = f(88u);
        image.nightEyeTint[2] = f(92u);
        image.nightEyeBrightness = f(96u);

        image.cinematicSaturation = f(100u);
        image.cinematicContrastAvgLum = f(104u);
        image.cinematicContrast = f(108u);
        image.cinematicTint[0] = f(112u);
        image.cinematicTint[1] = f(116u);
        image.cinematicTint[2] = f(120u);
        image.cinematicTintValue = f(124u);
        image.cinematicFlags = bytes[144u];
        haveDnam = true;
    });

    if (!haveDnam) {
        __android_log_print(ANDROID_LOG_WARN, TAG,
                            "Q12.8 IMAGE SPACE: cell=%08X world=%08X XCIM=%08X result=bad-dnam",
                            cellFormId, worldspaceFormId, imageSpaceFormId);
        return false;
    }

    // Corrupt/extreme plugin values must never poison the VR eye buffer. These
    // are broad safety bounds, not visual replacements for authored values.
    auto finiteOr = [](float value, float fallback) {
        return std::isfinite(value) ? value : fallback;
    };
    image.hdrBlurRadius = std::clamp(finiteOr(image.hdrBlurRadius, 1.0f), 0.0f, 16.0f);
    image.hdrBrightScale = std::clamp(finiteOr(image.hdrBrightScale, 1.0f), 0.0f, 8.0f);
    image.hdrBrightClamp = std::clamp(finiteOr(image.hdrBrightClamp, 1.0f), 0.0f, 8.0f);
    image.bloomBlurRadius = std::clamp(finiteOr(image.bloomBlurRadius, 1.0f), 0.0f, 16.0f);
    image.bloomAlphaExterior = std::clamp(finiteOr(image.bloomAlphaExterior, 0.0f), 0.0f, 4.0f);
    image.cinematicSaturation = std::clamp(finiteOr(image.cinematicSaturation, 1.0f), -4.0f, 4.0f);
    image.cinematicContrastAvgLum = std::clamp(finiteOr(image.cinematicContrastAvgLum, 0.5f), -4.0f, 4.0f);
    image.cinematicContrast = std::clamp(finiteOr(image.cinematicContrast, 1.0f), -4.0f, 4.0f);
    image.cinematicBrightness = std::clamp(finiteOr(image.cinematicBrightness, 1.0f), 0.0f, 4.0f);
    image.cinematicTintValue = std::clamp(finiteOr(image.cinematicTintValue, 0.0f), 0.0f, 1.0f);
    for (float& channel : image.cinematicTint) {
        channel = std::clamp(finiteOr(channel, 1.0f), 0.0f, 4.0f);
    }

    image.valid = true;
    gFo3ImageSpaceQ1280 = image;
    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "Q12.8 IMAGE SPACE READY: cell=%08X world=%08X IMGS=%08X EDID=%s flags=0x%02X sat=%.3f contrastAvg=%.3f contrast=%.3f brightness=%.3f tint=(%.3f %.3f %.3f) tintValue=%.3f hdrBlur=%.3f hdrBrightScale=%.3f hdrBrightClamp=%.3f bloomRadius=%.3f bloomExterior=%.3f",
                        cellFormId, worldspaceFormId, imageSpaceFormId,
                        image.editorId.empty() ? "<none>" : image.editorId.c_str(),
                        image.cinematicFlags,
                        image.cinematicSaturation,
                        image.cinematicContrastAvgLum,
                        image.cinematicContrast,
                        image.cinematicBrightness,
                        image.cinematicTint[0], image.cinematicTint[1], image.cinematicTint[2],
                        image.cinematicTintValue,
                        image.hdrBlurRadius, image.hdrBrightScale, image.hdrBrightClamp,
                        image.bloomBlurRadius, image.bloomAlphaExterior);
    return true;
}
