#include "fo3-terrain-q76.h"

#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <zlib.h>

namespace {

constexpr const char* TAG_Q76 = "FalloutQuest";
constexpr const char* ESM_PATH_Q76 =
        "/data/user/0/com.falloutquest.app/files/Fallout3/Data/Fallout3.esm";
constexpr uint32_t FLAG_COMPRESSED_Q76 = 0x00040000u;
constexpr uint64_t HEADER_SIZE_Q76 = 24u;
constexpr uint32_t MAX_RECORD_BYTES_Q76 = 64u * 1024u * 1024u;
constexpr int LAND_SIDE_Q76 = 33;
constexpr float CELL_SIZE_Q76 = 4096.0f;
constexpr float VERTEX_STEP_Q76 = CELL_SIZE_Q76 / 32.0f;
constexpr float HEIGHT_SCALE_Q76 = 8.0f;

#define Q76_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG_Q76, __VA_ARGS__)
#define Q76_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG_Q76, __VA_ARGS__)
#define Q76_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG_Q76, __VA_ARGS__)

struct GroupFrameQ76 {
    uint64_t end = 0;
    uint32_t label = 0;
    uint32_t type = 0;
};

struct CellGridQ76 {
    int32_t x = 0;
    int32_t y = 0;
    bool valid = false;
};

struct LandCellQ76 {
    uint32_t cellFormId = 0;
    int32_t gridX = 0;
    int32_t gridY = 0;
    float heights[LAND_SIDE_Q76 * LAND_SIDE_Q76]{};
    uint8_t colors[LAND_SIDE_Q76 * LAND_SIDE_Q76 * 3]{};
    bool hasColors = false;
};

struct TerrainVertexQ76 {
    float px, py, pz;
    float nx, ny, nz;
    float r, g, b;
};

GLuint gTerrainProgramQ76 = 0;
GLuint gTerrainVaoQ76 = 0;
GLuint gTerrainVboQ76 = 0;
GLint gTerrainMvpQ76 = -1;
GLsizei gTerrainVertexCountQ76 = 0;
bool gTerrainReadyQ76 = false;

uint16_t ReadLe16Q76(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8);
}

