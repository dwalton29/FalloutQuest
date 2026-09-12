#pragma once

#include "fo3-loading-screen-q1700.h"

#include <GLES3/gl3.h>
#include <android/log.h>

#include <algorithm>
#include <chrono>
#include <cstdint>

// Q16.8: keep Fallout 3's authored loading-menu identity, but adapt the final
// presentation for binocular VR instead of recreating the desktop 1280x960
// footprint at nearly eye-filling size.
//
// The LSCR DDS still comes from the user's Fallout3.esm / texture archives.
// The spinner follows loading_menu.xml's Interface\\Circular Loading\\loading01.nif
// pinwheel language, enlarged slightly for headset legibility. Both are drawn at
// identical per-eye coordinates (zero forced disparity) to avoid the near-plane
// convergence discomfort seen with the original Q16.0 card.

namespace fo3loadq1780 {

constexpr const char* TAG = "FalloutQuest";

inline GLuint gProgram = 0u;
inline GLuint gVao = 0u;
inline GLint gImageLoc = -1;
inline GLint gHaveImageLoc = -1;
inline GLint gScaleLoc = -1;
inline GLint gCenterLoc = -1;
inline GLint gViewportLoc = -1;
inline GLint gTimeLoc = -1;
inline GLint gFadeLoc = -1;
inline uint64_t gGeneration = ~uint64_t{0};
inline uint64_t gLoggedGeneration = ~uint64_t{0};
inline std::chrono::steady_clock::time_point gGenerationStart{};

inline GLuint Compile(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[2048]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "Q16.8 LOADING shader compile failed: %s", log);
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
        uniform float uHaveImage;
        uniform vec2 uScale;
        uniform vec2 uCenter;
        uniform vec2 uViewport;
        uniform float uTime;
        uniform float uFade;
        out vec4 fragColor;

        float sdSegment(vec2 p, vec2 a, vec2 b) {
            vec2 pa = p - a;
            vec2 ba = b - a;
            float h = clamp(dot(pa, ba) / max(dot(ba, ba), 0.000001), 0.0, 1.0);
            return length(pa - ba * h);
        }

        float lineMask(float d, float halfWidth, float feather) {
            return 1.0 - smoothstep(halfWidth, halfWidth + feather, d);
        }

        void main() {
            vec3 colour = vec3(0.0);
            vec2 halfScale = max(uScale * 0.5, vec2(0.0001));
            vec2 local = vUv - uCenter;

            // Comfortable, aspect-correct Fallout3.esm LSCR panel.
            if (abs(local.x) <= halfScale.x && abs(local.y) <= halfScale.y &&
                uHaveImage > 0.5) {
                vec2 uv = local / uScale + vec2(0.5);
                vec4 image = texture(uImage, uv);
                colour = image.rgb * image.a;
            }

            // loading_menu.xml's circular loader, moved immediately below the
            // image and enlarged from its tiny desktop 54px footprint for VR.
            float viewAspect = uViewport.x / max(uViewport.y, 1.0);
            vec2 spinnerCenter = vec2(uCenter.x, uCenter.y - halfScale.y - 0.060);
            vec2 p = vUv - spinnerCenter;
            p.x *= viewAspect;

            const float PI = 3.14159265359;
            const float TAU = 6.28318530718;
            const float radius = 0.026;
            float core = 0.0;
            float glow = 0.0;

            // Twelve-blade loading01-style pinwheel. The bright head advances
            // clockwise with a trailing falloff, matching the authored Idle
            // animation's visual language without bundling Bethesda assets.
            for (int i = 0; i < 12; ++i) {
                float fi = float(i);
                float a = fi * (TAU / 12.0) + PI * 0.5;
                vec2 dir = vec2(cos(a), sin(a));
                float d = sdSegment(p, dir * radius * 0.43, dir * radius * 0.96);
                float phase = mod(fi / 12.0 + uTime * 0.72, 1.0);
                float intensity = 0.12 + 0.88 * pow(phase, 3.0);
                core = max(core, lineMask(d, radius * 0.075, radius * 0.050) * intensity);
                glow = max(glow, lineMask(d, radius * 0.20, radius * 0.11) * intensity);
            }

            float hub = length(p);
            core = max(core, 1.0 - smoothstep(radius * 0.10, radius * 0.19, hub));
            glow = max(glow, 1.0 - smoothstep(radius * 0.13, radius * 0.38, hub));

            vec3 hud = vec3(26.0 / 255.0, 1.0, 128.0 / 255.0);
            colour += hud * glow * 0.17;
            colour = mix(colour, hud, clamp(core, 0.0, 1.0));
            colour *= clamp(uFade, 0.0, 1.0);
            fragColor = vec4(clamp(colour, 0.0, 1.0), 1.0);
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
        char log[2048]{};
        glGetProgramInfoLog(gProgram, sizeof(log), nullptr, log);
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "Q16.8 LOADING program link failed: %s", log);
        glDeleteProgram(gProgram);
        gProgram = 0u;
        return false;
    }

    glGenVertexArrays(1, &gVao);
    gImageLoc = glGetUniformLocation(gProgram, "uImage");
    gHaveImageLoc = glGetUniformLocation(gProgram, "uHaveImage");
    gScaleLoc = glGetUniformLocation(gProgram, "uScale");
    gCenterLoc = glGetUniformLocation(gProgram, "uCenter");
    gViewportLoc = glGetUniformLocation(gProgram, "uViewport");
    gTimeLoc = glGetUniformLocation(gProgram, "uTime");
    gFadeLoc = glGetUniformLocation(gProgram, "uFade");

    return gImageLoc >= 0 && gHaveImageLoc >= 0 && gScaleLoc >= 0 &&
           gCenterLoc >= 0 && gViewportLoc >= 0 && gTimeLoc >= 0 &&
           gFadeLoc >= 0;
}

