# Q16.2: comfortable Fallout 3 loading presentation for VR.
#
# Q16.0's CELL/XTEL transition, LSCR selection, DDS upload and loading-state
# lifetime remain authoritative. Q16.2 changes presentation only by routing the
# final eye draw through fo3-loading-comfort-q1720.h:
#
#   - 62% centered Fallout3.esm loading artwork instead of an 86% eye-filling card
#   - identical per-eye placement (zero forced disparity) so it reads optically far
#     away rather than forcing uncomfortable near convergence
#   - Fallout HUDMain-green clock/compass loading indicator beneath the artwork
#   - continuously rotating hand inspired by meshes\interface\loading\loadinganim01.nif
#
# Keeping this as a wrapper around Q16.0 avoids duplicating or replacing its
# inline loading globals, which are shared by the OpenXR host and scene renderer.

set(Q1720_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1720_Q4_INPUT}")
    message(FATAL_ERROR "Q16.2 expected final OpenXR source at ${Q1720_Q4_INPUT}")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fo3-loading-comfort-q1720.h")
    message(FATAL_ERROR "Q16.2 loading comfort renderer header is missing")
endif()
file(READ "${Q1720_Q4_INPUT}" Q1720_Q4_SOURCE)

# Include the isolated Q16.2 renderer first. It includes Q16.0's loading header,
# so any later include of that file is harmless via #pragma once.
string(PREPEND Q1720_Q4_SOURCE
       "#include \"fo3-loading-comfort-q1720.h\"\n")

# Route only the actual loading draw to Q16.2. Prepare/selection/state gates stay
# on Q1700 exactly as before.
set(Q1720_RENDER_OLD
    "RenderFo3LoadingScreenQ1700(framebuffer_, eye.width, eye.height);")
set(Q1720_RENDER_NEW
    "RenderFo3LoadingScreenQ1720(framebuffer_, eye.width, eye.height);")
string(FIND "${Q1720_Q4_SOURCE}" "${Q1720_RENDER_OLD}" Q1720_RENDER_POS)
if(Q1720_RENDER_POS EQUAL -1)
    message(FATAL_ERROR "Q16.2 could not find Q16.0 loading draw call")
endif()
string(REPLACE "${Q1720_RENDER_OLD}" "${Q1720_RENDER_NEW}"
       Q1720_Q4_SOURCE "${Q1720_Q4_SOURCE}")

# Visible in-headset build proof: Q16.1 -> Q16.2 on the left controller. Change
# only the final seven-segment digit and the build-label log identity; the Q16.1
# interaction HUD itself remains untouched.
set(Q1720_LABEL_OLD [==[
        q1600Digit(q1600X, 0x06u); // Q16.1: 1 = B C
]==])
set(Q1720_LABEL_NEW [==[
        q1600Digit(q1600X, 0x5Bu); // Q16.2: 2 = A B G E D
]==])
string(FIND "${Q1720_Q4_SOURCE}" "${Q1720_LABEL_OLD}" Q1720_LABEL_POS)
if(Q1720_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.2 could not find Q16.1 final build-label digit")
endif()
string(REPLACE "${Q1720_LABEL_OLD}" "${Q1720_LABEL_NEW}"
       Q1720_Q4_SOURCE "${Q1720_Q4_SOURCE}")
string(REPLACE "Q16.1 BUILD LABEL:" "Q16.2 BUILD LABEL:"
       Q1720_Q4_SOURCE "${Q1720_Q4_SOURCE}")
string(REPLACE "text=Q16.1 anchor=left-hand" "text=Q16.2 anchor=left-hand"
       Q1720_Q4_SOURCE "${Q1720_Q4_SOURCE}")

file(WRITE "${Q1720_Q4_INPUT}" "${Q1720_Q4_SOURCE}")

# Configure-time hard guards. Q16.2 must be an additive presentation layer, not
# a silent fallback to Q16.0's eye-filling renderer.
string(FIND "${Q1720_Q4_SOURCE}" "fo3-loading-comfort-q1720.h" Q1720_INCLUDE_OK)
string(FIND "${Q1720_Q4_SOURCE}" "RenderFo3LoadingScreenQ1720(framebuffer_, eye.width, eye.height);" Q1720_DRAW_OK)
string(FIND "${Q1720_Q4_SOURCE}" "RenderFo3LoadingScreenQ1700(framebuffer_, eye.width, eye.height);" Q1720_OLD_DRAW)
string(FIND "${Q1720_Q4_SOURCE}" "Q16.2: 2 = A B G E D" Q1720_LABEL_OK)
string(FIND "${Q1720_Q4_SOURCE}" "text=Q16.2 anchor=left-hand" Q1720_LABEL_TEXT_OK)
if(Q1720_INCLUDE_OK EQUAL -1 OR Q1720_DRAW_OK EQUAL -1 OR
   NOT Q1720_OLD_DRAW EQUAL -1 OR Q1720_LABEL_OK EQUAL -1 OR
   Q1720_LABEL_TEXT_OK EQUAL -1)
    message(FATAL_ERROR
        "Q16.2 verification failed: include=${Q1720_INCLUDE_OK} draw=${Q1720_DRAW_OK} oldDraw=${Q1720_OLD_DRAW} label=${Q1720_LABEL_OK} labelText=${Q1720_LABEL_TEXT_OK}")
endif()

message(STATUS "Q16.2 loading comfort enabled: 62% optically-distant LSCR + rotating HUDMain clock/compass")
