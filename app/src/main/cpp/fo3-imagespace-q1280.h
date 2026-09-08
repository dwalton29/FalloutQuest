#pragma once

#include "fo3-environment-q1000.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

struct Fo3ImageSpaceQ1280 {
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

inline Fo3ImageSpaceQ1280 gFo3ImageSpaceQ1280;

inline const Fo3ImageSpaceQ1280& GetFo3ImageSpaceQ1280() {
    return gFo3ImageSpaceQ1280;
}

inline void ResetFo3ImageSpaceQ1280() {
    gFo3ImageSpaceQ1280 = {};
}

namespace fo3imgq1290 {

struct WorldImageLinkQ1290 {
    uint32_t parentWorld = 0u;
    uint32_t imageSpace = 0u;
    uint8_t parentFlags = 0u;
};

inline bool ReadWorldImageLinkQ1290(uint32_t worldspaceFormId,
                                    WorldImageLinkQ1290& out) {
    using namespace fo3envq1000;
    out = {};
    std::vector<uint8_t> payload;
    if (!FindRecord("WRLD", worldspaceFormId, payload)) return false;
    WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "WNAM", 4u) == 0 && size >= 4u) {
            out.parentWorld = Read32(bytes);
        } else if (std::memcmp(type, "PNAM", 4u) == 0 && size >= 1u) {
            out.parentFlags = bytes[0];
        } else if (std::memcmp(type, "INAM", 4u) == 0 && size >= 4u) {
            out.imageSpace = Read32(bytes);
        }
    });
    return true;
}

inline bool ResolveWorldImageSpaceQ1290(uint32_t worldspaceFormId,
                                        uint32_t& imageSpaceFormId,
                                        uint32_t& sourceWorldFormId,
                                        bool& inheritedFromParent) {
    imageSpaceFormId = 0u;
    sourceWorldFormId = 0u;
    inheritedFromParent = false;

    uint32_t current = worldspaceFormId;
    for (int depth = 0; depth < 8 && current != 0u; ++depth) {
        WorldImageLinkQ1290 link;
        if (!ReadWorldImageLinkQ1290(current, link)) return false;

        // WRLD PNAM 0x20 means this child explicitly uses its parent's Image
        // Space data. Follow that ownership before considering the child's INAM.
        if (link.parentWorld != 0u && (link.parentFlags & 0x20u) != 0u) {
            __android_log_print(ANDROID_LOG_INFO, fo3envq1000::TAG,
                                "Q12.9 IMAGE SPACE PARENT: world=%08X parent=%08X flags=0x%02X useImageSpace=1",
                                current, link.parentWorld, link.parentFlags);
            inheritedFromParent = true;
            current = link.parentWorld;
            continue;
        }

        if (link.imageSpace != 0u) {
            imageSpaceFormId = link.imageSpace;
            sourceWorldFormId = current;
            return true;
        }

        __android_log_print(ANDROID_LOG_INFO, fo3envq1000::TAG,
                            "Q12.9 IMAGE SPACE WORLD: world=%08X INAM=00000000 parent=%08X flags=0x%02X result=no-world-imagespace",
                            current, link.parentWorld, link.parentFlags);
        return false;
    }
    return false;
}

inline uint32_t ResolveWeatherDayImadQ1290(uint32_t weatherFormId) {
    using namespace fo3envq1000;
    if (weatherFormId == 0u) return 0u;
    std::vector<uint8_t> payload;
    if (!FindRecord("WTHR", weatherFormId, payload)) return 0u;
    uint32_t dayImad = 0u;
    WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        // Fallout 3 uses binary subrecord names 00IAD..03IAD for
        // sunrise/day/sunset/night Image Space Modifiers.
        if (size >= 4u && static_cast<uint8_t>(type[0]) == 0x01u &&
            type[1] == 'I' && type[2] == 'A' && type[3] == 'D') {
            dayImad = Read32(bytes);
        }
    });
    return dayImad;
}

} // namespace fo3imgq1290

