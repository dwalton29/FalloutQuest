#pragma once

#include "fo3-imagespace-q1280.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// Q14.1: resolve the environment actually authored on Megaton exterior CELLs.
// CELL XCLR -> REGN Weather RDWT chooses the weather; CELL XCIM chooses ImageSpace.
// This deliberately does not invent a colour correction. It only changes which
// Fallout3.esm records feed the already-existing renderer.

struct Fo3CellEnvironmentQ1410 {
    bool valid = false;
    uint32_t cellFormId = 0u;
    std::string cellEditorId;
    uint32_t regionFormId = 0u;
    std::string regionEditorId;
    uint32_t weatherFormId = 0u;
    std::string weatherEditorId;
    int32_t weatherChance = 0;
    uint32_t imageSpaceFormId = 0u;
};

namespace fo3cellenvq1410 {

constexpr uint32_t MEGATON_WORLDSPACE_Q1410 = 0x00000A74u;
constexpr uint32_t MEGATON_ENTRANCE_CELL_Q1410 = 0x00002DBDu;
constexpr uint32_t MEGATON_PLAZA_CELL_Q1410 = 0x00002DBBu;

inline bool ReadCellLinksQ1410(uint32_t cellFormId,
                               std::string& editorId,
                               std::vector<uint32_t>& regions,
                               uint32_t& imageSpace) {
    using namespace fo3envq1000;
    editorId.clear();
    regions.clear();
    imageSpace = 0u;
    std::vector<uint8_t> payload;
    if (!FindRecord("CELL", cellFormId, payload)) return false;
    WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "EDID", 4u) == 0 && editorId.empty()) {
            editorId = CString(bytes, size);
        } else if (std::memcmp(type, "XCLR", 4u) == 0 && size >= 4u) {
            for (uint32_t p = 0u; p + 4u <= size; p += 4u) {
                const uint32_t region = Read32(bytes + p);
                if (region != 0u) regions.push_back(region);
            }
        } else if (std::memcmp(type, "XCIM", 4u) == 0 && size >= 4u) {
            imageSpace = Read32(bytes);
        }
    });
    return !regions.empty() || imageSpace != 0u;
}

inline bool ReadRegionWeatherQ1410(uint32_t regionFormId,
                                   uint32_t expectedWorld,
                                   std::string& regionEditorId,
                                   uint32_t& weatherFormId,
                                   int32_t& weatherChance) {
    using namespace fo3envq1000;
    regionEditorId.clear();
    weatherFormId = 0u;
    weatherChance = -1;
    std::vector<uint8_t> payload;
    if (!FindRecord("REGN", regionFormId, payload)) return false;

    uint32_t regionWorld = 0u;
    uint32_t currentDataType = 0xffffffffu;
    WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "EDID", 4u) == 0 && regionEditorId.empty()) {
            regionEditorId = CString(bytes, size);
        } else if (std::memcmp(type, "WNAM", 4u) == 0 && size >= 4u) {
            regionWorld = Read32(bytes);
        } else if (std::memcmp(type, "RDAT", 4u) == 0 && size >= 4u) {
            currentDataType = Read32(bytes);
        } else if (std::memcmp(type, "RDWT", 4u) == 0 && size >= 8u &&
                   currentDataType == 3u) {
            const uint32_t weather = Read32(bytes + 0u);
            const int32_t chance = ReadI32(bytes + 4u);
            if (weather != 0u && chance > weatherChance) {
                weatherFormId = weather;
                weatherChance = chance;
            }
        }
    });
    return (regionWorld == 0u || regionWorld == expectedWorld) && weatherFormId != 0u;
}

inline std::string ReadRecordEdidQ1410(const char type[4], uint32_t formId) {
    using namespace fo3envq1000;
    std::vector<uint8_t> payload;
    if (!FindRecord(type, formId, payload)) return {};
    std::string out;
    WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* subType, const uint8_t* bytes, uint32_t size) {
        if (out.empty() && std::memcmp(subType, "EDID", 4u) == 0) {
            out = CString(bytes, size);
        }
    });
    return out;
}

inline bool ResolveFromCellQ1410(uint32_t worldspaceFormId,
                                 uint32_t cellFormId,
                                 Fo3CellEnvironmentQ1410& out) {
    std::string cellEdid;
    std::vector<uint32_t> regions;
    uint32_t imageSpace = 0u;
    if (!ReadCellLinksQ1410(cellFormId, cellEdid, regions, imageSpace)) return false;

    int32_t bestChance = -1;
    uint32_t bestRegion = 0u;
    uint32_t bestWeather = 0u;
    std::string bestRegionEdid;
    for (uint32_t region : regions) {
        std::string regionEdid;
        uint32_t weather = 0u;
        int32_t chance = -1;
        if (!ReadRegionWeatherQ1410(region, worldspaceFormId, regionEdid, weather, chance)) continue;
        if (chance > bestChance) {
            bestChance = chance;
            bestRegion = region;
            bestWeather = weather;
            bestRegionEdid = std::move(regionEdid);
        }
    }
    if (bestWeather == 0u || imageSpace == 0u) return false;

    out = {};
    out.valid = true;
    out.cellFormId = cellFormId;
    out.cellEditorId = std::move(cellEdid);
    out.regionFormId = bestRegion;
    out.regionEditorId = std::move(bestRegionEdid);
    out.weatherFormId = bestWeather;
    out.weatherEditorId = ReadRecordEdidQ1410("WTHR", bestWeather);
    out.weatherChance = bestChance;
    out.imageSpaceFormId = imageSpace;
    return true;
}

