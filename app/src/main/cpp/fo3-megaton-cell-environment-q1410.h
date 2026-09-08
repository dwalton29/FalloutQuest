#pragma once

#include "fo3-authored-color-q1390.h"
#include "fo3-weather-light-q1320.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// Q14.1: resolve the environment from the actual exterior CELL instead of
// treating the WRLD climate's highest-chance weather as the whole town.
// Fallout 3 Megaton cells author:
//   CELL XCLR -> REGN weather entry -> WTHR
//   CELL XCIM -> IMGS
// This helper also corrects the FO3 152-byte IMGS DNAM cinematic tail and keeps
// WTHR FNAM fog power available to the shaders.
namespace fo3cellenvq1410 {

constexpr const char* TAG = "FalloutQuest";
constexpr float CELL_SIZE_UNITS = 4096.0f;

struct CellEnvironmentQ1410 {
    bool valid = false;
    uint32_t cellFormId = 0u;
    std::string cellEditorId;
    int32_t gridX = 0;
    int32_t gridY = 0;
    uint32_t regionFormId = 0u;
    uint32_t imageSpaceFormId = 0u;
    uint32_t weatherFormId = 0u;
    std::string weatherEditorId;
    int32_t weatherChance = -1;
    int gridFallbackDistance = 0;
};

struct GroupQ1410 {
    uint64_t end = 0u;
    uint32_t label = 0u;
    uint32_t type = 0u;
};

inline CellEnvironmentQ1410 gCellEnvironment;
inline float gFogDayPower = 1.0f;
inline float gFogNightPower = 1.0f;
inline float gFogPower = 1.0f;
inline int gLastFogHour = -1;

inline int32_t GameCoordToCell(float value) {
    if (!std::isfinite(value)) return 0;
    return static_cast<int32_t>(std::floor(value / CELL_SIZE_UNITS));
}

inline bool FindSpatialCell(uint32_t worldspaceFormId,
                            float gameX, float gameY,
                            CellEnvironmentQ1410& out) {
    using namespace fo3envq1000;
    out = {};
    const int32_t wantedX = GameCoordToCell(gameX);
    const int32_t wantedY = GameCoordToCell(gameY);

    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) return false;
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        return false;
    }

    std::vector<GroupQ1410> groups;
    CellEnvironmentQ1410 best;
    int bestDistance = 9999;

    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        while (!groups.empty() && offset >= groups.back().end) groups.pop_back();
        if (offset + HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[HEADER_SIZE]{};
        if (!ReadExact(file, header, sizeof(header))) break;
        const uint32_t sizeField = Read32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < HEADER_SIZE ||
                offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            groups.push_back(
                GroupQ1410{offset + sizeField, Read32(header + 8u), Read32(header + 12u)});
            continue;
        }

        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;

        bool inWorld = false;
        for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
            if (it->type == 1u) {
                inWorld = it->label == worldspaceFormId;
                break;
            }
        }

        if (!inWorld || std::memcmp(header, "CELL", 4u) != 0) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        const uint32_t flags = Read32(header + 8u);
        const uint32_t formId = Read32(header + 12u);
        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, flags, payload)) break;

        CellEnvironmentQ1410 candidate;
        candidate.cellFormId = formId;
        bool haveGrid = false;
        WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "EDID", 4u) == 0 && candidate.cellEditorId.empty()) {
                candidate.cellEditorId = CString(bytes, size);
            } else if (std::memcmp(type, "XCLC", 4u) == 0 && size >= 8u) {
                candidate.gridX = static_cast<int32_t>(Read32(bytes + 0u));
                candidate.gridY = static_cast<int32_t>(Read32(bytes + 4u));
                haveGrid = true;
            } else if (std::memcmp(type, "XCLR", 4u) == 0 && size >= 4u) {
                candidate.regionFormId = Read32(bytes);
            } else if (std::memcmp(type, "XCIM", 4u) == 0 && size >= 4u) {
                candidate.imageSpaceFormId = Read32(bytes);
            }
        });

        if (!haveGrid ||
            (candidate.regionFormId == 0u && candidate.imageSpaceFormId == 0u)) {
            continue;
        }

        const int distance =
            std::abs(candidate.gridX - wantedX) + std::abs(candidate.gridY - wantedY);
        if (distance < bestDistance) {
            best = candidate;
            bestDistance = distance;
            if (distance == 0) break;
        }
    }

    std::fclose(file);
    if (bestDistance > 2 || best.cellFormId == 0u) return false;
    best.gridFallbackDistance = bestDistance;
    best.valid = true;
    out = std::move(best);
    return true;
}

