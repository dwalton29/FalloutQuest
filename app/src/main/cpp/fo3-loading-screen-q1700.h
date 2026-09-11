#pragma once

#include "fo3-loading-state-q1700.h"
#include "fo3-texture-bsa.h"

#include <GLES3/gl3.h>
#include <android/log.h>
#include <zlib.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace fo3loadq1700 {

constexpr const char* TAG = "FalloutQuest";
constexpr const char* ESM_PATH =
    "/data/user/0/com.falloutquest.app/files/Fallout3/Data/Fallout3.esm";
constexpr size_t RECORD_HEADER = 24u;
constexpr uint32_t COMPRESSED_RECORD = 0x00040000u;

struct LoadingScreen {
    uint32_t formId = 0u;
    std::string editorId;
    std::string description;
    std::string iconPath;
    std::vector<uint32_t> locations;
};

inline std::vector<LoadingScreen> gScreens;
inline bool gScreensPrepared = false;
inline GLuint gProgram = 0u;
inline GLuint gVao = 0u;
inline GLuint gTexture = 0u;
inline GLint gImageLoc = -1;
inline GLint gScaleLoc = -1;
inline int gImageWidth = 0;
inline int gImageHeight = 0;
inline uint64_t gLoadedGeneration = ~uint64_t{0};
inline std::string gLoadedEditorId;
inline std::string gLoadedDescription;
inline std::string gLoadedIcon;

inline uint16_t Read16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           (static_cast<uint16_t>(p[1]) << 8u);
}

inline uint32_t Read32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8u) |
           (static_cast<uint32_t>(p[2]) << 16u) |
           (static_cast<uint32_t>(p[3]) << 24u);
}

inline std::string ReadString(const uint8_t* p, size_t n) {
    while (n > 0u && p[n - 1u] == 0u) --n;
    return std::string(reinterpret_cast<const char*>(p), n);
}

inline bool ReadExact(FILE* f, void* dst, size_t bytes) {
    return f && std::fread(dst, 1u, bytes, f) == bytes;
}

inline bool InflateRecord(const std::vector<uint8_t>& stored,
                          uint32_t flags,
                          std::vector<uint8_t>& payload) {
    if ((flags & COMPRESSED_RECORD) == 0u) {
        payload = stored;
        return true;
    }
    if (stored.size() < 5u) return false;
    const uint32_t wanted = Read32(stored.data());
    if (wanted == 0u || wanted > 8u * 1024u * 1024u) return false;
    payload.resize(wanted);
    uLongf outputBytes = static_cast<uLongf>(wanted);
    const int z = uncompress(payload.data(), &outputBytes,
                             stored.data() + 4u,
                             static_cast<uLong>(stored.size() - 4u));
    if (z != Z_OK || outputBytes != wanted) {
        payload.clear();
        return false;
    }
    return true;
}

inline void ParseLoadingRecord(uint32_t formId,
                               const std::vector<uint8_t>& payload) {
    LoadingScreen screen;
    screen.formId = formId;
    size_t pos = 0u;
    while (pos + 6u <= payload.size()) {
        char type[5]{
            static_cast<char>(payload[pos]), static_cast<char>(payload[pos + 1u]),
            static_cast<char>(payload[pos + 2u]), static_cast<char>(payload[pos + 3u]), 0};
        uint32_t size = Read16(payload.data() + pos + 4u);
        pos += 6u;

        if (std::memcmp(type, "XXXX", 4u) == 0) {
            if (size != 4u || pos + 4u + 6u > payload.size()) break;
            const uint32_t extended = Read32(payload.data() + pos);
            pos += 4u;
            type[0] = static_cast<char>(payload[pos]);
            type[1] = static_cast<char>(payload[pos + 1u]);
            type[2] = static_cast<char>(payload[pos + 2u]);
            type[3] = static_cast<char>(payload[pos + 3u]);
            type[4] = 0;
            pos += 6u; // next subrecord's 16-bit size is ignored by XXXX semantics
            size = extended;
        }

        if (pos + size > payload.size()) break;
        const uint8_t* data = payload.data() + pos;
        if (std::memcmp(type, "EDID", 4u) == 0) {
            screen.editorId = ReadString(data, size);
        } else if (std::memcmp(type, "DESC", 4u) == 0) {
            screen.description = ReadString(data, size);
        } else if (std::memcmp(type, "ICON", 4u) == 0) {
            screen.iconPath = ReadString(data, size);
        } else if (std::memcmp(type, "LNAM", 4u) == 0 && size >= 4u) {
            screen.locations.push_back(Read32(data));
        }
        pos += size;
    }
    if (!screen.iconPath.empty()) gScreens.push_back(std::move(screen));
}

