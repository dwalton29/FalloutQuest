#pragma once

#include <GLES3/gl3.h>
#include <android/log.h>
#include <zlib.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

struct Fo3EnvironmentQ1000 {
    bool valid = false;
    uint32_t worldspaceFormId = 0u;
    uint32_t climateFormId = 0u;
    uint32_t weatherFormId = 0u;
    std::string climateEditorId;
    std::string weatherEditorId;

    float skyUpper[3]{0.10f, 0.12f, 0.09f};
    float skyLower[3]{0.18f, 0.19f, 0.14f};
    float horizon[3]{0.24f, 0.23f, 0.17f};
    float fog[3]{0.24f, 0.23f, 0.17f};
    float ambient[3]{0.34f, 0.34f, 0.34f};
    float sunlight[3]{0.66f, 0.66f, 0.66f};
    float sun[3]{0.85f, 0.78f, 0.58f};
    float fogNear = 0.0f;
    float fogFar = 0.0f;

    // Q10.0 ports Bethesda's authored light colours first. The actual solar
    // vector depends on Fallout's game clock, which is not ported yet, so keep
    // the renderer's proven direction until the clock/time-of-day milestone.
    float sunDirection[3]{0.35f, 0.85f, 0.40f};
};

inline Fo3EnvironmentQ1000 gFo3EnvironmentQ1000;

namespace fo3envq1000 {

constexpr const char* TAG = "FalloutQuest";
constexpr const char* ESM_PATH =
    "/data/user/0/com.falloutquest.app/files/Fallout3/Data/Fallout3.esm";
constexpr uint32_t FLAG_COMPRESSED = 0x00040000u;
constexpr uint64_t HEADER_SIZE = 24u;
constexpr uint32_t MAX_RECORD_BYTES = 64u * 1024u * 1024u;

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

inline int32_t ReadI32(const uint8_t* p) {
    return static_cast<int32_t>(Read32(p));
}

inline float ReadFloat(const uint8_t* p) {
    const uint32_t bits = Read32(p);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
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

inline std::string CString(const uint8_t* data, uint32_t size) {
    size_t len = 0u;
    while (len < size && data[len] != 0u) ++len;
    return std::string(reinterpret_cast<const char*>(data), len);
}

inline bool FindRecord(const char wantedType[4], uint32_t wantedFormId,
                       std::vector<uint8_t>& payload) {
    payload.clear();
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
        const uint32_t sizeField = Read32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < HEADER_SIZE ||
                offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            continue;
        }

        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        const uint32_t flags = Read32(header + 8u);
        const uint32_t formId = Read32(header + 12u);
        if (formId == wantedFormId && std::memcmp(header, wantedType, 4u) == 0) {
            found = ReadPayload(file, sizeField, flags, payload);
            break;
        }
        if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
    }

    std::fclose(file);
    return found;
}

struct WorldClimateLink {
    uint32_t climate = 0u;
    uint32_t parent = 0u;
    uint8_t parentFlags = 0u;
};

inline bool ReadWorldClimateLink(uint32_t worldspaceFormId, WorldClimateLink& out) {
    out = {};
    std::vector<uint8_t> payload;
    if (!FindRecord("WRLD", worldspaceFormId, payload)) return false;
    WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "CNAM", 4u) == 0 && size >= 4u) {
            out.climate = Read32(bytes);
        } else if (std::memcmp(type, "WNAM", 4u) == 0 && size >= 4u) {
            out.parent = Read32(bytes);
        } else if (std::memcmp(type, "PNAM", 4u) == 0 && size >= 1u) {
            out.parentFlags = bytes[0];
        }
    });
    return true;
}

inline bool ResolveClimate(uint32_t worldspaceFormId, uint32_t& climateFormId) {
    climateFormId = 0u;
    uint32_t current = worldspaceFormId;
    for (int depth = 0; depth < 8 && current != 0u; ++depth) {
        WorldClimateLink link;
        if (!ReadWorldClimateLink(current, link)) return false;
        if (link.climate != 0u) {
            climateFormId = link.climate;
            return true;
        }
        if (link.parent == 0u || (link.parentFlags & 0x10u) == 0u) break;
        current = link.parent;
    }
    return false;
}

