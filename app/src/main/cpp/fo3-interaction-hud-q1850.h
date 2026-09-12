#pragma once

// Q16.13: stable Fallout 3 HUDMainMenu/Info renderer.
//
// Q16.12 proved the authored ESM wording path, but two runtime problems remained:
//   - the OpenXR host and HUD renderer both mutated std::string objects while the
//     interaction target appeared/disappeared;
//   - Fallout's Baked-in_Monofonto_Large.fnt stores the space width specially.
//     The space has no drawable quad, its ordinary advance field is zero, and the
//     authored spacing value (13 px in the vanilla font) occupies the offset slot
//     used only by non-rendering whitespace. Treating advance=0 literally collapsed
//     every word boundary.
//
// This renderer keeps prompt identity in fixed storage, allocates geometry only
// when the target text changes, and derives the whitespace width from the actual
// FNT record. Bethesda's text_box.xml geometry remains unchanged: horbuf 20,
// Info verbuf 10, centred text, glow correction -4/+7, 75x75 A button at left.

#include <GLES3/gl3.h>
#include <android/log.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace fo3hudq1850 {

constexpr const char* kTagQ1850 = "FalloutQuest";
constexpr size_t kPromptCapacityQ1850 = 512u;

struct HudStateQ1850 {
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
    float lineHeight = 0.0f;
    std::array<fo3q1790::Glyph, 256> glyphs{};
    fo3q1790::TaiSprite button{};
    std::array<char, kPromptCapacityQ1850> builtPrompt{};
    size_t builtPromptLength = 0u;
};

inline HudStateQ1850& StateQ1850() {
    static HudStateQ1850 state;
    return state;
}

struct GlStateGuardQ1850 {
    GLint program = 0;
    GLint vao = 0;
    GLint arrayBuffer = 0;
    GLint activeTexture = GL_TEXTURE0;
    GLint texture0 = 0;
    GLint blendSrcRgb = GL_ONE;
    GLint blendDstRgb = GL_ZERO;
    GLint blendSrcAlpha = GL_ONE;
    GLint blendDstAlpha = GL_ZERO;
    GLint blendEquationRgb = GL_FUNC_ADD;
    GLint blendEquationAlpha = GL_FUNC_ADD;
    GLint depthFunc = GL_LESS;
    GLint unpackAlignment = 4;
    GLboolean depthEnabled = GL_FALSE;
    GLboolean blendEnabled = GL_FALSE;
    GLboolean depthMask = GL_TRUE;

    GlStateGuardQ1850() {
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
        glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquationRgb);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEquationAlpha);
        glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
        depthEnabled = glIsEnabled(GL_DEPTH_TEST);
        blendEnabled = glIsEnabled(GL_BLEND);

        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture0);
        glActiveTexture(static_cast<GLenum>(activeTexture));
    }

    ~GlStateGuardQ1850() {
        glUseProgram(static_cast<GLuint>(program));
        glBindVertexArray(static_cast<GLuint>(vao));
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(arrayBuffer));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture0));
        glActiveTexture(static_cast<GLenum>(activeTexture));

        glBlendFuncSeparate(static_cast<GLenum>(blendSrcRgb),
                            static_cast<GLenum>(blendDstRgb),
                            static_cast<GLenum>(blendSrcAlpha),
                            static_cast<GLenum>(blendDstAlpha));
        glBlendEquationSeparate(static_cast<GLenum>(blendEquationRgb),
                                static_cast<GLenum>(blendEquationAlpha));
        glDepthFunc(static_cast<GLenum>(depthFunc));
        glDepthMask(depthMask);
        glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
        if (depthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    }
};