inline bool ParseImageSpaceQ1410(uint32_t imageSpaceFormId,
                                 uint32_t cellFormId,
                                 uint32_t worldspaceFormId,
                                 uint32_t weatherFormId,
                                 Fo3ImageSpaceQ1280& image) {
    using namespace fo3envq1000;
    std::vector<uint8_t> payload;
    if (!FindRecord("IMGS", imageSpaceFormId, payload)) return false;

    image = {};
    image.cellFormId = cellFormId;
    image.worldspaceFormId = worldspaceFormId;
    image.imageSpaceFormId = imageSpaceFormId;
    image.imageSpaceFromCell = true;
    image.imageSpaceInheritedFromParent = false;
    image.weatherDayImadFormId = fo3imgq1290::ResolveWeatherDayImadQ1290(weatherFormId);
    bool haveDnam = false;
    uint32_t dnamBytes = 0u;

    WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "EDID", 4u) == 0 && image.editorId.empty()) {
            image.editorId = CString(bytes, size);
            return;
        }
        if (std::memcmp(type, "DNAM", 4u) != 0 || size < 132u) return;
        dnamBytes = size;
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

        // Fallout 3's 152-byte DNAM layout places Cinematic Brightness before
        // the tint RGB triplet. Q12.9 had these fields shifted by one float.
        image.cinematicBrightness = f(112u);
        image.cinematicTint[0] = f(116u);
        image.cinematicTint[1] = f(120u);
        image.cinematicTint[2] = f(124u);
        image.cinematicTintValue = f(128u);
        image.cinematicFlags = size >= 149u ? bytes[148u] : 0u;
        haveDnam = true;
    });
    if (!haveDnam) return false;

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
    __android_log_print(
        ANDROID_LOG_INFO, fo3envq1000::TAG,
        "Q14.1 CELL IMAGE: cell=%08X IMGS=%08X EDID=%s DNAM=%u flags=0x%02X sat=%.3f contrastAvg=%.3f contrast=%.3f brightness=%.3f tint=(%.3f %.3f %.3f) tintValue=%.3f sunDimmer=%.3f brightClamp=%.3f bloomExterior=%.3f source=Fallout3.esm layout=FO3-152",
        cellFormId, imageSpaceFormId,
        image.editorId.empty() ? "<none>" : image.editorId.c_str(), dnamBytes,
        image.cinematicFlags, image.cinematicSaturation,
        image.cinematicContrastAvgLum, image.cinematicContrast,
        image.cinematicBrightness, image.cinematicTint[0], image.cinematicTint[1],
        image.cinematicTint[2], image.cinematicTintValue,
        image.hdrSunlightDimmer, image.hdrBrightClamp, image.bloomAlphaExterior);
    return true;
}

} // namespace fo3cellenvq1410

inline bool ResolveFo3CellEnvironmentQ1410(uint32_t worldspaceFormId,
                                           uint32_t preferredCellFormId,
                                           Fo3CellEnvironmentQ1410& out) {
    using namespace fo3cellenvq1410;
    out = {};
    if (worldspaceFormId != MEGATON_WORLDSPACE_Q1410) return false;

    if (preferredCellFormId != 0u &&
        ResolveFromCellQ1410(worldspaceFormId, preferredCellFormId, out)) {
        return true;
    }
    if (preferredCellFormId != MEGATON_ENTRANCE_CELL_Q1410 &&
        ResolveFromCellQ1410(worldspaceFormId, MEGATON_ENTRANCE_CELL_Q1410, out)) {
        return true;
    }
    return ResolveFromCellQ1410(worldspaceFormId, MEGATON_PLAZA_CELL_Q1410, out);
}

inline bool LoadFo3CellImageSpaceQ1410(const Fo3CellEnvironmentQ1410& cellEnv,
                                       uint32_t worldspaceFormId) {
    if (!cellEnv.valid || cellEnv.imageSpaceFormId == 0u) return false;
    Fo3ImageSpaceQ1280 image;
    if (!fo3cellenvq1410::ParseImageSpaceQ1410(
            cellEnv.imageSpaceFormId, cellEnv.cellFormId, worldspaceFormId,
            cellEnv.weatherFormId, image)) {
        return false;
    }
    gFo3ImageSpaceQ1280 = image;
    return true;
}