uint32_t ReadLe32Q76(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

float ReadLeFloatQ76(const uint8_t* p) {
    const uint32_t bits = ReadLe32Q76(p);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool ReadExactQ76(FILE* file, void* dst, size_t size) {
    return std::fread(dst, 1, size, file) == size;
}

int64_t FileSizeQ76(FILE* file) {
    const off_t current = ftello(file);
    if (current < 0) return -1;
    if (fseeko(file, 0, SEEK_END) != 0) return -1;
    const off_t end = ftello(file);
    fseeko(file, current, SEEK_SET);
    return static_cast<int64_t>(end);
}

bool InflateRecordQ76(const std::vector<uint8_t>& stored, std::vector<uint8_t>& out) {
    if (stored.size() < 4u) return false;
    const uint32_t inflatedSize = ReadLe32Q76(stored.data());
    if (inflatedSize == 0u || inflatedSize > MAX_RECORD_BYTES_Q76) return false;
    out.resize(inflatedSize);
    uLongf destLen = static_cast<uLongf>(out.size());
    const int result = uncompress(reinterpret_cast<Bytef*>(out.data()), &destLen,
                                  reinterpret_cast<const Bytef*>(stored.data() + 4u),
                                  static_cast<uLong>(stored.size() - 4u));
    if (result != Z_OK || destLen != inflatedSize) {
        out.clear();
        return false;
    }
    return true;
}

bool ReadPayloadQ76(FILE* file, uint32_t storedSize, uint32_t flags,
                    std::vector<uint8_t>& out) {
    if (storedSize == 0u || storedSize > MAX_RECORD_BYTES_Q76) return false;
    std::vector<uint8_t> stored(storedSize);
    if (!ReadExactQ76(file, stored.data(), stored.size())) return false;
    if ((flags & FLAG_COMPRESSED_Q76) == 0u) {
        out.swap(stored);
        return true;
    }
    return InflateRecordQ76(stored, out);
}

void WalkSubrecordsQ76(const uint8_t* data, size_t size,
                       const std::function<void(const char*, const uint8_t*, uint32_t)>& visitor) {
    size_t pos = 0u;
    uint32_t extendedSize = 0u;
    while (pos + 6u <= size) {
        const char* type = reinterpret_cast<const char*>(data + pos);
        const uint16_t size16 = ReadLe16Q76(data + pos + 4u);
        pos += 6u;
        if (std::memcmp(type, "XXXX", 4u) == 0) {
            if (size16 != 4u || pos + 4u > size) return;
            extendedSize = ReadLe32Q76(data + pos);
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

bool InWorldspaceQ76(const std::vector<GroupFrameQ76>& groups, uint32_t worldspaceFormId) {
    for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
        if (it->type == 1u && it->label == worldspaceFormId) return true;
    }
    return false;
}

uint32_t OwningCellQ76(const std::vector<GroupFrameQ76>& groups) {
    for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
        if (it->type == 6u || it->type == 8u || it->type == 9u || it->type == 10u) {
            return it->label;
        }
    }
    return 0u;
}

bool ParseVhgtQ76(const uint8_t* bytes, uint32_t size, float* heights) {
    constexpr uint32_t expected = 4u + LAND_SIDE_Q76 * LAND_SIDE_Q76 + 3u;
    if (size < expected) return false;

    float rowStart = ReadLeFloatQ76(bytes);
    const int8_t* deltas = reinterpret_cast<const int8_t*>(bytes + 4u);
    for (int y = 0; y < LAND_SIDE_Q76; ++y) {
        rowStart += static_cast<float>(deltas[y * LAND_SIDE_Q76]);
        float value = rowStart;
        heights[y * LAND_SIDE_Q76] = value * HEIGHT_SCALE_Q76;
        for (int x = 1; x < LAND_SIDE_Q76; ++x) {
            value += static_cast<float>(deltas[y * LAND_SIDE_Q76 + x]);
            heights[y * LAND_SIDE_Q76 + x] = value * HEIGHT_SCALE_Q76;
        }
    }
    return true;
}

bool CollectLandQ76(uint32_t worldspaceFormId, std::vector<LandCellQ76>& out) {
    out.clear();
    FILE* file = std::fopen(ESM_PATH_Q76, "rb");
    if (!file) return false;
    const int64_t fileSize = FileSizeQ76(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE_Q76)) {
        std::fclose(file);
        return false;
    }

    std::unordered_map<uint32_t, CellGridQ76> cellGrids;
    std::vector<GroupFrameQ76> groups;
    size_t landSeen = 0u;
    size_t vhgtDecoded = 0u;

    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        while (!groups.empty() && offset >= groups.back().end) groups.pop_back();
        if (offset + HEADER_SIZE_Q76 > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[HEADER_SIZE_Q76]{};
        if (!ReadExactQ76(file, header, sizeof(header))) break;
        const uint32_t sizeField = ReadLe32Q76(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < HEADER_SIZE_Q76 || offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            groups.push_back(GroupFrameQ76{offset + sizeField,
                                           ReadLe32Q76(header + 8u),
                                           ReadLe32Q76(header + 12u)});
            continue;
        }

        const uint32_t flags = ReadLe32Q76(header + 8u);
        const uint32_t formId = ReadLe32Q76(header + 12u);
        const uint64_t payloadEnd = offset + HEADER_SIZE_Q76 + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;

        if (!InWorldspaceQ76(groups, worldspaceFormId)) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        const bool isCell = std::memcmp(header, "CELL", 4u) == 0;
        const bool isLand = std::memcmp(header, "LAND", 4u) == 0;
        if (!isCell && !isLand) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        std::vector<uint8_t> payload;
        if (!ReadPayloadQ76(file, sizeField, flags, payload)) break;

        if (isCell) {
            CellGridQ76 grid;
            WalkSubrecordsQ76(payload.data(), payload.size(),
                              [&](const char* type, const uint8_t* bytes, uint32_t size) {
                if (std::memcmp(type, "XCLC", 4u) == 0 && size >= 8u) {
                    grid.x = static_cast<int32_t>(ReadLe32Q76(bytes));
                    grid.y = static_cast<int32_t>(ReadLe32Q76(bytes + 4u));
                    grid.valid = true;
                }
            });
            if (grid.valid) cellGrids[formId] = grid;
            continue;
        }

        ++landSeen;
        const uint32_t owner = OwningCellQ76(groups);
        auto gridIt = cellGrids.find(owner);
        if (owner == 0u || gridIt == cellGrids.end() || !gridIt->second.valid) continue;

        LandCellQ76 land;
        land.cellFormId = owner;
        land.gridX = gridIt->second.x;
        land.gridY = gridIt->second.y;
        bool haveHeight = false;
        WalkSubrecordsQ76(payload.data(), payload.size(),
                          [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "VHGT", 4u) == 0) {
                haveHeight = ParseVhgtQ76(bytes, size, land.heights);
            } else if (std::memcmp(type, "VCLR", 4u) == 0 &&
                       size >= LAND_SIDE_Q76 * LAND_SIDE_Q76 * 3u) {
                std::memcpy(land.colors, bytes, LAND_SIDE_Q76 * LAND_SIDE_Q76 * 3u);
                land.hasColors = true;
            }
        });
        if (haveHeight) {
            ++vhgtDecoded;
            out.push_back(land);
        }
    }

    std::fclose(file);
    Q76_LOGI("Q7.6 LAND DECODE: worldspace=%08X LANDseen=%zu VHGT=%zu cellsWithGrid=%zu",
             worldspaceFormId, landSeen, vhgtDecoded, cellGrids.size());
    return !out.empty();
}