inline bool ResolveRegionWeather(uint32_t regionFormId,
                                 uint32_t& weatherFormId,
                                 int32_t& chance,
                                 std::string& weatherEditorId) {
    using namespace fo3envq1000;
    weatherFormId = 0u;
    chance = -1;
    weatherEditorId.clear();
    if (regionFormId == 0u) return false;

    std::vector<uint8_t> payload;
    if (!FindRecord("REGN", regionFormId, payload)) return false;

    uint32_t currentDataType = 0xffffffffu;
    WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "RDAT", 4u) == 0 && size >= 4u) {
            currentDataType = Read32(bytes);
            return;
        }
        // RDAT type 3 is Weather. RDWT entries are {weather, chance, global}.
        if (currentDataType == 3u &&
            std::memcmp(type, "RDWT", 4u) == 0 && size >= 12u) {
            for (uint32_t pos = 0u; pos + 12u <= size; pos += 12u) {
                const uint32_t weather = Read32(bytes + pos);
                const int32_t entryChance = static_cast<int32_t>(Read32(bytes + pos + 4u));
                if (weather != 0u && entryChance > chance) {
                    weatherFormId = weather;
                    chance = entryChance;
                }
            }
        }
    });
    if (weatherFormId == 0u) return false;

    std::vector<uint8_t> weatherPayload;
    if (FindRecord("WTHR", weatherFormId, weatherPayload)) {
        WalkSubrecords(weatherPayload.data(), weatherPayload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "EDID", 4u) == 0 && weatherEditorId.empty()) {
                weatherEditorId = CString(bytes, size);
            }
        });
    }
    return true;
}

inline bool ReadFogPower(uint32_t weatherFormId,
                         float& dayPower, float& nightPower,
                         float dayFogRgb[3],
                         float& dayNear, float& dayFar) {
    using namespace fo3envq1000;
    dayPower = 1.0f;
    nightPower = 1.0f;
    dayNear = 0.0f;
    dayFar = 0.0f;
    dayFogRgb[0] = dayFogRgb[1] = dayFogRgb[2] = 0.0f;

    std::vector<uint8_t> payload;
    if (!FindRecord("WTHR", weatherFormId, payload)) return false;
    bool haveFnam = false;
    WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "NAM0", 4u) == 0 && size >= 160u) {
            const uint32_t p = 1u * 16u + 4u; // Fog / Day.
            dayFogRgb[0] = static_cast<float>(bytes[p + 0u]) / 255.0f;
            dayFogRgb[1] = static_cast<float>(bytes[p + 1u]) / 255.0f;
            dayFogRgb[2] = static_cast<float>(bytes[p + 2u]) / 255.0f;
        } else if (std::memcmp(type, "FNAM", 4u) == 0 && size >= 24u) {
            dayNear = ReadFloat(bytes + 0u);
            dayFar = ReadFloat(bytes + 4u);
            dayPower = ReadFloat(bytes + 16u);
            nightPower = ReadFloat(bytes + 20u);
            if (!std::isfinite(dayPower) || dayPower <= 0.0f) dayPower = 1.0f;
            if (!std::isfinite(nightPower) || nightPower <= 0.0f) nightPower = dayPower;
            haveFnam = true;
        }
    });
    return haveFnam;
}

