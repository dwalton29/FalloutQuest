#include "fo3-water-q2070.h"
#include "fo3-texture-bsa.h"

#include <GLES3/gl3.h>
#include <android/log.h>
#include <zlib.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

constexpr const char* TAG = "FalloutQuest";
constexpr const char* ESM_PATH =
    "/data/user/0/com.falloutquest.app/files/Fallout3/Data/Fallout3.esm";
constexpr uint32_t FLAG_COMPRESSED = 0x00040000u;
constexpr uint64_t HEADER_SIZE = 24u;
constexpr uint32_t MAX_RECORD_BYTES = 64u * 1024u * 1024u;
constexpr float CELL_SIZE = 4096.0f;
constexpr int WATER_RADIUS_CELLS = 3;

#define WLOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define WLOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define WLOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

struct GroupFrame {
    uint64_t end = 0u;
    uint32_t label = 0u;
    uint32_t type = 0u;
};

struct RawWaterCell {
    uint32_t formId = 0u;
    int32_t gridX = 0;
    int32_t gridY = 0;
    bool hasGrid = false;
    bool hasWaterHeight = false;
    float waterHeight = 0.0f;
    uint32_t waterType = 0u;
    std::string noiseTexture;
};

std::vector<Fo3WaterCellQ2070> gWaterCells;
uint32_t gWorldspace = 0u;

GLuint gProgram = 0u;
GLuint gVao = 0u;
GLuint gVbo = 0u;
GLuint gNoiseTexture = 0u;
std::string gNoiseTexturePath;
bool gNoiseTextureReal = false;

GLint gMvpLocation = -1;
GLint gNoiseLocation = -1;
GLint gSceneColorLocation = -1;
GLint gSceneDepthLocation = -1;
GLint gReflectionMapLocation = -1;
GLint gReflectionReadyLocation = -1;
GLint gReflectionMvpLocation = -1;
GLint gReflectivityLocation = -1;
GLint gReflectionHdrLocation = -1;
GLint gSceneSnapshotReadyLocation = -1;
GLint gViewportLocation = -1;
GLint gEyePositionLocation = -1;
GLint gSunDirectionLocation = -1;
GLint gSunColorLocation = -1;
GLint gSceneFogColorLocation = -1;
GLint gSceneFogNearFarLocation = -1;
GLint gSceneFogPowerLocation = -1;
GLint gClipNearFarLocation = -1;
GLint gUnitsPerMetreLocation = -1;
GLint gTimeLocation = -1;
GLint gShallowLocation = -1;
GLint gDeepLocation = -1;
GLint gReflectionLocation = -1;
GLint gNormalUvScaleLocation = -1;
GLint gFresnelLocation = -1;
GLint gShininessLocation = -1;
GLint gDepthFalloffLocation = -1;
GLint gWaterFogNearFarLocation = -1;
GLint gWaterFogAmountLocation = -1;
GLint gDistortionLocation = -1;
GLint gNoiseScaleLocation = -1;
GLint gLayerUvScaleLocation = -1;
GLint gLayerWindDirLocation = -1;
GLint gLayerWindSpeedLocation = -1;
GLint gLayerAmpLocation = -1;

bool gRenderLogged = false;
bool gNoiseLogged = false;
const auto gWaterTimeOrigin = std::chrono::steady_clock::now();

uint16_t ReadLe16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8u);
}

uint32_t ReadLe32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8u) |
           (static_cast<uint32_t>(p[2]) << 16u) |
           (static_cast<uint32_t>(p[3]) << 24u);
}