struct Vec3Q76 { float x, y, z; };

Vec3Q76 CrossQ76(const Vec3Q76& a, const Vec3Q76& b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

Vec3Q76 NormalizeQ76(Vec3Q76 v) {
    const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length < 1e-6f) return {0.0f, 1.0f, 0.0f};
    return {v.x / length, v.y / length, v.z / length};
}

Vec3Q76 ToVrQ76(float gameX, float gameY, float gameZ,
                float arrivalX, float arrivalY, float arrivalZ,
                float sceneForward, float floorY, float unitsPerMetre) {
    return {(gameX - arrivalX) / unitsPerMetre,
            floorY + (gameZ - arrivalZ) / unitsPerMetre,
            sceneForward - (gameY - arrivalY) / unitsPerMetre};
}

void VertexColorQ76(const LandCellQ76& land, int index, float& r, float& g, float& b) {
    if (land.hasColors) {
        r = static_cast<float>(land.colors[index * 3 + 0]) / 255.0f;
        g = static_cast<float>(land.colors[index * 3 + 1]) / 255.0f;
        b = static_cast<float>(land.colors[index * 3 + 2]) / 255.0f;
        // VCLR can be extremely dark in places. Keep enough ambient colour to
        // make the first terrain proof readable in an unlit VR scene.
        r = 0.18f + r * 0.62f;
        g = 0.17f + g * 0.62f;
        b = 0.14f + b * 0.62f;
    } else {
        r = 0.34f;
        g = 0.31f;
        b = 0.23f;
    }
}

