#pragma once

#include "rendering/environment/fo3-environment.h"
#include "fo3-texture-bsa.h"

#include <GLES3/gl3.h>
#include <android/log.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <time.h>
#include <vector>

namespace fo3sky {

struct CloudLayer {
    std::string texturePath;
    float color[4]{1.0f, 1.0f, 1.0f, 0.0f};
    float speed = 0.0f;
    GLuint texture = 0u;
    int width = 0;
    int height = 0;
    bool loaded = false;
};

struct WeatherSky {
    bool valid = false;
    uint32_t weatherFormId = 0u;
    std::string editorId;
    std::array<CloudLayer, 4> clouds;
    float sunColor[3]{1.0f, 1.0f, 1.0f};
    float sunGlare = 0.0f;
};

inline WeatherSky gWeatherSky;
inline GLuint gProgram = 0u;
inline GLint gMvpLoc = -1;
inline GLint gModeLoc = -1;
inline GLint gCloudTexLoc = -1;
inline GLint gCloudColorLoc = -1;
inline GLint gCloudOffsetLoc = -1;
inline GLint gSunDirectionLoc = -1;
inline GLint gSunColorLoc = -1;
inline GLint gSunGlareLoc = -1;
inline bool gLoggedGpu = false;

inline double MonotonicSeconds() {
    timespec ts{};
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0.0;
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) * 1.0e-9;
}

inline void DeleteWeatherTextures() {
    for (CloudLayer& layer : gWeatherSky.clouds) {
        if (layer.texture != 0u) glDeleteTextures(1, &layer.texture);
        layer.texture = 0u;
        layer.loaded = false;
    }
}

inline void ReadDayRgba(const uint8_t* data, uint32_t size,
                             uint32_t baseOffset, float out[4]) {
    // Time-of-day colour: sunrise RGBA, day RGBA, sunset RGBA, night RGBA.
    const uint32_t dayOffset = baseOffset + 4u;
    if (!data || dayOffset + 4u > size) return;
    out[0] = static_cast<float>(data[dayOffset + 0u]) / 255.0f;
    out[1] = static_cast<float>(data[dayOffset + 1u]) / 255.0f;
    out[2] = static_cast<float>(data[dayOffset + 2u]) / 255.0f;
    out[3] = static_cast<float>(data[dayOffset + 3u]) / 255.0f;
}

inline bool ParseWeatherSky(uint32_t weatherFormId, WeatherSky& out) {
    using namespace fo3env;
    out = {};
    out.weatherFormId = weatherFormId;
    if (weatherFormId == 0u) return false;

    std::vector<uint8_t> payload;
    if (!FindRecord("WTHR", weatherFormId, payload)) return false;

    fo3esm::WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "EDID", 4u) == 0 && out.editorId.empty()) {
            out.editorId = fo3esm::ZString(bytes, size);
        } else if (std::memcmp(type, "DNAM", 4u) == 0) {
            out.clouds[0].texturePath = fo3esm::ZString(bytes, size);
        } else if (std::memcmp(type, "CNAM", 4u) == 0) {
            out.clouds[1].texturePath = fo3esm::ZString(bytes, size);
        } else if (std::memcmp(type, "ANAM", 4u) == 0) {
            out.clouds[2].texturePath = fo3esm::ZString(bytes, size);
        } else if (std::memcmp(type, "BNAM", 4u) == 0) {
            out.clouds[3].texturePath = fo3esm::ZString(bytes, size);
        } else if (std::memcmp(type, "ONAM", 4u) == 0 && size >= 4u) {
            for (int i = 0; i < 4; ++i) {
                out.clouds[i].speed = static_cast<float>(bytes[i]) / 2550.0f;
            }
        } else if (std::memcmp(type, "PNAM", 4u) == 0 && size >= 64u) {
            for (uint32_t i = 0u; i < 4u; ++i) {
                ReadDayRgba(bytes, size, i * 16u, out.clouds[i].color);
            }
        } else if (std::memcmp(type, "NAM0", 4u) == 0 && size >= 160u) {
            // Category 5 is Sun; select Day within the four time-of-day colours.
            float rgba[4]{1.0f, 1.0f, 1.0f, 1.0f};
            ReadDayRgba(bytes, size, 5u * 16u, rgba);
            out.sunColor[0] = rgba[0];
            out.sunColor[1] = rgba[1];
            out.sunColor[2] = rgba[2];
        } else if (std::memcmp(type, "DATA", 4u) == 0 && size >= 5u) {
            out.sunGlare = static_cast<float>(bytes[4]) / 255.0f;
        }
    });

    out.valid = true;
    return true;
}