inline bool ChooseClimateWeather(uint32_t climateFormId,
                                 uint32_t& weatherFormId,
                                 std::string& climateEdid) {
    weatherFormId = 0u;
    climateEdid.clear();
    std::vector<uint8_t> payload;
    if (!FindRecord("CLMT", climateFormId, payload)) return false;

    int32_t bestChance = -1;
    uint32_t firstWeather = 0u;
    WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "EDID", 4u) == 0 && climateEdid.empty()) {
            climateEdid = CString(bytes, size);
        } else if (std::memcmp(type, "WLST", 4u) == 0) {
            for (uint32_t pos = 0u; pos + 12u <= size; pos += 12u) {
                const uint32_t weather = Read32(bytes + pos);
                const int32_t chance = ReadI32(bytes + pos + 4u);
                if (weather == 0u) continue;
                if (firstWeather == 0u) firstWeather = weather;
                if (chance > bestChance) {
                    bestChance = chance;
                    weatherFormId = weather;
                }
            }
        }
    });
    if (weatherFormId == 0u) weatherFormId = firstWeather;
    return weatherFormId != 0u;
}

inline void ReadDayColor(const uint8_t* nam0, uint32_t size,
                         int category, float out[3]) {
    const uint32_t offset = static_cast<uint32_t>(category) * 16u + 4u;
    if (!nam0 || offset + 4u > size) return;
    out[0] = static_cast<float>(nam0[offset + 0u]) / 255.0f;
    out[1] = static_cast<float>(nam0[offset + 1u]) / 255.0f;
    out[2] = static_cast<float>(nam0[offset + 2u]) / 255.0f;
}

inline bool ReadWeather(uint32_t weatherFormId, Fo3EnvironmentQ1000& env) {
    std::vector<uint8_t> payload;
    if (!FindRecord("WTHR", weatherFormId, payload)) return false;

    bool haveColors = false;
    WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "EDID", 4u) == 0 && env.weatherEditorId.empty()) {
            env.weatherEditorId = CString(bytes, size);
        } else if (std::memcmp(type, "NAM0", 4u) == 0 && size >= 160u) {
            // Fallout 3 WTHR NAM0: ten colour categories, each containing
            // Sunrise/Day/Sunset/Night RGBA. Q10.0 intentionally selects Day.
            ReadDayColor(bytes, size, 0, env.skyUpper);
            ReadDayColor(bytes, size, 1, env.fog);
            ReadDayColor(bytes, size, 3, env.ambient);
            ReadDayColor(bytes, size, 4, env.sunlight);
            ReadDayColor(bytes, size, 5, env.sun);
            ReadDayColor(bytes, size, 7, env.skyLower);
            ReadDayColor(bytes, size, 8, env.horizon);
            haveColors = true;
        } else if (std::memcmp(type, "FNAM", 4u) == 0 && size >= 24u) {
            env.fogNear = ReadFloat(bytes + 0u);
            env.fogFar = ReadFloat(bytes + 4u);
        }
    });
    return haveColors;
}

inline GLuint skyProgram = 0u;
inline GLuint skyVao = 0u;
inline GLuint skyVbo = 0u;
inline GLsizei skyVertexCount = 0;
inline GLint skyMvpLocation = -1;
inline GLint skyUpperLocation = -1;
inline GLint skyLowerLocation = -1;
inline GLint skyHorizonLocation = -1;
inline bool skyLogged = false;

inline GLuint CompileSkyShader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[1024]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "Q10.0 SKY shader compile failed: %s", log);
        glDeleteShader(shader);
        return 0u;
    }
    return shader;
}