inline void Prepare() {
    if (gScreensPrepared) return;
    gScreensPrepared = true;

    FILE* f = std::fopen(ESM_PATH, "rb");
    if (!f) {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "Q16.0 LSCR CACHE FAIL: Fallout3.esm open failed");
        return;
    }

    uint8_t header[RECORD_HEADER]{};
    size_t records = 0u;
    while (ReadExact(f, header, sizeof(header))) {
        const uint32_t sizeField = Read32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            // Group size includes this 24-byte header. Children immediately
            // follow, so continue sequentially instead of skipping the group.
            continue;
        }
        if (sizeField > 64u * 1024u * 1024u) break;

        const uint32_t flags = Read32(header + 8u);
        const uint32_t formId = Read32(header + 12u);
        if (std::memcmp(header, "LSCR", 4u) == 0) {
            std::vector<uint8_t> stored(sizeField);
            if (!ReadExact(f, stored.data(), stored.size())) break;
            std::vector<uint8_t> payload;
            if (InflateRecord(stored, flags, payload)) {
                ParseLoadingRecord(formId, payload);
                ++records;
            }
        } else if (std::fseek(f, static_cast<long>(sizeField), SEEK_CUR) != 0) {
            break;
        }
    }
    std::fclose(f);

    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "Q16.0 LSCR CACHE READY: records=%zu usableIcons=%zu source=Fallout3.esm",
                        records, gScreens.size());
}

inline bool ContainsLocation(const LoadingScreen& screen, uint32_t formId) {
    return formId != 0u &&
           std::find(screen.locations.begin(), screen.locations.end(), formId) !=
               screen.locations.end();
}

inline const LoadingScreen* Select(uint32_t cellFormId, uint32_t worldspaceFormId) {
    if (gScreens.empty()) return nullptr;
    std::vector<const LoadingScreen*> exact;
    std::vector<const LoadingScreen*> world;
    std::vector<const LoadingScreen*> generic;
    for (const LoadingScreen& screen : gScreens) {
        if (ContainsLocation(screen, cellFormId)) exact.push_back(&screen);
        else if (ContainsLocation(screen, worldspaceFormId)) world.push_back(&screen);
        else if (screen.locations.empty()) generic.push_back(&screen);
    }
    const std::vector<const LoadingScreen*>* choices = nullptr;
    if (!exact.empty()) choices = &exact;
    else if (!world.empty()) choices = &world;
    else if (!generic.empty()) choices = &generic;
    if (!choices || choices->empty()) choices = nullptr;
    if (!choices) return &gScreens[(cellFormId ^ worldspaceFormId) % gScreens.size()];
    const uint32_t hash = cellFormId * 1664525u + worldspaceFormId * 1013904223u;
    return (*choices)[hash % choices->size()];
}

inline GLuint Compile(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[1024]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "Q16.0 LOADING shader compile failed: %s", log);
        glDeleteShader(shader);
        return 0u;
    }
    return shader;
}

inline bool EnsureProgram() {
    if (gProgram && gVao) return true;
    static const char* vsSource = R"(
        #version 300 es
        precision highp float;
        out vec2 vUv;
        void main() {
            vec2 p;
            if (gl_VertexID == 0) p = vec2(-1.0, -1.0);
            else if (gl_VertexID == 1) p = vec2(3.0, -1.0);
            else p = vec2(-1.0, 3.0);
            vUv = p * 0.5 + 0.5;
            gl_Position = vec4(p, 0.0, 1.0);
        }
    )";
    static const char* fsSource = R"(
        #version 300 es
        precision highp float;
        in vec2 vUv;
        uniform sampler2D uImage;
        uniform vec2 uScale;
        out vec4 fragColor;
        void main() {
            vec2 centered = vUv - vec2(0.5);
            vec2 halfScale = max(uScale * 0.5, vec2(0.0001));
            if (abs(centered.x) > halfScale.x || abs(centered.y) > halfScale.y) {
                fragColor = vec4(0.0, 0.0, 0.0, 1.0);
                return;
            }
            vec2 uv = centered / uScale + vec2(0.5);
            vec4 image = texture(uImage, uv);
            fragColor = vec4(image.rgb * image.a, 1.0);
        }
    )";

    const GLuint vs = Compile(GL_VERTEX_SHADER, vsSource);
    const GLuint fs = Compile(GL_FRAGMENT_SHADER, fsSource);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    gProgram = glCreateProgram();
    glAttachShader(gProgram, vs);
    glAttachShader(gProgram, fs);
    glLinkProgram(gProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint linked = GL_FALSE;
    glGetProgramiv(gProgram, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char log[1024]{};
        glGetProgramInfoLog(gProgram, sizeof(log), nullptr, log);
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "Q16.0 LOADING program link failed: %s", log);
        glDeleteProgram(gProgram);
        gProgram = 0u;
        return false;
    }
    glGenVertexArrays(1, &gVao);
    gImageLoc = glGetUniformLocation(gProgram, "uImage");
    gScaleLoc = glGetUniformLocation(gProgram, "uScale");
    return gImageLoc >= 0 && gScaleLoc >= 0;
}

