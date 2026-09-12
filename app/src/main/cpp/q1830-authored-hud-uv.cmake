# Q16.11: fix the real Fallout 3 interaction HUD atlas orientation.
#
# Verified directly against the user's extracted vanilla files:
#   Baked-in_Monofonto_Large.fnt + its .tex atlas
#   InterfaceShared.tai + InterfaceShared0.dds
# The FNT/TAI V coordinates already address the correct rows for our raw GLES
# upload. Q16.9 incorrectly applied an extra 1-v conversion, which sampled the
# wrong packed glyph/sprite regions and produced jumbled characters.

set(Q1830_HUD_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/fo3-interaction-hud-q1790.h")
if(NOT EXISTS "${Q1830_HUD_INPUT}")
    message(FATAL_ERROR "Q16.11 expected Q16.9 interaction HUD header")
endif()
file(READ "${Q1830_HUD_INPUT}" Q1830_HUD_SOURCE)

set(Q1830_BUTTON_OLD [==[
    const float bvTop = 1.0f - button.v;
    const float bvBottom = 1.0f - (button.v + button.h);
]==])
set(Q1830_BUTTON_NEW [==[
    // InterfaceShared.tai is top-row addressed for the raw atlas upload used by
    // FalloutQuest. Use the authored values directly; do not invent a V flip.
    const float bvTop = button.v;
    const float bvBottom = button.v + button.h;
]==])
string(FIND "${Q1830_HUD_SOURCE}" "${Q1830_BUTTON_OLD}" Q1830_BUTTON_POS)
if(Q1830_BUTTON_POS EQUAL -1)
    message(FATAL_ERROR "Q16.11 could not find Q16.9 button V conversion")
endif()
string(REPLACE "${Q1830_BUTTON_OLD}" "${Q1830_BUTTON_NEW}"
       Q1830_HUD_SOURCE "${Q1830_HUD_SOURCE}")

set(Q1830_GLYPH_OLD [==[
                const float uTL = g.uv[0], vTL = 1.0f - g.uv[1];
                const float uTR = g.uv[2], vTR = 1.0f - g.uv[3];
                const float uBL = g.uv[4], vBL = 1.0f - g.uv[5];
                const float uBR = g.uv[6], vBR = 1.0f - g.uv[7];
]==])
set(Q1830_GLYPH_NEW [==[
                // Baked-in_Monofonto_Large.fnt stores the four authored atlas
                // UV corners directly. The raw .tex upload preserves that row
                // convention, so use the V values exactly as stored.
                const float uTL = g.uv[0], vTL = g.uv[1];
                const float uTR = g.uv[2], vTR = g.uv[3];
                const float uBL = g.uv[4], vBL = g.uv[5];
                const float uBR = g.uv[6], vBR = g.uv[7];
]==])
string(FIND "${Q1830_HUD_SOURCE}" "${Q1830_GLYPH_OLD}" Q1830_GLYPH_POS)
if(Q1830_GLYPH_POS EQUAL -1)
    message(FATAL_ERROR "Q16.11 could not find Q16.9 font V conversion")
endif()
string(REPLACE "${Q1830_GLYPH_OLD}" "${Q1830_GLYPH_NEW}"
       Q1830_HUD_SOURCE "${Q1830_HUD_SOURCE}")

# Make the runtime proof unambiguous without changing presentation.
string(REPLACE
    "Q16.9 REAL HUD DRAW: source=HUDMainMenu/Info button=glow_general_button_a.dds font=Baked-in_Monofonto_Large text=Open/Door"
    "Q16.11 REAL HUD DRAW: source=HUDMainMenu/Info button=glow_general_button_a.dds font=Baked-in_Monofonto_Large text=Open/Door uv=authored-no-flip"
    Q1830_HUD_SOURCE "${Q1830_HUD_SOURCE}")

set(Q1830_HUD_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/fo3-interaction-hud-q1830.h")
file(WRITE "${Q1830_HUD_OUTPUT}" "${Q1830_HUD_SOURCE}")

# The final OpenXR source includes the Q16.9 header by name. Route only that
# include to the generated corrected header; no UI geometry/layout is recreated.
set(Q1830_Q4_FILE "${CMAKE_CURRENT_BINARY_DIR}/q1800-q4-generated.cpp")
if(NOT EXISTS "${Q1830_Q4_FILE}")
    message(FATAL_ERROR "Q16.11 expected final q1800 OpenXR source")
endif()
file(READ "${Q1830_Q4_FILE}" Q1830_Q4_SOURCE)
set(Q1830_INCLUDE_OLD "#include \"fo3-interaction-hud-q1790.h\"")
set(Q1830_INCLUDE_NEW "#include \"${Q1830_HUD_OUTPUT}\"")
string(FIND "${Q1830_Q4_SOURCE}" "${Q1830_INCLUDE_OLD}" Q1830_INCLUDE_POS)
if(Q1830_INCLUDE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.11 could not find live Q16.9 HUD include")
endif()
string(REPLACE "${Q1830_INCLUDE_OLD}" "${Q1830_INCLUDE_NEW}"
       Q1830_Q4_SOURCE "${Q1830_Q4_SOURCE}")
file(WRITE "${Q1830_Q4_FILE}" "${Q1830_Q4_SOURCE}")

# Configure-time proof against the exact bug: no 1-v glyph/button conversion
# remains in the generated HUD, and the compiled host routes to it.
string(FIND "${Q1830_HUD_SOURCE}" "vTL = g.uv[1];" Q1830_FONT_OK)
string(FIND "${Q1830_HUD_SOURCE}" "const float bvTop = button.v;" Q1830_BUTTON_OK)
string(FIND "${Q1830_HUD_SOURCE}" "1.0f - g.uv[1]" Q1830_BAD_FONT)
string(FIND "${Q1830_HUD_SOURCE}" "1.0f - button.v" Q1830_BAD_BUTTON)
string(FIND "${Q1830_Q4_SOURCE}" "fo3-interaction-hud-q1830.h" Q1830_ROUTE_OK)
if(Q1830_FONT_OK EQUAL -1 OR Q1830_BUTTON_OK EQUAL -1 OR
   NOT Q1830_BAD_FONT EQUAL -1 OR NOT Q1830_BAD_BUTTON EQUAL -1 OR
   Q1830_ROUTE_OK EQUAL -1)
    message(FATAL_ERROR "Q16.11 authored HUD UV verification failed")
endif()

message(STATUS "Q16.11 authored HUD UV enabled: vanilla FNT/TAI coordinates used directly, no vertical flip")

# Q16.12 replaces the static two-line proof widget with Fallout3.esm-derived
# load-door wording, the actual single-string text_box.xml layout, and a GL-state
# sandbox that makes target show/hide a resource-stable operation.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1840-authored-door-prompt.cmake")
