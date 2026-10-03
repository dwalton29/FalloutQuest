#pragma once

// Q16.12 dynamic renderer for Fallout 3's HUDMainMenu/Info widget.
//
// IMPORTANT: this header is included immediately after the generated Q16.11
// fo3-interaction-hud-q1830.h, so the exact vanilla FNT/TAI parsers and glyph
// structures in namespace fo3q1790 are already available. No Bethesda asset is
// embedded here; all pixels still come from the user's Fallout - Textures.bsa.
//
// Q16.9 built one static two-line "Open" / "Door" VBO. Fallout's text_box.xml
// actually receives one interaction string and derives its box from that text.
// Q16.12 therefore rebuilds only the small text VBO when the authored prompt
// changes, while keeping textures/program/resources alive when aim is lost.

#include <GLES3/gl3.h>
#include <android/log.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace fo3hudq1840 {

constexpr const char* kTagQ1840 = "FalloutQuest";

struct HudStateQ1840 {
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
    std::string prompt;
};

inline HudStateQ1840& StateQ1840() {
    static HudStateQ1840 state;
    return state;
}

// Snapshot BEFORE EnsureResourcesQ1840(). Q16.9 captured state only after its
// first-time GL initialization had already bound/unbound textures and VAOs. That
// meant the first prompt draw could permanently clobber the caller's bindings;
// losing aim on the next frame then exposed the damaged render state.
struct GlStateGuardQ1840 {
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

