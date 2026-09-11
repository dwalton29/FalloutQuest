#pragma once

#include "fo3-time-of-day-q1400.h"
#include "fo3-texture-bsa.h"

#include <GLES3/gl3.h>
#include <android/log.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

// Q15.16: standalone PC-captured SKY/SKYTEX renderer.
//
// This intentionally does NOT mutate Q15.15's world/material/post path. It
// replaces only the old Q13.3 sky draw in the final eye source.
//
// Megaton PC apitrace ground truth:
//   SKY gradient: raw WTHR BlendColor RGB, PS Params.y = 1.55.
//   Sun:          textured SKYTEX, SRC_ALPHA/ONE additive, Params.y = 1.55.
//   Clouds:       textured SKYTEX, BlendColor alpha 1.0, Params.y = 1.55,
//                 TexCoordYOff animates V/Y rather than U/X.
//   Output:        D3DRS_SRGBWRITEENABLE = 0.

namespace fo3pcskyq1660 {

constexpr const char* TAG = "FalloutQuest";
constexpr float PI = 3.14159265358979323846f;
constexpr float PC_SKY_RGB_SCALE = 1.55f;

struct CloudGpuQ1660 {
    std::string path;
    GLuint texture = 0u;
    float speed = 0.0f;
    bool loaded = false;
};

inline std::array<CloudGpuQ1660, 4> gCloudsQ1660{};
inline uint32_t gWeatherFormQ1660 = 0u;
inline GLuint gSunTextureQ1660 = 0u;
inline bool gSunTextureAttemptedQ1660 = false;

inline GLuint gProgramQ1660 = 0u;
inline GLint gMvpLocQ1660 = -1;
inline GLint gModeLocQ1660 = -1;
inline GLint gTextureLocQ1660 = -1;
inline GLint gUpperLocQ1660 = -1;
inline GLint gLowerLocQ1660 = -1;
inline GLint gHorizonLocQ1660 = -1;
inline GLint gColourLocQ1660 = -1;
inline GLint gOffsetLocQ1660 = -1;
inline GLint gSunDirectionLocQ1660 = -1;
inline bool gLoggedQ1660 = false;

inline void DeleteCloudsQ1660() {
    for (CloudGpuQ1660& layer : gCloudsQ1660) {
        if (layer.texture != 0u) glDeleteTextures(1, &layer.texture);
        layer = {};
    }
    gWeatherFormQ1660 = 0u;
}

inline GLuint UploadTextureQ1660(const std::string& path, bool repeat) {
    if (path.empty()) return 0u;
    Fo3RgbaTexture decoded;
    if (!LoadFalloutTextureRgba(path, decoded) || decoded.width <= 0 ||
        decoded.height <= 0 || decoded.rgba.empty()) {
        __android_log_print(ANDROID_LOG_WARN, TAG,
                            "Q15.16 SKY DDS MISS: path=%s", path.c_str());
        return 0u;
    }

    GLuint texture = 0u;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                    repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                    repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 decoded.width, decoded.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, decoded.rgba.data());
    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "Q15.16 SKY DDS READY: path=%s resolved=%s size=%dx%d format=%s repeat=%d",
                        path.c_str(), decoded.sourcePath.c_str(), decoded.width,
                        decoded.height, decoded.format.c_str(), repeat ? 1 : 0);
    return texture;
}

inline bool EnsureSunTextureQ1660() {
    if (gSunTextureQ1660 != 0u) return true;
    if (gSunTextureAttemptedQ1660) return false;
    gSunTextureAttemptedQ1660 = true;

    // Fallout3.exe names Sky\\Sun.dds explicitly. Keep SunGlare as a data-driven
    // fallback only in case the BSA layout differs on this install.
    gSunTextureQ1660 = UploadTextureQ1660("Sky\\Sun.dds", false);
    if (gSunTextureQ1660 == 0u) {
        gSunTextureQ1660 = UploadTextureQ1660("Sky\\SunGlare.dds", false);
    }
    return gSunTextureQ1660 != 0u;
}