inline bool EnsureSkyGpu() {
    if (skyProgram && skyVao && skyVertexCount > 0) return true;

    static const char* vertexSource = R"(
        #version 300 es
        layout(location = 0) in vec3 aPosition;
        uniform mat4 uMvp;
        out vec3 vDirection;
        void main() {
            vDirection = aPosition;
            gl_Position = uMvp * vec4(aPosition, 1.0);
        }
    )";
    static const char* fragmentSource = R"(
        #version 300 es
        precision mediump float;
        in vec3 vDirection;
        uniform vec3 uSkyUpper;
        uniform vec3 uSkyLower;
        uniform vec3 uHorizon;
        out vec4 fragColor;
        void main() {
            float y = clamp(normalize(vDirection).y, 0.0, 1.0);
            vec3 lower = mix(uHorizon, uSkyLower, smoothstep(0.0, 0.32, y));
            vec3 colour = mix(lower, uSkyUpper, smoothstep(0.28, 0.95, y));
            fragColor = vec4(colour, 1.0);
        }
    )";

    const GLuint vs = CompileSkyShader(GL_VERTEX_SHADER, vertexSource);
    const GLuint fs = CompileSkyShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    skyProgram = glCreateProgram();
    glAttachShader(skyProgram, vs);
    glAttachShader(skyProgram, fs);
    glLinkProgram(skyProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint linked = GL_FALSE;
    glGetProgramiv(skyProgram, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char log[1024]{};
        glGetProgramInfoLog(skyProgram, sizeof(log), nullptr, log);
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "Q10.0 SKY shader link failed: %s", log);
        glDeleteProgram(skyProgram);
        skyProgram = 0u;
        return false;
    }

    constexpr int stacks = 12;
    constexpr int slices = 24;
    constexpr float radius = 50.0f;
    constexpr float pi = 3.14159265358979323846f;
    std::vector<float> vertices;
    vertices.reserve(static_cast<size_t>(stacks * slices * 6 * 3));
    auto push = [&](float latitude, float longitude) {
        const float cl = std::cos(latitude);
        vertices.push_back(radius * cl * std::cos(longitude));
        vertices.push_back(radius * std::sin(latitude));
        vertices.push_back(radius * cl * std::sin(longitude));
    };
    for (int stack = 0; stack < stacks; ++stack) {
        const float lat0 = -0.5f * pi + pi * static_cast<float>(stack) / stacks;
        const float lat1 = -0.5f * pi + pi * static_cast<float>(stack + 1) / stacks;
        for (int slice = 0; slice < slices; ++slice) {
            const float lon0 = 2.0f * pi * static_cast<float>(slice) / slices;
            const float lon1 = 2.0f * pi * static_cast<float>(slice + 1) / slices;
            push(lat0, lon0); push(lat1, lon0); push(lat1, lon1);
            push(lat0, lon0); push(lat1, lon1); push(lat0, lon1);
        }
    }

    glGenVertexArrays(1, &skyVao);
    glBindVertexArray(skyVao);
    glGenBuffers(1, &skyVbo);
    glBindBuffer(GL_ARRAY_BUFFER, skyVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    skyVertexCount = static_cast<GLsizei>(vertices.size() / 3u);
    skyMvpLocation = glGetUniformLocation(skyProgram, "uMvp");
    skyUpperLocation = glGetUniformLocation(skyProgram, "uSkyUpper");
    skyLowerLocation = glGetUniformLocation(skyProgram, "uSkyLower");
    skyHorizonLocation = glGetUniformLocation(skyProgram, "uHorizon");
    return skyMvpLocation >= 0 && skyUpperLocation >= 0 &&
           skyLowerLocation >= 0 && skyHorizonLocation >= 0;
}

} // namespace fo3envq1000

inline void ResetFo3EnvironmentQ1000() {
    gFo3EnvironmentQ1000 = Fo3EnvironmentQ1000{};
}

inline const Fo3EnvironmentQ1000& GetFo3EnvironmentQ1000() {
    return gFo3EnvironmentQ1000;
}