inline bool EnsureResourcesQ1850() {
    HudStateQ1850& s = StateQ1850();
    if (s.attempted) return s.ready;
    s.attempted = true;

    fo3q1790::Font font;
    fo3q1790::TaiSprite button;
    if (!fo3q1790::ParseFont(font) || !fo3q1790::ParseButtonTai(button)) {
        __android_log_print(ANDROID_LOG_ERROR, kTagQ1850,
                            "Q16.13 HUD ASSET INIT FAILED: vanilla FNT/TAI unavailable");
        return false;
    }

    Fo3RgbaTexture interfaceAtlas;
    if (!LoadFalloutTextureRgba(button.atlasPath, interfaceAtlas)) {
        __android_log_print(ANDROID_LOG_ERROR, kTagQ1850,
                            "Q16.13 HUD ASSET INIT FAILED: interface atlas=%s",
                            button.atlasPath.c_str());
        return false;
    }

    s.program = fo3q1790::BuildProgram();
    if (!s.program) return false;
    s.fontTexture = fo3q1790::UploadTexture(font.textureWidth, font.textureHeight,
                                            font.rgba.data());
    s.interfaceTexture = fo3q1790::UploadTexture(interfaceAtlas.width,
                                                 interfaceAtlas.height,
                                                 interfaceAtlas.rgba.data());
    if (!s.fontTexture || !s.interfaceTexture) return false;

    glGenVertexArrays(1, &s.vao);
    glGenBuffers(1, &s.vbo);
    s.mvpLoc = glGetUniformLocation(s.program, "uMvp");
    s.tintLoc = glGetUniformLocation(s.program, "uTint");
    s.texLoc = glGetUniformLocation(s.program, "uTex");
    s.lineHeight = font.lineHeight;
    s.glyphs = font.glyphs;
    s.button = button;
    s.ready = s.vao != 0u && s.vbo != 0u &&
              s.mvpLoc >= 0 && s.tintLoc >= 0 && s.texLoc >= 0;

    const fo3q1790::Glyph& space = s.glyphs[static_cast<uint8_t>(' ')];
    const float spaceAdvance =
        (space.advance > 0.0f) ? space.advance : std::max(0.0f, space.yOffset);
    __android_log_print(s.ready ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR,
                        kTagQ1850,
                        "Q16.13 HUD ASSETS READY: ready=%d font=%s interface=%s lineHeight=%.1f space=(width=%.1f height=%.1f rawAdvance=%.1f authoredAdvance=%.1f) storage=fixed",
                        s.ready ? 1 : 0, font.texPath.c_str(), button.atlasPath.c_str(),
                        s.lineHeight, space.width, space.height, space.advance, spaceAdvance);
    return s.ready;
}

inline size_t PromptLengthQ1850(const char* text) {
    if (!text) return 0u;
    size_t n = 0u;
    while (n + 1u < kPromptCapacityQ1850 && text[n] != '\0') ++n;
    return n;
}

inline float GlyphAdvanceQ1850(const HudStateQ1850& s, unsigned char ch) {
    const fo3q1790::Glyph& g = s.glyphs[ch];
    if (ch == static_cast<unsigned char>(' ') &&
        g.width <= 0.0f && g.height <= 0.0f && g.advance <= 0.0f) {
        // Vanilla Baked-in_Monofonto_Large: the non-rendering space glyph stores
        // its authored 13 px word gap in this slot while the ordinary advance is
        // zero. Read the value from the FNT instead of inventing a VR constant.
        return std::max(0.0f, g.yOffset);
    }
    return std::max(0.0f, g.advance);
}

inline float TextWidthQ1850(const HudStateQ1850& s,
                            const char* text,
                            size_t length) {
    float width = 0.0f;
    for (size_t i = 0u; i < length; ++i) {
        width += GlyphAdvanceQ1850(s, static_cast<unsigned char>(text[i]));
    }
    return width;
}

