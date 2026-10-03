#pragma once

#include "fo3-esm-reader.h"

#include <android/log.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

struct Fo3PlacedLight {
    uint32_t refFormId = 0u;
    uint32_t baseFormId = 0u;
    std::string editorId;
    float position[3]{0.0f, 0.0f, 0.0f};
    float color[3]{1.0f, 1.0f, 1.0f};
    float radius = 1.0f;
    float falloff = 1.0f;
    float fade = 1.0f;
    uint32_t flags = 0u;
};

constexpr int FO3_SHADER_LIGHTS = 8;
inline std::vector<Fo3PlacedLight> gFo3PlacedLights;
inline int gFo3SelectedLightCount = 0;
inline float gFo3SelectedLightPosRadius[FO3_SHADER_LIGHTS * 4]{};
inline float gFo3SelectedLightColorFalloff[FO3_SHADER_LIGHTS * 4]{};
inline float gFo3EyePosition[3]{0.0f, 0.0f, 0.0f};

namespace fo3visual {

constexpr const char* TAG = "FalloutQuest";
constexpr const char* ESM_PATH =
    "/data/user/0/com.falloutquest.app/files/Fallout3/Data/Fallout3.esm";
constexpr uint32_t FLAG_INITIALLY_DISABLED = 0x00000800u;
constexpr float FO3_UNITS_PER_METRE = 70.0f;
constexpr float FLOOR_Y = -1.55f;
constexpr float SCENE_FORWARD = 0.0f;

struct GroupFrame {
    uint64_t end = 0u;
    uint32_t label = 0u;
    uint32_t type = 0u;
};

struct RawLightRef {
    uint32_t refFormId = 0u;
    uint32_t baseFormId = 0u;
    uint32_t flags = 0u;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    bool valid = false;
};

struct LightBase {
    uint32_t formId = 0u;
    std::string editorId;
    uint32_t radius = 0u;
    uint8_t color[4]{255u, 255u, 255u, 255u};
    uint32_t flags = 0u;
    float falloff = 1.0f;
    float fade = 1.0f;
    bool valid = false;
};

inline bool InWorldspace(const std::vector<GroupFrame>& groups, uint32_t worldspace) {
    for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
        if (it->type == 1u && it->label == worldspace) return true;
    }
    return false;
}

} // namespace fo3visual

inline void ResetFo3PlacedLights() {
    gFo3PlacedLights.clear();
    gFo3SelectedLightCount = 0;
    std::fill(std::begin(gFo3SelectedLightPosRadius),
              std::end(gFo3SelectedLightPosRadius), 0.0f);
    std::fill(std::begin(gFo3SelectedLightColorFalloff),
              std::end(gFo3SelectedLightColorFalloff), 0.0f);
}

