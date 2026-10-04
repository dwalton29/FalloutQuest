#pragma once

// Single live prompt renderer and shared loot font resources. Metrics and
// geometry are owned by fo3font; presentation remains HUDMainMenu/text_box.xml.
#include "fo3-interaction-hud-assets.h"
#include "fo3-font-diagnostics.h"
#include "fo3-hud-cached-state.h"

#include <GLES3/gl3.h>
#include <android/log.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace fo3hudrenderer {

constexpr const char* kTag = "FalloutQuest";
constexpr size_t kPromptCapacity = 512u;

struct HudState : fo3font::Metrics {
    bool attempted = false;
    bool ready = false;
    GLuint program = 0u;
    GLuint vao = 0u;
    GLuint vbo = 0u;
    GLuint fontTexture = 0u;
    GLuint interfaceTexture = 0u;
    GLint mvpLoc = -1;
    GLint tintLoc = -1;
    GLint texLoc = -1;
    GLsizei buttonFirst = 0;
    GLsizei buttonCount = 0;
    GLsizei textFirst = 0;
    GLsizei textCount = 0;
    fo3hudassets::TaiSprite button{};
    std::array<char, kPromptCapacity> builtPrompt{};
    size_t builtPromptLength = 0u;
};

inline HudState& State() {
    static HudState state;
    return state;
}

// The prompt is drawn after the world and before the post/composite path.
// Restore only renderer-owned cached state; never synchronously query Adreno
// merely because an Open/Close/Take prompt became visible.
using GlStateGuard = CachedStateGuard;

inline bool EnsureResources() {
    HudState& s = State();
    if (s.attempted) return s.ready;
    s.attempted = true;

    fo3hudassets::Font font;
    fo3hudassets::TaiSprite button;
    if (!fo3hudassets::ParseFont(font) || !fo3hudassets::ParseButtonTai(button)) {
        __android_log_print(ANDROID_LOG_ERROR, kTag,
                            "HUD ASSET INIT FAILED: vanilla FNT/TAI unavailable");
        return false;
    }

    Fo3RgbaTexture interfaceAtlas;
    if (!LoadFalloutTextureRgba(button.atlasPath, interfaceAtlas)) {
        __android_log_print(ANDROID_LOG_ERROR, kTag,
                            "HUD ASSET INIT FAILED: interface atlas=%s",
                            button.atlasPath.c_str());
        return false;
    }

    s.program = fo3hudassets::BuildProgram();
    if (!s.program) return false;
    s.fontTexture = fo3hudassets::UploadTexture(font.textureWidth, font.textureHeight,
                                            font.rgba.data());
    s.interfaceTexture = fo3hudassets::UploadTexture(interfaceAtlas.width,
                                                 interfaceAtlas.height,
                                                 interfaceAtlas.rgba.data());
    if (!s.fontTexture || !s.interfaceTexture) return false;

    glGenVertexArrays(1, &s.vao);
    glGenBuffers(1, &s.vbo);
    s.mvpLoc = glGetUniformLocation(s.program, "uMvp");
    s.tintLoc = glGetUniformLocation(s.program, "uTint");
    s.texLoc = glGetUniformLocation(s.program, "uTex");
    static_cast<fo3font::Metrics&>(s) = std::move(static_cast<fo3font::Metrics&>(font));
    s.button = button;
    s.ready = s.vao != 0u && s.vbo != 0u &&
              s.mvpLoc >= 0 && s.tintLoc >= 0 && s.texLoc >= 0;

    __android_log_print(s.ready ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR,kTag,
        "HUD ASSETS READY: ready=%d font=%s interface=%s baseLine=%.1f layout=Fallout-AddChar",
        s.ready ? 1 : 0,font.texPath.c_str(),button.atlasPath.c_str(),s.baseLine);
    return s.ready;
}

inline size_t PromptLength(const char* text) {
    if (!text) return 0u;
    size_t n = 0u;
    while (n + 1u < kPromptCapacity && text[n] != '\0') ++n;
    return n;
}

inline float TextWidth(const HudState& s,const char* text,size_t length) {
    return fo3font::MeasureText(s,std::string_view(text,length));
}

