#pragma once

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
#include <zlib.h>

struct Fo3PlacedLightQ1010 {
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

constexpr int FO3_SHADER_LIGHTS_Q1010 = 8;
inline std::vector<Fo3PlacedLightQ1010> gFo3PlacedLightsQ1010;
inline int gFo3SelectedLightCountQ1010 = 0;
inline float gFo3SelectedLightPosRadiusQ1010[FO3_SHADER_LIGHTS_Q1010 * 4]{};
inline float gFo3SelectedLightColorFalloffQ1010[FO3_SHADER_LIGHTS_Q1010 * 4]{};
inline float gFo3EyePositionQ1010[3]{0.0f, 0.0f, 0.0f};

namespace fo3visualq1010 {

constexpr const char* TAG = "FalloutQuest";
constexpr const char* ESM_PATH =
    "/data/user/0/com.falloutquest.app/files/Fallout3/Data/Fallout3.esm";
constexpr uint32_t FLAG_COMPRESSED = 0x00040000u;
constexpr uint32_t FLAG_INITIALLY_DISABLED = 0x00000800u;
constexpr uint64_t HEADER_SIZE = 24u;
constexpr uint32_t MAX_RECORD_BYTES = 64u * 1024u * 1024u;
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

inline uint16_t Read16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8u);
}

inline uint32_t Read32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8u) |
           (static_cast<uint32_t>(p[2]) << 16u) |
           (static_cast<uint32_t>(p[3]) << 24u);
}

inline float ReadFloat(const uint8_t* p) {
    const uint32_t bits = Read32(p);
    float out = 0.0f;
    std::memcpy(&out, &bits, sizeof(out));
    return out;
}

inline bool ReadExact(FILE* file, void* dst, size_t size) {
    return std::fread(dst, 1u, size, file) == size;
}

inline int64_t FileSize(FILE* file) {
    const off_t current = ftello(file);
    if (current < 0) return -1;
    if (fseeko(file, 0, SEEK_END) != 0) return -1;
    const off_t end = ftello(file);
    fseeko(file, current, SEEK_SET);
    return static_cast<int64_t>(end);
}

inline bool Inflate(const std::vector<uint8_t>& stored, std::vector<uint8_t>& out) {
    if (stored.size() < 4u) return false;
    const uint32_t inflatedSize = Read32(stored.data());
    if (inflatedSize == 0u || inflatedSize > MAX_RECORD_BYTES) return false;
    out.resize(inflatedSize);
    uLongf dst = static_cast<uLongf>(out.size());
    const int result = uncompress(reinterpret_cast<Bytef*>(out.data()), &dst,
                                  reinterpret_cast<const Bytef*>(stored.data() + 4u),
                                  static_cast<uLong>(stored.size() - 4u));
    if (result != Z_OK || dst != inflatedSize) {
        out.clear();
        return false;
    }
    return true;
}

inline bool ReadPayload(FILE* file, uint32_t storedSize, uint32_t flags,
                        std::vector<uint8_t>& out) {
    if (storedSize == 0u || storedSize > MAX_RECORD_BYTES) return false;
    std::vector<uint8_t> stored(storedSize);
    if (!ReadExact(file, stored.data(), stored.size())) return false;
    if ((flags & FLAG_COMPRESSED) == 0u) {
        out.swap(stored);
        return true;
    }
    return Inflate(stored, out);
}

inline void WalkSubrecords(
    const uint8_t* data, size_t size,
    const std::function<void(const char*, const uint8_t*, uint32_t)>& visitor) {
    size_t pos = 0u;
    uint32_t extendedSize = 0u;
    while (pos + 6u <= size) {
        const char* type = reinterpret_cast<const char*>(data + pos);
        const uint16_t size16 = Read16(data + pos + 4u);
        pos += 6u;
        if (std::memcmp(type, "XXXX", 4u) == 0) {
            if (size16 != 4u || pos + 4u > size) return;
            extendedSize = Read32(data + pos);
            pos += 4u;
            continue;
        }
        const uint32_t subSize = extendedSize ? extendedSize : size16;
        extendedSize = 0u;
        if (subSize > size - pos) return;
        visitor(type, data + pos, subSize);
        pos += subSize;
    }
}