inline void AddQuadQ1850(std::vector<fo3q1790::Vertex>& verts,
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

inline bool SameBuiltPromptQ1850(const HudStateQ1850& s,
                                 const char* text,
                                 size_t length) {
    return s.builtPromptLength == length &&
           length > 0u &&
           std::memcmp(s.builtPrompt.data(), text, length) == 0;
}

inline bool BuildPromptGeometryQ1850(const char* promptChars) {
    HudStateQ1850& s = StateQ1850();
    if (!s.ready || !promptChars || !promptChars[0]) return false;
    const size_t length = PromptLengthQ1850(promptChars);
    if (length == 0u) return false;
    if (SameBuiltPromptQ1850(s, promptChars, length) && s.textCount > 0) return true;

    // Exact text_box.xml/HUDMainMenu Info layout values recovered from the
    // user's Fallout - Misc.bsa. Only pixel->metre scale and arm anchor are VR.
    const float textWidth = TextWidthQ1850(s, promptChars, length);
    const float textHeight = s.lineHeight;
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

    std::vector<fo3q1790::Vertex> verts;
    verts.reserve(6u * (1u + length));

    s.buttonFirst = static_cast<GLsizei>(verts.size());
    const float bu0 = s.button.u;
    const float bu1 = s.button.u + s.button.w;
    const float bvTop = s.button.v;
    const float bvBottom = s.button.v + s.button.h;
    AddQuadQ1850(verts,
                 X(buttonX0), Y(buttonY0),
                 X(buttonX0 + 75.0f), Y(buttonY0 + 75.0f), anchorZ,
                 bu0, bvTop, bu1, bvTop,
                 bu0, bvBottom, bu1, bvBottom);
    s.buttonCount = static_cast<GLsizei>(verts.size()) - s.buttonFirst;

    s.textFirst = static_cast<GLsizei>(verts.size());
    float penX = textCenterX - textWidth * 0.5f;
    size_t spaces = 0u;
    for (size_t i = 0u; i < length; ++i) {
        const unsigned char ch = static_cast<unsigned char>(promptChars[i]);
        const fo3q1790::Glyph& g = s.glyphs[ch];
        if (ch == static_cast<unsigned char>(' ')) ++spaces;
        if (g.width > 0.0f && g.height > 0.0f) {
            const float gx0 = penX + g.xOffset;
            const float gy0 = textTopY + g.yOffset;
            const float gx1 = gx0 + g.width;
            const float gy1 = gy0 + g.height;
            AddQuadQ1850(verts, X(gx0), Y(gy0), X(gx1), Y(gy1), anchorZ,
                         g.uv[0], g.uv[1], g.uv[2], g.uv[3],
                         g.uv[4], g.uv[5], g.uv[6], g.uv[7]);
        }
        penX += GlyphAdvanceQ1850(s, ch);
    }
    s.textCount = static_cast<GLsizei>(verts.size()) - s.textFirst;
    if (s.textCount <= 0) return false;

    glBindVertexArray(s.vao);
    glBindBuffer(GL_ARRAY_BUFFER, s.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(fo3q1790::Vertex)),
                 verts.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          sizeof(fo3q1790::Vertex), reinterpret_cast<const void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                          sizeof(fo3q1790::Vertex),
                          reinterpret_cast<const void*>(3u * sizeof(float)));

    s.builtPrompt.fill('\0');
    const size_t copyLength = std::min(length, s.builtPrompt.size() - 1u);
    std::memcpy(s.builtPrompt.data(), promptChars, copyLength);
    s.builtPromptLength = copyLength;

    const fo3q1790::Glyph& space = s.glyphs[static_cast<uint8_t>(' ')];
    const float spaceAdvance = GlyphAdvanceQ1850(s, static_cast<unsigned char>(' '));
    __android_log_print(ANDROID_LOG_INFO, kTagQ1850,
                        "Q16.13 HUD TEXT GEOMETRY: text=\"%.*s\" chars=%zu spaces=%zu textWidth=%.1f box=(%.1fx%.1f) rawSpaceAdvance=%.1f authoredSpaceAdvance=%.1f singleLine=1 storage=fixed",
                        static_cast<int>(copyLength), s.builtPrompt.data(), copyLength,
                        spaces, textWidth, boxWidth, boxHeight,
                        space.advance, spaceAdvance);
    return true;
}

inline void RenderQ1850(const float* mvp, const char* promptChars) {
    if (!mvp || !promptChars || !promptChars[0]) return;

    GlStateGuardQ1850 guard;
    glActiveTexture(GL_TEXTURE0);
    if (!EnsureResourcesQ1850()) return;
    if (!BuildPromptGeometryQ1850(promptChars)) return;

    HudStateQ1850& s = StateQ1850();
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

inline void ShutdownQ1850() {
    HudStateQ1850& s = StateQ1850();
    if (s.vbo) glDeleteBuffers(1, &s.vbo);
    if (s.vao) glDeleteVertexArrays(1, &s.vao);
    if (s.fontTexture) glDeleteTextures(1, &s.fontTexture);
    if (s.interfaceTexture) glDeleteTextures(1, &s.interfaceTexture);
    if (s.program) glDeleteProgram(s.program);
    s = {};
}

} // namespace fo3hudq1850

inline void RenderFo3InteractionHudQ1850(const float* mvp, const char* prompt) {
    fo3hudq1850::RenderQ1850(mvp, prompt);
}

inline void ShutdownFo3InteractionHudQ1850() {
    fo3hudq1850::ShutdownQ1850();
}
