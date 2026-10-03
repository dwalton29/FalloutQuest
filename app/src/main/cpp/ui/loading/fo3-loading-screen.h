#pragma once

#include "fo3-loading-state.h"
#include "fo3-texture-bsa.h"
#include "fo3-loading-catalog.h"

#include <GLES3/gl3.h>
#include <android/log.h>
#include <zlib.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace fo3loading {

constexpr const char* TAG = "FalloutQuest";
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
                            "LOADING shader compile failed: %s", log);
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
                            "LOADING program link failed: %s", log);
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
    const uint64_t generation = GetFo3LoadingGeneration();
    if (generation == gLoadedGeneration) return gTexture != 0u;
    gLoadedGeneration = generation;

    if (gTexture) glDeleteTextures(1, &gTexture);
    gTexture = 0u;
    gImageWidth = gImageHeight = 0;
    gLoadedEditorId.clear();
    gLoadedDescription.clear();
    gLoadedIcon.clear();

    Prepare();
    const uint32_t cell = GetFo3LoadingCell();
    const uint32_t world = GetFo3LoadingWorldspace();
    const LoadingScreen* selected = Select(cell, world);
    if (!selected) {
        __android_log_print(ANDROID_LOG_WARN, TAG,
                            "LSCR SELECT MISS: cell=%08X worldspace=%08X",
                            cell, world);
        return false;
    }

    Fo3RgbaTexture decoded;
    if (!LoadFalloutTextureRgba(selected->iconPath, decoded) ||
        decoded.width <= 0 || decoded.height <= 0 || decoded.rgba.empty()) {
        __android_log_print(ANDROID_LOG_WARN, TAG,
                            "LSCR DDS MISS: EDID=%s icon=%s cell=%08X worldspace=%08X",
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
                        "LSCR ACTIVE: cell=%08X worldspace=%08X EDID=%s icon=%s size=%dx%d locationMatched=%d DESC=%s",
                        cell, world, gLoadedEditorId.c_str(), gLoadedIcon.c_str(),
                        gImageWidth, gImageHeight,
                        ContainsLocation(*selected, cell) || ContainsLocation(*selected, world) ? 1 : 0,
                        gLoadedDescription.empty() ? "<none>" : gLoadedDescription.c_str());
    return true;
}

inline void Render(GLuint framebuffer, GLsizei width, GLsizei height) {
    if (!IsFo3LoadingVisible() || width <= 0 || height <= 0) return;
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

} // namespace fo3loading

inline void PrepareFo3LoadingScreens() { fo3loading::Prepare(); }
inline void RenderFo3LoadingScreenBase(GLuint framebuffer, GLsizei width, GLsizei height) {
    fo3loading::Render(framebuffer, width, height);
}
inline void ShutdownFo3LoadingScreenBase() { fo3loading::Shutdown(); }