    GlStateGuardQ1840() {
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

        // Texture binding is per active unit. Read unit zero without leaving the
        // caller on a different active unit before initialization begins.
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture0);
        glActiveTexture(static_cast<GLenum>(activeTexture));
    }

    ~GlStateGuardQ1840() {
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

inline bool EnsureResourcesQ1840() {
    HudStateQ1840& s = StateQ1840();
    if (s.attempted) return s.ready;
    s.attempted = true;

    fo3q1790::Font font;
    fo3q1790::TaiSprite button;
    if (!fo3q1790::ParseFont(font) || !fo3q1790::ParseButtonTai(button)) {
        __android_log_print(ANDROID_LOG_ERROR, kTagQ1840,
                            "Q16.12 HUD ASSET INIT FAILED: vanilla FNT/TAI unavailable");
        return false;
    }

    Fo3RgbaTexture interfaceAtlas;
    if (!LoadFalloutTextureRgba(button.atlasPath, interfaceAtlas)) {
        __android_log_print(ANDROID_LOG_ERROR, kTagQ1840,
                            "Q16.12 HUD ASSET INIT FAILED: interface atlas=%s",
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

    __android_log_print(s.ready ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR,
                        kTagQ1840,
                        "Q16.12 HUD GL SAFE INIT: ready=%d stateCapturedBeforeAssets=1 font=%s interface=%s",
                        s.ready ? 1 : 0, font.texPath.c_str(), button.atlasPath.c_str());
    return s.ready;
}

inline float TextWidthQ1840(const HudStateQ1840& s, const std::string& text) {
    float width = 0.0f;
    for (unsigned char ch : text) width += s.glyphs[ch].advance;
    return width;
}

inline void AddQuadQ1840(std::vector<fo3q1790::Vertex>& verts,
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

inline bool BuildPromptGeometryQ1840(const std::string& prompt) {
    HudStateQ1840& s = StateQ1840();
    if (!s.ready || prompt.empty()) return false;
    if (s.prompt == prompt && s.textCount > 0) return true;

    // menus/prefabs/text_box.xml + HUDMainMenu/Info, exactly:
    //   fixedwidth=0          -> width = text width + horbuf
    //   horbuf=20
    //   Info verbuf=10       -> height = text height + 10
    //   justify=center
    //   glow text x -= 4
    //   glow text y += 7 after vertical centering
    //   Xbox image = 75x75
    //   left placement x = box.x - (75 - 10) = box.x - 65
    // This is ONE interaction string, not Q16.9's invented two-line layout.
    const float textWidth = TextWidthQ1840(s, prompt);
    const float textHeight = s.lineHeight;
    const float boxWidth = textWidth + 20.0f;
    const float boxHeight = textHeight + 10.0f;
    const float boxX = 0.0f;
    const float textCenterX = boxX + boxWidth * 0.5f - 4.0f;
    const float textTopY = (boxHeight - textHeight) * 0.5f + 7.0f;
    const float buttonX0 = boxX - 65.0f;
    const float buttonY0 = (boxHeight - 75.0f) * 0.5f;

    // VR-only adaptation: convert Fallout UI pixels to metres and mount the
    // otherwise-authored widget above the right-controller/forearm proxy.
    const float contentMinX = buttonX0;
    const float contentMaxX = boxX + boxWidth;
    const float contentCenterX = (contentMinX + contentMaxX) * 0.5f;
    const float contentCenterY = boxHeight * 0.5f;
    constexpr float metresPerPixel = 0.00080f;
    constexpr float anchorY = 0.120f;
    constexpr float anchorZ = 0.068f;
    const auto X = [&](float px) {
        return (px - contentCenterX) * metresPerPixel;
    };
    const auto Y = [&](float py) {
        return anchorY - (py - contentCenterY) * metresPerPixel;
    };

    std::vector<fo3q1790::Vertex> verts;
    verts.reserve(6u * (1u + prompt.size()));

    s.buttonFirst = static_cast<GLsizei>(verts.size());
    const float bu0 = s.button.u;
    const float bu1 = s.button.u + s.button.w;
    const float bvTop = s.button.v;
    const float bvBottom = s.button.v + s.button.h;
    AddQuadQ1840(verts,
                 X(buttonX0), Y(buttonY0),
                 X(buttonX0 + 75.0f), Y(buttonY0 + 75.0f), anchorZ,
                 bu0, bvTop, bu1, bvTop,
                 bu0, bvBottom, bu1, bvBottom);
    s.buttonCount = static_cast<GLsizei>(verts.size()) - s.buttonFirst;

    s.textFirst = static_cast<GLsizei>(verts.size());
    float penX = textCenterX - textWidth * 0.5f;
    for (unsigned char ch : prompt) {
        const fo3q1790::Glyph& g = s.glyphs[ch];
        if (g.width > 0.0f && g.height > 0.0f) {
            const float gx0 = penX + g.xOffset;
            const float gy0 = textTopY + g.yOffset;
            const float gx1 = gx0 + g.width;
            const float gy1 = gy0 + g.height;
            // Q16.11 verified these are already authored for the raw .tex row
            // convention. Do not vertically flip them.
            AddQuadQ1840(verts, X(gx0), Y(gy0), X(gx1), Y(gy1), anchorZ,
                         g.uv[0], g.uv[1], g.uv[2], g.uv[3],
                         g.uv[4], g.uv[5], g.uv[6], g.uv[7]);
        }
        penX += g.advance;
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

    s.prompt = prompt;
    __android_log_print(ANDROID_LOG_INFO, kTagQ1840,
                        "Q16.12 HUD TEXT GEOMETRY: text=\"%s\" textWidth=%.1f box=(%.1fx%.1f) button=75x75 singleLine=1 horbuf=20 verbuf=10 glowOffset=(-4,+7)",
                        prompt.c_str(), textWidth, boxWidth, boxHeight);
    return true;
}

inline void RenderQ1840(const float* mvp, const char* promptChars) {
    if (!mvp || !promptChars || !promptChars[0]) return;

    // The guard is intentionally the first GL operation in this function. Force
    // any first-time UploadTexture work onto unit zero, whose binding the guard
    // owns and restores, rather than clobbering an arbitrary caller-active unit.
    GlStateGuardQ1840 guard;
    glActiveTexture(GL_TEXTURE0);
    if (!EnsureResourcesQ1840()) return;

    const std::string prompt(promptChars);
    if (!BuildPromptGeometryQ1840(prompt)) return;

    HudStateQ1840& s = StateQ1840();
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                        GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(s.program);
    glUniformMatrix4fv(s.mvpLoc, 1, GL_FALSE, mvp);
    // FalloutPrefs.ini uHUDColor=452952319 -> HUDMain RGB 26,255,128.
    glUniform4f(s.tintLoc, 26.0f / 255.0f, 1.0f, 128.0f / 255.0f, 1.0f);
    glUniform1i(s.texLoc, 0);
    glBindVertexArray(s.vao);
    glActiveTexture(GL_TEXTURE0);

    glBindTexture(GL_TEXTURE_2D, s.interfaceTexture);
    glDrawArrays(GL_TRIANGLES, s.buttonFirst, s.buttonCount);
    glBindTexture(GL_TEXTURE_2D, s.fontTexture);
    glDrawArrays(GL_TRIANGLES, s.textFirst, s.textCount);

    static std::string lastLoggedPrompt;
    if (lastLoggedPrompt != prompt) {
        lastLoggedPrompt = prompt;
        __android_log_print(ANDROID_LOG_INFO, kTagQ1840,
                            "Q16.12 REAL HUD DRAW: text=\"%s\" source=HUDMainMenu/Info layout=text_box.xml singleLine=1 uv=authored-no-flip glSandbox=1",
                            prompt.c_str());
    }
}

inline void ShutdownQ1840() {
    HudStateQ1840& s = StateQ1840();
    if (s.vbo) glDeleteBuffers(1, &s.vbo);
    if (s.vao) glDeleteVertexArrays(1, &s.vao);
    if (s.fontTexture) glDeleteTextures(1, &s.fontTexture);
    if (s.interfaceTexture) glDeleteTextures(1, &s.interfaceTexture);
    if (s.program) glDeleteProgram(s.program);
    s = {};
}

} // namespace fo3hudq1840

inline void RenderFo3InteractionHudQ1840(const float* mvp, const char* prompt) {
    fo3hudq1840::RenderQ1840(mvp, prompt);
}

inline void ShutdownFo3InteractionHudQ1840() {
    fo3hudq1840::ShutdownQ1840();
}