float ReadLeFloat(const uint8_t* p) {
    const uint32_t bits = ReadLe32(p);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool ReadExact(FILE* file, void* dst, size_t size) {
    return std::fread(dst, 1, size, file) == size;
}

int64_t FileSize(FILE* file) {
    const off_t current = ftello(file);
    if (current < 0) return -1;
    if (fseeko(file, 0, SEEK_END) != 0) return -1;
    const off_t end = ftello(file);
    fseeko(file, current, SEEK_SET);
    return static_cast<int64_t>(end);
}

bool InflateRecord(const std::vector<uint8_t>& stored,
                   std::vector<uint8_t>& out) {
    if (stored.size() < 4u) return false;
    const uint32_t inflatedSize = ReadLe32(stored.data());
    if (inflatedSize == 0u || inflatedSize > MAX_RECORD_BYTES) return false;
    out.resize(inflatedSize);
    uLongf destLen = static_cast<uLongf>(out.size());
    const int result = uncompress(
        reinterpret_cast<Bytef*>(out.data()), &destLen,
        reinterpret_cast<const Bytef*>(stored.data() + 4u),
        static_cast<uLong>(stored.size() - 4u));
    if (result != Z_OK || destLen != inflatedSize) {
        out.clear();
        return false;
    }
    return true;
}

bool ReadPayload(FILE* file, uint32_t storedSize, uint32_t flags,
                 std::vector<uint8_t>& out) {
    if (storedSize == 0u || storedSize > MAX_RECORD_BYTES) return false;
    std::vector<uint8_t> stored(storedSize);
    if (!ReadExact(file, stored.data(), stored.size())) return false;
    if ((flags & FLAG_COMPRESSED) == 0u) {
        out.swap(stored);
        return true;
    }
    return InflateRecord(stored, out);
}

template <typename Visitor>
void WalkSubrecords(const uint8_t* data, size_t size, Visitor&& visitor) {
    size_t pos = 0u;
    uint32_t extendedSize = 0u;
    while (pos + 6u <= size) {
        const char* type = reinterpret_cast<const char*>(data + pos);
        const uint16_t size16 = ReadLe16(data + pos + 4u);
        pos += 6u;
        if (std::memcmp(type, "XXXX", 4u) == 0) {
            if (size16 != 4u || pos + 4u > size) return;
            extendedSize = ReadLe32(data + pos);
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

std::string ReadCString(const uint8_t* bytes, uint32_t size) {
    size_t len = 0u;
    while (len < size && bytes[len] != 0u) ++len;
    return std::string(reinterpret_cast<const char*>(bytes), len);
}

bool InWorldspace(const std::vector<GroupFrame>& groups,
                  uint32_t worldspaceFormId) {
    for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
        if (it->type == 1u && it->label == worldspaceFormId) return true;
    }
    return false;
}

void ReadColor(const uint8_t* p, float out[4]) {
    out[0] = static_cast<float>(p[0]) / 255.0f;
    out[1] = static_cast<float>(p[1]) / 255.0f;
    out[2] = static_cast<float>(p[2]) / 255.0f;
    out[3] = static_cast<float>(p[3]) / 255.0f;
}

bool DecodeVisualData(const uint8_t* bytes, uint32_t size,
                      Fo3WaterTypeQ2070& out) {
    // Fallout 3 WATR visual data layout (DNAM 196-byte form / long DATA form).
    // Offsets correspond directly to the GECK/xEdit water properties.
    if (!bytes || size < 196u) return false;
    out.sunPower = ReadLeFloat(bytes + 16u);
    out.reflectivity = ReadLeFloat(bytes + 20u);
    out.fresnelAmount = ReadLeFloat(bytes + 24u);
    out.aboveFogNear = ReadLeFloat(bytes + 32u);
    out.aboveFogFar = ReadLeFloat(bytes + 36u);
    ReadColor(bytes + 40u, out.shallowColor);
    ReadColor(bytes + 44u, out.deepColor);
    ReadColor(bytes + 48u, out.reflectionColor);
    out.noiseScale = ReadLeFloat(bytes + 96u);
    out.windDirection[0] = ReadLeFloat(bytes + 100u);
    out.windDirection[1] = ReadLeFloat(bytes + 104u);
    out.windDirection[2] = ReadLeFloat(bytes + 108u);
    out.windSpeed[0] = ReadLeFloat(bytes + 112u);
    out.windSpeed[1] = ReadLeFloat(bytes + 116u);
    out.windSpeed[2] = ReadLeFloat(bytes + 120u);
    out.depthFalloffStart = ReadLeFloat(bytes + 124u);
    out.depthFalloffEnd = ReadLeFloat(bytes + 128u);
    out.aboveFogAmount = ReadLeFloat(bytes + 132u);
    out.normalUvScale = ReadLeFloat(bytes + 136u);
    out.distortionAmount = ReadLeFloat(bytes + 152u);
    out.shininess = ReadLeFloat(bytes + 156u);
    out.reflectionHdrMultiplier = ReadLeFloat(bytes + 160u);
    out.uvScale[0] = ReadLeFloat(bytes + 172u);
    out.uvScale[1] = ReadLeFloat(bytes + 176u);
    out.uvScale[2] = ReadLeFloat(bytes + 180u);
    out.amplitudeScale[0] = ReadLeFloat(bytes + 184u);
    out.amplitudeScale[1] = ReadLeFloat(bytes + 188u);
    out.amplitudeScale[2] = ReadLeFloat(bytes + 192u);
    return true;
}

bool ReadWorldDefaults(uint32_t worldspaceFormId,
                       uint32_t& defaultWaterType,
                       std::string& noiseTexture) {
    defaultWaterType = 0u;
    noiseTexture.clear();
    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) return false;
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        return false;
    }

    bool found = false;
    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        if (offset + HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[HEADER_SIZE]{};
        if (!ReadExact(file, header, sizeof(header))) break;
        const uint32_t sizeField = ReadLe32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) continue;

        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        const uint32_t formId = ReadLe32(header + 12u);
        if (std::memcmp(header, "WRLD", 4u) != 0 ||
            formId != worldspaceFormId) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        const uint32_t flags = ReadLe32(header + 8u);
        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, flags, payload)) break;
        WalkSubrecords(payload.data(), payload.size(),
            [&](const char* type, const uint8_t* bytes, uint32_t subSize) {
                if (std::memcmp(type, "NAM2", 4u) == 0 && subSize >= 4u) {
                    defaultWaterType = ReadLe32(bytes);
                } else if (std::memcmp(type, "XNAM", 4u) == 0) {
                    noiseTexture = ReadCString(bytes, subSize);
                }
            });
        found = true;
        break;
    }

    std::fclose(file);
    return found;
}

bool CollectWaterCells(uint32_t worldspaceFormId,
                       float arrivalX, float arrivalY,
                       std::vector<RawWaterCell>& out) {
    out.clear();
    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) return false;
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        return false;
    }

    const int32_t targetX =
        static_cast<int32_t>(std::floor(arrivalX / CELL_SIZE));
    const int32_t targetY =
        static_cast<int32_t>(std::floor(arrivalY / CELL_SIZE));

    std::vector<GroupFrame> groups;
    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        while (!groups.empty() && offset >= groups.back().end) groups.pop_back();
        if (offset + HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[HEADER_SIZE]{};
        if (!ReadExact(file, header, sizeof(header))) break;
        const uint32_t sizeField = ReadLe32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < HEADER_SIZE ||
                offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            groups.push_back(GroupFrame{offset + sizeField,
                                        ReadLe32(header + 8u),
                                        ReadLe32(header + 12u)});
            continue;
        }

        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        if (!InWorldspace(groups, worldspaceFormId) ||
            std::memcmp(header, "CELL", 4u) != 0) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        const uint32_t flags = ReadLe32(header + 8u);
        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, flags, payload)) break;

        RawWaterCell cell;
        cell.formId = ReadLe32(header + 12u);
        WalkSubrecords(payload.data(), payload.size(),
            [&](const char* type, const uint8_t* bytes, uint32_t subSize) {
                if (std::memcmp(type, "XCLC", 4u) == 0 && subSize >= 8u) {
                    cell.gridX = static_cast<int32_t>(ReadLe32(bytes));
                    cell.gridY = static_cast<int32_t>(ReadLe32(bytes + 4u));
                    cell.hasGrid = true;
                } else if (std::memcmp(type, "XCLW", 4u) == 0 &&
                           subSize >= 4u) {
                    cell.waterHeight = ReadLeFloat(bytes);
                    // CELL XCLW uses INT32_MIN-as-float (-2147483648) as
                    // the no-water sentinel in the shipped master.
                    cell.hasWaterHeight =
                        std::isfinite(cell.waterHeight) &&
                        cell.waterHeight > -2.0e9f &&
                        std::fabs(cell.waterHeight) < 1.0e30f;
                } else if (std::memcmp(type, "XCWT", 4u) == 0 &&
                           subSize >= 4u) {
                    cell.waterType = ReadLe32(bytes);
                } else if (std::memcmp(type, "XNAM", 4u) == 0) {
                    cell.noiseTexture = ReadCString(bytes, subSize);
                }
            });

        if (cell.hasGrid && cell.hasWaterHeight &&
            std::abs(cell.gridX - targetX) <= WATER_RADIUS_CELLS &&
            std::abs(cell.gridY - targetY) <= WATER_RADIUS_CELLS) {
            out.push_back(std::move(cell));
        }
    }

    std::fclose(file);
    return true;
}