inline bool SyncWeatherTexturesQ1660() {
    const Fo3EnvironmentQ1000& env = GetFo3EnvironmentQ1000();
    if (!env.valid || env.weatherFormId == 0u) return false;
    if (gWeatherFormQ1660 == env.weatherFormId) return true;

    DeleteCloudsQ1660();
    fo3skyq1330::WeatherSkyQ1330 parsed;
    if (!fo3skyq1330::ParseWeatherSkyQ1330(env.weatherFormId, parsed)) return false;

    for (int i = 0; i < 4; ++i) {
        gCloudsQ1660[i].path = parsed.clouds[i].texturePath;
        gCloudsQ1660[i].speed = parsed.clouds[i].speed;
        if (!gCloudsQ1660[i].path.empty()) {
            gCloudsQ1660[i].texture = UploadTextureQ1660(gCloudsQ1660[i].path, true);
            gCloudsQ1660[i].loaded = gCloudsQ1660[i].texture != 0u;
        }
    }
    gWeatherFormQ1660 = env.weatherFormId;
    (void)EnsureSunTextureQ1660();
    glBindTexture(GL_TEXTURE_2D, 0u);
    return true;
}

inline GLuint CompileQ1660(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[2048]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "Q15.16 SKY shader compile failed: %s", log);
        glDeleteShader(shader);
        return 0u;
    }
    return shader;
}

inline bool EnsureProgramQ1660() {
    if (gProgramQ1660 != 0u) return true;

    static const char* vsSource = R"(
        #version 300 es
        layout(location = 0) in vec3 aPosition;
        uniform mat4 uMvp;
        out vec3 vDirection;
        void main() {
            vDirection = aPosition;
            gl_Position = uMvp * vec4(aPosition, 1.0);
        }
    )";

    static const char* fsSource = R"(
        #version 300 es
        precision highp float;
        in vec3 vDirection;
        uniform int uMode;
        uniform sampler2D uTexture;
        uniform vec3 uSkyUpper;
        uniform vec3 uSkyLower;
        uniform vec3 uHorizon;
        uniform vec4 uColour;
        uniform float uOffset;
        uniform vec3 uSunDirection;
        out vec4 fragColor;

        const float PI = 3.14159265358979323846;
        const float PC_SCALE = 1.55;

        void main() {
            vec3 d = normalize(vDirection);

            if (uMode == 0) {
                // Q10's dome is retained as geometry, but the colour domain and
                // 1.55 output scale now match the captured PC SKY shader.
                float y = clamp(d.y, 0.0, 1.0);
                vec3 lower = mix(uHorizon, uSkyLower,
                                 smoothstep(0.0, 0.32, y));
                vec3 colour = mix(lower, uSkyUpper,
                                  smoothstep(0.28, 0.95, y));
                fragColor = vec4(colour * PC_SCALE, 1.0);
                return;
            }

            if (d.y < -0.03) discard;

            if (uMode == 1) {
                float longitude = atan(d.z, d.x) / (2.0 * PI) + 0.5;
                float latitude = acos(clamp(d.y, -1.0, 1.0)) / PI;
                // Captured SKYTEX VS: TexCoordYOff is added to V/Y.
                vec2 uv = vec2(longitude, latitude * 1.65 + uOffset);
                vec4 texel = texture(uTexture, uv);
                float horizonFade = smoothstep(-0.02, 0.18, d.y);
                float alpha = texel.a * horizonFade;
                if (alpha <= 0.002) discard;
                fragColor = vec4(texel.rgb * uColour.rgb * PC_SCALE, alpha);
                return;
            }

            // Textured sun. PC uses a four-vertex additive quad. We project the
            // same authored texture onto the existing stereo dome so the asset,
            // colour domain and blend equation are correct without inventing a
            // second eye-dependent billboard transform.
            vec3 sunDir = normalize(uSunDirection);
            float alignment = dot(d, sunDir);
            if (alignment <= 0.20) discard;
            vec3 referenceUp = abs(sunDir.y) > 0.96
                ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
            vec3 right = normalize(cross(referenceUp, sunDir));
            vec3 up = normalize(cross(sunDir, right));
            const float halfSpan = 0.78;
            vec2 uv = vec2(0.5) + vec2(dot(d, right), dot(d, up)) /
                                   (2.0 * halfSpan);
            if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) discard;
            vec4 texel = texture(uTexture, uv);
            if (texel.a <= 0.001) discard;
            fragColor = vec4(texel.rgb * uColour.rgb * PC_SCALE, texel.a);
        }
    )";

    const GLuint vs = CompileQ1660(GL_VERTEX_SHADER, vsSource);
    const GLuint fs = CompileQ1660(GL_FRAGMENT_SHADER, fsSource);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    gProgramQ1660 = glCreateProgram();
    glAttachShader(gProgramQ1660, vs);
    glAttachShader(gProgramQ1660, fs);
    glLinkProgram(gProgramQ1660);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint linked = GL_FALSE;
    glGetProgramiv(gProgramQ1660, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char log[2048]{};
        glGetProgramInfoLog(gProgramQ1660, sizeof(log), nullptr, log);
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "Q15.16 SKY program link failed: %s", log);
        glDeleteProgram(gProgramQ1660);
        gProgramQ1660 = 0u;
        return false;
    }

    gMvpLocQ1660 = glGetUniformLocation(gProgramQ1660, "uMvp");
    gModeLocQ1660 = glGetUniformLocation(gProgramQ1660, "uMode");
    gTextureLocQ1660 = glGetUniformLocation(gProgramQ1660, "uTexture");
    gUpperLocQ1660 = glGetUniformLocation(gProgramQ1660, "uSkyUpper");
    gLowerLocQ1660 = glGetUniformLocation(gProgramQ1660, "uSkyLower");
    gHorizonLocQ1660 = glGetUniformLocation(gProgramQ1660, "uHorizon");
    gColourLocQ1660 = glGetUniformLocation(gProgramQ1660, "uColour");
    gOffsetLocQ1660 = glGetUniformLocation(gProgramQ1660, "uOffset");
    gSunDirectionLocQ1660 = glGetUniformLocation(gProgramQ1660, "uSunDirection");
    return gMvpLocQ1660 >= 0 && gModeLocQ1660 >= 0 &&
           gTextureLocQ1660 >= 0 && gColourLocQ1660 >= 0;
}

