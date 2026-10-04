#pragma once
#include "fo3-esm-reader.h"
#include <cstring>
#include <string>
#include <vector>
struct Fo3ImageSpace {
    bool valid = false;
    uint32_t cellFormId = 0u;
    uint32_t worldspaceFormId = 0u;
    uint32_t imageSpaceFormId = 0u;
    uint32_t imageSpaceSourceWorldFormId = 0u;
    uint32_t weatherDayImadFormId = 0u;
    bool imageSpaceFromCell = false;
    bool imageSpaceInheritedFromParent = false;
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


namespace fo3imagespace {
inline bool ParseImageSpacePayload(const std::vector<uint8_t>& imagePayload, Fo3ImageSpace& image) {
    bool haveDnam = false;
    fo3esm::WalkSubrecords(imagePayload.data(), imagePayload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "EDID", 4u) == 0 && image.editorId.empty()) {
            image.editorId = fo3esm::ZString(bytes, size);
            return;
        }
        if (std::memcmp(type, "DNAM", 4u) != 0 || (size != 132u && size != 148u && size != 152u)) return;

        auto f = [&](uint32_t offset) { return fo3esm::ReadF32(bytes + offset); };
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
        // Fallout 3 has two legacy DNAM layouts without HDR Skin Dimmer:
        // 132-byte and 148-byte. Only the 152-byte layout stores the float
        // at offset 56; all following fields shift by four bytes otherwise.
        const bool hasSkinDimmer = size == 152u;
        image.hdrSkinDimmer = hasSkinDimmer ? f(56u) : 1.0f;
        const uint32_t shift = hasSkinDimmer ? 0u : 4u;

        image.bloomBlurRadius = f(60u - shift);
        image.bloomAlphaInterior = f(64u - shift);
        image.bloomAlphaExterior = f(68u - shift);

        image.nightEyeTint[0] = f(84u - shift);
        image.nightEyeTint[1] = f(88u - shift);
        image.nightEyeTint[2] = f(92u - shift);
        image.nightEyeBrightness = f(96u - shift);

        image.cinematicSaturation = f(100u - shift);
        image.cinematicContrastAvgLum = f(104u - shift);
        image.cinematicContrast = f(108u - shift);
        image.cinematicBrightness = f(112u - shift);
        image.cinematicTint[0] = f(116u - shift);
        image.cinematicTint[1] = f(120u - shift);
        image.cinematicTint[2] = f(124u - shift);
        image.cinematicTintValue = f(128u - shift);
        image.cinematicFlags = size == 152u ? bytes[148u] : 0x0fu; // Legacy controls have no v13 flags.
        haveDnam = true;
    });

    image.valid = haveDnam;
    return haveDnam;
}

}
