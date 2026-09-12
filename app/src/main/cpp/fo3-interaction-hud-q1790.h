#pragma once

// Q16.9: Fallout 3's authored HUDMainMenu/Info interaction widget.
// No Bethesda assets are embedded here. The runtime reads the user's own
// Fallout - Textures.bsa and interprets the exact vanilla FNT/TAI/TEX/DDS data.

#include "fo3-texture-bsa.h"

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
#include <zlib.h>

namespace fo3q1790 {

constexpr const char* kTag = "FalloutQuest";
constexpr const char* kTextureBsaPaths[] = {
    "/data/user/0/com.falloutquest.app/files/Fallout3/Data/Fallout - Textures.bsa",
    "/data/user/0/com.falloutquest.app/files/Fallout3/Data/textures.bsa",
};
constexpr uint32_t kBsaVersion = 104u;
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
inline bool ReadExact(FILE* f, void* dst, size_t bytes) {
    return std::fread(dst, 1, bytes, f) == bytes;
}
inline std::string Normalize(std::string s) {
    for (char& c : s) {
        if (c == '/') c = '\\';
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    while (!s.empty() && (s.front() == '\\' || s.front() == '/')) s.erase(s.begin());
    return s;
}
inline std::string StripTextures(std::string s) {
    s = Normalize(std::move(s));
    const std::string prefix = "textures\\";
    if (s.rfind(prefix, 0) == 0) s.erase(0, prefix.size());
    return s;
}
inline bool ReadCString(FILE* f, std::string& out) {
    out.clear();
    for (size_t i = 0; i < 8192u; ++i) {
        const int ch = std::fgetc(f);
        if (ch == EOF) return false;
        if (ch == 0) return true;
        out.push_back(static_cast<char>(ch));
    }
    return false;
}

struct BsaEntry {
    uint32_t storedBytes = 0;
    uint32_t offset = 0;
    bool compressionToggle = false;
};
struct PendingEntry {
    std::string folder;
    BsaEntry entry;
};

inline bool FindRawEntry(FILE* f, const std::string& request,
                         BsaEntry& out, uint32_t& outFlags, std::string& outPath) {
    uint8_t hdr[36]{};
    if (!ReadExact(f, hdr, sizeof(hdr)) || std::memcmp(hdr, "BSA\0", 4) != 0) return false;
    const uint32_t version = ReadU32(hdr + 4);
    const uint32_t foldersOffset = ReadU32(hdr + 8);
    const uint32_t archiveFlags = ReadU32(hdr + 12);
    const uint32_t folderCount = ReadU32(hdr + 16);
    const uint32_t fileCount = ReadU32(hdr + 20);
    if (version != kBsaVersion || (archiveFlags & 1u) == 0u || (archiveFlags & 2u) == 0u ||
        folderCount == 0u || folderCount > 1000000u ||
        fileCount == 0u || fileCount > 3000000u || foldersOffset < 36u) return false;
    if (fseeko(f, static_cast<off_t>(foldersOffset), SEEK_SET) != 0) return false;

    std::vector<uint32_t> folderCounts;
    folderCounts.reserve(folderCount);
    for (uint32_t i = 0; i < folderCount; ++i) {
        uint8_t record[16]{};
        if (!ReadExact(f, record, sizeof(record))) return false;
        const uint32_t count = ReadU32(record + 8);
        if (count > fileCount) return false;
        folderCounts.push_back(count);
    }

    std::vector<PendingEntry> pending;
    pending.reserve(fileCount);
    for (uint32_t count : folderCounts) {
        uint8_t len = 0;
        if (!ReadExact(f, &len, 1) || len == 0u) return false;
        std::vector<char> folderBytes(len);
        if (!ReadExact(f, folderBytes.data(), folderBytes.size())) return false;
        if (!folderBytes.empty() && folderBytes.back() == '\0') folderBytes.pop_back();
        const std::string folder = Normalize(std::string(folderBytes.begin(), folderBytes.end()));
        for (uint32_t j = 0; j < count; ++j) {
            uint8_t rec[16]{};
            if (!ReadExact(f, rec, sizeof(rec))) return false;
            const uint32_t rawSize = ReadU32(rec + 8);
            PendingEntry p;
            p.folder = folder;
            p.entry.storedBytes = rawSize & 0x3fffffffu;
            p.entry.compressionToggle = (rawSize & 0x40000000u) != 0u;
            p.entry.offset = ReadU32(rec + 12);
            pending.push_back(std::move(p));
        }
    }

    const std::string wanted = StripTextures(request);
    for (PendingEntry& p : pending) {
        std::string fileName;
        if (!ReadCString(f, fileName)) return false;
        std::string full = p.folder;
        if (!full.empty() && !fileName.empty()) full += "\\";
        full += Normalize(fileName);
        if (StripTextures(full) == wanted) {
            out = p.entry;
            outFlags = archiveFlags;
            outPath = full;
            return true;
        }
    }
    return true;
}

inline bool LoadRawFromOneBsa(const char* path, const std::string& request,
                              std::vector<uint8_t>& out, std::string* resolved) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;

    BsaEntry entry;
    uint32_t flags = 0;
    std::string storedPath;
    if (!FindRawEntry(f, request, entry, flags, storedPath) || storedPath.empty()) {
        std::fclose(f);
        return false;
    }
    if (entry.storedBytes == 0u || entry.storedBytes > kMaxRawBytes ||
        fseeko(f, static_cast<off_t>(entry.offset), SEEK_SET) != 0) {
        std::fclose(f);
        return false;
    }

    size_t remaining = entry.storedBytes;
    if ((flags & 0x100u) != 0u) {
        uint8_t nameLen = 0;
        if (!ReadExact(f, &nameLen, 1) || remaining < static_cast<size_t>(nameLen) + 1u) {
            std::fclose(f);
            return false;
        }
        if (fseeko(f, static_cast<off_t>(nameLen), SEEK_CUR) != 0) {
            std::fclose(f);
            return false;
        }
        remaining -= static_cast<size_t>(nameLen) + 1u;
    }

    const bool compressed = ((flags & 4u) != 0u) != entry.compressionToggle;
    bool ok = false;
    out.clear();
    if (!compressed) {
        if (remaining > 0u && remaining <= kMaxRawBytes) {
            out.resize(remaining);
            ok = ReadExact(f, out.data(), out.size());
        }
    } else if (remaining >= 4u) {
        uint8_t sizeBytes[4]{};
        if (ReadExact(f, sizeBytes, sizeof(sizeBytes))) {
            const uint32_t originalSize = ReadU32(sizeBytes);
            remaining -= 4u;
            if (originalSize > 0u && originalSize <= kMaxRawBytes &&
                remaining > 0u && remaining <= kMaxRawBytes) {
                std::vector<uint8_t> packed(remaining);
                if (ReadExact(f, packed.data(), packed.size())) {
                    out.resize(originalSize);
                    uLongf dstLen = static_cast<uLongf>(out.size());
                    const int zr = uncompress(reinterpret_cast<Bytef*>(out.data()), &dstLen,
                                              reinterpret_cast<const Bytef*>(packed.data()),
                                              static_cast<uLong>(packed.size()));
                    ok = zr == Z_OK && dstLen == originalSize;
                }
            }
        }
    }
    std::fclose(f);
    if (!ok) {
        out.clear();
        return false;
    }
    if (resolved) *resolved = storedPath;
    __android_log_print(ANDROID_LOG_INFO, kTag,
                        "Q16.9 RAW UI ASSET: path=%s bytes=%zu compressed=%d",
                        storedPath.c_str(), out.size(), compressed ? 1 : 0);
    return true;
}

inline bool LoadRaw(const std::string& request, std::vector<uint8_t>& out,
                    std::string* resolved = nullptr) {
    for (const char* path : kTextureBsaPaths) {
        if (LoadRawFromOneBsa(path, request, out, resolved)) return true;
    }
    __android_log_print(ANDROID_LOG_ERROR, kTag,
                        "Q16.9 RAW UI ASSET MISS: %s", request.c_str());
    return false;
}

struct Glyph {
    std::array<float, 8> uv{};
    float width = 0.0f;
    float height = 0.0f;
    float xOffset = 0.0f;
    float yOffset = 0.0f;
    float advance = 0.0f;
};
struct Font {
    float lineHeight = 0.0f;
    std::array<Glyph, 256> glyphs{};
    std::string texPath;
    int textureWidth = 0;
    int textureHeight = 0;
    std::vector<uint8_t> rgba;
};

inline bool ParseFont(Font& out) {
    std::vector<uint8_t> fnt;
    if (!LoadRaw("Textures\\Fonts\\Baked-in_Monofonto_Large.fnt", fnt)) return false;
    constexpr size_t kHeaderBytes = 296u;
    constexpr size_t kGlyphBytes = 56u;
    if (fnt.size() < kHeaderBytes + 256u * kGlyphBytes) return false;

    out.lineHeight = ReadF32(fnt.data());
    const char* atlasChars = reinterpret_cast<const char*>(fnt.data() + 12);
    size_t atlasLen = 0;
    while (atlasLen < 284u && atlasChars[atlasLen] != '\0') ++atlasLen;
    std::string atlasName(atlasChars, atlasLen);
    if (atlasName.empty()) return false;
    out.texPath = "Textures\\Fonts\\" + atlasName + ".tex";

    for (size_t i = 0; i < 256u; ++i) {
        const uint8_t* p = fnt.data() + kHeaderBytes + i * kGlyphBytes;
        Glyph& g = out.glyphs[i];
        for (int k = 0; k < 8; ++k) g.uv[static_cast<size_t>(k)] = ReadF32(p + 4u * static_cast<size_t>(k + 1));
        g.width = ReadF32(p + 36);
        g.height = ReadF32(p + 40);
        g.xOffset = ReadF32(p + 44);
        g.yOffset = ReadF32(p + 48);
        g.advance = ReadF32(p + 52);
    }

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

    const Glyph& o = out.glyphs[static_cast<uint8_t>('O')];
    __android_log_print(ANDROID_LOG_INFO, kTag,
                        "Q16.9 FNT READY: lineHeight=%.1f atlas=%s %dx%d O=(%.0fx%.0f adv=%.0f)",
                        out.lineHeight, out.texPath.c_str(), out.textureWidth, out.textureHeight,
                        o.width, o.height, o.advance);
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
                            "Q16.9 TAI READY: alias=%s atlas=%s uv=(%.6f %.6f %.6f %.6f)",
                            alias.c_str(), out.atlasPath.c_str(), out.u, out.v, out.w, out.h);
        return true;
    }
    return false;
}