inline bool RuntimeWeightsQ1660(fo3todq1400::TimeWeightsQ1400& weights) {
    using namespace fo3todq1400;
    const Fo3EnvironmentQ1000& env = GetFo3EnvironmentQ1000();
    if (!gRuntime.ready || !env.valid ||
        gRuntime.weatherFormId != env.weatherFormId || !gRuntime.weather.haveNam0) {
        return false;
    }
    weights = WeightsForHour(gTestHour, gRuntime.climate);
    return true;
}

inline void SampleRawNam0Q1660(int cls,
                               const fo3todq1400::TimeWeightsQ1400& weights,
                               float out[3]) {
    using namespace fo3todq1400;
    out[0] = out[1] = out[2] = 0.0f;
    if (!gRuntime.ready || cls < 0 || cls >= 10) return;
    for (int tod = 0; tod < 4; ++tod) {
        for (int c = 0; c < 3; ++c) {
            out[c] += gRuntime.weather.encoded[cls][tod][c] * weights.w[tod];
        }
    }
}

inline void SampleRawCloudQ1660(int layer,
                                const fo3todq1400::TimeWeightsQ1400& weights,
                                float out[3]) {
    using namespace fo3todq1400;
    out[0] = out[1] = out[2] = 0.0f;
    if (!gRuntime.ready || !gRuntime.weather.havePnam || layer < 0 || layer >= 4) return;
    for (int tod = 0; tod < 4; ++tod) {
        for (int c = 0; c < 3; ++c) {
            out[c] += gRuntime.weather.cloudEncoded[layer][tod][c] * weights.w[tod];
        }
    }
}