inline bool UploadCloud(CloudLayer& layer, int index) {
    // WTHR PNAM's fourth byte is not a cloud-opacity gate. Stock
    // WastelandClear authors useful cloud RGB with A=0, so treating that byte
    // as opacity discards the entire authored layer. Visibility comes from the
    // cloud DDS alpha; PNAM contributes the time-of-day RGB modulation only.
    if (layer.texturePath.empty()) return false;
    Fo3RgbaTexture decoded;
    if (!LoadFalloutTextureRgba(layer.texturePath, decoded) ||
        decoded.width <= 0 || decoded.height <= 0 || decoded.rgba.empty()) {
        __android_log_print(ANDROID_LOG_WARN, fo3env::TAG,
                            "Q13.3 CLOUD DDS MISS: layer=%d path=%s",
                            index, layer.texturePath.c_str());
        return false;
    }

    glGenTextures(1, &layer.texture);
    glBindTexture(GL_TEXTURE_2D, layer.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 decoded.width, decoded.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, decoded.rgba.data());
    layer.width = decoded.width;
    layer.height = decoded.height;
    layer.loaded = true;
    __android_log_print(ANDROID_LOG_INFO, fo3env::TAG,
                        "Q13.3 CLOUD DDS READY: layer=%d path=%s resolved=%s size=%dx%d format=%s alpha=%.3f speed=%.5f",
                        index, layer.texturePath.c_str(), decoded.sourcePath.c_str(),
                        decoded.width, decoded.height, decoded.format.c_str(),
                        layer.color[3], layer.speed);
    return true;
}

inline bool EnsureWeather() {
    const Fo3Environment& env = GetFo3Environment();
    if (!env.valid || env.weatherFormId == 0u) return false;
    if (gWeatherSky.valid && gWeatherSky.weatherFormId == env.weatherFormId) {
        return true;
    }

    DeleteWeatherTextures();
    WeatherSky parsed;
    if (!ParseWeatherSky(env.weatherFormId, parsed)) {
        gWeatherSky = {};
        return false;
    }
    gWeatherSky = std::move(parsed);

    int authored = 0;
    int loaded = 0;
    for (int i = 0; i < 4; ++i) {
        if (!gWeatherSky.clouds[i].texturePath.empty()) ++authored;
        if (UploadCloud(gWeatherSky.clouds[i], i)) ++loaded;
    }
    glBindTexture(GL_TEXTURE_2D, 0u);

    __android_log_print(
        ANDROID_LOG_INFO, fo3env::TAG,
        "Q13.3 WEATHER SKY READY: weather=%08X EDID=%s cloudPaths=%d cloudLoaded=%d sunColor=(%.3f %.3f %.3f) sunGlare=%.3f mode=DAY",
        gWeatherSky.weatherFormId,
        gWeatherSky.editorId.empty() ? "<none>" : gWeatherSky.editorId.c_str(),
        authored, loaded,
        gWeatherSky.sunColor[0], gWeatherSky.sunColor[1],
        gWeatherSky.sunColor[2], gWeatherSky.sunGlare);
    return true;
}

inline GLuint CompileShader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[1024]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        __android_log_print(ANDROID_LOG_ERROR, fo3env::TAG,
                            "Q13.3 SKY shader compile failed: %s", log);
        glDeleteShader(shader);
        return 0u;
    }
    return shader;
}