struct Vertex {
    float x, y, z;
    float u, v;
};

struct HudState {
    bool attempted = false;
    bool ready = false;
    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint fontTexture = 0;
    GLuint interfaceTexture = 0;
    GLint mvpLoc = -1;
    GLint tintLoc = -1;
    GLint texLoc = -1;
    GLsizei buttonFirst = 0;
    GLsizei buttonCount = 0;
    GLsizei textFirst = 0;
    GLsizei textCount = 0;
};
inline HudState& State() {
    static HudState s;
    return s;
}

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
                            "Q16.9 HUD SHADER FAILED: %.*s", static_cast<int>(n), log);
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
                            "Q16.9 HUD PROGRAM FAILED: %.*s", static_cast<int>(n), log);
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

inline void AddQuad(std::vector<Vertex>& verts,
                    float x0, float y0, float x1, float y1, float z,
                    float uTL, float vTL, float uTR, float vTR,
                    float uBL, float vBL, float uBR, float vBR) {
    verts.push_back({x0, y0, z, uTL, vTL});
    verts.push_back({x0, y1, z, uBL, vBL});
    verts.push_back({x1, y0, z, uTR, vTR});
    verts.push_back({x1, y0, z, uTR, vTR});
    verts.push_back({x0, y1, z, uBL, vBL});
    verts.push_back({x1, y1, z, uBR, vBR});
}

