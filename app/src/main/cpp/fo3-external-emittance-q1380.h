#pragma once

#include "fo3-visual-depth-q1010.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct Fo3ExternalEmittanceQ1380 {
    bool valid = false;
    bool regionDriven = false;
    uint32_t referenceFormId = 0u;
    uint32_t emittanceFormId = 0u;
    uint32_t weatherFormId = 0u;
    float color[3]{0.0f, 0.0f, 0.0f};
    std::string editorId;
};

inline std::unordered_map<uint32_t, Fo3ExternalEmittanceQ1380> gFo3ExternalEmittanceQ1380;
inline uint32_t gFo3ExternalEmittanceWorldQ1380 = 0u;
inline bool gFo3ExternalEmittanceLoadedQ1380 = false;

namespace fo3emittanceq1380 {

using fo3visualq1010::CString;
using fo3visualq1010::FileSize;
using fo3visualq1010::FLAG_COMPRESSED;
using fo3visualq1010::GroupFrame;
using fo3visualq1010::HEADER_SIZE;
using fo3visualq1010::InWorldspace;
using fo3visualq1010::Read32;
using fo3visualq1010::ReadExact;
using fo3visualq1010::ReadFloat;
using fo3visualq1010::ReadPayload;
using fo3visualq1010::WalkSubrecords;

constexpr const char* TAG = "FalloutQuest";
constexpr const char* ESM_PATH = fo3visualq1010::ESM_PATH;
constexpr uint32_t EXTERNAL_EMITTANCE_SHADER_FLAG = 0x20000000u;

struct RawRef {
    uint32_t refFormId = 0u;
    uint32_t emittanceFormId = 0u;
};

struct FixedLight {
    bool valid = false;
    std::string editorId;
    float color[3]{0.0f, 0.0f, 0.0f};
    float fade = 1.0f;
};

struct RegionLink {
    bool valid = false;
    std::string editorId;
    uint32_t weatherFormId = 0u;
};

struct WeatherDay {
    bool valid = false;
    std::string editorId;
    float sunlight[3]{0.0f, 0.0f, 0.0f};
};

inline bool SeekNextRecord(FILE* file, int64_t fileSize,
                           std::vector<GroupFrame>& groups,
                           uint8_t header[HEADER_SIZE], uint64_t& offset,
                           uint32_t& sizeField, uint32_t& flags,
                           uint32_t& formId, uint64_t& payloadEnd) {
    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) return false;
        offset = static_cast<uint64_t>(rawOffset);
        while (!groups.empty() && offset >= groups.back().end) groups.pop_back();
        if (offset + HEADER_SIZE > static_cast<uint64_t>(fileSize)) return false;
        if (!ReadExact(file, header, HEADER_SIZE)) return false;
        sizeField = Read32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < HEADER_SIZE ||
                offset + sizeField > static_cast<uint64_t>(fileSize)) return false;
            groups.push_back(GroupFrame{offset + sizeField,
                                        Read32(header + 8u),
                                        Read32(header + 12u)});
            continue;
        }
        flags = Read32(header + 8u);
        formId = Read32(header + 12u);
        payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) return false;
        return true;
    }
}

inline bool Rewind(FILE* file) {
    return fseeko(file, 0, SEEK_SET) == 0;
}

inline void LogExample(const Fo3ExternalEmittanceQ1380& value) {
    __android_log_print(ANDROID_LOG_INFO, TAG,
        "Q13.8 XEMI: ref=%08X emittance=%08X type=%s EDID=%s weather=%08X dayColor=(%.3f %.3f %.3f)",
        value.referenceFormId, value.emittanceFormId,
        value.regionDriven ? "REGN" : "LIGH",
        value.editorId.empty() ? "<none>" : value.editorId.c_str(),
        value.weatherFormId,
        value.color[0], value.color[1], value.color[2]);
}

} // namespace fo3emittanceq1380

inline void ResetFo3ExternalEmittanceQ1380() {
    gFo3ExternalEmittanceQ1380.clear();
    gFo3ExternalEmittanceWorldQ1380 = 0u;
    gFo3ExternalEmittanceLoadedQ1380 = false;
}