inline void AppendPromptQuad(std::vector<fo3hudassets::Vertex>& verts,
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

inline bool SameBuiltPrompt(const HudState& s,
                                 const char* text,
                                 size_t length) {
    return s.builtPromptLength == length &&
           length > 0u &&
           std::memcmp(s.builtPrompt.data(), text, length) == 0;
}

inline void LogTextDiagnostics(const char* text,size_t length,float width) {
#ifndef NDEBUG
    if (!fo3fontdebug::LogBytes("emitted-glyphs",std::string_view(text,length))) return;
    __android_log_print(ANDROID_LOG_INFO,kTag,"HUD TEXT LAYOUT: width=%.1f baseLine=%.1f",width,State().baseLine);
    const auto& f=State();
    for(size_t i=0;i<length;++i) {
        const auto byte=static_cast<uint8_t>(text[i]);
        if(byte!=0x20&&byte!=0x2d&&byte!=0x2e)continue;
        const auto index=fo3font::GlyphIndex(byte);const auto& g=f.glyphs[index];
        __android_log_print(ANDROID_LOG_INFO,kTag,"HUD TEXT GLYPH: offset=%zu byte=%02X index=%u quad=%d advance=%.1f topEdge=%.1f",
            i,byte,index,fo3font::Drawable(f,g),fo3font::GlyphRenderAdvance(g),g.topEdge);
    }
#else
    (void)text;(void)length;(void)width;
#endif
}

inline bool BuildPromptGeometry(const char* promptChars) {
    HudState& s = State();
    if (!s.ready || !promptChars || !promptChars[0]) return false;
    const size_t length = PromptLength(promptChars);
    if (length == 0u) return false;
    if (SameBuiltPrompt(s, promptChars, length) && s.textCount > 0) return true;

    // Exact text_box.xml/HUDMainMenu Info layout values recovered from the
    // user's Fallout - Misc.bsa. Only pixel->metre scale and arm anchor are VR.
    const float textWidth = TextWidth(s, promptChars, length);
    const float textHeight = s.baseLine;
    const float boxWidth = textWidth + 20.0f; // _horbuf
    const float boxHeight = textHeight + 10.0f; // Info _verbuf override
    const float boxX = 0.0f;
    const float textCenterX = boxX + boxWidth * 0.5f - 4.0f; // glow correction
    const float textTopY = (boxHeight - textHeight) * 0.5f + 7.0f;
    const float buttonX0 = boxX - 65.0f; // 75 - 10 overlap from prefab
    const float buttonY0 = (boxHeight - 75.0f) * 0.5f;

    const float contentMinX = buttonX0;
    const float contentMaxX = boxX + boxWidth;
    const float contentCenterX = (contentMinX + contentMaxX) * 0.5f;
    const float contentCenterY = boxHeight * 0.5f;
    constexpr float metresPerPixel = 0.00080f;
    constexpr float anchorY = 0.120f;
    constexpr float anchorZ = 0.068f;
    const auto X = [&](float px) { return (px - contentCenterX) * metresPerPixel; };
    const auto Y = [&](float py) { return anchorY - (py - contentCenterY) * metresPerPixel; };

    std::vector<fo3hudassets::Vertex> verts;
    verts.reserve(6u * (1u + length));

    s.buttonFirst = static_cast<GLsizei>(verts.size());
    const float bu0 = s.button.u;
    const float bu1 = s.button.u + s.button.w;
    const float bvTop = s.button.v;
    const float bvBottom = s.button.v + s.button.h;
    AppendPromptQuad(verts,
                 X(buttonX0), Y(buttonY0),
                 X(buttonX0 + 75.0f), Y(buttonY0 + 75.0f), anchorZ,
                 bu0, bvTop, bu1, bvTop,
                 bu0, bvBottom, bu1, bvBottom);
    s.buttonCount = static_cast<GLsizei>(verts.size()) - s.buttonFirst;

    s.textFirst = static_cast<GLsizei>(verts.size());
    float penX = textCenterX - textWidth * 0.5f;
    fo3font::AppendText(s,std::string_view(promptChars,length),penX,textTopY,
        [&](uint8_t, const fo3font::Glyph& g,const fo3font::Quad& q) {
            AppendPromptQuad(verts,X(q.left),Y(q.top),X(q.right),Y(q.bottom),anchorZ,
                g.uv[0],g.uv[1],g.uv[2],g.uv[3],g.uv[4],g.uv[5],g.uv[6],g.uv[7]);
        });
    s.textCount = static_cast<GLsizei>(verts.size()) - s.textFirst;
    if (s.textCount <= 0) return false;

    glBindVertexArray(s.vao);
    glBindBuffer(GL_ARRAY_BUFFER, s.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(fo3hudassets::Vertex)),
                 verts.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          sizeof(fo3hudassets::Vertex), reinterpret_cast<const void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                          sizeof(fo3hudassets::Vertex),
                          reinterpret_cast<const void*>(3u * sizeof(float)));

    s.builtPrompt.fill('\0');
    const size_t copyLength = std::min(length, s.builtPrompt.size() - 1u);
    std::memcpy(s.builtPrompt.data(), promptChars, copyLength);
    s.builtPromptLength = copyLength;

    LogTextDiagnostics(promptChars,copyLength,textWidth);
    return true;
}

inline void Render(const float* mvp, const char* promptChars) {
    if (!mvp || !promptChars || !promptChars[0]) return;

    GlStateGuard guard;
    glActiveTexture(GL_TEXTURE0);
    if (!EnsureResources()) return;
    if (!BuildPromptGeometry(promptChars)) return;

    HudState& s = State();
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                        GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(s.program);
    glUniformMatrix4fv(s.mvpLoc, 1, GL_FALSE, mvp);
    glUniform4f(s.tintLoc, 26.0f / 255.0f, 1.0f, 128.0f / 255.0f, 1.0f);
    glUniform1i(s.texLoc, 0);
    glBindVertexArray(s.vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s.interfaceTexture);
    glDrawArrays(GL_TRIANGLES, s.buttonFirst, s.buttonCount);
    glBindTexture(GL_TEXTURE_2D, s.fontTexture);
    glDrawArrays(GL_TRIANGLES, s.textFirst, s.textCount);
}

inline void Shutdown() {
    HudState& s = State();
    if (s.vbo) glDeleteBuffers(1, &s.vbo);
    if (s.vao) glDeleteVertexArrays(1, &s.vao);
    if (s.fontTexture) glDeleteTextures(1, &s.fontTexture);
    if (s.interfaceTexture) glDeleteTextures(1, &s.interfaceTexture);
    if (s.program) glDeleteProgram(s.program);
    s = {};
}

} // namespace fo3hudrenderer

inline void RenderFo3InteractionHudBase(const float* mvp, const char* prompt) {
    fo3hudrenderer::Render(mvp, prompt);
}

inline void ShutdownFo3InteractionHudBase() {
    fo3hudrenderer::Shutdown();
}
