#pragma once

#include "fo3-loading-screen-q1700.h"

#include <GLES3/gl3.h>
#include <android/log.h>

#include <algorithm>
#include <chrono>
#include <cstdint>

// Q16.6: Fallout 3 loading-menu presentation reconstructed from the user's
// Fallout - Misc.bsa / menus/loading_menu.xml.
//
// Authored vanilla facts used here:
//   - loading menu coordinate system is 1280x960 (640,480 centre => 4:3)
//   - LoadingMenu menufade = 0.75 seconds
//   - loading_world_image fills the authored screen
//   - loading_pinwheel uses Interface\Circular Loading\loading01.nif
//   - loading_pinwheel animation = Idle
//   - the visible wheel footprint is 54x54 pixels
//   - the wheel is centred horizontally near the bottom of the authored canvas
//
// FalloutQuest keeps the menu at zero stereo disparity for VR comfort. The
// Bethesda LSCR DDS remains user-owned and is still selected by Q16.0 from the
// ESM/Textures BSA. The circular NIF is represented procedurally for now so the
// loading menu does not depend on animated-UI-NIF playback; its authored size,
// location and timing are preserved.

namespace fo3loadq1760 {

constexpr const char* TAG = "FalloutQuest";

inline GLuint gProgram = 0u;
inline GLuint gVao = 0u;
inline GLint gImageLoc = -1;
inline GLint gHaveImageLoc = -1;
inline GLint gPanelScaleLoc = -1;
inline GLint gPanelCenterLoc = -1;
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
                            "Q16.6 LOADING shader compile failed: %s", log);
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
        uniform vec2 uPanelScale;
        uniform vec2 uPanelCenter;
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
            // Vanilla LoadingMenu background is effectively black around the
            // authored full-screen world image. FalloutQuest maps the 1280x960
            // UI canvas into a comfortable zero-disparity 4:3 VR panel.
            vec3 colour = vec3(0.0);
            vec2 panel = (vUv - uPanelCenter) / max(uPanelScale, vec2(0.0001)) + vec2(0.5);
            bool inPanel = panel.x >= 0.0 && panel.x <= 1.0 &&
                           panel.y >= 0.0 && panel.y <= 1.0;

            if (inPanel && uHaveImage > 0.5) {
                // loading_world_image is explicitly screen width x screen height
                // in loading_menu.xml, so do not add Q16.2's inner image card.
                vec4 image = texture(uImage, panel);
                colour = image.rgb * image.a;
            }

            // loading_menu.xml's loading_pinwheel is a 54x54 NIF centred near
            // the bottom of the 1280x960 authored screen. XML offsets place the
            // visible wheel centre at roughly y=871 from the top, i.e. 89 px
            // above the bottom (0.093 of panel height).
            vec2 spinnerCenter = vec2(
                uPanelCenter.x,
                uPanelCenter.y - uPanelScale.y * 0.5 + uPanelScale.y * 0.093);

            // Authored wheel width = 54 / 1280 of the 4:3 canvas. Convert that
            // into eye UV while correcting x by framebuffer aspect so it remains
            // physically circular rather than becoming an ellipse.
            float viewAspect = uViewport.x / max(uViewport.y, 1.0);
            float radius = uPanelScale.x * (27.0 / 1280.0) * viewAspect;
            vec2 p = vUv - spinnerCenter;
            p.x *= viewAspect;

            const float PI = 3.14159265359;
            const float TAU = 6.28318530718;
            float core = 0.0;
            float glow = 0.0;

            // Procedural fallback for Interface\Circular Loading\loading01.nif:
            // twelve short radial blades with a moving intensity head/trail. This
            // is intentionally a pinwheel, not Q16.2's incorrect clock face.
            for (int i = 0; i < 12; ++i) {
                float fi = float(i);
                float a = fi * (TAU / 12.0) + PI * 0.5;
                vec2 dir = vec2(cos(a), sin(a));
                float d = sdSegment(p, dir * radius * 0.48, dir * radius * 0.94);

                float phase = mod(fi / 12.0 + uTime * 0.72, 1.0);
                float intensity = 0.16 + 0.84 * pow(phase, 3.0);
                core = max(core, lineMask(d, radius * 0.075, radius * 0.055) * intensity);
                glow = max(glow, lineMask(d, radius * 0.19, radius * 0.11) * intensity);
            }

            // Small hub visible in the original circular loader footprint.
            float hub = length(p);
            core = max(core, 1.0 - smoothstep(radius * 0.12, radius * 0.20, hub));
            glow = max(glow, 1.0 - smoothstep(radius * 0.14, radius * 0.42, hub));

            vec3 hud = vec3(26.0 / 255.0, 1.0, 128.0 / 255.0);
            colour += hud * glow * 0.16;
            colour = mix(colour, hud, clamp(core, 0.0, 1.0));

            // loading_menu.xml: <menufade> 0.75 </menufade>
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
                            "Q16.6 LOADING program link failed: %s", log);
        glDeleteProgram(gProgram);
        gProgram = 0u;
        return false;
    }

    glGenVertexArrays(1, &gVao);
    gImageLoc = glGetUniformLocation(gProgram, "uImage");
    gHaveImageLoc = glGetUniformLocation(gProgram, "uHaveImage");
    gPanelScaleLoc = glGetUniformLocation(gProgram, "uPanelScale");
    gPanelCenterLoc = glGetUniformLocation(gProgram, "uPanelCenter");
    gViewportLoc = glGetUniformLocation(gProgram, "uViewport");
    gTimeLoc = glGetUniformLocation(gProgram, "uTime");
    gFadeLoc = glGetUniformLocation(gProgram, "uFade");

    return gImageLoc >= 0 && gHaveImageLoc >= 0 && gPanelScaleLoc >= 0 &&
           gPanelCenterLoc >= 0 && gViewportLoc >= 0 && gTimeLoc >= 0 &&
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

    // Keep Q16.0's proven ESM location selection and DDS upload. Unlike Q16.2,
    // a DDS miss no longer suppresses the menu/pinwheel itself.
    const bool haveImage = fo3loadq1700::LoadForCurrentGeneration();
    if (!EnsureProgram()) {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glViewport(0, 0, width, height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

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

    glUseProgram(gProgram);
    glBindVertexArray(gVao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, haveImage ? fo3loadq1700::gTexture : 0u);
    glUniform1i(gImageLoc, 0);
    glUniform1f(gHaveImageLoc, haveImage ? 1.0f : 0.0f);

    const float viewAspect = static_cast<float>(width) /
                             static_cast<float>(std::max<GLsizei>(height, 1));
    constexpr float authoredAspect = 4.0f / 3.0f;

    // A comfortable virtual 4:3 Fallout screen. Zero disparity keeps the panel
    // optically distant while retaining substantially more of the vanilla full-
    // screen composition than Q16.2's 62% artwork card.
    float panelW = 0.86f;
    float panelH = panelW * viewAspect / authoredAspect;
    if (panelH > 0.82f) {
        panelH = 0.82f;
        panelW = panelH * authoredAspect / std::max(viewAspect, 0.001f);
    }
    panelW = std::clamp(panelW, 0.20f, 0.94f);
    panelH = std::clamp(panelH, 0.20f, 0.86f);

    glUniform2f(gPanelScaleLoc, panelW, panelH);
    glUniform2f(gPanelCenterLoc, 0.50f, 0.50f);
    glUniform2f(gViewportLoc, static_cast<float>(width), static_cast<float>(height));

    const float elapsed = std::chrono::duration<float>(now - gGenerationStart).count();
    glUniform1f(gTimeLoc, elapsed);
    glUniform1f(gFadeLoc, std::clamp(elapsed / 0.75f, 0.0f, 1.0f));
    glDrawArrays(GL_TRIANGLES, 0, 3);

    if (generation != gLoggedGeneration) {
        gLoggedGeneration = generation;
        __android_log_print(
            ANDROID_LOG_INFO, TAG,
            "Q16.6 VANILLA LOADING MENU: generation=%llu canvas=1280x960 panel=(%.3f %.3f) menufade=0.75 worldImage=%d pinwheel=Interface\\Circular Loading\\loading01.nif authoredSize=54x54 animation=Idle stereo=zeroDisparity",
            static_cast<unsigned long long>(generation), panelW, panelH,
            haveImage ? 1 : 0);
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

} // namespace fo3loadq1760

inline void RenderFo3LoadingScreenQ1760(GLuint framebuffer, GLsizei width, GLsizei height) {
    fo3loadq1760::Render(framebuffer, width, height);
}

inline void ShutdownFo3LoadingScreenQ1760() {
    fo3loadq1760::Shutdown();
    ShutdownFo3LoadingScreenQ1700();
}