inline void RenderQ1660(const float* mvp16) {
    if (!mvp16 || !fo3envq1000::EnsureSkyGpu() ||
        !EnsureProgramQ1660() || !SyncWeatherTexturesQ1660()) return;

    const Fo3EnvironmentQ1000& env = GetFo3EnvironmentQ1000();
    fo3todq1400::TimeWeightsQ1400 weights;
    const bool haveRuntime = RuntimeWeightsQ1660(weights);

    float skyUpper[3]{env.skyUpper[0], env.skyUpper[1], env.skyUpper[2]};
    float skyLower[3]{env.skyLower[0], env.skyLower[1], env.skyLower[2]};
    float horizon[3]{env.horizon[0], env.horizon[1], env.horizon[2]};
    float sun[3]{env.sun[0], env.sun[1], env.sun[2]};
    if (haveRuntime) {
        SampleRawNam0Q1660(0, weights, skyUpper);
        SampleRawNam0Q1660(7, weights, skyLower);
        SampleRawNam0Q1660(8, weights, horizon);
        SampleRawNam0Q1660(5, weights, sun);
    }

    GLint previousProgram = 0, previousVao = 0, previousActiveTexture = 0;
    GLint previousTexture0 = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture0);
    const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLboolean previousDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
    GLint oldSrcRgb = GL_ONE, oldDstRgb = GL_ZERO;
    GLint oldSrcAlpha = GL_ONE, oldDstAlpha = GL_ZERO;
    glGetIntegerv(GL_BLEND_SRC_RGB, &oldSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &oldDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &oldSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &oldDstAlpha);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glUseProgram(gProgramQ1660);
    glBindVertexArray(fo3envq1000::skyVao);
    glUniformMatrix4fv(gMvpLocQ1660, 1, GL_FALSE, mvp16);
    glUniform1i(gTextureLocQ1660, 0);

    // SKY gradient. Alpha is 1, so PC's SRC_ALPHA/INVSRC_ALPHA blend resolves
    // to the same replacement colour; draw opaque for clarity.
    glDisable(GL_BLEND);
    glUniform1i(gModeLocQ1660, 0);
    glUniform3fv(gUpperLocQ1660, 1, skyUpper);
    glUniform3fv(gLowerLocQ1660, 1, skyLower);
    glUniform3fv(gHorizonLocQ1660, 1, horizon);
    glDrawArrays(GL_TRIANGLES, 0, fo3envq1000::skyVertexCount);

    // Sun: exact PC blend equation SRC_ALPHA, ONE.
    if (EnsureSunTextureQ1660()) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glBindTexture(GL_TEXTURE_2D, gSunTextureQ1660);
        glUniform1i(gModeLocQ1660, 2);
        glUniform4f(gColourLocQ1660, sun[0], sun[1], sun[2], 1.0f);
        glUniform3fv(gSunDirectionLocQ1660, 1, env.sunDirection);
        glDrawArrays(GL_TRIANGLES, 0, fo3envq1000::skyVertexCount);
    }

    // Clouds: PC uses SRC_ALPHA, INVSRC_ALPHA and BlendColor alpha 1.0 for the
    // visible Wasteland horizon layer. PNAM alpha is deliberately ignored.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUniform1i(gModeLocQ1660, 1);
    const double seconds = fo3skyq1330::MonotonicSecondsQ1330();
    int renderedClouds = 0;
    for (int layer = 0; layer < 4; ++layer) {
        if (!gCloudsQ1660[layer].loaded || gCloudsQ1660[layer].texture == 0u) continue;
        float colour[3]{0.0f, 0.0f, 0.0f};
        if (haveRuntime) {
            SampleRawCloudQ1660(layer, weights, colour);
        } else {
            fo3skyq1330::WeatherSkyQ1330 parsed;
            if (fo3skyq1330::ParseWeatherSkyQ1330(env.weatherFormId, parsed)) {
                for (int c = 0; c < 3; ++c) colour[c] = parsed.clouds[layer].color[c];
            }
        }
        // Alpha placeholder layers in clear weather have black BlendColor and
        // therefore contribute nothing. Avoid a pointless black alpha blend.
        if (colour[0] + colour[1] + colour[2] <= 0.001f) continue;

        const float offset = static_cast<float>(
            std::fmod(seconds * static_cast<double>(gCloudsQ1660[layer].speed), 1.0));
        glBindTexture(GL_TEXTURE_2D, gCloudsQ1660[layer].texture);
        glUniform4f(gColourLocQ1660, colour[0], colour[1], colour[2], 1.0f);
        glUniform1f(gOffsetLocQ1660, offset);
        glDrawArrays(GL_TRIANGLES, 0, fo3envq1000::skyVertexCount);
        ++renderedClouds;
    }

    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture0));
    glBindVertexArray(static_cast<GLuint>(previousVao));
    glUseProgram(static_cast<GLuint>(previousProgram));
    glDepthMask(previousDepthMask);
    if (depthWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (cullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    glBlendFuncSeparate(static_cast<GLenum>(oldSrcRgb), static_cast<GLenum>(oldDstRgb),
                        static_cast<GLenum>(oldSrcAlpha), static_cast<GLenum>(oldDstAlpha));
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));

    if (!gLoggedQ1660) {
        gLoggedQ1660 = true;
        __android_log_print(
            ANDROID_LOG_INFO, TAG,
            "Q15.16 PC SKY: weather=%08X rawWthr=1 rgbScale=1.55 cloudVScroll=1 pnamAlphaVisibility=0 cloudsRendered=%d texturedSun=%d q1515WorldUntouched=1",
            env.weatherFormId, renderedClouds, gSunTextureQ1660 != 0u ? 1 : 0);
    }
}

} // namespace fo3pcskyq1660

inline void RenderFo3PcSkyQ1660(const float* mvp16) {
    fo3pcskyq1660::RenderQ1660(mvp16);
}