inline bool InWorldspace(const std::vector<GroupFrame>& groups, uint32_t worldspace) {
    for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
        if (it->type == 1u && it->label == worldspace) return true;
    }
    return false;
}

inline std::string CString(const uint8_t* bytes, uint32_t size) {
    size_t len = 0u;
    while (len < size && bytes[len] != 0u) ++len;
    return std::string(reinterpret_cast<const char*>(bytes), len);
}

} // namespace fo3visualq1010

inline void ResetFo3PlacedLightsQ1010() {
    gFo3PlacedLightsQ1010.clear();
    gFo3SelectedLightCountQ1010 = 0;
    std::fill(std::begin(gFo3SelectedLightPosRadiusQ1010),
              std::end(gFo3SelectedLightPosRadiusQ1010), 0.0f);
    std::fill(std::begin(gFo3SelectedLightColorFalloffQ1010),
              std::end(gFo3SelectedLightColorFalloffQ1010), 0.0f);
}

inline bool LoadFo3PlacedLightsQ1010(uint32_t worldspaceFormId,
                                     float arrivalX, float arrivalY, float arrivalZ) {
    using namespace fo3visualq1010;
    ResetFo3PlacedLightsQ1010();
    if (worldspaceFormId == 0u) return false;

    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) return false;
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
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
        if (offset + HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[HEADER_SIZE]{};
        if (!ReadExact(file, header, sizeof(header))) break;
        const uint32_t sizeField = Read32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < HEADER_SIZE || offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            groups.push_back(GroupFrame{offset + sizeField, Read32(header + 8u), Read32(header + 12u)});
            continue;
        }

        const uint32_t recordFlags = Read32(header + 8u);
        const uint32_t formId = Read32(header + 12u);
        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        if (!InWorldspace(groups, worldspaceFormId) ||
            std::memcmp(header, "REFR", 4u) != 0 ||
            (recordFlags & FLAG_INITIALLY_DISABLED) != 0u) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, recordFlags, payload)) break;
        RawLightRef ref;
        ref.refFormId = formId;
        ref.flags = recordFlags;
        bool haveBase = false, haveTransform = false;
        WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "NAME", 4u) == 0 && size >= 4u) {
                ref.baseFormId = Read32(bytes);
                haveBase = ref.baseFormId != 0u;
            } else if (std::memcmp(type, "DATA", 4u) == 0 && size >= 24u) {
                ref.x = ReadFloat(bytes + 0u);
                ref.y = ReadFloat(bytes + 4u);
                ref.z = ReadFloat(bytes + 8u);
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
        if (offset + HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;
        uint8_t header[HEADER_SIZE]{};
        if (!ReadExact(file, header, sizeof(header))) break;
        const uint32_t sizeField = Read32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < HEADER_SIZE || offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            continue;
        }
        const uint32_t flags = Read32(header + 8u);
        const uint32_t formId = Read32(header + 12u);
        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        if (std::memcmp(header, "LIGH", 4u) != 0 || wantedBases.find(formId) == wantedBases.end()) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, flags, payload)) break;
        LightBase base;
        base.formId = formId;
        WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "EDID", 4u) == 0) {
                base.editorId = CString(bytes, size);
            } else if (std::memcmp(type, "DATA", 4u) == 0 && size >= 32u) {
                base.radius = Read32(bytes + 4u);
                base.color[0] = bytes[8u];
                base.color[1] = bytes[9u];
                base.color[2] = bytes[10u];
                base.color[3] = bytes[11u];
                base.flags = Read32(bytes + 12u);
                base.falloff = ReadFloat(bytes + 16u);
                base.valid = base.radius > 0u;
            } else if (std::memcmp(type, "FNAM", 4u) == 0 && size >= 4u) {
                base.fade = ReadFloat(bytes);
            }
        });
        if (base.valid) bases[formId] = std::move(base);
    }
    std::fclose(file);

    size_t offByDefault = 0u;
    size_t spotLights = 0u;
    size_t negativeLights = 0u;
    float minFadeQ1220 = 1e30f;
    float maxFadeQ1220 = 0.0f;
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

        Fo3PlacedLightQ1010 light;
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
        minFadeQ1220 = std::min(minFadeQ1220, authoredFade);
        maxFadeQ1220 = std::max(maxFadeQ1220, authoredFade);
        gFo3PlacedLightsQ1010.push_back(std::move(light));
    }

    if (gFo3PlacedLightsQ1010.empty()) {
        minFadeQ1220 = 0.0f;
        maxFadeQ1220 = 0.0f;
    }
    __android_log_print(ANDROID_LOG_INFO, TAG,
        "Q12.2 LIGH READY: worldspace=%08X refs=%zu lightBases=%zu activeLights=%zu offByDefault=%zu spot=%zu negative=%zu shaderNearest=%d fadeRange=(%.3f %.3f) authoredFade=1 source=Fallout3.esm",
        worldspaceFormId, refs.size(), bases.size(), gFo3PlacedLightsQ1010.size(),
        offByDefault, spotLights, negativeLights, FO3_SHADER_LIGHTS_Q1010,
        minFadeQ1220, maxFadeQ1220);
    return !gFo3PlacedLightsQ1010.empty();
}

