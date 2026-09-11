#pragma once

#include "fo3-loading-screen-q1700.h"

#include <GLES3/gl3.h>
#include <android/log.h>

#include <algorithm>
#include <chrono>
#include <cstdint>

namespace fo3loadq1720 {

constexpr const char* TAG = "FalloutQuest";

inline GLuint gProgram = 0u;
inline GLuint gVao = 0u;
inline GLint gImageLoc = -1;
inline GLint gScaleLoc = -1;
inline GLint gCenterLoc = -1;
inline GLint gViewportLoc = -1;
inline GLint gTimeLoc = -1;
inline uint64_t gLoggedGeneration = ~uint64_t{0};

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
                            "Q16.2 LOADING shader compile failed: %s", log);
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
        uniform vec2 uCenter;
        uniform vec2 uViewport;
        uniform float uTime;
        out vec4 fragColor;

        float sdSegment(vec2 p, vec2 a, vec2 b) {
            vec2 pa = p - a;
            vec2 ba = b - a;
            float h = clamp(dot(pa, ba) / max(dot(ba, ba), 0.000001), 0.0, 1.0);
            return length(pa - ba * h);
        }

        float stroke(float d, float halfWidth, float feather) {
            return 1.0 - smoothstep(halfWidth, halfWidth + feather, d);
        }

        void main() {
            vec3 colour = vec3(0.0);
            vec2 halfScale = max(uScale * 0.5, vec2(0.0001));
            vec2 panel = vUv - uCenter;

            // Real Fallout3.esm LSCR artwork. The panel is deliberately much
            // smaller than Q16.0 and sits above optical centre, leaving space
            // for the loading dial beneath it.
            if (abs(panel.x) <= halfScale.x && abs(panel.y) <= halfScale.y) {
                vec2 uv = panel / uScale + vec2(0.5);
                vec4 image = texture(uImage, uv);
                colour = image.rgb * image.a;
            }

            // Fallout 3 loadinganim01 visual language: a minimal clock/compass
            // dial with a continuously rotating hand. Keep it circular despite
            // the eye framebuffer aspect ratio.
            float aspect = uViewport.x / max(uViewport.y, 1.0);
            vec2 spinnerCenter = vec2(uCenter.x, uCenter.y - halfScale.y - 0.070);
            vec2 p = vUv - spinnerCenter;
            p.x *= aspect;

            const float PI = 3.14159265359;
            const float TAU = 6.28318530718;
            const float radius = 0.031;
            float core = 0.0;
            float glow = 0.0;

            // Outer clock ring.
            float ringDistance = abs(length(p) - radius);
            core = max(core, stroke(ringDistance, 0.00115, 0.00115));
            glow = max(glow, stroke(ringDistance, 0.0048, 0.0030));

            // Twelve clock-face marks. The four cardinal marks are longer.
            float angle = atan(p.y, p.x);
            float sector = TAU / 12.0;
            float tickAngle = floor((angle + sector * 0.5) / sector) * sector;
            float tickIndex = mod(floor((angle + PI + sector * 0.5) / sector), 12.0);
            float cardinal = 1.0 - step(0.1, mod(tickIndex, 3.0));
            vec2 tickDir = vec2(cos(tickAngle), sin(tickAngle));
            float tickInner = mix(radius - 0.0050, radius - 0.0082, cardinal);
            float tickDistance = sdSegment(p, tickDir * tickInner,
                                           tickDir * (radius - 0.0008));
            core = max(core, stroke(tickDistance, 0.00095, 0.0011));
            glow = max(glow, stroke(tickDistance, 0.0038, 0.0028));

            // Rotating luminous pointer plus a short counterweight.
            float spin = -uTime * 2.55 + PI * 0.5;
            vec2 needleDir = vec2(cos(spin), sin(spin));
            float needleDistance = sdSegment(p, -needleDir * 0.0080,
                                             needleDir * 0.0240);
            core = max(core, stroke(needleDistance, 0.00125, 0.0010));
            glow = max(glow, stroke(needleDistance, 0.0052, 0.0030));

            // Small centre hub.
            float hubDistance = abs(length(p) - 0.0032);
            core = max(core, stroke(hubDistance, 0.0010, 0.0010));
            glow = max(glow, stroke(hubDistance, 0.0042, 0.0028));

            // Fallout 3 HUDMain green, RGB(26,255,128), with restrained glow.
            vec3 hud = vec3(26.0 / 255.0, 1.0, 128.0 / 255.0);
            colour += hud * glow * 0.14;
            colour = mix(colour, hud, clamp(core, 0.0, 1.0));

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
                            "Q16.2 LOADING program link failed: %s", log);
        glDeleteProgram(gProgram);
        gProgram = 0u;
        return false;
    }

    glGenVertexArrays(1, &gVao);
    gImageLoc = glGetUniformLocation(gProgram, "uImage");
    gScaleLoc = glGetUniformLocation(gProgram, "uScale");
    gCenterLoc = glGetUniformLocation(gProgram, "uCenter");
    gViewportLoc = glGetUniformLocation(gProgram, "uViewport");
    gTimeLoc = glGetUniformLocation(gProgram, "uTime");

    return gImageLoc >= 0 && gScaleLoc >= 0 && gCenterLoc >= 0 &&
           gViewportLoc >= 0 && gTimeLoc >= 0;
}