bool ResolveWaterTypes(const std::unordered_set<uint32_t>& wanted,
                       std::unordered_map<uint32_t, Fo3WaterTypeQ2070>& out) {
    out.clear();
    if (wanted.empty()) return true;

    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) return false;
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        return false;
    }

    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        if (offset + HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[HEADER_SIZE]{};
        if (!ReadExact(file, header, sizeof(header))) break;
        const uint32_t sizeField = ReadLe32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) continue;

        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        const uint32_t formId = ReadLe32(header + 12u);
        if (std::memcmp(header, "WATR", 4u) != 0 ||
            wanted.find(formId) == wanted.end()) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        const uint32_t flags = ReadLe32(header + 8u);
        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, flags, payload)) break;

        Fo3WaterTypeQ2070 water;
        water.formId = formId;
        bool visualDecoded = false;
        WalkSubrecords(payload.data(), payload.size(),
            [&](const char* type, const uint8_t* bytes, uint32_t subSize) {
                if (std::memcmp(type, "EDID", 4u) == 0) {
                    water.editorId = ReadCString(bytes, subSize);
                } else if (std::memcmp(type, "NNAM", 4u) == 0) {
                    water.noiseTexturePath = ReadCString(bytes, subSize);
                } else if (std::memcmp(type, "ANAM", 4u) == 0 && subSize >= 1u) {
                    water.opacity = bytes[0];
                } else if (std::memcmp(type, "FNAM", 4u) == 0 && subSize >= 1u) {
                    water.flags = bytes[0];
                } else if ((std::memcmp(type, "DNAM", 4u) == 0 ||
                            std::memcmp(type, "DATA", 4u) == 0) &&
                           subSize >= 196u && !visualDecoded) {
                    visualDecoded = DecodeVisualData(bytes, subSize, water);
                }
            });

        water.valid = visualDecoded;
        out[formId] = water;
        if (out.size() == wanted.size()) break;
    }

    std::fclose(file);
    return true;
}

GLuint CompileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[1024]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        WLOGE("Q20.7A WATER shader compile failed: %s", log);
        glDeleteShader(shader);
        return 0u;
    }
    return shader;
}

