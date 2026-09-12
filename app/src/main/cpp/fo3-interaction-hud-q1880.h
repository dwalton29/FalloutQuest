#pragma once

// Q16.16: correct Bethesda bitmap-font metrics for HUDMainMenu/Info.
//
// The Q16.9/Q16.13 parser recovered the correct 56-byte glyph records and UVs,
// but mislabeled the final three floats. In Fallout 3's bitmap FNT they are:
//   +44 left kerning
//   +48 right kerning
//   +52 ascent
// rather than xOffset / yOffset / horizontal advance.
//
// Correct layout therefore uses:
//   glyph X      = penX + leftKerning
//   glyph Y      = lineTop + (fontSize - ascent)
//   next pen X   = penX + width + rightKerning
//
// This also naturally resolves the space glyph: width=0, rightKerning=13,
// ascent=0 -> authored 13px word advance without a special-case invented gap.
//
// All presentation values remain sourced from Fallout 3's HUDMainMenu/Info and
// text_box.xml. Only pixel->metre scale / right-arm placement remain VR-specific.

#include "fo3-interaction-hud-q1850.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace fo3hudq1880 {

constexpr const char* kTagQ1880 = "FalloutQuest";

inline float LeftKerningQ1880(const fo3q1790::Glyph& g) {
    // Legacy field name retained in q1790 for ABI/source stability.
    return g.xOffset;
}

inline float RightKerningQ1880(const fo3q1790::Glyph& g) {
    // Legacy field name retained in q1790 for ABI/source stability.
    return g.yOffset;
}

inline float AscentQ1880(const fo3q1790::Glyph& g) {
    // Legacy field name retained in q1790 for ABI/source stability.
    return g.advance;
}

inline float GlyphAdvanceQ1880(const fo3q1790::Glyph& g) {
    return g.width + RightKerningQ1880(g);
}

inline float TextWidthQ1880(const fo3hudq1850::HudStateQ1850& s,
                            const char* text,
                            size_t length) {
    float width = 0.0f;
    for (size_t i = 0u; i < length; ++i) {
        width += GlyphAdvanceQ1880(s.glyphs[static_cast<unsigned char>(text[i])]);
    }
    return width;
}

inline bool BuildPromptGeometryQ1880(const char* promptChars) {
    fo3hudq1850::HudStateQ1850& s = fo3hudq1850::StateQ1850();
    if (!s.ready || !promptChars || !promptChars[0]) return false;

    const size_t length = fo3hudq1850::PromptLengthQ1850(promptChars);
    if (length == 0u) return false;
    if (fo3hudq1850::SameBuiltPromptQ1850(s, promptChars, length) && s.textCount > 0) {
        return true;
    }

    // Exact authored text_box.xml / HUDMainMenu Info values already recovered:
    // dynamic width + 20 horbuf, 10 verbuf, centre justify, glow -4/+7,
    // and the 75x75 Xbox button on the left.
    const float textWidth = TextWidthQ1880(s, promptChars, length);
    const float textHeight = s.lineHeight;
    const float boxWidth = textWidth + 20.0f;
    const float boxHeight = textHeight + 10.0f;
    const float boxX = 0.0f;
    const float textCenterX = boxX + boxWidth * 0.5f - 4.0f;
    const float textTopY = (boxHeight - textHeight) * 0.5f + 7.0f;
    const float buttonX0 = boxX - 65.0f;
    const float buttonY0 = (boxHeight - 75.0f) * 0.5f;

    const float contentMinX = buttonX0;
    const float contentMaxX = boxX + boxWidth;
    const float contentCenterX = (contentMinX + contentMaxX) * 0.5f;
    const float contentCenterY = boxHeight * 0.5f;

    // VR adaptation only: Fallout's authored 2D pixels -> world metres / arm anchor.
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
    fo3hudq1850::AddQuadQ1850(
        verts,
        X(buttonX0), Y(buttonY0),
        X(buttonX0 + 75.0f), Y(buttonY0 + 75.0f), anchorZ,
        bu0, bvTop, bu1, bvTop,
        bu0, bvBottom, bu1, bvBottom);
    s.buttonCount = static_cast<GLsizei>(verts.size()) - s.buttonFirst;

    s.textFirst = static_cast<GLsizei>(verts.size());
    float penX = textCenterX - textWidth * 0.5f;
    size_t spaces = 0u;
    float minBaseline = 1.0e30f;
    float maxBaseline = -1.0e30f;

    for (size_t i = 0u; i < length; ++i) {
        const unsigned char ch = static_cast<unsigned char>(promptChars[i]);
        const fo3q1790::Glyph& g = s.glyphs[ch];
        if (ch == static_cast<unsigned char>(' ')) ++spaces;

        const float leftKerning = LeftKerningQ1880(g);
        const float rightKerning = RightKerningQ1880(g);
        const float ascent = AscentQ1880(g);

        if (g.width > 0.0f && g.height > 0.0f) {
            // Bethesda bitmap-font bearing: top bearing is fontSize - ascent.
            // Therefore textTop + (lineHeight-ascent) + ascent is the same
            // baseline for every glyph, regardless of glyph height.
            const float gx0 = penX + leftKerning;
            const float gy0 = textTopY + (s.lineHeight - ascent);
            const float gx1 = gx0 + g.width;
            const float gy1 = gy0 + g.height;
            const float baseline = gy0 + ascent;
            minBaseline = std::min(minBaseline, baseline);
            maxBaseline = std::max(maxBaseline, baseline);

            fo3hudq1850::AddQuadQ1850(
                verts, X(gx0), Y(gy0), X(gx1), Y(gy1), anchorZ,
                g.uv[0], g.uv[1], g.uv[2], g.uv[3],
                g.uv[4], g.uv[5], g.uv[6], g.uv[7]);
        }

        // Authored horizontal cursor motion: width + right kerning.
        // For the vanilla space glyph this is 0 + 13 = 13 px.
        penX += g.width + rightKerning;
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
    const float spaceAdvance = GlyphAdvanceQ1880(space);
    const float baselineSpread =
        (minBaseline <= maxBaseline) ? (maxBaseline - minBaseline) : 0.0f;

    __android_log_print(
        ANDROID_LOG_INFO, kTagQ1880,
        "Q16.16 FNT METRICS: text=\"%.*s\" chars=%zu spaces=%zu textWidth=%.1f box=(%.1fx%.1f) spaceAdvance=%.1f baselineSpread=%.3f semantics=left-right-kerning+ascent",
        static_cast<int>(copyLength), s.builtPrompt.data(), copyLength,
        spaces, textWidth, boxWidth, boxHeight, spaceAdvance, baselineSpread);
    return true;
}

inline void RenderQ1880(const float* mvp, const char* promptChars) {
    if (!mvp || !promptChars || !promptChars[0]) return;

    fo3hudq1850::GlStateGuardQ1850 guard;
    glActiveTexture(GL_TEXTURE0);
    if (!fo3hudq1850::EnsureResourcesQ1850()) return;
    if (!BuildPromptGeometryQ1880(promptChars)) return;

    fo3hudq1850::HudStateQ1850& s = fo3hudq1850::StateQ1850();
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

    static bool loggedDraw = false;
    if (!loggedDraw) {
        loggedDraw = true;
        __android_log_print(ANDROID_LOG_INFO, kTagQ1880,
                            "Q16.16 REAL HUD DRAW: source=Fallout3-FNT/TAI metricSemantics=kerning+ascent");
    }
}

} // namespace fo3hudq1880

inline void RenderFo3InteractionHudQ1880(const float* mvp, const char* prompt) {
    fo3hudq1880::RenderQ1880(mvp, prompt);
}
