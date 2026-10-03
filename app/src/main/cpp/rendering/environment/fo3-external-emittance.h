#pragma once

#include "rendering/environment/fo3-visual-depth.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct Fo3ExternalEmittance {
    bool valid = false;
    bool regionDriven = false;
    uint32_t referenceFormId = 0u;
    uint32_t emittanceFormId = 0u;
    uint32_t weatherFormId = 0u;
    float color[3]{0.0f, 0.0f, 0.0f};
    std::string editorId;
};

inline std::unordered_map<uint32_t, Fo3ExternalEmittance> gFo3ExternalEmittance;
inline uint32_t gFo3ExternalEmittanceWorld = 0u;
inline bool gFo3ExternalEmittanceLoaded = false;

namespace fo3emittance {

using fo3visual::GroupFrame;
using fo3visual::InWorldspace;

constexpr const char* TAG = "FalloutQuest";
constexpr const char* ESM_PATH = fo3visual::ESM_PATH;
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
                           uint8_t header[fo3esm::HEADER_SIZE], uint64_t& offset,
                           uint32_t& sizeField, uint32_t& flags,
                           uint32_t& formId, uint64_t& payloadEnd) {
    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) return false;
        offset = static_cast<uint64_t>(rawOffset);
        while (!groups.empty() && offset >= groups.back().end) groups.pop_back();
        if (offset + fo3esm::HEADER_SIZE > static_cast<uint64_t>(fileSize)) return false;
        if (!fo3esm::ReadExact(file, header, fo3esm::HEADER_SIZE)) return false;
        sizeField = fo3esm::ReadU32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < fo3esm::HEADER_SIZE ||
                offset + sizeField > static_cast<uint64_t>(fileSize)) return false;
            groups.push_back(GroupFrame{offset + sizeField,
                                        fo3esm::ReadU32(header + 8u),
                                        fo3esm::ReadU32(header + 12u)});
            continue;
        }
        flags = fo3esm::ReadU32(header + 8u);
        formId = fo3esm::ReadU32(header + 12u);
        payloadEnd = offset + fo3esm::HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) return false;
        return true;
    }
}

inline bool Rewind(FILE* file) {
    return fseeko(file, 0, SEEK_SET) == 0;
}

inline void LogExample(const Fo3ExternalEmittance& value) {
    __android_log_print(ANDROID_LOG_INFO, TAG,
        "Q13.8 XEMI: ref=%08X emittance=%08X type=%s EDID=%s weather=%08X dayColor=(%.3f %.3f %.3f)",
        value.referenceFormId, value.emittanceFormId,
        value.regionDriven ? "REGN" : "LIGH",
        value.editorId.empty() ? "<none>" : value.editorId.c_str(),
        value.weatherFormId,
        value.color[0], value.color[1], value.color[2]);
}

} // namespace fo3emittance

inline void ResetFo3ExternalEmittance() {
    gFo3ExternalEmittance.clear();
    gFo3ExternalEmittanceWorld = 0u;
    gFo3ExternalEmittanceLoaded = false;
}