inline void Render(GLuint framebuffer, GLsizei width, GLsizei height) {
    if (!IsFo3LoadingVisibleQ1700() || width <= 0 || height <= 0) return;

    // Keep Q16.0's proven ESM LSCR selection/DDS upload exactly as-is. Q16.2
    // owns presentation only.
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

    if (!haveImage || !EnsureProgram()) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    } else {
        glUseProgram(gProgram);
        glBindVertexArray(gVao);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fo3loadq1700::gTexture);
        glUniform1i(gImageLoc, 0);

        const float viewAspect = static_cast<float>(width) /
                                 static_cast<float>(height);
        const float imageAspect = static_cast<float>(fo3loadq1700::gImageWidth) /
                                  static_cast<float>(std::max(fo3loadq1700::gImageHeight, 1));

        // Q16.0 filled 86% of each eye. 62% plus zero forced stereo disparity
        // makes the panel read optically far away instead of sitting on the
        // viewer's face and forcing uncomfortable convergence.
        float sx = 0.62f;
        float sy = sx * viewAspect / std::max(imageAspect, 0.001f);
        if (sy > 0.48f) {
            sy = 0.48f;
            sx = sy * imageAspect / std::max(viewAspect, 0.001f);
        }
        sx = std::clamp(sx, 0.05f, 0.80f);
        sy = std::clamp(sy, 0.05f, 0.60f);

        glUniform2f(gScaleLoc, sx, sy);
        glUniform2f(gCenterLoc, 0.50f, 0.60f);
        glUniform2f(gViewportLoc, static_cast<float>(width), static_cast<float>(height));

        static const auto start = std::chrono::steady_clock::now();
        const float seconds = std::chrono::duration<float>(
            std::chrono::steady_clock::now() - start).count();
        glUniform1f(gTimeLoc, seconds);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        const uint64_t generation = GetFo3LoadingGenerationQ1700();
        if (generation != gLoggedGeneration) {
            gLoggedGeneration = generation;
            __android_log_print(ANDROID_LOG_INFO, TAG,
                                "Q16.2 LOADING COMFORT: generation=%llu panelScale=(%.3f %.3f) center=(0.50 0.60) stereo=zeroDisparity spinner=loadinganim01-style HUDMainRGB=(26,255,128)",
                                static_cast<unsigned long long>(generation), sx, sy);
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
    gLoggedGeneration = ~uint64_t{0};
}

} // namespace fo3loadq1720

inline void RenderFo3LoadingScreenQ1720(GLuint framebuffer, GLsizei width, GLsizei height) {
    fo3loadq1720::Render(framebuffer, width, height);
}

inline void ShutdownFo3LoadingScreenQ1720() {
    fo3loadq1720::Shutdown();
    ShutdownFo3LoadingScreenQ1700();
}
