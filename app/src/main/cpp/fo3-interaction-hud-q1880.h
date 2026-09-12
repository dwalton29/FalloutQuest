#pragma once

// Q16.16: baseline-aware Fallout 3 HUDMainMenu/Info text geometry.
//
// Q16.13 correctly reads the user's vanilla Baked-in_Monofonto_Large FNT and
// uses the authored horizontal advances, including the special 13px space.
// Device imagery exposed one remaining interpretation bug: Q16.13 treated the
// per-glyph vertical offset as a top-edge offset. In the actual font records the
// visible prompt glyphs share the same vertical bearing (-10) while their bitmap
// heights vary (for example O=36, e=29, t=33). Top-anchoring those bitmaps makes
// their bottoms wander, matching the high/low text seen in-headset.
//
// Q16.16 preserves every horizontal metric and every HUDMainMenu/text_box.xml
// presentation value. It changes only vertical placement: derive one baseline
// from the tallest drawable glyph and the FNT's vertical bearing, then place each
// glyph upward from that baseline. This corrects our interpretation of the real
// FNT data; it does not invent replacement font metrics.
//
// IMPORTANT: this header is included only after Q16.11's generated HUD header.
// That generated header is the single live definition of the fo3q1790 FNT/TAI
// types. Do not include q1790.h here or those definitions are emitted twice.

#include "fo3-interaction-hud-q1850.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace fo3hudq1880 {

constexpr const char* kTagQ1880 = "FalloutQuest";

inline bool BuildPromptGeometryQ1880(const char* promptChars) {
    fo3hudq1850::HudStateQ1850& s = fo3hudq1850::StateQ1850();
    if (!s.ready || !promptChars || !promptChars[0]) return false;

    const size_t length = fo3hudq1850::PromptLengthQ1850(promptChars);
    if (length == 0u) return false;
    if (fo3hudq1850::SameBuiltPromptQ1850(s, promptChars, length) && s.textCount > 0) {
        return true;
    }

    // Exact authored text_box.xml / HUDMainMenu Info values already recovered.
    // Keep Q16.13's horizontal FNT interpretation byte-for-byte in this pass.
    const float textWidth = fo3hudq1850::TextWidthQ1850(s, promptChars, length);
    const float textHeight = s.lineHeight;
    const float boxWidth = textWidth + 20.0f;       // _horbuf
    const float boxHeight = textHeight + 10.0f;    // HUD Info _verbuf override
    const float boxX = 0.0f;
    const float textCenterX = boxX + boxWidth * 0.5f - 4.0f; // glow correction
    const float textTopY = (boxHeight - textHeight) * 0.5f + 7.0f;
    const float buttonX0 = boxX - 65.0f;            // 75 - 10 prefab overlap
    const float buttonY0 = (boxHeight - 75.0f) * 0.5f;

    // Establish one baseline from the tallest drawable glyph in this string and
    // its actual FNT vertical bearing. The current prompt's visible glyphs all
    // carry yOffset=-10 while their bitmap heights differ, so shorter lowercase
    // letters now end on the same baseline instead of sharing a common top edge.
    float maxDrawableHeight = 0.0f;
    float referenceYOffset = 0.0f;
    for (size_t i = 0u; i < length; ++i) {
        const unsigned char ch = static_cast<unsigned char>(promptChars[i]);
        const fo3q1790::Glyph& g = s.glyphs[ch];
        if (g.width > 0.0f && g.height > maxDrawableHeight) {
            maxDrawableHeight = g.height;
            referenceYOffset = g.yOffset;
        }
    }
    if (maxDrawableHeight <= 0.0f) return false;
    const float baselineY = textTopY + referenceYOffset + maxDrawableHeight;

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
    float minGlyphBottom = 1.0e30f;
    float maxGlyphBottom = -1.0e30f;

    for (size_t i = 0u; i < length; ++i) {
        const unsigned char ch = static_cast<unsigned char>(promptChars[i]);
        const fo3q1790::Glyph& g = s.glyphs[ch];

        if (g.width > 0.0f && g.height > 0.0f) {
            const float gx0 = penX + g.xOffset;
            const float glyphBottomY = baselineY + (g.yOffset - referenceYOffset);
            const float gy0 = glyphBottomY - g.height;
            const float gx1 = gx0 + g.width;
            const float gy1 = glyphBottomY;

            minGlyphBottom = std::min(minGlyphBottom, glyphBottomY);
            maxGlyphBottom = std::max(maxGlyphBottom, glyphBottomY);

            fo3hudq1850::AddQuadQ1850(
                verts, X(gx0), Y(gy0), X(gx1), Y(gy1), anchorZ,
                g.uv[0], g.uv[1], g.uv[2], g.uv[3],
                g.uv[4], g.uv[5], g.uv[6], g.uv[7]);
        }

        // Deliberately unchanged from Q16.13: preserve the actual FNT horizontal
        // advance and its special authored space handling until visually retested.
        penX += fo3hudq1850::GlyphAdvanceQ1850(s, ch);
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

    const float bottomSpread =
        (minGlyphBottom <= maxGlyphBottom) ? (maxGlyphBottom - minGlyphBottom) : 0.0f;
    __android_log_print(
        ANDROID_LOG_INFO, kTagQ1880,
        "Q16.16 HUD BASELINE: text=\"%.*s\" maxHeight=%.1f referenceYOffset=%.1f baselineY=%.1f textTopY=%.1f bottomSpread=%.3f horizontal=Q16.13-authored source=vanilla-FNT",
        static_cast<int>(copyLength), s.builtPrompt.data(),
        maxDrawableHeight, referenceYOffset, baselineY, textTopY, bottomSpread);
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
                            "Q16.16 REAL HUD DRAW: source=Fallout3-FNT/TAI vertical=shared-baseline horizontal=Q16.13-authored");
    }
}

} // namespace fo3hudq1880

inline void RenderFo3InteractionHudQ1880(const float* mvp, const char* prompt) {
    fo3hudq1880::RenderQ1880(mvp, prompt);
}