inline bool LoadFo3ExternalEmittanceQ1380(uint32_t worldspaceFormId) {
    using namespace fo3emittanceq1380;
    if (gFo3ExternalEmittanceLoadedQ1380 &&
        gFo3ExternalEmittanceWorldQ1380 == worldspaceFormId) {
        return !gFo3ExternalEmittanceQ1380.empty();
    }

    ResetFo3ExternalEmittanceQ1380();
    gFo3ExternalEmittanceWorldQ1380 = worldspaceFormId;
    gFo3ExternalEmittanceLoadedQ1380 = true;
    if (worldspaceFormId == 0u) return false;

    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) {
        __android_log_print(ANDROID_LOG_WARN, TAG,
                            "Q13.8 XEMI ESM open failed: %s", ESM_PATH);
        return false;
    }
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        return false;
    }

    // Pass 1: external-emittance assignments on references in this exact WRLD.
    std::vector<RawRef> refs;
    std::unordered_set<uint32_t> wantedEmittance;
    std::vector<GroupFrame> groups;
    while (true) {
        uint8_t header[HEADER_SIZE]{};
        uint64_t offset = 0u, payloadEnd = 0u;
        uint32_t sizeField = 0u, flags = 0u, formId = 0u;
        if (!SeekNextRecord(file, fileSize, groups, header, offset,
                            sizeField, flags, formId, payloadEnd)) break;
        if (!InWorldspace(groups, worldspaceFormId) ||
            std::memcmp(header, "REFR", 4u) != 0) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }
        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, flags, payload)) break;
        uint32_t xemi = 0u;
        WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "XEMI", 4u) == 0 && size >= 4u) {
                xemi = Read32(bytes);
            }
        });
        if (xemi != 0u) {
            refs.push_back(RawRef{formId, xemi});
            wantedEmittance.insert(xemi);
        }
    }

    // Pass 2: resolve XEMI targets as fixed LIGH or daylight REGN -> WTHR.
    std::unordered_map<uint32_t, FixedLight> fixedLights;
    std::unordered_map<uint32_t, RegionLink> regions;
    std::unordered_set<uint32_t> wantedWeather;
    groups.clear();
    if (!Rewind(file)) {
        std::fclose(file);
        return false;
    }
    while (true) {
        uint8_t header[HEADER_SIZE]{};
        uint64_t offset = 0u, payloadEnd = 0u;
        uint32_t sizeField = 0u, flags = 0u, formId = 0u;
        if (!SeekNextRecord(file, fileSize, groups, header, offset,
                            sizeField, flags, formId, payloadEnd)) break;
        const bool wanted = wantedEmittance.find(formId) != wantedEmittance.end();
        const bool isLight = std::memcmp(header, "LIGH", 4u) == 0;
        const bool isRegion = std::memcmp(header, "REGN", 4u) == 0;
        if (!wanted || (!isLight && !isRegion)) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }
        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, flags, payload)) break;
        if (isLight) {
            FixedLight value;
            bool haveColor = false;
            WalkSubrecords(payload.data(), payload.size(),
                           [&](const char* type, const uint8_t* bytes, uint32_t size) {
                if (std::memcmp(type, "EDID", 4u) == 0) {
                    value.editorId = CString(bytes, size);
                } else if (std::memcmp(type, "DATA", 4u) == 0 && size >= 12u) {
                    value.color[0] = static_cast<float>(bytes[8u]) / 255.0f;
                    value.color[1] = static_cast<float>(bytes[9u]) / 255.0f;
                    value.color[2] = static_cast<float>(bytes[10u]) / 255.0f;
                    haveColor = true;
                } else if (std::memcmp(type, "FNAM", 4u) == 0 && size >= 4u) {
                    value.fade = ReadFloat(bytes);
                }
            });
            if (!std::isfinite(value.fade)) value.fade = 1.0f;
            value.fade = std::clamp(value.fade, 0.0f, 16.0f);
            for (float& c : value.color) c *= value.fade;
            value.valid = haveColor;
            if (value.valid) fixedLights[formId] = value;
        } else {
            RegionLink value;
            WalkSubrecords(payload.data(), payload.size(),
                           [&](const char* type, const uint8_t* bytes, uint32_t size) {
                if (std::memcmp(type, "EDID", 4u) == 0) {
                    value.editorId = CString(bytes, size);
                } else if (std::memcmp(type, "RDWT", 4u) == 0 && size >= 4u &&
                           value.weatherFormId == 0u) {
                    value.weatherFormId = Read32(bytes);
                }
            });
            value.valid = value.weatherFormId != 0u;
            if (value.valid) {
                wantedWeather.insert(value.weatherFormId);
                regions[formId] = value;
            }
        }
    }

    // Pass 3: exterior emittance REGN records are weather-driven. At Q13.8 the
    // renderer is still on the authored Day endpoint, so use NAM0 Sunlight/Day.
    // Q14.x will interpolate these same authored endpoints with game time.
    std::unordered_map<uint32_t, WeatherDay> weatherDay;
    groups.clear();
    if (!Rewind(file)) {
        std::fclose(file);
        return false;
    }
    while (true) {
        uint8_t header[HEADER_SIZE]{};
        uint64_t offset = 0u, payloadEnd = 0u;
        uint32_t sizeField = 0u, flags = 0u, formId = 0u;
        if (!SeekNextRecord(file, fileSize, groups, header, offset,
                            sizeField, flags, formId, payloadEnd)) break;
        if (std::memcmp(header, "WTHR", 4u) != 0 ||
            wantedWeather.find(formId) == wantedWeather.end()) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }
        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, flags, payload)) break;
        WeatherDay value;
        WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "EDID", 4u) == 0) {
                value.editorId = CString(bytes, size);
            } else if (std::memcmp(type, "NAM0", 4u) == 0 && size >= 80u) {
                // WTHR NAM0 = ten colour classes, each four time endpoints.
                // Sunlight class is index 4 (offset 64), Day is endpoint 1 (+4).
                const uint8_t* day = bytes + 68u;
                value.sunlight[0] = static_cast<float>(day[0]) / 255.0f;
                value.sunlight[1] = static_cast<float>(day[1]) / 255.0f;
                value.sunlight[2] = static_cast<float>(day[2]) / 255.0f;
                value.valid = true;
            }
        });
        if (value.valid) weatherDay[formId] = value;
    }
    std::fclose(file);

    size_t fixedCount = 0u, regionCount = 0u, unresolved = 0u;
    for (const RawRef& raw : refs) {
        Fo3ExternalEmittanceQ1380 value;
        value.referenceFormId = raw.refFormId;
        value.emittanceFormId = raw.emittanceFormId;
        const auto fixed = fixedLights.find(raw.emittanceFormId);
        if (fixed != fixedLights.end()) {
            value.valid = true;
            value.regionDriven = false;
            value.editorId = fixed->second.editorId;
            for (int i = 0; i < 3; ++i) value.color[i] = fixed->second.color[i];
            ++fixedCount;
        } else {
            const auto region = regions.find(raw.emittanceFormId);
            if (region != regions.end()) {
                const auto weather = weatherDay.find(region->second.weatherFormId);
                if (weather != weatherDay.end()) {
                    value.valid = true;
                    value.regionDriven = true;
                    value.editorId = region->second.editorId;
                    value.weatherFormId = region->second.weatherFormId;
                    for (int i = 0; i < 3; ++i) value.color[i] = weather->second.sunlight[i];
                    ++regionCount;
                }
            }
        }
        if (value.valid) gFo3ExternalEmittanceQ1380[raw.refFormId] = value;
        else ++unresolved;
    }

    __android_log_print(ANDROID_LOG_INFO, TAG,
        "Q13.8 EXTERNAL EMITTANCE READY: world=%08X xemiRefs=%zu resolved=%zu fixedLIGH=%zu regionWTHR=%zu unresolved=%zu time=DAY source=Fallout3.esm shaderFlag=0x%08X",
        worldspaceFormId, refs.size(), gFo3ExternalEmittanceQ1380.size(),
        fixedCount, regionCount, unresolved, EXTERNAL_EMITTANCE_SHADER_FLAG);

    size_t logged = 0u;
    for (const auto& pair : gFo3ExternalEmittanceQ1380) {
        if (logged >= 6u) break;
        LogExample(pair.second);
        ++logged;
    }
    return !gFo3ExternalEmittanceQ1380.empty();
}

inline bool ResolveFo3ExternalEmittanceQ1380(uint32_t worldspaceFormId,
                                              uint32_t referenceFormId,
                                              Fo3ExternalEmittanceQ1380& out) {
    if (!gFo3ExternalEmittanceLoadedQ1380 ||
        gFo3ExternalEmittanceWorldQ1380 != worldspaceFormId) {
        LoadFo3ExternalEmittanceQ1380(worldspaceFormId);
    }
    const auto found = gFo3ExternalEmittanceQ1380.find(referenceFormId);
    if (found == gFo3ExternalEmittanceQ1380.end()) {
        out = {};
        return false;
    }
    out = found->second;
    return out.valid;
}