inline bool EnsureProgram() {
    if (gProgram != 0u) return true;

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
        uniform int uMode;
        uniform sampler2D uCloud;
        uniform vec4 uCloudColor;
        uniform float uCloudOffset;
        uniform vec3 uSunDirection;
        uniform vec3 uSunColor;
        uniform float uSunGlare;
        out vec4 fragColor;

        const float PI = 3.14159265358979323846;

        void main() {
            vec3 d = normalize(vDirection);
            if (d.y < -0.03) discard;

            if (uMode == 0) {
                float alignment = max(dot(d, normalize(uSunDirection)), 0.0);
                float disc = smoothstep(0.9978, 0.99965, alignment);
                float halo = pow(alignment, 72.0) * clamp(uSunGlare, 0.0, 1.0);
                float alpha = clamp(disc + halo * 0.42, 0.0, 1.0);
                vec3 colour = uSunColor * (disc * 1.30 + halo * 0.45);
                if (alpha <= 0.001) discard;
                fragColor = vec4(colour, alpha);
                return;
            }

            float longitude = atan(d.z, d.x) / (2.0 * PI) + 0.5;
            float latitude = acos(clamp(d.y, -1.0, 1.0)) / PI;
            vec2 uv = vec2(longitude + uCloudOffset, latitude * 1.65);
            vec4 texel = texture(uCloud, uv);
            float horizonFade = smoothstep(-0.02, 0.18, d.y);
            float alpha = texel.a * horizonFade;
            if (alpha <= 0.002) discard;
            fragColor = vec4(texel.rgb * uCloudColor.rgb, alpha);
        }
    )";

    const GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexSource);
    const GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);
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
        __android_log_print(ANDROID_LOG_ERROR, fo3env::TAG,
                            "Q13.3 SKY program link failed: %s", log);
        glDeleteProgram(gProgram);
        gProgram = 0u;
        return false;
    }

    gMvpLoc = glGetUniformLocation(gProgram, "uMvp");
    gModeLoc = glGetUniformLocation(gProgram, "uMode");
    gCloudTexLoc = glGetUniformLocation(gProgram, "uCloud");
    gCloudColorLoc = glGetUniformLocation(gProgram, "uCloudColor");
    gCloudOffsetLoc = glGetUniformLocation(gProgram, "uCloudOffset");
    gSunDirectionLoc = glGetUniformLocation(gProgram, "uSunDirection");
    gSunColorLoc = glGetUniformLocation(gProgram, "uSunColor");
    gSunGlareLoc = glGetUniformLocation(gProgram, "uSunGlare");
    return gMvpLoc >= 0 && gModeLoc >= 0 && gCloudTexLoc >= 0;
}

inline void RenderLayers(const float* mvp16) {
    if (!mvp16 || !EnsureWeather() || !EnsureProgram()) return;
    if (!fo3env::EnsureSkyGpu()) return;

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
    GLint oldSrcRgb = GL_ONE, oldDstRgb = GL_ZERO, oldSrcAlpha = GL_ONE, oldDstAlpha = GL_ZERO;
    glGetIntegerv(GL_BLEND_SRC_RGB, &oldSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &oldDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &oldSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &oldDstAlpha);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glUseProgram(gProgram);
    glUniformMatrix4fv(gMvpLoc, 1, GL_FALSE, mvp16);
    glBindVertexArray(fo3env::skyVao);

    // Sun first. Clouds draw afterwards so their authored alpha can obscure it.
    const Fo3Environment& env = GetFo3Environment();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glUniform1i(gModeLoc, 0);
    glUniform3fv(gSunDirectionLoc, 1, env.sunDirection);
    glUniform3fv(gSunColorLoc, 1, gWeatherSky.sunColor);
    glUniform1f(gSunGlareLoc, gWeatherSky.sunGlare);
    glDrawArrays(GL_TRIANGLES, 0, fo3env::skyVertexCount);

    // Four authored WTHR cloud layers. Q14 updates their RGB from the active
    // time-of-day endpoints; DDS alpha supplies the actual cloud coverage mask.
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUniform1i(gModeLoc, 1);
    glUniform1i(gCloudTexLoc, 0);
    const double now = MonotonicSeconds();
    int rendered = 0;
    for (int i = 0; i < 4; ++i) {
        const CloudLayer& layer = gWeatherSky.clouds[i];
        if (!layer.loaded || layer.texture == 0u) continue;
        const float offset = static_cast<float>(std::fmod(now * layer.speed, 1.0));
        glBindTexture(GL_TEXTURE_2D, layer.texture);
        glUniform4fv(gCloudColorLoc, 1, layer.color);
        glUniform1f(gCloudOffsetLoc, offset);
        glDrawArrays(GL_TRIANGLES, 0, fo3env::skyVertexCount);
        ++rendered;
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

    if (!gLoggedGpu) {
        gLoggedGpu = true;
        __android_log_print(ANDROID_LOG_INFO, fo3env::TAG,
                            "Q13.3 WEATHER SKY GPU: cloudLayersRendered=%d sunDisc=1 authoredDDS=1 stereoSharedState=1",
                            rendered);
    }
}

} // namespace fo3sky

inline void RenderFo3WeatherSky(const float* mvp16) {
    // Keep Q10.0's proven WTHR gradient/fog-colour base, then add the authored
    // weather texture layers and sun treatment without changing scene geometry.
    RenderFo3EnvironmentSky(mvp16);
    fo3sky::RenderLayers(mvp16);
}