inline void UpdateFo3VisualEyeQ1010(float x, float y, float z) {
    gFo3EyePositionQ1010[0] = x;
    gFo3EyePositionQ1010[1] = y;
    gFo3EyePositionQ1010[2] = z;

    // Q12.2: rank all candidates then keep the true nearest/radius-weighted
    // eight. The old fixed-array insertion stopped increasing `used` at eight,
    // so later lights could overwrite the last slot even when they ranked worse.
    std::vector<std::pair<float, size_t>> best;
    best.reserve(gFo3PlacedLightsQ1010.size());
    for (size_t i = 0u; i < gFo3PlacedLightsQ1010.size(); ++i) {
        const Fo3PlacedLightQ1010& light = gFo3PlacedLightsQ1010[i];
        const float dx = x - light.position[0];
        const float dy = y - light.position[1];
        const float dz = z - light.position[2];
        const float score = (dx*dx + dy*dy + dz*dz) /
                            std::max(light.radius * light.radius, 0.25f);
        best.emplace_back(score, i);
    }
    std::sort(best.begin(), best.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    if (best.size() > static_cast<size_t>(FO3_SHADER_LIGHTS_Q1010))
        best.resize(static_cast<size_t>(FO3_SHADER_LIGHTS_Q1010));

    gFo3SelectedLightCountQ1010 = static_cast<int>(best.size());
    std::fill(std::begin(gFo3SelectedLightPosRadiusQ1010),
              std::end(gFo3SelectedLightPosRadiusQ1010), 0.0f);
    std::fill(std::begin(gFo3SelectedLightColorFalloffQ1010),
              std::end(gFo3SelectedLightColorFalloffQ1010), 0.0f);
    for (int slot = 0; slot < gFo3SelectedLightCountQ1010; ++slot) {
        const Fo3PlacedLightQ1010& light = gFo3PlacedLightsQ1010[best[static_cast<size_t>(slot)].second];
        float* pr = &gFo3SelectedLightPosRadiusQ1010[slot * 4];
        float* cf = &gFo3SelectedLightColorFalloffQ1010[slot * 4];
        pr[0] = light.position[0]; pr[1] = light.position[1]; pr[2] = light.position[2]; pr[3] = light.radius;
        cf[0] = light.color[0]; cf[1] = light.color[1]; cf[2] = light.color[2]; cf[3] = light.falloff;
    }
}
