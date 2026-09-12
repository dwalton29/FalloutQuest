# Q16.6: replace Q16.2's guessed card/clock presentation with a loading menu
# reconstructed from Fallout - Misc.bsa / menus/loading_menu.xml.
#
# Q16.0 still owns LSCR selection, texture decoding and transition lifetime.
# Q16.4 still owns gate targeting; Q16.5 still owns the arm interaction HUD.

set(Q1760_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1760_Q4_INPUT}")
    message(FATAL_ERROR "Q16.6 expected final OpenXR source at ${Q1760_Q4_INPUT}")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fo3-loading-vanilla-q1760.h")
    message(FATAL_ERROR "Q16.6 vanilla loading renderer header is missing")
endif()
file(READ "${Q1760_Q4_INPUT}" Q1760_Q4_SOURCE)

# Load Q16.6's isolated presentation wrapper. It reuses Q16.0's inline LSCR
# state/texture cache but owns the final eye composition.
string(PREPEND Q1760_Q4_SOURCE
       "#include \"fo3-loading-vanilla-q1760.h\"\n")

set(Q1760_RENDER_OLD
    "RenderFo3LoadingScreenQ1720(framebuffer_, eye.width, eye.height);")
set(Q1760_RENDER_NEW
    "RenderFo3LoadingScreenQ1760(framebuffer_, eye.width, eye.height);")
string(FIND "${Q1760_Q4_SOURCE}" "${Q1760_RENDER_OLD}" Q1760_RENDER_POS)
if(Q1760_RENDER_POS EQUAL -1)
    message(FATAL_ERROR "Q16.6 could not find Q16.2 loading draw call")
endif()
string(REPLACE "${Q1760_RENDER_OLD}" "${Q1760_RENDER_NEW}"
       Q1760_Q4_SOURCE "${Q1760_Q4_SOURCE}")

# If the generated host explicitly shuts down Q16.0's loading resources, route
# through Q16.6 so its GL objects are also released. This is optional because the
# host lifetime currently ends with the GL context anyway.
string(REPLACE
    "ShutdownFo3LoadingScreenQ1720();"
    "ShutdownFo3LoadingScreenQ1760();"
    Q1760_Q4_SOURCE "${Q1760_Q4_SOURCE}")

# Visible build proof: Q16.5 -> Q16.6.
set(Q1760_LABEL_OLD [==[
        q1600Digit(q1600X, 0x6Du); // Q16.5: 5 = A F G C D
]==])
set(Q1760_LABEL_NEW [==[
        q1600Digit(q1600X, 0x7Du); // Q16.6: 6 = A F G E D C
]==])
string(FIND "${Q1760_Q4_SOURCE}" "${Q1760_LABEL_OLD}" Q1760_LABEL_POS)
if(Q1760_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.6 could not find Q16.5 final build-label digit")
endif()
string(REPLACE "${Q1760_LABEL_OLD}" "${Q1760_LABEL_NEW}"
       Q1760_Q4_SOURCE "${Q1760_Q4_SOURCE}")
string(REPLACE "Q16.5 BUILD LABEL:" "Q16.6 BUILD LABEL:"
       Q1760_Q4_SOURCE "${Q1760_Q4_SOURCE}")
string(REPLACE "text=Q16.5 anchor=left-hand" "text=Q16.6 anchor=left-hand"
       Q1760_Q4_SOURCE "${Q1760_Q4_SOURCE}")

file(WRITE "${Q1760_Q4_INPUT}" "${Q1760_Q4_SOURCE}")

# Hard guards: Q16.6 must supersede Q16.2's draw while preserving the Q16.5 HUD
# and the rest of the transition stack.
string(FIND "${Q1760_Q4_SOURCE}" "fo3-loading-vanilla-q1760.h" Q1760_INCLUDE_OK)
string(FIND "${Q1760_Q4_SOURCE}" "RenderFo3LoadingScreenQ1760(framebuffer_, eye.width, eye.height);" Q1760_DRAW_OK)
string(FIND "${Q1760_Q4_SOURCE}" "RenderFo3LoadingScreenQ1720(framebuffer_, eye.width, eye.height);" Q1760_OLD_DRAW)
string(FIND "${Q1760_Q4_SOURCE}" "Q16.6: 6 = A F G E D C" Q1760_LABEL_OK)
string(FIND "${Q1760_Q4_SOURCE}" "q1750ButtonSegments = 32" Q1760_HUD_OK)
if(Q1760_INCLUDE_OK EQUAL -1 OR Q1760_DRAW_OK EQUAL -1 OR
   NOT Q1760_OLD_DRAW EQUAL -1 OR Q1760_LABEL_OK EQUAL -1 OR
   Q1760_HUD_OK EQUAL -1)
    message(FATAL_ERROR
        "Q16.6 verification failed: include=${Q1760_INCLUDE_OK} draw=${Q1760_DRAW_OK} old=${Q1760_OLD_DRAW} label=${Q1760_LABEL_OK} hud=${Q1760_HUD_OK}")
endif()

message(STATUS "Q16.6 vanilla loading menu enabled: 1280x960 canvas, full-screen LSCR, menufade=0.75, 54x54 circular loading01 pinwheel")

# Q16.7 keeps the vanilla loading/menu and arm HUD intact, but makes the first
# Capital Wasteland transition local enough for Quest and temporarily removes
# the closed Megaton entrance assembly so open-world walking can be tested.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1770-megaton-exterior-unblock.cmake")