void PushTriangleQ76(std::vector<TerrainVertexQ76>& vertices,
                     const Vec3Q76& a, const Vec3Q76& b, const Vec3Q76& c,
                     const float ca[3], const float cb[3], const float cc[3]) {
    const Vec3Q76 ab{b.x - a.x, b.y - a.y, b.z - a.z};
    const Vec3Q76 ac{c.x - a.x, c.y - a.y, c.z - a.z};
    Vec3Q76 n = NormalizeQ76(CrossQ76(ab, ac));
    if (n.y < 0.0f) n = {-n.x, -n.y, -n.z};
    vertices.push_back({a.x, a.y, a.z, n.x, n.y, n.z, ca[0], ca[1], ca[2]});
    vertices.push_back({b.x, b.y, b.z, n.x, n.y, n.z, cb[0], cb[1], cb[2]});
    vertices.push_back({c.x, c.y, c.z, n.x, n.y, n.z, cc[0], cc[1], cc[2]});
}

GLuint CompileTerrainShaderQ76(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[1024]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        Q76_LOGE("Q7.6 terrain shader compile failed: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint CreateTerrainProgramQ76() {
    static const char* vsSource = R"(
        #version 300 es
        layout(location=0) in vec3 aPosition;
        layout(location=1) in vec3 aNormal;
        layout(location=2) in vec3 aColor;
        uniform mat4 uMvp;
        out vec3 vNormal;
        out vec3 vColor;
        void main() {
            vNormal = aNormal;
            vColor = aColor;
            gl_Position = uMvp * vec4(aPosition, 1.0);
        }
    )";
    static const char* fsSource = R"(
        #version 300 es
        precision mediump float;
        in vec3 vNormal;
        in vec3 vColor;
        out vec4 fragColor;
        void main() {
            vec3 lightDir = normalize(vec3(0.35, 0.85, 0.40));
            float lambert = max(dot(normalize(vNormal), lightDir), 0.0);
            fragColor = vec4(vColor * (0.48 + 0.52 * lambert), 1.0);
        }
    )";
    GLuint vs = CompileTerrainShaderQ76(GL_VERTEX_SHADER, vsSource);
    GLuint fs = CompileTerrainShaderQ76(GL_FRAGMENT_SHADER, fsSource);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[1024]{};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        Q76_LOGE("Q7.6 terrain shader link failed: %s", log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

} // namespace

