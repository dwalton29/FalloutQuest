# Q16.1: replace Q16.0's temporary single-pass green interaction prompt with
# Fallout 3 HUDMainMenu's vanilla Info-widget presentation semantics.
#
# Vanilla hud_main_menu.xml defines Info as a 320x80 HUDMain-colour region with
# centered text, glow enabled, full line alpha, the Xbox A button on the left,
# and a 10px vertical buffer. FalloutQuest keeps the interaction in right-hand
# world space for VR, but mirrors those visual semantics here.
#
# Q16.0 CELL/XTEL traversal and input are deliberately untouched.

set(Q1710_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1710_Q4_INPUT}")
    message(FATAL_ERROR "Q16.1 expected final OpenXR source at ${Q1710_Q4_INPUT}")
endif()
file(READ "${Q1710_Q4_INPUT}" Q1710_Q4_SOURCE)

# Change the visible left-hand build proof from Q16.0 to Q16.1.
set(Q1710_LABEL_OLD [==[
        q1600Digit(q1600X, 0x3Fu); // Q16.0: 0 = A B C D E F
]==])
set(Q1710_LABEL_NEW [==[
        q1600Digit(q1600X, 0x06u); // Q16.1: 1 = B C
]==])
string(FIND "${Q1710_Q4_SOURCE}" "${Q1710_LABEL_OLD}" Q1710_LABEL_POS)
if(Q1710_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.1 could not find Q16.0 final build-label digit")
endif()
string(REPLACE "${Q1710_LABEL_OLD}" "${Q1710_LABEL_NEW}"
       Q1710_Q4_SOURCE "${Q1710_Q4_SOURCE}")
string(REPLACE "Q16.0" "Q16.1" Q1710_Q4_SOURCE "${Q1710_Q4_SOURCE}")

# Q16.0 already builds compact A + OPEN vector glyphs in right-controller local
# space. Add the missing Xbox-style button ring around the A. This keeps the
# existing glyph baseline while changing the visual language from debug text to
# the vanilla text_box/Info button-on-left presentation.
set(Q1710_GEOMETRY_OLD [==[
        // A
        q1700Line(q1700X, q1700Y, q1700X + q1700W * 0.5f, q1700Y + q1700H);
]==])
set(Q1710_GEOMETRY_NEW [==[
        // Q16.1 vanilla Info widget: Xbox A button glyph on the LEFT.
        // The UI XML resolves &xbuttona; through text_box.xml. FalloutQuest has
        // no Gamebryo UI runtime, so reproduce the authored circular controller
        // glyph around the same A strokes in the HUD's 320:80 (~4:1) layout.
        {
            constexpr int kButtonSegments = 24;
            const float cx = q1700X + q1700W * 0.5f;
            const float cy = q1700Y + q1700H * 0.5f;
            const float rx = q1700W * 0.79f;
            const float ry = q1700H * 0.58f;
            for (int i = 0; i < kButtonSegments; ++i) {
                const float a0 = (6.28318530718f * static_cast<float>(i)) /
                                 static_cast<float>(kButtonSegments);
                const float a1 = (6.28318530718f * static_cast<float>(i + 1)) /
                                 static_cast<float>(kButtonSegments);
                q1700Line(cx + std::cos(a0) * rx, cy + std::sin(a0) * ry,
                          cx + std::cos(a1) * rx, cy + std::sin(a1) * ry);
            }
        }

        // A
        q1700Line(q1700X, q1700Y, q1700X + q1700W * 0.5f, q1700Y + q1700H);
]==])
string(FIND "${Q1710_Q4_SOURCE}" "${Q1710_GEOMETRY_OLD}" Q1710_GEOMETRY_POS)
if(Q1710_GEOMETRY_POS EQUAL -1)
    message(FATAL_ERROR "Q16.1 could not find Q16.0 A prompt geometry")
endif()
string(REPLACE "${Q1710_GEOMETRY_OLD}" "${Q1710_GEOMETRY_NEW}"
       Q1710_Q4_SOURCE "${Q1710_Q4_SOURCE}")

# Replace Q16.0's one bright debug draw with the vanilla HUDMain treatment.
# Fallout 3's default iSystemColorHUDMain is RGB(26,255,128). The original Info
# control has _glow=true and _line_alpha=255. The existing tiny OpenXR line
# shader is RGB-only, so the halo is reproduced as dim additive RGB passes,
# followed by the full-strength core.
set(Q1710_DRAW_OLD [==[
                    if (doorAimActiveQ1700_) {
                        glDepthFunc(GL_ALWAYS);
                        glLineWidth(2.0f);
                        SetMvpAndColor(mvp, 0.20f, 1.0f, 0.30f);
                        glDrawArrays(GL_LINES, interactionStartVertex_, interactionVertexCount_);
                        glDepthFunc(GL_LEQUAL);
                    }
]==])
set(Q1710_DRAW_NEW [==[
                    if (doorAimActiveQ1700_) {
                        // hud_main_menu.xml -> Info -> text_box.xml semantics:
                        // HUDMain system colour, glow=true, line_alpha=255,
                        // xbuttona on the left. Keep it hand-anchored for VR.
                        const GLboolean q1710BlendWasEnabled = glIsEnabled(GL_BLEND);
                        GLint q1710BlendSrcRgb = GL_ONE;
                        GLint q1710BlendDstRgb = GL_ZERO;
                        GLint q1710BlendSrcAlpha = GL_ONE;
                        GLint q1710BlendDstAlpha = GL_ZERO;
                        glGetIntegerv(GL_BLEND_SRC_RGB, &q1710BlendSrcRgb);
                        glGetIntegerv(GL_BLEND_DST_RGB, &q1710BlendDstRgb);
                        glGetIntegerv(GL_BLEND_SRC_ALPHA, &q1710BlendSrcAlpha);
                        glGetIntegerv(GL_BLEND_DST_ALPHA, &q1710BlendDstAlpha);

                        glDepthFunc(GL_ALWAYS);
                        glEnable(GL_BLEND);
                        glBlendFunc(GL_ONE, GL_ONE);

                        // Soft Gamebryo-style glow using the RGB-only line shader.
                        glLineWidth(7.0f);
                        SetMvpAndColor(mvp,
                                       (26.0f / 255.0f) * 0.10f,
                                       0.10f,
                                       (128.0f / 255.0f) * 0.10f);
                        glDrawArrays(GL_LINES, interactionStartVertex_, interactionVertexCount_);
                        glLineWidth(4.0f);
                        SetMvpAndColor(mvp,
                                       (26.0f / 255.0f) * 0.22f,
                                       0.22f,
                                       (128.0f / 255.0f) * 0.22f);
                        glDrawArrays(GL_LINES, interactionStartVertex_, interactionVertexCount_);

                        // Full-alpha vanilla HUDMain core: RGB(26,255,128).
                        glBlendFunc(GL_ONE, GL_ZERO);
                        glLineWidth(2.0f);
                        SetMvpAndColor(mvp, 26.0f / 255.0f, 1.0f, 128.0f / 255.0f);
                        glDrawArrays(GL_LINES, interactionStartVertex_, interactionVertexCount_);

                        glBlendFuncSeparate(q1710BlendSrcRgb, q1710BlendDstRgb,
                                            q1710BlendSrcAlpha, q1710BlendDstAlpha);
                        if (!q1710BlendWasEnabled) glDisable(GL_BLEND);
                        glLineWidth(1.0f);
                        glDepthFunc(GL_LEQUAL);
                    }
]==])
string(FIND "${Q1710_Q4_SOURCE}" "${Q1710_DRAW_OLD}" Q1710_DRAW_POS)
if(Q1710_DRAW_POS EQUAL -1)
    message(FATAL_ERROR "Q16.1 could not find Q16.0 right-hand prompt draw")
endif()
string(REPLACE "${Q1710_DRAW_OLD}" "${Q1710_DRAW_NEW}"
       Q1710_Q4_SOURCE "${Q1710_Q4_SOURCE}")

file(WRITE "${Q1710_Q4_INPUT}" "${Q1710_Q4_SOURCE}")

string(FIND "${Q1710_Q4_SOURCE}" "kButtonSegments = 24" Q1710_RING_OK)
string(FIND "${Q1710_Q4_SOURCE}" "26.0f / 255.0f" Q1710_COLOR_OK)
string(FIND "${Q1710_Q4_SOURCE}" "glBlendFunc(GL_ONE, GL_ONE)" Q1710_GLOW_OK)
string(FIND "${Q1710_Q4_SOURCE}" "Q16.1: 1 = B C" Q1710_LABEL_OK)
if(Q1710_RING_OK EQUAL -1 OR Q1710_COLOR_OK EQUAL -1 OR
   Q1710_GLOW_OK EQUAL -1 OR Q1710_LABEL_OK EQUAL -1)
    message(FATAL_ERROR
        "Q16.1 HUD verification failed: ring=${Q1710_RING_OK} color=${Q1710_COLOR_OK} glow=${Q1710_GLOW_OK} label=${Q1710_LABEL_OK}")
endif()

message(STATUS "Q16.1 vanilla HUDMain Info interaction enabled: [A] OPEN, HUDMain RGB(26,255,128), glow=true")

# Q16.2 keeps the interaction/traversal work above intact and changes only the
# loading-screen presentation for comfortable VR viewing.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1720-loading-screen-comfort.cmake")