inline void Render(GLuint framebuffer, GLsizei width, GLsizei height) {
    if (!IsFo3LoadingVisibleQ1700() || width <= 0 || height <= 0) return;

    const uint64_t generation = GetFo3LoadingGenerationQ1700();
    const auto now = std::chrono::steady_clock::now();
    if (generation != gGeneration) {
        gGeneration = generation;
        gGenerationStart = now;
    }

    const bool haveImage = fo3loadq1700::LoadForCurrentGeneration();

    GLint oldProgram = 0;
    GLint oldVao = 0;
    GLint oldActiveTexture = 0;
    GLint oldTexture = 0;
    GLint oldFramebuffer = 0;
    GLint oldViewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &oldActiveTexture);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldFramebuffer);
    glGetIntegerv(GL_VIEWPORT, oldViewport);
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

    if (!EnsureProgram()) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    } else {
        glUseProgram(gProgram);
        glBindVertexArray(gVao);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, haveImage ? fo3loadq1700::gTexture : 0u);
        glUniform1i(gImageLoc, 0);
        glUniform1f(gHaveImageLoc, haveImage ? 1.0f : 0.0f);

        const float viewAspect = static_cast<float>(width) /
                                 static_cast<float>(std::max<GLsizei>(height, 1));
        const float imageAspect = haveImage
            ? static_cast<float>(fo3loadq1700::gImageWidth) /
              static_cast<float>(std::max(fo3loadq1700::gImageHeight, 1))
            : (4.0f / 3.0f);

        float sx = 0.62f;
        float sy = sx * viewAspect / std::max(imageAspect, 0.001f);
        if (sy > 0.46f) {
            sy = 0.46f;
            sx = sy * imageAspect / std::max(viewAspect, 0.001f);
        }
        sx = std::clamp(sx, 0.08f, 0.72f);
        sy = std::clamp(sy, 0.08f, 0.50f);

        glUniform2f(gScaleLoc, sx, sy);
        glUniform2f(gCenterLoc, 0.50f, 0.60f);
        glUniform2f(gViewportLoc, static_cast<float>(width), static_cast<float>(height));

        const float elapsed = std::chrono::duration<float>(now - gGenerationStart).count();
        glUniform1f(gTimeLoc, elapsed);
        glUniform1f(gFadeLoc, std::clamp(elapsed / 0.35f, 0.0f, 1.0f));
        glDrawArrays(GL_TRIANGLES, 0, 3);

        if (generation != gLoggedGeneration) {
            gLoggedGeneration = generation;
            __android_log_print(
                ANDROID_LOG_INFO, TAG,
                "Q16.8 VR LOADING MENU DRAW: generation=%llu image=%d panel=(%.3f %.3f) center=(0.50 0.60) spinner=loading01-style radius=0.026 stereo=zeroDisparity",
                static_cast<unsigned long long>(generation), haveImage ? 1 : 0,
                sx, sy);
        }
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(oldTexture));
    glActiveTexture(static_cast<GLenum>(oldActiveTexture));
    glBindVertexArray(static_cast<GLuint>(oldVao));
    glUseProgram(static_cast<GLuint>(oldProgram));
    glDepthMask(oldDepthMask);
    if (oldDepth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (oldBlend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(oldFramebuffer));
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
}

inline void Shutdown() {
    if (gVao) glDeleteVertexArrays(1, &gVao);
    if (gProgram) glDeleteProgram(gProgram);
    gVao = 0u;
    gProgram = 0u;
    gGeneration = ~uint64_t{0};
    gLoggedGeneration = ~uint64_t{0};
}

} // namespace fo3loadq1780

inline void RenderFo3LoadingScreenQ1780(GLuint framebuffer, GLsizei width, GLsizei height) {
    fo3loadq1780::Render(framebuffer, width, height);
}

inline void ShutdownFo3LoadingScreenQ1780() {
    fo3loadq1780::Shutdown();
    ShutdownFo3LoadingScreenQ1700();
}