inline float TextWidth(const Font& f, const char* text) {
    float width = 0.0f;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) {
        width += f.glyphs[*p].advance;
    }
    return width;
}

inline bool Initialize() {
    HudState& s = State();
    if (s.attempted) return s.ready;
    s.attempted = true;

    Font font;
    TaiSprite button;
    if (!ParseFont(font) || !ParseButtonTai(button)) {
        __android_log_print(ANDROID_LOG_ERROR, kTag,
                            "Q16.9 REAL HUD UNAVAILABLE: vanilla FNT/TAI assets could not be read");
        return false;
    }

    Fo3RgbaTexture interfaceAtlas;
    if (!LoadFalloutTextureRgba(button.atlasPath, interfaceAtlas)) {
        __android_log_print(ANDROID_LOG_ERROR, kTag,
                            "Q16.9 REAL HUD UNAVAILABLE: InterfaceShared atlas failed: %s",
                            button.atlasPath.c_str());
        return false;
    }

    s.program = BuildProgram();
    if (!s.program) return false;
    s.fontTexture = UploadTexture(font.textureWidth, font.textureHeight, font.rgba.data());
    s.interfaceTexture = UploadTexture(interfaceAtlas.width, interfaceAtlas.height,
                                       interfaceAtlas.rgba.data());
    if (!s.fontTexture || !s.interfaceTexture) return false;

    // HUDMainMenu/Info -> text_box.xml, evaluated using the real font metrics:
    // text_box horbuf=20, Info verbuf=10, centered text, glow correction -4/+7,
    // 75x75 Xbox button placed on the left. Only the physical scale/arm anchor
    // below are VR adaptations.
    const float openWidth = TextWidth(font, "Open");
    const float doorWidth = TextWidth(font, "Door");
    const float textWidth = std::max(openWidth, doorWidth);
    const float textHeight = font.lineHeight * 2.0f;
    const float boxWidth = textWidth + 20.0f;
    const float boxHeight = textHeight + 10.0f;
    const float textCenterX = boxWidth * 0.5f - 4.0f;
    const float textTopY = (boxHeight - textHeight) * 0.5f + 7.0f;
    const float buttonX0 = -65.0f;
    const float buttonY0 = (boxHeight - 75.0f) * 0.5f;

    // Authored content bounds in Fallout UI pixels. Center these bounds on the
    // controller-local X axis; place the widget ~12 cm above the controller.
    const float contentMinX = -65.0f;
    const float contentMaxX = std::max(boxWidth, textCenterX + textWidth * 0.5f);
    const float contentCenterX = (contentMinX + contentMaxX) * 0.5f;
    const float contentCenterY = boxHeight * 0.5f;
    constexpr float metresPerPixel = 0.00080f;
    constexpr float anchorY = 0.120f;
    constexpr float anchorZ = 0.068f;
    const auto X = [&](float px) { return (px - contentCenterX) * metresPerPixel; };
    const auto Y = [&](float py) { return anchorY - (py - contentCenterY) * metresPerPixel; };

    std::vector<Vertex> verts;
    verts.reserve(6u * (1u + 8u));

    // text_box.xml: 75x75 image, TAI alias glow_general_button_a.dds.
    s.buttonFirst = static_cast<GLsizei>(verts.size());
    const float bu0 = button.u;
    const float bu1 = button.u + button.w;
    const float bvTop = 1.0f - button.v;
    const float bvBottom = 1.0f - (button.v + button.h);
    AddQuad(verts,
            X(buttonX0), Y(buttonY0),
            X(buttonX0 + 75.0f), Y(buttonY0 + 75.0f), anchorZ,
            bu0, bvTop, bu1, bvTop, bu0, bvBottom, bu1, bvBottom);
    s.buttonCount = static_cast<GLsizei>(verts.size()) - s.buttonFirst;

    s.textFirst = static_cast<GLsizei>(verts.size());
    auto addText = [&](const char* text, float lineY) {
        const float width = TextWidth(font, text);
        float penX = textCenterX - width * 0.5f;
        for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) {
            const Glyph& g = font.glyphs[*p];
            if (g.width > 0.0f && g.height > 0.0f) {
                const float gx0 = penX + g.xOffset;
                const float gy0 = lineY + g.yOffset;
                const float gx1 = gx0 + g.width;
                const float gy1 = gy0 + g.height;
                const float uTL = g.uv[0], vTL = 1.0f - g.uv[1];
                const float uTR = g.uv[2], vTR = 1.0f - g.uv[3];
                const float uBL = g.uv[4], vBL = 1.0f - g.uv[5];
                const float uBR = g.uv[6], vBR = 1.0f - g.uv[7];
                AddQuad(verts, X(gx0), Y(gy0), X(gx1), Y(gy1), anchorZ,
                        uTL, vTL, uTR, vTR, uBL, vBL, uBR, vBR);
            }
            penX += g.advance;
        }
    };
    addText("Open", textTopY);
    addText("Door", textTopY + font.lineHeight);
    s.textCount = static_cast<GLsizei>(verts.size()) - s.textFirst;

    glGenVertexArrays(1, &s.vao);
    glGenBuffers(1, &s.vbo);
    glBindVertexArray(s.vao);
    glBindBuffer(GL_ARRAY_BUFFER, s.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(Vertex)),
                 verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(3u * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    s.mvpLoc = glGetUniformLocation(s.program, "uMvp");
    s.tintLoc = glGetUniformLocation(s.program, "uTint");
    s.texLoc = glGetUniformLocation(s.program, "uTex");
    s.ready = s.vao && s.vbo && s.mvpLoc >= 0 && s.tintLoc >= 0 && s.texLoc >= 0;

    __android_log_print(s.ready ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, kTag,
                        "Q16.9 REAL HUD ASSETS READY: ready=%d font=%s interface=%s OpenWidth=%.1f DoorWidth=%.1f button=75x75 scale=%.6fm/px",
                        s.ready ? 1 : 0, font.texPath.c_str(), button.atlasPath.c_str(),
                        openWidth, doorWidth, metresPerPixel);
    return s.ready;
}

} // namespace fo3q1790