inline bool LoadFo3EnvironmentQ1000(uint32_t worldspaceFormId) {
    Fo3EnvironmentQ1000 env;
    env.worldspaceFormId = worldspaceFormId;
    uint32_t climate = 0u;
    if (!fo3envq1000::ResolveClimate(worldspaceFormId, climate) || climate == 0u) {
        ResetFo3EnvironmentQ1000();
        __android_log_print(ANDROID_LOG_WARN, fo3envq1000::TAG,
                            "Q10.0 ENV FALLBACK: worldspace=%08X reason=no-climate",
                            worldspaceFormId);
        return false;
    }
    env.climateFormId = climate;

    uint32_t weather = 0u;
    if (!fo3envq1000::ChooseClimateWeather(climate, weather, env.climateEditorId) ||
        weather == 0u) {
        ResetFo3EnvironmentQ1000();
        __android_log_print(ANDROID_LOG_WARN, fo3envq1000::TAG,
                            "Q10.0 ENV FALLBACK: worldspace=%08X climate=%08X reason=no-weather",
                            worldspaceFormId, climate);
        return false;
    }
    env.weatherFormId = weather;
    if (!fo3envq1000::ReadWeather(weather, env)) {
        ResetFo3EnvironmentQ1000();
        __android_log_print(ANDROID_LOG_WARN, fo3envq1000::TAG,
                            "Q10.0 ENV FALLBACK: worldspace=%08X climate=%08X weather=%08X reason=no-NAM0",
                            worldspaceFormId, climate, weather);
        return false;
    }

    env.valid = true;
    gFo3EnvironmentQ1000 = env;
    __android_log_print(
        ANDROID_LOG_INFO, fo3envq1000::TAG,
        "Q10.0 ENV READY: worldspace=%08X climate=%08X(%s) weather=%08X(%s) mode=DAY skyUpper=(%.3f %.3f %.3f) skyLower=(%.3f %.3f %.3f) horizon=(%.3f %.3f %.3f) ambient=(%.3f %.3f %.3f) sunlight=(%.3f %.3f %.3f) fog=(%.3f %.3f %.3f) fogNear=%.1f fogFar=%.1f sunDirection=legacy-until-game-clock",
        env.worldspaceFormId, env.climateFormId,
        env.climateEditorId.empty() ? "<none>" : env.climateEditorId.c_str(),
        env.weatherFormId,
        env.weatherEditorId.empty() ? "<none>" : env.weatherEditorId.c_str(),
        env.skyUpper[0], env.skyUpper[1], env.skyUpper[2],
        env.skyLower[0], env.skyLower[1], env.skyLower[2],
        env.horizon[0], env.horizon[1], env.horizon[2],
        env.ambient[0], env.ambient[1], env.ambient[2],
        env.sunlight[0], env.sunlight[1], env.sunlight[2],
        env.fog[0], env.fog[1], env.fog[2], env.fogNear, env.fogFar);
    return true;
}

inline void RenderFo3SkyQ1000(const float* mvp16) {
    if (!mvp16) return;
    const Fo3EnvironmentQ1000& env = GetFo3EnvironmentQ1000();
    if (!env.valid || !fo3envq1000::EnsureSkyGpu()) return;

    GLint previousProgram = 0;
    GLint previousVao = 0;
    GLboolean previousDepthMask = GL_TRUE;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
    const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glUseProgram(fo3envq1000::skyProgram);
    glUniformMatrix4fv(fo3envq1000::skyMvpLocation, 1, GL_FALSE, mvp16);
    glUniform3fv(fo3envq1000::skyUpperLocation, 1, env.skyUpper);
    glUniform3fv(fo3envq1000::skyLowerLocation, 1, env.skyLower);
    glUniform3fv(fo3envq1000::skyHorizonLocation, 1, env.horizon);
    glBindVertexArray(fo3envq1000::skyVao);
    glDrawArrays(GL_TRIANGLES, 0, fo3envq1000::skyVertexCount);

    glBindVertexArray(static_cast<GLuint>(previousVao));
    glUseProgram(static_cast<GLuint>(previousProgram));
    glDepthMask(previousDepthMask);
    if (depthWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (cullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);

    if (!fo3envq1000::skyLogged) {
        fo3envq1000::skyLogged = true;
        __android_log_print(ANDROID_LOG_INFO, fo3envq1000::TAG,
                            "Q10.0 SKY VISIBLE: worldspace=%08X weather=%08X authoredWTHRGradient=1 stereo=1",
                            env.worldspaceFormId, env.weatherFormId);
    }
}
