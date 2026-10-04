#pragma once

// Fallout 3's authored HUDMainMenu/Info interaction widget.
// No Bethesda assets are embedded here. The runtime reads the user's own
// Fallout - Textures.bsa and interprets the exact vanilla FNT/TAI/TEX/DDS data.

#include "fo3-texture-bsa.h"
#include "fo3-asset-store.h"
#include "fo3-font-layout.h"

#include <GLES3/gl3.h>
#include <android/log.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace fo3hudassets {

constexpr const char* kTag = "FalloutQuest";
constexpr size_t kMaxRawBytes = 16u * 1024u * 1024u;

inline uint32_t ReadU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}
inline float ReadF32(const uint8_t* p) {
    uint32_t raw = ReadU32(p);
    float value = 0.0f;
    std::memcpy(&value, &raw, sizeof(value));
    return value;
}
inline bool LoadRaw(const std::string& request, std::vector<uint8_t>& out,
                    std::string* resolved = nullptr) {
    fo3assets::BsaFileInfo info;
    if (fo3assets::LoadTextureFile(request, out, &info, kMaxRawBytes)) {
        if (resolved) *resolved = info.path;
        __android_log_print(ANDROID_LOG_INFO, kTag,
                            "RAW UI ASSET: path=%s bytes=%zu compressed=%d",
                            info.path.c_str(), out.size(), info.compressed ? 1 : 0);
        return true;
    }
    __android_log_print(ANDROID_LOG_ERROR, kTag,
                        "RAW UI ASSET MISS: %s", request.c_str());
    return false;
}

using Glyph = fo3font::Glyph;
struct Font : fo3font::Metrics {
    std::string texPath;
    int textureWidth = 0, textureHeight = 0;
    std::vector<uint8_t> rgba;
};

inline bool ParseFont(Font& out) {
    std::vector<uint8_t> fnt;
    if (!LoadRaw("Textures\\Fonts\\Baked-in_Monofonto_Large.fnt", fnt)) return false;
    if (!fo3font::ParseFalloutFont(fnt, out)) return false;
    // This UI's original baked font has one atlas. Fail explicitly for a font
    // requiring multiple textures rather than sampling the wrong glyph atlas.
    if (out.textureCount != 1) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "FNT UNSUPPORTED: textures=%u", out.textureCount);
        return false;
    }
    const auto rawSpace = out.glyphs[0x20];
    fo3font::ApplyFalloutLoadSemantics(out);
    out.texPath = "Textures\\Fonts\\" + out.textureFiles[0] + ".tex";
    __android_log_print(ANDROID_LOG_INFO,kTag,
        "FNT METRICS: bytes=%zu record=0x38 table=0x128 count=256 baseLine=%.1f spaceRaw=(w=%.1f h=%.1f lead=%.1f spacing=%.1f top=%.1f) spaceRenderAdvance=%.1f",
        fnt.size(),out.baseLine,rawSpace.width,rawSpace.height,rawSpace.leadingEdge,rawSpace.spacing,rawSpace.topEdge,
        fo3font::GlyphRenderAdvance(out.glyphs[0x20]));
    std::vector<uint8_t> tex;
    if (!LoadRaw(out.texPath, tex) || tex.size() < 8u) return false;
    const uint32_t w = ReadU32(tex.data());
    const uint32_t h = ReadU32(tex.data() + 4);
    const uint64_t pixelBytes = static_cast<uint64_t>(w) * static_cast<uint64_t>(h) * 4u;
    if (w == 0u || h == 0u || w > 4096u || h > 4096u ||
        pixelBytes > tex.size() - 8u) return false;
    out.textureWidth = static_cast<int>(w);
    out.textureHeight = static_cast<int>(h);
    out.rgba.assign(tex.begin() + 8,
                    tex.begin() + 8 + static_cast<std::ptrdiff_t>(pixelBytes));

    for (unsigned char ch : std::string(" .-01AMaegty")) {
        const auto& g=out.glyphs[ch];
        __android_log_print(ANDROID_LOG_INFO,kTag,
            "FNT GLYPH: byte=%02X index=%u width=%.1f height=%.1f leading=%.1f spacing=%.1f top=%.1f advance=%.1f drawable=%d",
            ch,ch,g.width,g.height,g.leadingEdge,g.spacing,g.topEdge,fo3font::GlyphRenderAdvance(g),fo3font::Drawable(out,g));
    }
    return true;
}