inline bool CorrectImageSpaceLayout(Fo3ImageSpaceQ1280& image) {
    using namespace fo3envq1000;
    if (!image.valid || image.imageSpaceFormId == 0u) return false;
    std::vector<uint8_t> payload;
    if (!FindRecord("IMGS", image.imageSpaceFormId, payload)) return false;

    bool corrected = false;
    WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "DNAM", 4u) != 0 || size < 152u) return;
        auto f = [&](uint32_t offset) { return ReadFloat(bytes + offset); };

        // FO3's 152-byte ImageSpace layout:
        // saturation @100, contrast avg @104, contrast @108,
        // brightness @112, tint RGB @116..124, tint value @128,
        // 16 unused bytes @132..147, cinematic flags @148.
        image.cinematicSaturation = f(100u);
        image.cinematicContrastAvgLum = f(104u);
        image.cinematicContrast = f(108u);
        image.cinematicBrightness = f(112u);
        image.cinematicTint[0] = f(116u);
        image.cinematicTint[1] = f(120u);
        image.cinematicTint[2] = f(124u);
        image.cinematicTintValue = f(128u);
        image.cinematicFlags = bytes[148u];

        image.cinematicSaturation =
            std::clamp(std::isfinite(image.cinematicSaturation)
                           ? image.cinematicSaturation : 1.0f, -4.0f, 4.0f);
        image.cinematicContrastAvgLum =
            std::clamp(std::isfinite(image.cinematicContrastAvgLum)
                           ? image.cinematicContrastAvgLum : 0.5f, -4.0f, 4.0f);
        image.cinematicContrast =
            std::clamp(std::isfinite(image.cinematicContrast)
                           ? image.cinematicContrast : 1.0f, -4.0f, 4.0f);
        image.cinematicBrightness =
            std::clamp(std::isfinite(image.cinematicBrightness)
                           ? image.cinematicBrightness : 1.0f, 0.0f, 4.0f);
        image.cinematicTintValue =
            std::clamp(std::isfinite(image.cinematicTintValue)
                           ? image.cinematicTintValue : 0.0f, 0.0f, 1.0f);
        for (float& c : image.cinematicTint) {
            c = std::clamp(std::isfinite(c) ? c : 1.0f, 0.0f, 4.0f);
        }
        corrected = true;
    });
    return corrected;
}

// Used by Q14.0's BuildEndpointImages through a temporary preprocessor alias.
// It fixes the base IMGS parser before weather IMAD endpoints are layered on.
inline bool LoadFo3ImageSpaceBaseQ1410(uint32_t cellFormId,
                                       uint32_t worldspaceFormId) {
    const bool ready = LoadFo3ImageSpaceQ1280(cellFormId, worldspaceFormId);
    if (ready) CorrectImageSpaceLayout(gFo3ImageSpaceQ1280);
    return ready;
}