bool InitializeFo3TerrainQ76(uint32_t worldspaceFormId,
                            float arrivalX, float arrivalY, float arrivalZ,
                            float sceneForward, float floorY,
                            float unitsPerMetre) {
    ShutdownFo3TerrainQ76();
    if (worldspaceFormId == 0u || unitsPerMetre <= 1.0f) return false;

    std::vector<LandCellQ76> lands;
    if (!CollectLandQ76(worldspaceFormId, lands)) {
        Q76_LOGE("Q7.6 TERRAIN FAILED: worldspace=%08X reason=no-decodable-LAND", worldspaceFormId);
        return false;
    }

    std::vector<TerrainVertexQ76> vertices;
    vertices.reserve(lands.size() * 32u * 32u * 6u);
    float minY = 1e30f;
    float maxY = -1e30f;

    for (const LandCellQ76& land : lands) {
        Vec3Q76 p[LAND_SIDE_Q76 * LAND_SIDE_Q76];
        float colors[LAND_SIDE_Q76 * LAND_SIDE_Q76][3];
        for (int y = 0; y < LAND_SIDE_Q76; ++y) {
            for (int x = 0; x < LAND_SIDE_Q76; ++x) {
                const int i = y * LAND_SIDE_Q76 + x;
                const float gx = static_cast<float>(land.gridX) * CELL_SIZE_Q76 + x * VERTEX_STEP_Q76;
                const float gy = static_cast<float>(land.gridY) * CELL_SIZE_Q76 + y * VERTEX_STEP_Q76;
                p[i] = ToVrQ76(gx, gy, land.heights[i],
                               arrivalX, arrivalY, arrivalZ,
                               sceneForward, floorY, unitsPerMetre);
                minY = std::min(minY, p[i].y);
                maxY = std::max(maxY, p[i].y);
                VertexColorQ76(land, i, colors[i][0], colors[i][1], colors[i][2]);
            }
        }
        for (int y = 0; y < 32; ++y) {
            for (int x = 0; x < 32; ++x) {
                const int i00 = y * LAND_SIDE_Q76 + x;
                const int i10 = i00 + 1;
                const int i01 = i00 + LAND_SIDE_Q76;
                const int i11 = i01 + 1;
                // Winding chosen for the OpenXR coordinate conversion. Terrain
                // draw also disables culling, so malformed edge normals cannot hide it.
                PushTriangleQ76(vertices, p[i00], p[i01], p[i10],
                                colors[i00], colors[i01], colors[i10]);
                PushTriangleQ76(vertices, p[i10], p[i01], p[i11],
                                colors[i10], colors[i01], colors[i11]);
            }
        }
    }

    if (vertices.empty()) return false;
    gTerrainProgramQ76 = CreateTerrainProgramQ76();
    if (!gTerrainProgramQ76) return false;
    gTerrainMvpQ76 = glGetUniformLocation(gTerrainProgramQ76, "uMvp");

    glGenVertexArrays(1, &gTerrainVaoQ76);
    glBindVertexArray(gTerrainVaoQ76);
    glGenBuffers(1, &gTerrainVboQ76);
    glBindBuffer(GL_ARRAY_BUFFER, gTerrainVboQ76);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(TerrainVertexQ76)),
                 vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertexQ76),
                          reinterpret_cast<const void*>(offsetof(TerrainVertexQ76, px)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertexQ76),
                          reinterpret_cast<const void*>(offsetof(TerrainVertexQ76, nx)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertexQ76),
                          reinterpret_cast<const void*>(offsetof(TerrainVertexQ76, r)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    gTerrainVertexCountQ76 = static_cast<GLsizei>(vertices.size());
    gTerrainReadyQ76 = true;
    Q76_LOGI("Q7.6 TERRAIN READY: worldspace=%08X LAND=%zu vertices=%d triangles=%d heightRangeVR=(%.2f %.2f) origin=XTEL",
             worldspaceFormId, lands.size(), gTerrainVertexCountQ76,
             gTerrainVertexCountQ76 / 3, minY, maxY);
    return true;
}

void RenderFo3TerrainQ76(const float* mvp) {
    if (!gTerrainReadyQ76 || !mvp || !gTerrainProgramQ76 || !gTerrainVaoQ76 || gTerrainVertexCountQ76 <= 0) return;

    GLint previousProgram = 0;
    GLint previousVao = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);

    if (cullWasEnabled) glDisable(GL_CULL_FACE);
    if (blendWasEnabled) glDisable(GL_BLEND);
    glUseProgram(gTerrainProgramQ76);
    glUniformMatrix4fv(gTerrainMvpQ76, 1, GL_FALSE, mvp);
    glBindVertexArray(gTerrainVaoQ76);
    glDrawArrays(GL_TRIANGLES, 0, gTerrainVertexCountQ76);
    glBindVertexArray(static_cast<GLuint>(previousVao));
    glUseProgram(static_cast<GLuint>(previousProgram));
    if (blendWasEnabled) glEnable(GL_BLEND);
    if (cullWasEnabled) glEnable(GL_CULL_FACE);
}

void ShutdownFo3TerrainQ76() {
    if (gTerrainVboQ76) glDeleteBuffers(1, &gTerrainVboQ76);
    if (gTerrainVaoQ76) glDeleteVertexArrays(1, &gTerrainVaoQ76);
    if (gTerrainProgramQ76) glDeleteProgram(gTerrainProgramQ76);
    gTerrainVboQ76 = 0;
    gTerrainVaoQ76 = 0;
    gTerrainProgramQ76 = 0;
    gTerrainMvpQ76 = -1;
    gTerrainVertexCountQ76 = 0;
    gTerrainReadyQ76 = false;
}