struct TaiSprite {
    std::string atlasPath;
    float u = 0.0f;
    float v = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

inline std::string Trim(std::string s) {
    const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

inline bool ParseButtonTai(TaiSprite& out) {
    std::vector<uint8_t> raw;
    if (!LoadRaw("Textures\\Interface\\InterfaceShared.tai", raw)) return false;
    const std::string text(reinterpret_cast<const char*>(raw.data()), raw.size());
    std::istringstream lines(text);
    std::string line;
    const std::string alias = "glow_general_button_a.dds";
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind(alias, 0) != 0) continue;
        size_t pos = alias.size();
        while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos]))) ++pos;
        if (pos >= line.size()) return false;
        std::vector<std::string> fields;
        std::string rest = line.substr(pos);
        size_t start = 0;
        while (start <= rest.size()) {
            const size_t comma = rest.find(',', start);
            fields.push_back(Trim(rest.substr(start, comma == std::string::npos ? std::string::npos : comma - start)));
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
        if (fields.size() < 8u) return false;
        out.atlasPath = "Textures\\Interface\\" + fields[0];
        out.u = std::strtof(fields[3].c_str(), nullptr);
        out.v = std::strtof(fields[4].c_str(), nullptr);
        out.w = std::strtof(fields[6].c_str(), nullptr);
        out.h = std::strtof(fields[7].c_str(), nullptr);
        if (out.atlasPath.empty() || out.w <= 0.0f || out.h <= 0.0f) return false;
        __android_log_print(ANDROID_LOG_INFO, kTag,
                            "TAI READY: alias=%s atlas=%s uv=(%.6f %.6f %.6f %.6f)",
                            alias.c_str(), out.atlasPath.c_str(), out.u, out.v, out.w, out.h);
        return true;
    }
    return false;
}

struct Vertex {
    float x, y, z;
    float u, v;
};

inline GLuint CompileShader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[1024]{};
        GLsizei n = 0;
        glGetShaderInfoLog(shader, sizeof(log), &n, log);
        __android_log_print(ANDROID_LOG_ERROR, kTag,
                            "HUD SHADER FAILED: %.*s", static_cast<int>(n), log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

inline GLuint BuildProgram() {
    static const char* vs = R"(#version 300 es
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aUv;
uniform mat4 uMvp;
out vec2 vUv;
void main() {
    gl_Position = uMvp * vec4(aPos, 1.0);
    vUv = aUv;
})";
    static const char* fs = R"(#version 300 es
precision mediump float;
in vec2 vUv;
uniform sampler2D uTex;
uniform vec4 uTint;
out vec4 fragColor;
void main() {
    vec4 texel = texture(uTex, vUv);
    fragColor = vec4(texel.rgb * uTint.rgb, texel.a * uTint.a);
})";
    const GLuint v = CompileShader(GL_VERTEX_SHADER, vs);
    const GLuint f = CompileShader(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) {
        if (v) glDeleteShader(v);
        if (f) glDeleteShader(f);
        return 0;
    }
    const GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = GL_FALSE;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[1024]{};
        GLsizei n = 0;
        glGetProgramInfoLog(p, sizeof(log), &n, log);
        __android_log_print(ANDROID_LOG_ERROR, kTag,
                            "HUD PROGRAM FAILED: %.*s", static_cast<int>(n), log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

inline GLuint UploadTexture(int w, int h, const uint8_t* rgba) {
    if (w <= 0 || h <= 0 || !rgba) return 0;
    GLint oldUnpack = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &oldUnpack);
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glPixelStorei(GL_UNPACK_ALIGNMENT, oldUnpack);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

} // namespace fo3hudassets