inline void RenderFo3InteractionHudQ1790(const float* mvp) {
    using namespace fo3q1790;
    if (!mvp || !Initialize()) return;
    HudState& s = State();

    static bool logged = false;
    if (!logged) {
        logged = true;
        __android_log_print(ANDROID_LOG_INFO, kTag,
                            "Q16.9 REAL HUD DRAW: source=HUDMainMenu/Info button=glow_general_button_a.dds font=Baked-in_Monofonto_Large text=Open/Door");
    }

    GLint oldProgram = 0, oldVao = 0, oldBuffer = 0, oldActiveTexture = 0, oldTexture = 0;
    GLint oldBlendSrcRgb = 0, oldBlendDstRgb = 0, oldBlendSrcAlpha = 0, oldBlendDstAlpha = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &oldBuffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &oldActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
    glGetIntegerv(GL_BLEND_SRC_RGB, &oldBlendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &oldBlendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &oldBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &oldBlendDstAlpha);
    const GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendWas = glIsEnabled(GL_BLEND);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(s.program);
    glUniformMatrix4fv(s.mvpLoc, 1, GL_FALSE, mvp);
    // FalloutPrefs.ini uHUDColor=452952319 -> HUDMain RGB 26,255,128, alpha 255.
    glUniform4f(s.tintLoc, 26.0f / 255.0f, 1.0f, 128.0f / 255.0f, 1.0f);
    glUniform1i(s.texLoc, 0);
    glBindVertexArray(s.vao);

    glBindTexture(GL_TEXTURE_2D, s.interfaceTexture);
    glDrawArrays(GL_TRIANGLES, s.buttonFirst, s.buttonCount);
    glBindTexture(GL_TEXTURE_2D, s.fontTexture);
    glDrawArrays(GL_TRIANGLES, s.textFirst, s.textCount);

    glBindVertexArray(static_cast<GLuint>(oldVao));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(oldBuffer));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(oldTexture));
    glUseProgram(static_cast<GLuint>(oldProgram));
    glBlendFuncSeparate(static_cast<GLenum>(oldBlendSrcRgb), static_cast<GLenum>(oldBlendDstRgb),
                        static_cast<GLenum>(oldBlendSrcAlpha), static_cast<GLenum>(oldBlendDstAlpha));
    if (!blendWas) glDisable(GL_BLEND);
    if (depthWas) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    glActiveTexture(static_cast<GLenum>(oldActiveTexture));
}

inline void ShutdownFo3InteractionHudQ1790() {
    using namespace fo3q1790;
    HudState& s = State();
    if (s.vbo) glDeleteBuffers(1, &s.vbo);
    if (s.vao) glDeleteVertexArrays(1, &s.vao);
    if (s.fontTexture) glDeleteTextures(1, &s.fontTexture);
    if (s.interfaceTexture) glDeleteTextures(1, &s.interfaceTexture);
    if (s.program) glDeleteProgram(s.program);
    s = {};
}