bool EnsureNoiseTexture(const std::string& path) {
    if (gNoiseTexture && path == gNoiseTexturePath) return true;

    if (gNoiseTexture) {
        glDeleteTextures(1, &gNoiseTexture);
        gNoiseTexture = 0u;
    }
    gNoiseTexturePath = path;
    gNoiseTextureReal = false;

    Fo3RgbaTexture decoded;
    const bool loaded = !path.empty() && LoadFalloutTextureRgba(path, decoded) &&
        decoded.width > 0 && decoded.height > 0 && !decoded.rgba.empty();

    uint8_t fallback[4]{128u, 128u, 255u, 255u};
    const uint8_t* pixels = loaded ? decoded.rgba.data() : fallback;
    const int width = loaded ? decoded.width : 1;
    const int height = loaded ? decoded.height : 1;

    glGenTextures(1, &gNoiseTexture);
    glBindTexture(GL_TEXTURE_2D, gNoiseTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    gNoiseTextureReal = loaded;

    WLOGI("Q20.8A WATER NOISE: path=%s loaded=%d size=%dx%d format=%s source=%s prepass=INLINE_RECOVERED_3LAYER_SOBEL",
          path.empty() ? "<none>" : path.c_str(), loaded ? 1 : 0,
          width, height,
          loaded ? decoded.format.c_str() : "fallback-flat",
          loaded ? "Fallout-BSA-DDS" : "fallback");
    return gNoiseTexture != 0u;
}

bool EnsureRenderer() {
    if (gProgram && gVao && gVbo) return true;

    static const char* vs = R"(
        #version 300 es
        precision highp float;
        layout(location=0) in vec3 aPosition;
        layout(location=1) in vec2 aGameXY;
        uniform mat4 uMvp;
        out highp vec3 vWorldPos;
        out highp vec2 vGameXY;
        void main() {
            vWorldPos = aPosition;
            vGameXY = aGameXY;
            gl_Position = uMvp * vec4(aPosition, 1.0);
        }
    )";

    static const char* fs = R"(
        #version 300 es
        precision highp float;

        in highp vec3 vWorldPos;
        in highp vec2 vGameXY;

        uniform mat4 uMvp;
        uniform sampler2D uNoise;
        uniform sampler2D uSceneColor;
        uniform sampler2D uSceneDepth;
        uniform sampler2D uReflectionMap;
        uniform float uReflectionReady;
        uniform mat4 uReflectionMvp;
        uniform float uReflectivity;
        uniform float uReflectionHdr;
        uniform float uSceneSnapshotReady;
        uniform vec2 uViewport;
        uniform vec3 uEyePosition;
        uniform vec3 uSunDirection;
        uniform vec3 uSunColor;
        uniform vec3 uSceneFogColor;
        uniform vec2 uSceneFogNearFar;
        uniform float uSceneFogPower;
        uniform vec2 uClipNearFar;
        uniform float uUnitsPerMetre;
        uniform float uTime;

        uniform vec3 uShallow;
        uniform vec3 uDeep;
        uniform vec3 uReflection;
        uniform float uNormalUvScale;
        uniform float uFresnel;
        uniform float uShininess;
        uniform vec2 uDepthFalloff;
        uniform vec2 uWaterFogNearFar;
        uniform float uWaterFogAmount;
        uniform float uDistortion;
        uniform float uNoiseScale;
        uniform vec3 uLayerUvScale;
        uniform vec3 uLayerWindDir;
        uniform vec3 uLayerWindSpeed;
        uniform vec3 uLayerAmp;

        out vec4 fragColor;

        const float PI = 3.14159265358979323846;

        vec3 SceneToGame(vec3 v) {
            // Fallout game axes -> Quest scene axes are (x,z,-y).
            return vec3(v.x, -v.z, v.y);
        }

        vec2 LayerScroll(float degrees, float speed) {
            float radians = degrees * (PI / 180.0);
            return fract(vec2(sin(radians), cos(radians)) * (speed * uTime));
        }

        float NoiseHeight(vec2 tileUv) {
            // Recovered ISNOISESCROLLANDBLEND.pso:
            // layer 0 samples B, layer 1 G, layer 2 R; each unpacks to
            // [-1,1], receives its authored amplitude, then the sum is packed.
            vec3 scale = max(vec3(1.0), ceil(uLayerUvScale * 0.01));
            vec2 uv0 = tileUv * scale.x +
                LayerScroll(uLayerWindDir.x, uLayerWindSpeed.x);
            vec2 uv1 = tileUv * scale.y +
                LayerScroll(uLayerWindDir.y, uLayerWindSpeed.y);
            vec2 uv2 = tileUv * scale.z +
                LayerScroll(uLayerWindDir.z, uLayerWindSpeed.z);
            float l0 = textureLod(uNoise, uv0, 0.0).b * 2.0 - 1.0;
            float l1 = textureLod(uNoise, uv1, 0.0).g * 2.0 - 1.0;
            float l2 = textureLod(uNoise, uv2, 0.0).r * 2.0 - 1.0;
            return (dot(vec3(l0, l1, l2), uLayerAmp) * 0.5) + 0.5;
        }

        vec3 RecoveredNoiseNormal(vec2 tileUv) {
            // Recovered ISNOISENORMALMAP.pso: literal one-texel 256x256
            // Sobel stencil, multiplied by DNAM NoiseScale, z=1, normalize.
            const float d = 1.0 / 256.0;
            float w  = abs(NoiseHeight(tileUv + vec2(-d,  0.0)));
            float e  = abs(NoiseHeight(tileUv + vec2( d,  0.0)));
            float n  = abs(NoiseHeight(tileUv + vec2(0.0,  d)));
            float s  = abs(NoiseHeight(tileUv + vec2(0.0, -d)));
            float nw = abs(NoiseHeight(tileUv + vec2(-d,  d)));
            float ne = abs(NoiseHeight(tileUv + vec2( d,  d)));
            float sw = abs(NoiseHeight(tileUv + vec2(-d, -d)));
            float se = abs(NoiseHeight(tileUv + vec2( d, -d)));
            return normalize(vec3(
                (2.0*w + nw + sw - 2.0*e - ne - se) * uNoiseScale,
                (2.0*n + nw + ne - 2.0*s - sw - se) * uNoiseScale,
                1.0));
        }

        float LinearDepth(float depth01) {
            float n = uClipNearFar.x;
            float f = uClipNearFar.y;
            return (n * f) / max(f - depth01 * (f - n), 1.0e-6);
        }

        float SceneFogAmount(float distanceMetres) {
            if (uSceneFogNearFar.y <= uSceneFogNearFar.x + 0.001) return 0.0;
            float q = clamp(
                (distanceMetres - uSceneFogNearFar.x) /
                (uSceneFogNearFar.y - uSceneFogNearFar.x), 0.0, 1.0);
            return pow(q, max(uSceneFogPower, 0.01));
        }

        void main() {
            vec3 eyeScene = uEyePosition - vWorldPos;
            float eyeDistanceMetres = length(eyeScene);
            vec3 Vscene = normalize(eyeScene);
            vec3 Vgame = normalize(SceneToGame(eyeScene));
            vec3 sunGame = normalize(SceneToGame(uSunDirection));

            float horizontalDistanceGame =
                length(vec2(eyeScene.x, eyeScene.z)) * uUnitsPerMetre;
            float noiseFade =
                clamp((8192.0 - horizontalDistanceGame) / 4096.0, 0.0, 1.0);

            vec2 screenUv = gl_FragCoord.xy / max(uViewport, vec2(1.0));
            bool haveScene = uSceneSnapshotReady > 0.5;

            float depthT = 1.0;
            vec2 rawDepth = vec2(1.0);
            vec2 correctedDepth = vec2(1.0);
            vec3 scenePoint = vWorldPos;

            if (haveScene) {
                float sceneDepth01 = texture(uSceneDepth, screenUv).r;
                float sceneDistance = LinearDepth(sceneDepth01);
                float waterDistance = LinearDepth(gl_FragCoord.z);
                bool validDepth =
                    sceneDepth01 > 0.0 && sceneDepth01 <= 1.0 &&
                    sceneDistance > waterDistance + 1.0e-5 &&
                    waterDistance > 1.0e-5;

                if (validDepth) {
                    float rayScale = sceneDistance / waterDistance;
                    scenePoint = uEyePosition +
                        (vWorldPos - uEyePosition) * rayScale;
                    float fogFarGame = max(uWaterFogNearFar.y, 1.0);
                    float slantGame =
                        length(scenePoint - vWorldPos) * uUnitsPerMetre;
                    float verticalGame =
                        max(vWorldPos.y - scenePoint.y, 0.0) * uUnitsPerMetre;
                    rawDepth = vec2(slantGame, verticalGame) / fogFarGame;
                    correctedDepth = clamp(
                        mix(vec2(1.0), rawDepth, noiseFade), 0.0, 1.0);
                    float span = uDepthFalloff.y - uDepthFalloff.x;
                    if (span > 1.0e-6) {
                        depthT = clamp(
                            (rawDepth.y - uDepthFalloff.x) / span,
                            0.0, 1.0);
                    }
                } else {
                    haveScene = false;
                }
            }

            // WATER001/WATER000 vertex shader supplies worldXY / TexScale.
            vec2 waterUv = vGameXY / max(uNormalUvScale, 1.0);
            vec3 preNormalGame = RecoveredNoiseNormal(waterUv);

            // WATER001 pixel path: sampled normal * shoreline depth, add
            // (0,0,1), distance-fade XY, normalize.
            vec3 normalGame = preNormalGame * depthT;
            normalGame.z += 1.0;
            normalGame.xy *= noiseFade;
            normalGame = normalize(normalGame);

            vec3 normalScene = normalize(vec3(
                normalGame.x, normalGame.z, -normalGame.y));

            vec3 body = mix(uShallow, uDeep, correctedDepth.y);
            vec3 bodyLightDir =
                normalize(vec3(sunGame.x, 4.0 * sunGame.y, sunGame.z));
            body *= clamp(dot(normalGame, bodyLightDir), 0.0, 1.0);

            float ndotv = clamp(dot(normalGame, Vgame), 0.0, 1.0);
            float oneMinus = 1.0 - ndotv;
            float fresnel5 = oneMinus * oneMinus;
            fresnel5 *= fresnel5 * oneMinus;
            float fresnel = clamp(
                uFresnel + (1.0 - uFresnel) * fresnel5, 0.0, 1.0);
            float fresneled = clamp(fresnel * correctedDepth.x, 0.0, 1.0);

            vec3 reflectedGame = reflect(-Vgame, normalGame);
            float sunSpec = pow(
                clamp(dot(reflectedGame, sunGame), 0.0, 1.0),
                max(uShininess, 1.0));
            float skyGlint = pow(
                clamp(dot(normalGame.xz,
                          normalize(vec2(-0.57, 0.82))), 0.0, 1.0),
                100.0);
            vec3 specular = (sunSpec + skyGlint) * uSunColor;

            if (!haveScene) {
                // Exact WATER003-style source-backed fallback when the Quest
                // opaque-scene snapshot is unavailable: no invented refraction.
                vec3 fallback = mix(body, uReflection, fresneled) + specular;
                float sceneFog = SceneFogAmount(eyeDistanceMetres);
                fallback = mix(fallback, uSceneFogColor, sceneFog);
                fragColor = vec4(max(fallback, vec3(0.0)), 1.0);
                return;
            }

            // WATER001 recovered refraction displacement. DistortionAmount and
            // reconstructed vertical column are in Fallout game-unit domain.
            float distanceRamp =
                clamp(horizontalDistanceGame / 5000.0, 0.0, 1.0);
            float distortionScale = mix(4.0, uDistortion, distanceRamp);
            vec2 deltaGame =
                rawDepth.y * depthT * distortionScale * normalGame.xy;
            vec3 displacedScene = vWorldPos + vec3(
                deltaGame.x / uUnitsPerMetre,
                0.0,
                -deltaGame.y / uUnitsPerMetre);

            vec4 displacedClip = uMvp * vec4(displacedScene, 1.0);
            vec2 refractionUv = screenUv;
            if (displacedClip.w > 1.0e-5) {
                vec2 displacedNdc = displacedClip.xy / displacedClip.w;
                vec2 candidate = displacedNdc * 0.5 + 0.5;
                if (all(greaterThanEqual(candidate, vec2(0.0))) &&
                    all(lessThanEqual(candidate, vec2(1.0)))) {
                    // The Quest snapshot contains the whole opaque scene, while
                    // retail's RefractionMap was selectively populated. Reject
                    // a displaced tap if it lands on foreground geometry.
                    float tapDepth = texture(uSceneDepth, candidate).r;
                    float tapDistance = LinearDepth(tapDepth);
                    float displacedWaterDistance =
                        LinearDepth(clamp(
                            displacedClip.z / displacedClip.w * 0.5 + 0.5,
                            0.0, 1.0));
                    if (tapDistance > displacedWaterDistance) {
                        refractionUv = candidate;
                    }
                }
            }

            vec3 refraction = texture(uSceneColor, refractionUv).rgb;

            // Retail WATER001 de-fogs the RefractionMap, performs the water
            // optical composite, then reapplies ordinary scene fog.
            float refractedDistance = length(scenePoint - uEyePosition);
            float displacedFog = SceneFogAmount(refractedDistance);
            vec3 refracted = (refraction -
                displacedFog * uSceneFogColor) /
                max(1.0 - displacedFog, 1.0e-4);

            float waterFogRange =
                max(uWaterFogNearFar.y - uWaterFogNearFar.x, 1.0e-3);
            float aboveWaterFog =
                (1.0 - clamp(
                    uWaterFogNearFar.y * (1.0 - correctedDepth.x) /
                    waterFogRange, 0.0, 1.0)) *
                clamp(uWaterFogAmount, 0.0, 1.0);

            vec3 transmitted = mix(
                refracted, body,
                clamp(depthT * aboveWaterFog, 0.0, 1.0));

            // SP17 WATER000: project the same normal-displaced surface point
            // into the planar ReflectionMap. VarAmounts.y is WATR
            // ReflectivityAmount and is a LERP weight from the authored
            // ReflectionColor toward the RT; FresnelRI.w is the engine-scaled
            // ReflectionHDRMult lane.
            vec3 reflectionTerm = uReflection;
            if (uReflectionReady > 0.5) {
                vec4 reflectionClip =
                    uReflectionMvp * vec4(displacedScene, 1.0);
                if (reflectionClip.w > 1.0e-5) {
                    vec2 reflectionUv =
                        reflectionClip.xy / reflectionClip.w * 0.5 + 0.5;
                    if (all(greaterThanEqual(reflectionUv, vec2(0.0))) &&
                        all(lessThanEqual(reflectionUv, vec2(1.0)))) {
                        vec3 reflectionRt =
                            texture(uReflectionMap, reflectionUv).rgb;
                        reflectionTerm = mix(
                            uReflection, reflectionRt,
                            clamp(uReflectivity, 0.0, 1.0));
                    }
                }
            }
            reflectionTerm *= max(uReflectionHdr, 1.0);

            vec3 bodyReflection =
                mix(body, reflectionTerm, fresneled);
            vec3 color =
                mix(transmitted, bodyReflection, correctedDepth.y);
            color += specular;

            float finalFog = SceneFogAmount(eyeDistanceMetres);
            color = mix(color, uSceneFogColor, finalFog);
            fragColor = vec4(max(color, vec3(0.0)), 1.0);
        }
    )";

    GLuint v = CompileShader(GL_VERTEX_SHADER, vs);
    GLuint f = CompileShader(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) {
        if (v) glDeleteShader(v);
        if (f) glDeleteShader(f);
        return false;
    }

    gProgram = glCreateProgram();
    glAttachShader(gProgram, v);
    glAttachShader(gProgram, f);
    glLinkProgram(gProgram);
    glDeleteShader(v);
    glDeleteShader(f);

    GLint linked = GL_FALSE;
    glGetProgramiv(gProgram, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char log[1024]{};
        glGetProgramInfoLog(gProgram, sizeof(log), nullptr, log);
        WLOGE("Q20.8A WATER program link failed: %s", log);
        glDeleteProgram(gProgram);
        gProgram = 0u;
        return false;
    }

    gMvpLocation = glGetUniformLocation(gProgram, "uMvp");
    gNoiseLocation = glGetUniformLocation(gProgram, "uNoise");
    gSceneColorLocation = glGetUniformLocation(gProgram, "uSceneColor");
    gSceneDepthLocation = glGetUniformLocation(gProgram, "uSceneDepth");
    gReflectionMapLocation = glGetUniformLocation(gProgram, "uReflectionMap");
    gReflectionReadyLocation = glGetUniformLocation(gProgram, "uReflectionReady");
    gReflectionMvpLocation = glGetUniformLocation(gProgram, "uReflectionMvp");
    gReflectivityLocation = glGetUniformLocation(gProgram, "uReflectivity");
    gReflectionHdrLocation = glGetUniformLocation(gProgram, "uReflectionHdr");
    gSceneSnapshotReadyLocation = glGetUniformLocation(gProgram, "uSceneSnapshotReady");
    gViewportLocation = glGetUniformLocation(gProgram, "uViewport");
    gEyePositionLocation = glGetUniformLocation(gProgram, "uEyePosition");
    gSunDirectionLocation = glGetUniformLocation(gProgram, "uSunDirection");
    gSunColorLocation = glGetUniformLocation(gProgram, "uSunColor");
    gSceneFogColorLocation = glGetUniformLocation(gProgram, "uSceneFogColor");
    gSceneFogNearFarLocation = glGetUniformLocation(gProgram, "uSceneFogNearFar");
    gSceneFogPowerLocation = glGetUniformLocation(gProgram, "uSceneFogPower");
    gClipNearFarLocation = glGetUniformLocation(gProgram, "uClipNearFar");
    gUnitsPerMetreLocation = glGetUniformLocation(gProgram, "uUnitsPerMetre");
    gTimeLocation = glGetUniformLocation(gProgram, "uTime");
    gShallowLocation = glGetUniformLocation(gProgram, "uShallow");
    gDeepLocation = glGetUniformLocation(gProgram, "uDeep");
    gReflectionLocation = glGetUniformLocation(gProgram, "uReflection");
    gNormalUvScaleLocation = glGetUniformLocation(gProgram, "uNormalUvScale");
    gFresnelLocation = glGetUniformLocation(gProgram, "uFresnel");
    gShininessLocation = glGetUniformLocation(gProgram, "uShininess");
    gDepthFalloffLocation = glGetUniformLocation(gProgram, "uDepthFalloff");
    gWaterFogNearFarLocation = glGetUniformLocation(gProgram, "uWaterFogNearFar");
    gWaterFogAmountLocation = glGetUniformLocation(gProgram, "uWaterFogAmount");
    gDistortionLocation = glGetUniformLocation(gProgram, "uDistortion");
    gNoiseScaleLocation = glGetUniformLocation(gProgram, "uNoiseScale");
    gLayerUvScaleLocation = glGetUniformLocation(gProgram, "uLayerUvScale");
    gLayerWindDirLocation = glGetUniformLocation(gProgram, "uLayerWindDir");
    gLayerWindSpeedLocation = glGetUniformLocation(gProgram, "uLayerWindSpeed");
    gLayerAmpLocation = glGetUniformLocation(gProgram, "uLayerAmp");

    glGenVertexArrays(1, &gVao);
    glGenBuffers(1, &gVbo);
    glBindVertexArray(gVao);
    glBindBuffer(GL_ARRAY_BUFFER, gVbo);
    glBufferData(GL_ARRAY_BUFFER, 30u * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                          5 * sizeof(float),
                          reinterpret_cast<const void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    WLOGI("Q20.9 WATER PROGRAM READY: pixelPath=SP17-WATER000 samplers=ReflectionMap+RefractionMap+NoiseMap+DepthMap reflection=PLANAR_SCENE_RT noisePrepass=INLINE-ISNOISESCROLLANDBLEND+ISNOISENORMALMAP displacement=deferred-WATER017");
    return gMvpLocation >= 0 && gNoiseLocation >= 0 &&
           gSceneSnapshotReadyLocation >= 0;
}

} // namespace