inline bool LoadFo3CellEnvironmentQ1410(uint32_t persistentCellFormId,
                                        uint32_t worldspaceFormId,
                                        float arrivalX, float arrivalY) {
    using namespace fo3envq1000;

    CellEnvironmentQ1410 cell;
    if (!FindSpatialCell(worldspaceFormId, arrivalX, arrivalY, cell)) {
        __android_log_print(
            ANDROID_LOG_WARN, TAG,
            "Q14.1 CELL ENV FAILED: persistentCell=%08X world=%08X arrival=(%.1f %.1f) reason=no-authored-spatial-cell",
            persistentCellFormId, worldspaceFormId, arrivalX, arrivalY);
        return LoadFo3ImageSpaceQ1320(persistentCellFormId, worldspaceFormId);
    }

    uint32_t regionWeather = 0u;
    int32_t chance = -1;
    std::string weatherEdid;
    if (!ResolveRegionWeather(cell.regionFormId, regionWeather, chance, weatherEdid)) {
        __android_log_print(
            ANDROID_LOG_WARN, TAG,
            "Q14.1 CELL ENV FAILED: cell=%08X EDID=%s region=%08X reason=no-region-weather",
            cell.cellFormId, cell.cellEditorId.empty() ? "<none>" : cell.cellEditorId.c_str(),
            cell.regionFormId);
        return LoadFo3ImageSpaceQ1320(cell.cellFormId, worldspaceFormId);
    }
    cell.weatherFormId = regionWeather;
    cell.weatherChance = chance;
    cell.weatherEditorId = weatherEdid;

    // Retain the WRLD-authored climate (time spans), but replace the selected
    // climate-list weather with the CELL region's 100%-chance weather.
    if (!LoadFo3EnvironmentQ1000(worldspaceFormId)) return false;
    Fo3EnvironmentQ1000& env = gFo3EnvironmentQ1000;
    env.weatherFormId = regionWeather;
    env.weatherEditorId.clear();
    if (!ReadWeather(regionWeather, env)) return false;

    // The Q14.0 per-frame sampler writes environment RGB in the established
    // Q13.9 domain before the first visible draw. Mark the old one-shot state
    // dirty so no stale world/weather conversion can be reused.
    fo3colorq1390::gEnvironmentWorld = 0u;
    fo3colorq1390::gEnvironmentConverted = false;
    fo3colorq1390::gSkyWeather = 0u;
    fo3colorq1390::gSkyConverted = false;

    // Load the CELL-authored XCIM, correct the 152-byte FO3 layout, then apply
    // the current weather's Day IMAD once. Q14.0 immediately replaces this with
    // interpolated endpoint values once the test clock starts.
    if (!LoadFo3ImageSpaceBaseQ1410(cell.cellFormId, worldspaceFormId)) return false;
    fo3imadq1300::ApplyWeatherImadQ1300(gFo3ImageSpaceQ1280);
    fo3weatherq1320::ApplyWeatherLightingQ1320(gFo3ImageSpaceQ1280);

    float dayFogRgb[3]{};
    float dayNear = 0.0f, dayFar = 0.0f;
    ReadFogPower(regionWeather, gFogDayPower, gFogNightPower,
                 dayFogRgb, dayNear, dayFar);
    gFogPower = gFogDayPower;
    gLastFogHour = -1;

    gCellEnvironment = cell;
    gCellEnvironment.valid = true;

    const Fo3ImageSpaceQ1280& image = gFo3ImageSpaceQ1280;
    __android_log_print(
        ANDROID_LOG_INFO, TAG,
        "Q14.1 CELL ENV READY: persistentCell=%08X spatialCell=%08X EDID=%s grid=(%d %d) gridFallback=%d region=%08X weather=%08X EDID=%s chance=%d imagespace=%08X IMGS=%08X IMGS_EDID=%s fogDayEncoded=(%.3f %.3f %.3f) fogNear=%.1f fogFar=%.1f fogPowerDay=%.3f fogPowerNight=%.3f source=Fallout3.esm",
        persistentCellFormId, cell.cellFormId,
        cell.cellEditorId.empty() ? "<none>" : cell.cellEditorId.c_str(),
        cell.gridX, cell.gridY, cell.gridFallbackDistance,
        cell.regionFormId, regionWeather,
        weatherEdid.empty() ? "<none>" : weatherEdid.c_str(), chance,
        cell.imageSpaceFormId, image.imageSpaceFormId,
        image.editorId.empty() ? "<none>" : image.editorId.c_str(),
        dayFogRgb[0], dayFogRgb[1], dayFogRgb[2],
        dayNear, dayFar, gFogDayPower, gFogNightPower);

    __android_log_print(
        ANDROID_LOG_INFO, TAG,
        "Q14.1 IMGS LAYOUT: IMGS=%08X EDID=%s DNAM=152 brightness=%.3f saturation=%.3f contrastAvg=%.3f contrast=%.3f tint=(%.3f %.3f %.3f) tintValue=%.3f flags=0x%02X layout=FO3-152",
        image.imageSpaceFormId,
        image.editorId.empty() ? "<none>" : image.editorId.c_str(),
        image.cinematicBrightness, image.cinematicSaturation,
        image.cinematicContrastAvgLum, image.cinematicContrast,
        image.cinematicTint[0], image.cinematicTint[1], image.cinematicTint[2],
        image.cinematicTintValue, image.cinematicFlags);
    return true;
}