inline bool LoadFo3ExternalEmittance(uint32_t worldspaceFormId) {
    using namespace fo3emittance;
    if (gFo3ExternalEmittanceLoaded &&
        gFo3ExternalEmittanceWorld == worldspaceFormId) {
        return !gFo3ExternalEmittance.empty();
    }

    ResetFo3ExternalEmittance();
    gFo3ExternalEmittanceWorld = worldspaceFormId;
    gFo3ExternalEmittanceLoaded = true;
    if (worldspaceFormId == 0u) return false;

    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) {
        __android_log_print(ANDROID_LOG_WARN, TAG,
                            "Q13.8 XEMI ESM open failed: %s", ESM_PATH);
        return false;
    }
    const int64_t fileSize = fo3esm::FileSize(file);
    if (fileSize < static_cast<int64_t>(fo3esm::HEADER_SIZE)) {
        std::fclose(file);
        return false;
    }

    // Pass 1: external-emittance assignments on references in this exact WRLD.
    std::vector<RawRef> refs;
    std::unordered_set<uint32_t> wantedEmittance;
    std::vector<GroupFrame> groups;
    while (true) {
        uint8_t header[fo3esm::HEADER_SIZE]{};
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
        if (!fo3esm::ReadPayloadCurrent(file, sizeField, flags, payload)) break;
        uint32_t xemi = 0u;
        fo3esm::WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "XEMI", 4u) == 0 && size >= 4u) {
                xemi = fo3esm::ReadU32(bytes);
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
        uint8_t header[fo3esm::HEADER_SIZE]{};
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
        if (!fo3esm::ReadPayloadCurrent(file, sizeField, flags, payload)) break;
        if (isLight) {
            FixedLight value;
            bool haveColor = false;
            fo3esm::WalkSubrecords(payload.data(), payload.size(),
                           [&](const char* type, const uint8_t* bytes, uint32_t size) {
                if (std::memcmp(type, "EDID", 4u) == 0) {
                    value.editorId = fo3esm::ZString(bytes, size);
                } else if (std::memcmp(type, "DATA", 4u) == 0 && size >= 12u) {
                    value.color[0] = static_cast<float>(bytes[8u]) / 255.0f;
                    value.color[1] = static_cast<float>(bytes[9u]) / 255.0f;
                    value.color[2] = static_cast<float>(bytes[10u]) / 255.0f;
                    haveColor = true;
                } else if (std::memcmp(type, "FNAM", 4u) == 0 && size >= 4u) {
                    value.fade = fo3esm::ReadF32(bytes);
                }
            });
            if (!std::isfinite(value.fade)) value.fade = 1.0f;
            value.fade = std::clamp(value.fade, 0.0f, 16.0f);
            for (float& c : value.color) c *= value.fade;
            value.valid = haveColor;
            if (value.valid) fixedLights[formId] = value;
        } else {
            RegionLink value;
            fo3esm::WalkSubrecords(payload.data(), payload.size(),
                           [&](const char* type, const uint8_t* bytes, uint32_t size) {
                if (std::memcmp(type, "EDID", 4u) == 0) {
                    value.editorId = fo3esm::ZString(bytes, size);
                } else if (std::memcmp(type, "RDWT", 4u) == 0 && size >= 4u &&
                           value.weatherFormId == 0u) {
                    value.weatherFormId = fo3esm::ReadU32(bytes);
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
        uint8_t header[fo3esm::HEADER_SIZE]{};
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
        if (!fo3esm::ReadPayloadCurrent(file, sizeField, flags, payload)) break;
        WeatherDay value;
        fo3esm::WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "EDID", 4u) == 0) {
                value.editorId = fo3esm::ZString(bytes, size);
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
        Fo3ExternalEmittance value;
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
        if (value.valid) gFo3ExternalEmittance[raw.refFormId] = value;
        else ++unresolved;
    }

    __android_log_print(ANDROID_LOG_INFO, TAG,
        "Q13.8 EXTERNAL EMITTANCE READY: world=%08X xemiRefs=%zu resolved=%zu fixedLIGH=%zu regionWTHR=%zu unresolved=%zu time=DAY source=Fallout3.esm shaderFlag=0x%08X",
        worldspaceFormId, refs.size(), gFo3ExternalEmittance.size(),
        fixedCount, regionCount, unresolved, EXTERNAL_EMITTANCE_SHADER_FLAG);

    size_t logged = 0u;
    for (const auto& pair : gFo3ExternalEmittance) {
        if (logged >= 6u) break;
        LogExample(pair.second);
        ++logged;
    }
    return !gFo3ExternalEmittance.empty();
}

inline bool ResolveFo3ExternalEmittance(uint32_t worldspaceFormId,
                                              uint32_t referenceFormId,
                                              Fo3ExternalEmittance& out) {
    if (!gFo3ExternalEmittanceLoaded ||
        gFo3ExternalEmittanceWorld != worldspaceFormId) {
        LoadFo3ExternalEmittance(worldspaceFormId);
    }
    const auto found = gFo3ExternalEmittance.find(referenceFormId);
    if (found == gFo3ExternalEmittance.end()) {
        out = {};
        return false;
    }
    out = found->second;
    return out.valid;
}