bool LoadFo3WaterSceneQ2070(uint32_t worldspaceFormId,
                            float arrivalX, float arrivalY) {
    gWaterCells.clear();
    gWorldspace = worldspaceFormId;
    gRenderLogged = false;
    if (worldspaceFormId == 0u) return false;

    uint32_t defaultWaterType = 0u;
    std::string worldNoise;
    ReadWorldDefaults(worldspaceFormId, defaultWaterType, worldNoise);

    std::vector<RawWaterCell> raw;
    if (!CollectWaterCells(worldspaceFormId, arrivalX, arrivalY, raw)) {
        WLOGE("Q20.7A WATER LOAD FAILED: worldspace=%08X reason=cell-scan",
              worldspaceFormId);
        return false;
    }

    std::unordered_set<uint32_t> wanted;
    for (const RawWaterCell& cell : raw) {
        const uint32_t type = cell.waterType ? cell.waterType : defaultWaterType;
        if (type != 0u) wanted.insert(type);
    }

    std::unordered_map<uint32_t, Fo3WaterTypeQ2070> types;
    if (!ResolveWaterTypes(wanted, types)) {
        WLOGE("Q20.7A WATER LOAD FAILED: worldspace=%08X reason=watr-scan",
              worldspaceFormId);
        return false;
    }

    for (const RawWaterCell& rawCell : raw) {
        Fo3WaterCellQ2070 cell;
        cell.cellFormId = rawCell.formId;
        cell.gridX = rawCell.gridX;
        cell.gridY = rawCell.gridY;
        cell.waterHeightGame = rawCell.waterHeight;
        cell.waterTypeFormId =
            rawCell.waterType ? rawCell.waterType : defaultWaterType;

        const auto it = types.find(cell.waterTypeFormId);
        if (it != types.end()) cell.type = it->second;
        if (!rawCell.noiseTexture.empty()) {
            cell.noiseTexturePath = rawCell.noiseTexture;
        } else if (!cell.type.noiseTexturePath.empty()) {
            cell.noiseTexturePath = cell.type.noiseTexturePath;
        } else {
            cell.noiseTexturePath = worldNoise;
        }

        WLOGI("Q20.7A WATER CELL: worldspace=%08X cell=%08X grid=(%d,%d) heightGame=%.3f type=%08X EDID=%s noise=%s",
              worldspaceFormId, cell.cellFormId, cell.gridX, cell.gridY,
              cell.waterHeightGame, cell.waterTypeFormId,
              cell.type.editorId.empty() ? "<none>" : cell.type.editorId.c_str(),
              cell.noiseTexturePath.empty() ? "<none>" : cell.noiseTexturePath.c_str());
        if (cell.type.valid) {
            WLOGI("Q20.7A WATER WATR: type=%08X shallowRGB=(%.6f %.6f %.6f) deepRGB=(%.6f %.6f %.6f) reflectionRGB=(%.6f %.6f %.6f) sunPower=%.3f reflectivity=%.3f fresnel=%.3f aboveFogNear=%.3f aboveFogFar=%.3f fogAmount=%.3f depthFalloff=(%.6f %.6f) distortion=%.3f shininess=%.3f reflectionHDR=%.3f",
                  cell.type.formId,
                  cell.type.shallowColor[0], cell.type.shallowColor[1], cell.type.shallowColor[2],
                  cell.type.deepColor[0], cell.type.deepColor[1], cell.type.deepColor[2],
                  cell.type.reflectionColor[0], cell.type.reflectionColor[1], cell.type.reflectionColor[2],
                  cell.type.sunPower, cell.type.reflectivity, cell.type.fresnelAmount,
                  cell.type.aboveFogNear, cell.type.aboveFogFar,
                  cell.type.aboveFogAmount,
                  cell.type.depthFalloffStart, cell.type.depthFalloffEnd,
                  cell.type.distortionAmount, cell.type.shininess,
                  cell.type.reflectionHdrMultiplier);
            WLOGI("Q20.8A WATER NOISE DATA: type=%08X noiseScale=%.6f normalUvScale=%.6f uvScale=(%.6f %.6f %.6f) windDirDeg=(%.6f %.6f %.6f) windSpeed=(%.6f %.6f %.6f) amplitude=(%.6f %.6f %.6f) source=WATR-DNAM",
                  cell.type.formId,
                  cell.type.noiseScale, cell.type.normalUvScale,
                  cell.type.uvScale[0], cell.type.uvScale[1], cell.type.uvScale[2],
                  cell.type.windDirection[0], cell.type.windDirection[1], cell.type.windDirection[2],
                  cell.type.windSpeed[0], cell.type.windSpeed[1], cell.type.windSpeed[2],
                  cell.type.amplitudeScale[0], cell.type.amplitudeScale[1], cell.type.amplitudeScale[2]);
        }
        gWaterCells.push_back(std::move(cell));
    }

    WLOGI("Q20.7A WATER DATA READY: worldspace=%08X targetGrid=(%d,%d) radius=%d cells=%zu defaultType=%08X worldNoise=%s source=Fallout3.esm XCLW+XCWT+WATR",
          worldspaceFormId,
          static_cast<int>(std::floor(arrivalX / CELL_SIZE)),
          static_cast<int>(std::floor(arrivalY / CELL_SIZE)),
          WATER_RADIUS_CELLS, gWaterCells.size(), defaultWaterType,
          worldNoise.empty() ? "<none>" : worldNoise.c_str());
    return !gWaterCells.empty();
}