inline bool LoadFo3ImageSpaceQ1280(uint32_t cellFormId, uint32_t worldspaceFormId) {
    using namespace fo3envq1000;
    ResetFo3ImageSpaceQ1280();

    uint32_t imageSpaceFormId = 0u;
    bool imageSpaceFromCell = false;
    uint32_t imageSpaceSourceWorld = 0u;
    bool imageSpaceInheritedFromParent = false;

    std::vector<uint8_t> cellPayload;
    if (!FindRecord("CELL", cellFormId, cellPayload)) {
        __android_log_print(ANDROID_LOG_WARN, TAG,
                            "Q12.9 IMAGE SPACE: cell=%08X world=%08X result=cell-record-missing",
                            cellFormId, worldspaceFormId);
        return false;
    }

    WalkSubrecords(cellPayload.data(), cellPayload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "XCIM", 4u) == 0 && size >= 4u) {
            imageSpaceFormId = Read32(bytes);
        }
    });

    if (imageSpaceFormId != 0u) {
        imageSpaceFromCell = true;
        __android_log_print(ANDROID_LOG_INFO, TAG,
                            "Q12.9 IMAGE SPACE RESOLVE: cell=%08X world=%08X source=CELL XCIM=%08X",
                            cellFormId, worldspaceFormId, imageSpaceFormId);
    } else {
        fo3imgq1290::ResolveWorldImageSpaceQ1290(
            worldspaceFormId, imageSpaceFormId, imageSpaceSourceWorld,
            imageSpaceInheritedFromParent);
        __android_log_print(ANDROID_LOG_INFO, TAG,
                            "Q12.9 IMAGE SPACE RESOLVE: cell=%08X world=%08X XCIM=00000000 source=%s sourceWorld=%08X INAM=%08X inherited=%d",
                            cellFormId, worldspaceFormId,
                            imageSpaceFormId != 0u ? "WRLD" : "neutral",
                            imageSpaceSourceWorld, imageSpaceFormId,
                            imageSpaceInheritedFromParent ? 1 : 0);
    }

    const Fo3EnvironmentQ1000& env = GetFo3EnvironmentQ1000();
    const uint32_t weatherDayImad =
        fo3imgq1290::ResolveWeatherDayImadQ1290(env.weatherFormId);
    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "Q12.9 WEATHER IMAD: weather=%08X EDID=%s dayIMAD=%08X applied=0 auditOnly=1",
                        env.weatherFormId,
                        env.weatherEditorId.empty() ? "<none>" : env.weatherEditorId.c_str(),
                        weatherDayImad);

    if (imageSpaceFormId == 0u) {
        __android_log_print(ANDROID_LOG_INFO, TAG,
                            "Q12.9 IMAGE SPACE: cell=%08X world=%08X result=neutral-after-world-resolution",
                            cellFormId, worldspaceFormId);
        return false;
    }

    std::vector<uint8_t> imagePayload;
    if (!FindRecord("IMGS", imageSpaceFormId, imagePayload)) {
        __android_log_print(ANDROID_LOG_WARN, TAG,
                            "Q12.9 IMAGE SPACE: cell=%08X world=%08X IMGS=%08X result=imgs-record-missing",
                            cellFormId, worldspaceFormId, imageSpaceFormId);
        return false;
    }

    Fo3ImageSpaceQ1280 image;
    image.cellFormId = cellFormId;
    image.worldspaceFormId = worldspaceFormId;
    image.imageSpaceFormId = imageSpaceFormId;
    image.imageSpaceSourceWorldFormId = imageSpaceSourceWorld;
    image.weatherDayImadFormId = weatherDayImad;
    image.imageSpaceFromCell = imageSpaceFromCell;
    image.imageSpaceInheritedFromParent = imageSpaceInheritedFromParent;
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
        image.cinematicBrightness = f(128u);
        image.cinematicFlags = bytes[144u];
        haveDnam = true;
    });

    if (!haveDnam) {
        __android_log_print(ANDROID_LOG_WARN, TAG,
                            "Q12.9 IMAGE SPACE: cell=%08X world=%08X IMGS=%08X result=bad-dnam",
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
                        "Q12.9 IMAGE SPACE READY: cell=%08X world=%08X IMGS=%08X EDID=%s source=%s sourceWorld=%08X inherited=%d flags=0x%02X sat=%.3f contrastAvg=%.3f contrast=%.3f brightness=%.3f tint=(%.3f %.3f %.3f) tintValue=%.3f hdrBlur=%.3f hdrBrightScale=%.3f hdrBrightClamp=%.3f bloomRadius=%.3f bloomExterior=%.3f dayIMAD=%08X",
                        cellFormId, worldspaceFormId, imageSpaceFormId,
                        image.editorId.empty() ? "<none>" : image.editorId.c_str(),
                        image.imageSpaceFromCell ? "CELL" : "WRLD",
                        image.imageSpaceSourceWorldFormId,
                        image.imageSpaceInheritedFromParent ? 1 : 0,
                        image.cinematicFlags,
                        image.cinematicSaturation,
                        image.cinematicContrastAvgLum,
                        image.cinematicContrast,
                        image.cinematicBrightness,
                        image.cinematicTint[0], image.cinematicTint[1], image.cinematicTint[2],
                        image.cinematicTintValue,
                        image.hdrBlurRadius, image.hdrBrightScale, image.hdrBrightClamp,
                        image.bloomBlurRadius, image.bloomAlphaExterior,
                        image.weatherDayImadFormId);
    return true;
}