inline bool LoadForCurrentGeneration() {
    const uint64_t generation = GetFo3LoadingGenerationQ1700();
    if (generation == gLoadedGeneration) return gTexture != 0u;
    gLoadedGeneration = generation;

    if (gTexture) glDeleteTextures(1, &gTexture);
    gTexture = 0u;
    gImageWidth = gImageHeight = 0;
    gLoadedEditorId.clear();
    gLoadedDescription.clear();
    gLoadedIcon.clear();

    Prepare();
    const uint32_t cell = GetFo3LoadingCellQ1700();
    const uint32_t world = GetFo3LoadingWorldspaceQ1700();
    const LoadingScreen* selected = Select(cell, world);
    if (!selected) {
        __android_log_print(ANDROID_LOG_WARN, TAG,
                            "Q16.0 LSCR SELECT MISS: cell=%08X worldspace=%08X",
                            cell, world);
        return false;
    }

    Fo3RgbaTexture decoded;
    if (!LoadFalloutTextureRgba(selected->iconPath, decoded) ||
        decoded.width <= 0 || decoded.height <= 0 || decoded.rgba.empty()) {
        __android_log_print(ANDROID_LOG_WARN, TAG,
                            "Q16.0 LSCR DDS MISS: EDID=%s icon=%s cell=%08X worldspace=%08X",
                            selected->editorId.c_str(), selected->iconPath.c_str(), cell, world);
        return false;
    }

    glGenTextures(1, &gTexture);
    glBindTexture(GL_TEXTURE_2D, gTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, decoded.width, decoded.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, decoded.rgba.data());
    gImageWidth = decoded.width;
    gImageHeight = decoded.height;
    gLoadedEditorId = selected->editorId;
    gLoadedDescription = selected->description;
    gLoadedIcon = selected->iconPath;

    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "Q16.0 LSCR ACTIVE: cell=%08X worldspace=%08X EDID=%s icon=%s size=%dx%d locationMatched=%d DESC=%s",
                        cell, world, gLoadedEditorId.c_str(), gLoadedIcon.c_str(),
                        gImageWidth, gImageHeight,
                        ContainsLocation(*selected, cell) || ContainsLocation(*selected, world) ? 1 : 0,
                        gLoadedDescription.empty() ? "<none>" : gLoadedDescription.c_str());
    return true;
}

inline void Render(GLuint framebuffer, GLsizei width, GLsizei height) {
    if (!IsFo3LoadingVisibleQ1700() || width <= 0 || height <= 0) return;
    const bool haveImage = LoadForCurrentGeneration();

    GLint oldProgram = 0, oldVao = 0, oldActiveTexture = 0, oldTexture = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &oldActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
    const GLboolean oldDepth = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean oldBlend = glIsEnabled(GL_BLEND);
    GLboolean oldDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &oldDepthMask);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);

    if (!haveImage || !EnsureProgram()) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    } else {
        glUseProgram(gProgram);
        glBindVertexArray(gVao);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gTexture);
        glUniform1i(gImageLoc, 0);

        const float viewAspect = static_cast<float>(width) / static_cast<float>(height);
        const float imageAspect = static_cast<float>(gImageWidth) /
                                  static_cast<float>(std::max(gImageHeight, 1));
        float sx = 0.86f;
        float sy = sx * viewAspect / std::max(imageAspect, 0.001f);
        if (sy > 0.78f) {
            sy = 0.78f;
            sx = sy * imageAspect / std::max(viewAspect, 0.001f);
        }
        glUniform2f(gScaleLoc, std::clamp(sx, 0.05f, 0.95f),
                    std::clamp(sy, 0.05f, 0.95f));
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(oldTexture));
    glActiveTexture(static_cast<GLenum>(oldActiveTexture));
    glBindVertexArray(static_cast<GLuint>(oldVao));
    glUseProgram(static_cast<GLuint>(oldProgram));
    glDepthMask(oldDepthMask);
    if (oldDepth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (oldBlend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
}

inline void Shutdown() {
    if (gTexture) glDeleteTextures(1, &gTexture);
    if (gVao) glDeleteVertexArrays(1, &gVao);
    if (gProgram) glDeleteProgram(gProgram);
    gTexture = gVao = gProgram = 0u;
    gLoadedGeneration = ~uint64_t{0};
}

} // namespace fo3loadq1700

inline void PrepareFo3LoadingScreensQ1700() { fo3loadq1700::Prepare(); }
inline void RenderFo3LoadingScreenQ1700(GLuint framebuffer, GLsizei width, GLsizei height) {
    fo3loadq1700::Render(framebuffer, width, height);
}
inline void ShutdownFo3LoadingScreenQ1700() { fo3loadq1700::Shutdown(); }