inline bool LoadFo3PlacedLights(uint32_t worldspaceFormId,
                                     float arrivalX, float arrivalY, float arrivalZ) {
    using namespace fo3visual;
    ResetFo3PlacedLights();
    if (worldspaceFormId == 0u) return false;

    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) return false;
    const int64_t fileSize = fo3esm::FileSize(file);
    if (fileSize < static_cast<int64_t>(fo3esm::HEADER_SIZE)) {
        std::fclose(file);
        return false;
    }

    std::vector<GroupFrame> groups;
    std::vector<RawLightRef> refs;
    std::unordered_set<uint32_t> wantedBases;

    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        while (!groups.empty() && offset >= groups.back().end) groups.pop_back();
        if (offset + fo3esm::HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[fo3esm::HEADER_SIZE]{};
        if (!fo3esm::ReadExact(file, header, sizeof(header))) break;
        const uint32_t sizeField = fo3esm::ReadU32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < fo3esm::HEADER_SIZE || offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            groups.push_back(GroupFrame{offset + sizeField, fo3esm::ReadU32(header + 8u), fo3esm::ReadU32(header + 12u)});
            continue;
        }

        const uint32_t recordFlags = fo3esm::ReadU32(header + 8u);
        const uint32_t formId = fo3esm::ReadU32(header + 12u);
        const uint64_t payloadEnd = offset + fo3esm::HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        if (!InWorldspace(groups, worldspaceFormId) ||
            std::memcmp(header, "REFR", 4u) != 0 ||
            (recordFlags & FLAG_INITIALLY_DISABLED) != 0u) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        std::vector<uint8_t> payload;
        if (!fo3esm::ReadPayloadCurrent(file, sizeField, recordFlags, payload)) break;
        RawLightRef ref;
        ref.refFormId = formId;
        ref.flags = recordFlags;
        bool haveBase = false, haveTransform = false;
        fo3esm::WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "NAME", 4u) == 0 && size >= 4u) {
                ref.baseFormId = fo3esm::ReadU32(bytes);
                haveBase = ref.baseFormId != 0u;
            } else if (std::memcmp(type, "DATA", 4u) == 0 && size >= 24u) {
                ref.x = fo3esm::ReadF32(bytes + 0u);
                ref.y = fo3esm::ReadF32(bytes + 4u);
                ref.z = fo3esm::ReadF32(bytes + 8u);
                haveTransform = true;
            }
        });
        ref.valid = haveBase && haveTransform;
        if (ref.valid) {
            refs.push_back(ref);
            wantedBases.insert(ref.baseFormId);
        }
    }

    if (fseeko(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        return false;
    }

    std::unordered_map<uint32_t, LightBase> bases;
    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        if (offset + fo3esm::HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;
        uint8_t header[fo3esm::HEADER_SIZE]{};
        if (!fo3esm::ReadExact(file, header, sizeof(header))) break;
        const uint32_t sizeField = fo3esm::ReadU32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < fo3esm::HEADER_SIZE || offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            continue;
        }
        const uint32_t flags = fo3esm::ReadU32(header + 8u);
        const uint32_t formId = fo3esm::ReadU32(header + 12u);
        const uint64_t payloadEnd = offset + fo3esm::HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        if (std::memcmp(header, "LIGH", 4u) != 0 || wantedBases.find(formId) == wantedBases.end()) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        std::vector<uint8_t> payload;
        if (!fo3esm::ReadPayloadCurrent(file, sizeField, flags, payload)) break;
        LightBase base;
        base.formId = formId;
        fo3esm::WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "EDID", 4u) == 0) {
                base.editorId = fo3esm::ZString(bytes, size);
            } else if (std::memcmp(type, "DATA", 4u) == 0 && size >= 32u) {
                base.radius = fo3esm::ReadU32(bytes + 4u);
                base.color[0] = bytes[8u];
                base.color[1] = bytes[9u];
                base.color[2] = bytes[10u];
                base.color[3] = bytes[11u];
                base.flags = fo3esm::ReadU32(bytes + 12u);
                base.falloff = fo3esm::ReadF32(bytes + 16u);
                base.valid = base.radius > 0u;
            } else if (std::memcmp(type, "FNAM", 4u) == 0 && size >= 4u) {
                base.fade = fo3esm::ReadF32(bytes);
            }
        });
        if (base.valid) bases[formId] = std::move(base);
    }
    std::fclose(file);

    size_t offByDefault = 0u;
    size_t spotLights = 0u;
    size_t negativeLights = 0u;
    float minFade = 1e30f;
    float maxFade = 0.0f;
    for (const RawLightRef& ref : refs) {
        const auto found = bases.find(ref.baseFormId);
        if (found == bases.end()) continue;
        const LightBase& base = found->second;
        if ((base.flags & 0x00000020u) != 0u) {
            ++offByDefault;
            continue;
        }
        if ((base.flags & 0x00000200u) != 0u) ++spotLights;
        if ((base.flags & 0x00000004u) != 0u) ++negativeLights;

        Fo3PlacedLight light;
        light.refFormId = ref.refFormId;
        light.baseFormId = ref.baseFormId;
        light.editorId = base.editorId;
        light.position[0] = (ref.x - arrivalX) / FO3_UNITS_PER_METRE;
        light.position[1] = FLOOR_Y + (ref.z - arrivalZ) / FO3_UNITS_PER_METRE;
        light.position[2] = SCENE_FORWARD - (ref.y - arrivalY) / FO3_UNITS_PER_METRE;
        const float sign = (base.flags & 0x00000004u) != 0u ? -1.0f : 1.0f;
        const float authoredFade = std::isfinite(base.fade)
            ? std::clamp(base.fade, 0.0f, 16.0f)
            : 1.0f;
        light.fade = authoredFade;
        light.color[0] = sign * static_cast<float>(base.color[0]) / 255.0f * authoredFade;
        light.color[1] = sign * static_cast<float>(base.color[1]) / 255.0f * authoredFade;
        light.color[2] = sign * static_cast<float>(base.color[2]) / 255.0f * authoredFade;
        light.radius = std::max(0.10f, static_cast<float>(base.radius) / FO3_UNITS_PER_METRE);
        light.falloff = std::clamp(base.falloff, 0.25f, 8.0f);
        light.flags = base.flags;
        minFade = std::min(minFade, authoredFade);
        maxFade = std::max(maxFade, authoredFade);
        gFo3PlacedLights.push_back(std::move(light));
    }

    if (gFo3PlacedLights.empty()) {
        minFade = 0.0f;
        maxFade = 0.0f;
    }
    __android_log_print(ANDROID_LOG_INFO, TAG,
        "Q12.2 LIGH READY: worldspace=%08X refs=%zu lightBases=%zu activeLights=%zu offByDefault=%zu spot=%zu negative=%zu shaderNearest=%d fadeRange=(%.3f %.3f) authoredFade=1 source=Fallout3.esm",
        worldspaceFormId, refs.size(), bases.size(), gFo3PlacedLights.size(),
        offByDefault, spotLights, negativeLights, FO3_SHADER_LIGHTS,
        minFade, maxFade);
    return !gFo3PlacedLights.empty();
}