void ClearFo3WaterSceneQ2070() {
    gWaterCells.clear();
    gWorldspace = 0u;
    gRenderLogged = false;
}

const std::vector<Fo3WaterCellQ2070>& GetFo3WaterCellsQ2070() {
    return gWaterCells;
}

void RenderFo3WaterSurfaceQ2070(const float* mvp16,
                                float originGameX,
                                float originGameY,
                                float originGameZ,
                                float floorY,
                                float sceneForward,
                                float unitsPerMetre,
                                uint32_t sceneColorTexture,
                                uint32_t sceneDepthTexture,
                                int sceneWidth,
                                int sceneHeight,
                                bool sceneSnapshotReady,
                                const float eyePosition[3],
                                const float sunDirection[3],
                                const float sunColor[3],
                                const float sceneFogColor[3],
                                float sceneFogNearMetres,
                                float sceneFogFarMetres,
                                float sceneFogPower,
                                float nearClipMetres,
                                float farClipMetres) {
    if (!mvp16 || gWaterCells.empty() || unitsPerMetre <= 0.0f) return;
    if (!EnsureRenderer()) return;

    const Fo3WaterCellQ2070& firstCell = gWaterCells.front();
    if (!EnsureNoiseTexture(firstCell.noiseTexturePath)) return;

    GLint previousProgram = 0;
    GLint previousVao = 0;
    GLint previousArrayBuffer = 0;
    GLint previousActiveTexture = GL_TEXTURE0;
    GLint previousTexture0 = 0;
    GLint previousTexture1 = 0;
    GLint previousTexture2 = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture0);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture1);
    glActiveTexture(GL_TEXTURE2);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture2);

    const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean previousDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
    GLint previousDepthFunc = GL_LESS;
    glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);

    const float elapsedSeconds = static_cast<float>(
        std::chrono::duration<double>(
            std::chrono::steady_clock::now() - gWaterTimeOrigin).count());

    glUseProgram(gProgram);
    glUniformMatrix4fv(gMvpLocation, 1, GL_FALSE, mvp16);
    glUniform1i(gNoiseLocation, 0);
    glUniform1i(gSceneColorLocation, 1);
    glUniform1i(gSceneDepthLocation, 2);
    glUniform1f(gSceneSnapshotReadyLocation,
                sceneSnapshotReady && sceneColorTexture && sceneDepthTexture ? 1.0f : 0.0f);
    glUniform2f(gViewportLocation,
                static_cast<float>(std::max(sceneWidth, 1)),
                static_cast<float>(std::max(sceneHeight, 1)));
    if (eyePosition) glUniform3fv(gEyePositionLocation, 1, eyePosition);
    else glUniform3f(gEyePositionLocation, 0.0f, 0.0f, 0.0f);
    if (sunDirection) glUniform3fv(gSunDirectionLocation, 1, sunDirection);
    else glUniform3f(gSunDirectionLocation, 0.35f, 0.85f, 0.40f);
    if (sunColor) glUniform3fv(gSunColorLocation, 1, sunColor);
    else glUniform3f(gSunColorLocation, 1.0f, 1.0f, 1.0f);
    if (sceneFogColor) glUniform3fv(gSceneFogColorLocation, 1, sceneFogColor);
    else glUniform3f(gSceneFogColorLocation, 0.0f, 0.0f, 0.0f);
    glUniform2f(gSceneFogNearFarLocation,
                sceneFogNearMetres, sceneFogFarMetres);
    glUniform1f(gSceneFogPowerLocation, sceneFogPower);
    glUniform2f(gClipNearFarLocation, nearClipMetres, farClipMetres);
    glUniform1f(gUnitsPerMetreLocation, unitsPerMetre);
    glUniform1f(gTimeLocation, elapsedSeconds);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gNoiseTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,
                  sceneSnapshotReady ? static_cast<GLuint>(sceneColorTexture) : 0u);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D,
                  sceneSnapshotReady ? static_cast<GLuint>(sceneDepthTexture) : 0u);

    glBindVertexArray(gVao);
    glBindBuffer(GL_ARRAY_BUFFER, gVbo);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    // WATER001 is an opaque optical composite: the sampled RefractionMap is
    // already present in RGB. Do not alpha-blend it over the scene a second time.
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);

    size_t draws = 0u;
    for (const Fo3WaterCellQ2070& cell : gWaterCells) {
        const float gameMinX = static_cast<float>(cell.gridX) * CELL_SIZE;
        const float gameMaxX = gameMinX + CELL_SIZE;
        const float gameMinY = static_cast<float>(cell.gridY) * CELL_SIZE;
        const float gameMaxY = gameMinY + CELL_SIZE;
        const float x0 = (gameMinX - originGameX) / unitsPerMetre;
        const float x1 = (gameMaxX - originGameX) / unitsPerMetre;
        const float z0 = sceneForward - (gameMinY - originGameY) / unitsPerMetre;
        const float z1 = sceneForward - (gameMaxY - originGameY) / unitsPerMetre;
        const float y = floorY +
            (cell.waterHeightGame - originGameZ) / unitsPerMetre;

        // position.xyz + exact Fallout game-space XY for WATER vso TexScale UV.
        const float vertices[30] = {
            x0,y,z0, gameMinX,gameMinY,
            x1,y,z0, gameMaxX,gameMinY,
            x1,y,z1, gameMaxX,gameMaxY,
            x0,y,z0, gameMinX,gameMinY,
            x1,y,z1, gameMaxX,gameMaxY,
            x0,y,z1, gameMinX,gameMaxY
        };
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

        const Fo3WaterTypeQ2070& w = cell.type;
        glUniform3fv(gShallowLocation, 1, w.shallowColor);
        glUniform3fv(gDeepLocation, 1, w.deepColor);
        glUniform3fv(gReflectionLocation, 1, w.reflectionColor);
        glUniform1f(gNormalUvScaleLocation,
                    std::max(w.normalUvScale, 1.0f));
        glUniform1f(gFresnelLocation,
                    std::clamp(w.fresnelAmount, 0.0f, 1.0f));
        glUniform1f(gShininessLocation,
                    std::max(w.shininess, 1.0f));
        glUniform2f(gDepthFalloffLocation,
                    w.depthFalloffStart, w.depthFalloffEnd);
        glUniform2f(gWaterFogNearFarLocation,
                    w.aboveFogNear, w.aboveFogFar);
        glUniform1f(gWaterFogAmountLocation,
                    std::clamp(w.aboveFogAmount, 0.0f, 1.0f));
        glUniform1f(gDistortionLocation,
                    std::max(w.distortionAmount, 0.0f));
        glUniform1f(gNoiseScaleLocation,
                    std::max(w.noiseScale, 0.0f));
        glUniform3fv(gLayerUvScaleLocation, 1, w.uvScale);
        glUniform3fv(gLayerWindDirLocation, 1, w.windDirection);
        glUniform3fv(gLayerWindSpeedLocation, 1, w.windSpeed);
        glUniform3fv(gLayerAmpLocation, 1, w.amplitudeScale);

        // If a later streamed CELL resolves a different NNAM, switch its real
        // authored texture before drawing that CELL.
        if (cell.noiseTexturePath != gNoiseTexturePath) {
            EnsureNoiseTexture(cell.noiseTexturePath);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, gNoiseTexture);
        }

        glDrawArrays(GL_TRIANGLES, 0, 6);
        ++draws;
    }

    if (!gRenderLogged) {
        gRenderLogged = true;
        WLOGI("Q20.8A WATER001 DRAW: worldspace=%08X cells=%zu draws=%zu snapshotReady=%d scene=%dx%d noiseReal=%d depthTest=LEQUAL depthWrite=0 blend=OPAQUE_RGB refraction=OPAQUE_SCENE_RESOLVE depth=SCENE_DEPTH_RECONSTRUCTION fresnel=SP17 shallowDeep=SP17 waterFog=SP17 reflection=AUTHORED_CONSTANT displacement=DEFERRED_WATER017 vrAdaptation=legacy-RefractionMap+DepthMap-from-resolved-Quest-scene",
              gWorldspace, gWaterCells.size(), draws,
              sceneSnapshotReady ? 1 : 0, sceneWidth, sceneHeight,
              gNoiseTextureReal ? 1 : 0);
    }

    glDepthFunc(static_cast<GLenum>(previousDepthFunc));
    glDepthMask(previousDepthMask);
    if (depthWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (cullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture2));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture1));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture0));
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));

    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previousArrayBuffer));
    glBindVertexArray(static_cast<GLuint>(previousVao));
    glUseProgram(static_cast<GLuint>(previousProgram));
}

void ShutdownFo3WaterQ2070() {
    if (gNoiseTexture) glDeleteTextures(1, &gNoiseTexture);
    if (gVbo) glDeleteBuffers(1, &gVbo);
    if (gVao) glDeleteVertexArrays(1, &gVao);
    if (gProgram) glDeleteProgram(gProgram);
    gNoiseTexture = gVbo = gVao = gProgram = 0u;
    gNoiseTexturePath.clear();
    gNoiseTextureReal = false;
    gMvpLocation = -1;
    gRenderLogged = false;
    gNoiseLogged = false;
    gWaterCells.clear();
    gWorldspace = 0u;
}
