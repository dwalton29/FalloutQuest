#include "fo3-water-q2070.h"

#include <GLES3/gl3.h>
#include <android/log.h>
#include <zlib.h>

#include <algorithm>
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
GLint gMvpLocation = -1;
GLint gColorLocation = -1;
bool gRenderLogged = false;

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
                    cell.hasWaterHeight =
                        std::isfinite(cell.waterHeight) &&
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

bool EnsureRenderer() {
    if (gProgram && gVao && gVbo) return true;

    static const char* vs = R"(
        #version 300 es
        layout(location=0) in vec3 aPosition;
        uniform mat4 uMvp;
        void main() {
            gl_Position = uMvp * vec4(aPosition, 1.0);
        }
    )";
    static const char* fs = R"(
        #version 300 es
        precision mediump float;
        uniform vec3 uColor;
        out vec4 fragColor;
        void main() {
            // Q20.7A is placement/data proof only. Use the authored WATR
            // shallow colour without inventing Fallout's WATER017 blending.
            fragColor = vec4(uColor, 1.0);
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
        WLOGE("Q20.7A WATER program link failed: %s", log);
        glDeleteProgram(gProgram);
        gProgram = 0u;
        return false;
    }

    gMvpLocation = glGetUniformLocation(gProgram, "uMvp");
    gColorLocation = glGetUniformLocation(gProgram, "uColor");
    glGenVertexArrays(1, &gVao);
    glGenBuffers(1, &gVbo);
    glBindVertexArray(gVao);
    glBindBuffer(GL_ARRAY_BUFFER, gVbo);
    glBufferData(GL_ARRAY_BUFFER, 18u * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    return gMvpLocation >= 0 && gColorLocation >= 0;
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
                                float unitsPerMetre) {
    if (!mvp16 || gWaterCells.empty() || unitsPerMetre <= 0.0f) return;
    if (!EnsureRenderer()) return;

    GLint previousProgram = 0;
    GLint previousVao = 0;
    GLint previousArrayBuffer = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);
    const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean previousDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
    GLint previousDepthFunc = GL_LESS;
    glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);

    glUseProgram(gProgram);
    glUniformMatrix4fv(gMvpLocation, 1, GL_FALSE, mvp16);
    glBindVertexArray(gVao);
    glBindBuffer(GL_ARRAY_BUFFER, gVbo);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);

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

        const float vertices[18] = {
            x0, y, z0,  x1, y, z0,  x1, y, z1,
            x0, y, z0,  x1, y, z1,  x0, y, z1
        };
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

        const float* color = cell.type.valid
            ? cell.type.shallowColor
            : Fo3WaterTypeQ2070{}.shallowColor;
        glUniform3f(gColorLocation,
                    cell.type.valid ? color[0] : 0.16f,
                    cell.type.valid ? color[1] : 0.22f,
                    cell.type.valid ? color[2] : 0.14f);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    if (!gRenderLogged) {
        gRenderLogged = true;
        WLOGI("Q20.7A WATER SURFACE DRAW: worldspace=%08X cells=%zu mode=ESM-XCLW-plane diagnosticColour=WATR-shallow depthTest=LEQUAL depthWrite=0 reflection=DEFERRED refraction=DEFERRED depthColour=DEFERRED displacement=DEFERRED pcTargetShader=WATER-family",
              gWorldspace, gWaterCells.size());
    }

    glDepthFunc(static_cast<GLenum>(previousDepthFunc));
    glDepthMask(previousDepthMask);
    if (depthWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (cullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previousArrayBuffer));
    glBindVertexArray(static_cast<GLuint>(previousVao));
    glUseProgram(static_cast<GLuint>(previousProgram));
}

void ShutdownFo3WaterQ2070() {
    if (gVbo) glDeleteBuffers(1, &gVbo);
    if (gVao) glDeleteVertexArrays(1, &gVao);
    if (gProgram) glDeleteProgram(gProgram);
    gVbo = gVao = gProgram = 0u;
    gMvpLocation = gColorLocation = -1;
    gRenderLogged = false;
    gWaterCells.clear();
    gWorldspace = 0u;
}