inline void UpdateFo3VisualEye(float x, float y, float z) {
    gFo3EyePosition[0] = x;
    gFo3EyePosition[1] = y;
    gFo3EyePosition[2] = z;

    // Q12.2: rank all candidates then keep the true nearest/radius-weighted
    // eight. The old fixed-array insertion stopped increasing `used` at eight,
    // so later lights could overwrite the last slot even when they ranked worse.
    std::vector<std::pair<float, size_t>> best;
    best.reserve(gFo3PlacedLights.size());
    for (size_t i = 0u; i < gFo3PlacedLights.size(); ++i) {
        const Fo3PlacedLight& light = gFo3PlacedLights[i];
        const float dx = x - light.position[0];
        const float dy = y - light.position[1];
        const float dz = z - light.position[2];
        const float score = (dx*dx + dy*dy + dz*dz) /
                            std::max(light.radius * light.radius, 0.25f);
        best.emplace_back(score, i);
    }
    std::sort(best.begin(), best.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    if (best.size() > static_cast<size_t>(FO3_SHADER_LIGHTS))
        best.resize(static_cast<size_t>(FO3_SHADER_LIGHTS));

    gFo3SelectedLightCount = static_cast<int>(best.size());
    std::fill(std::begin(gFo3SelectedLightPosRadius),
              std::end(gFo3SelectedLightPosRadius), 0.0f);
    std::fill(std::begin(gFo3SelectedLightColorFalloff),
              std::end(gFo3SelectedLightColorFalloff), 0.0f);
    for (int slot = 0; slot < gFo3SelectedLightCount; ++slot) {
        const Fo3PlacedLight& light = gFo3PlacedLights[best[static_cast<size_t>(slot)].second];
        float* pr = &gFo3SelectedLightPosRadius[slot * 4];
        float* cf = &gFo3SelectedLightColorFalloff[slot * 4];
        pr[0] = light.position[0]; pr[1] = light.position[1]; pr[2] = light.position[2]; pr[3] = light.radius;
        cf[0] = light.color[0]; cf[1] = light.color[1]; cf[2] = light.color[2]; cf[3] = light.falloff;
    }
}
