# Q16.16: fix the vertical wobble / over-wide interaction text by interpreting
# Fallout 3's actual bitmap FNT glyph metrics correctly.
#
# Q16.9 recovered the right record size/UVs, but named the final three floats
# xOffset/yOffset/advance. The vanilla bytes prove they are left kerning, right
# kerning, and ascent. Q16.13 consequently used right kerning for vertical Y and
# ascent for horizontal advance. Q16.16 keeps the exact same Bethesda assets and
# HUDMainMenu/text_box presentation, changing only those engine metric semantics.

set(Q1880_Q4_FILE "${CMAKE_CURRENT_BINARY_DIR}/q1800-q4-generated.cpp")
if(NOT EXISTS "${Q1880_Q4_FILE}")
    message(FATAL_ERROR "Q16.16 expected final OpenXR source")
endif()
file(READ "${Q1880_Q4_FILE}" Q1880_Q4_SOURCE)

# Add the corrected renderer. It reuses Q16.13's GL state sandbox, actual FNT/TEX
# atlas, actual InterfaceShared TAI/DDS button, and fixed-storage lifetime.
string(PREPEND Q1880_Q4_SOURCE "#include \"fo3-interaction-hud-q1880.h\"\n")

set(Q1880_RENDER_OLD "RenderFo3InteractionHudQ1850")
set(Q1880_RENDER_NEW "RenderFo3InteractionHudQ1880")
string(FIND "${Q1880_Q4_SOURCE}" "${Q1880_RENDER_OLD}" Q1880_RENDER_POS)
if(Q1880_RENDER_POS EQUAL -1)
    message(FATAL_ERROR "Q16.16 could not find live Q16.13 interaction HUD render call")
endif()
string(REPLACE "${Q1880_RENDER_OLD}" "${Q1880_RENDER_NEW}"
       Q1880_Q4_SOURCE "${Q1880_Q4_SOURCE}")

# Headset-visible proof Q16.15 -> Q16.16.
set(Q1880_LABEL_OLD [==[
        q1600Digit(q1600X, 0x06u); // Q16.15: 1 = B C
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x6Du); // Q16.15: 5 = A F G C D
]==])
set(Q1880_LABEL_NEW [==[
        q1600Digit(q1600X, 0x06u); // Q16.16: 1 = B C
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x7Du); // Q16.16: 6 = A F G E D C
]==])
string(FIND "${Q1880_Q4_SOURCE}" "${Q1880_LABEL_OLD}" Q1880_LABEL_POS)
if(Q1880_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.16 could not find Q16.15 build-label digits")
endif()
string(REPLACE "${Q1880_LABEL_OLD}" "${Q1880_LABEL_NEW}"
       Q1880_Q4_SOURCE "${Q1880_Q4_SOURCE}")
string(REPLACE "Q16.15 BUILD LABEL:" "Q16.16 BUILD LABEL:"
       Q1880_Q4_SOURCE "${Q1880_Q4_SOURCE}")
string(REPLACE "text=Q16.15 anchor=left-hand" "text=Q16.16 anchor=left-hand"
       Q1880_Q4_SOURCE "${Q1880_Q4_SOURCE}")
string(REPLACE "Q16.15 AUTHORED DOOR FACING" "Q16.16 AUTHORED DOOR FACING"
       Q1880_Q4_SOURCE "${Q1880_Q4_SOURCE}")

file(WRITE "${Q1880_Q4_FILE}" "${Q1880_Q4_SOURCE}")

# Configure-time proof. The old renderer may still be defined in its included
# header for teardown/back-compat, but the final OpenXR host must call Q1880.
string(FIND "${Q1880_Q4_SOURCE}" "fo3-interaction-hud-q1880.h" Q1880_INCLUDE_OK)
string(FIND "${Q1880_Q4_SOURCE}" "RenderFo3InteractionHudQ1880" Q1880_RENDER_OK)
string(FIND "${Q1880_Q4_SOURCE}" "Q16.16: 6 = A F G E D C" Q1880_LABEL_OK)
string(FIND "${Q1880_Q4_SOURCE}" "Q16.13 HUD TARGET HIDE:" Q1880_HIDE_OK)
if(Q1880_INCLUDE_OK EQUAL -1 OR Q1880_RENDER_OK EQUAL -1 OR
   Q1880_LABEL_OK EQUAL -1 OR Q1880_HIDE_OK EQUAL -1)
    message(FATAL_ERROR "Q16.16 corrected FNT renderer verification failed")
endif()

message(STATUS "Q16.16 Fallout FNT metrics enabled: left/right kerning + ascent baseline; Q16.15 live door cache retained")