inline float FogPowerForHour(float hour,
                             float sunriseBegin, float sunriseEnd,
                             float sunsetBegin, float sunsetEnd) {
    if (!std::isfinite(hour)) hour = 12.0f;
    hour = std::fmod(hour, 24.0f);
    if (hour < 0.0f) hour += 24.0f;
    const float sunriseMid = 0.5f * (sunriseBegin + sunriseEnd);
    const float sunsetMid = 0.5f * (sunsetBegin + sunsetEnd);

    float sunriseWeight = 0.0f;
    float sunsetWeight = 0.0f;
    float nightWeight = 0.0f;
    auto clamp01 = [](float v) { return std::clamp(v, 0.0f, 1.0f); };

    if (hour < sunriseBegin || hour >= sunsetEnd) {
        nightWeight = 1.0f;
    } else if (hour < sunriseMid) {
        const float t = clamp01((hour - sunriseBegin) /
            std::max(sunriseMid - sunriseBegin, 0.001f));
        nightWeight = 1.0f - t;
        sunriseWeight = t;
    } else if (hour < sunriseEnd) {
        const float t = clamp01((hour - sunriseMid) /
            std::max(sunriseEnd - sunriseMid, 0.001f));
        sunriseWeight = 1.0f - t;
    } else if (hour < sunsetBegin) {
        // Day: no night contribution.
    } else if (hour < sunsetMid) {
        const float t = clamp01((hour - sunsetBegin) /
            std::max(sunsetMid - sunsetBegin, 0.001f));
        sunsetWeight = t;
    } else {
        const float t = clamp01((hour - sunsetMid) /
            std::max(sunsetEnd - sunsetMid, 0.001f));
        sunsetWeight = 1.0f - t;
        nightWeight = t;
    }

    const float nightAmount =
        clamp01(nightWeight + 0.5f * (sunriseWeight + sunsetWeight));
    return gFogDayPower + (gFogNightPower - gFogDayPower) * nightAmount;
}

inline void UpdateFo3FogPowerQ1410(float hour,
                                   float sunriseBegin, float sunriseEnd,
                                   float sunsetBegin, float sunsetEnd) {
    gFogPower = std::clamp(
        FogPowerForHour(hour, sunriseBegin, sunriseEnd, sunsetBegin, sunsetEnd),
        0.01f, 8.0f);
    const int bucket = static_cast<int>(std::floor(hour));
    if (bucket != gLastFogHour) {
        gLastFogHour = bucket;
        __android_log_print(ANDROID_LOG_INFO, TAG,
                            "Q14.1 FOG POWER: hour=%.2f power=%.3f source=WTHR-FNAM",
                            hour, gFogPower);
    }
}

inline float GetFo3FogPowerQ1410() {
    return std::clamp(gFogPower, 0.01f, 8.0f);
}

} // namespace fo3cellenvq1410

inline bool LoadFo3ImageSpaceBaseQ1410(uint32_t cellFormId,
                                       uint32_t worldspaceFormId) {
    return fo3cellenvq1410::LoadFo3ImageSpaceBaseQ1410(cellFormId, worldspaceFormId);
}

inline bool LoadFo3CellEnvironmentQ1410(uint32_t persistentCellFormId,
                                        uint32_t worldspaceFormId,
                                        float arrivalX, float arrivalY) {
    return fo3cellenvq1410::LoadFo3CellEnvironmentQ1410(
        persistentCellFormId, worldspaceFormId, arrivalX, arrivalY);
}

inline void UpdateFo3FogPowerQ1410(float hour,
                                   float sunriseBegin, float sunriseEnd,
                                   float sunsetBegin, float sunsetEnd) {
    fo3cellenvq1410::UpdateFo3FogPowerQ1410(
        hour, sunriseBegin, sunriseEnd, sunsetBegin, sunsetEnd);
}

inline float GetFo3FogPowerQ1410() {
    return fo3cellenvq1410::GetFo3FogPowerQ1410();
}
