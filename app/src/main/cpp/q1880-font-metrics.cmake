# Q16.16: fix the high/low interaction letters by correcting only the vertical
# interpretation of Fallout 3's actual bitmap FNT records.
#
# The vanilla prompt glyphs share the same vertical bearing while their bitmap
# heights differ. Q16.13 applied that bearing to each glyph's top edge, producing
# a visibly wandering baseline. Q16.16 keeps the exact Q16.13 horizontal advances,
# word spacing, button, wording, scale and text_box.xml layout, but positions the
# glyph bitmaps against one FNT-derived baseline.

set(Q1880_Q4_FILE "${CMAKE_CURRENT_BINARY_DIR}/q1800-q4-generated.cpp")
if(NOT EXISTS "${Q1880_Q4_FILE}")
    message(FATAL_ERROR "Q16.16 expected final OpenXR source")
endif()
file(READ "${Q1880_Q4_FILE}" Q1880_Q4_SOURCE)

# Q16.11 generated a corrected copy of q1790.h and routed the live host to that
# generated header. Q16.16 must come AFTER that include (and after Q16.13), or
# including the source q1790 header again would emit the same fo3q1790 symbols
# twice. Insert the baseline renderer directly after Q16.13's live renderer.
set(Q1880_INCLUDE_OLD "#include \"fo3-interaction-hud-q1850.h\"")
set(Q1880_INCLUDE_NEW
    "#include \"fo3-interaction-hud-q1850.h\"\n#include \"fo3-interaction-hud-q1880.h\"")
string(FIND "${Q1880_Q4_SOURCE}" "${Q1880_INCLUDE_OLD}" Q1880_INCLUDE_POS)
if(Q1880_INCLUDE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.16 could not find Q16.13 HUD include")
endif()
string(REPLACE "${Q1880_INCLUDE_OLD}" "${Q1880_INCLUDE_NEW}"
       Q1880_Q4_SOURCE "${Q1880_Q4_SOURCE}")

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

# Configure-time proof. Q16.15's door cache and Q16.13 show/hide state machine
# must survive; only the live HUD draw target and visible build label change.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fo3-interaction-hud-q1880.h" Q1880_HUD_HEADER)
string(FIND "${Q1880_HUD_HEADER}" "Q16.16 HUD BASELINE:" Q1880_BASELINE_OK)
string(FIND "${Q1880_HUD_HEADER}" "#include \"fo3-interaction-hud-q1790.h\"" Q1880_BAD_SOURCE_INCLUDE)
string(FIND "${Q1880_Q4_SOURCE}" "fo3-interaction-hud-q1880.h" Q1880_INCLUDE_OK)
string(FIND "${Q1880_Q4_SOURCE}" "fo3-interaction-hud-q1830.h" Q1880_AUTHORED_INCLUDE_OK)
string(FIND "${Q1880_Q4_SOURCE}" "RenderFo3InteractionHudQ1880" Q1880_RENDER_OK)
string(FIND "${Q1880_Q4_SOURCE}" "Q16.16: 6 = A F G E D C" Q1880_LABEL_OK)
string(FIND "${Q1880_Q4_SOURCE}" "Q16.13 HUD TARGET HIDE:" Q1880_HIDE_OK)
if(Q1880_BASELINE_OK EQUAL -1 OR NOT Q1880_BAD_SOURCE_INCLUDE EQUAL -1 OR
   Q1880_INCLUDE_OK EQUAL -1 OR Q1880_AUTHORED_INCLUDE_OK EQUAL -1 OR
   Q1880_RENDER_OK EQUAL -1 OR Q1880_LABEL_OK EQUAL -1 OR Q1880_HIDE_OK EQUAL -1)
    message(FATAL_ERROR "Q16.16 baseline-aware HUD verification failed")
endif()

message(STATUS "Q16.16 Fallout HUD baseline enabled: vertical FNT bearing fixed; Q16.13 horizontal metrics and Q16.15 live door cache retained")

# Q16.17 needs its stream state visible to the mature scene-completion function.
# q6a-native.cpp owns one anonymous renderer namespace. Inject these declarations
# immediately INSIDE that namespace so they name the same objects whose actual
# definitions q1890 emits later beside the streaming helpers. The earlier attempt
# prepended them before namespace {, which compiled but created unrelated global
# symbols and therefore failed at link time.
set(Q1880_Q1617_NATIVE_FILE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp")
file(READ "${Q1880_Q1617_NATIVE_FILE}" Q1880_Q1617_NATIVE_SOURCE)
string(FIND "${Q1880_Q1617_NATIVE_SOURCE}"
       "PrimeFo3AuthoredDoorAnchorsQ1870(gCurrentCellFormId);"
       Q1880_Q1617_CACHE_PRIME_OK)
if(Q1880_Q1617_CACHE_PRIME_OK EQUAL -1)
    message(FATAL_ERROR "Q16.17 prerequisite missing: Q16.15 cache-prime hook")
endif()
set(Q1880_Q1617_FORWARD_DECLS [==[
extern const float Q1890_EXTERIOR_CELL_SIZE;
extern bool gExteriorStreamingActiveQ1890;
extern bool gExteriorStreamBusyQ1890;
extern uint32_t gExteriorWorldspaceQ1890;
extern uint32_t gExteriorPersistentCellQ1890;
extern float gExteriorOriginXQ1890;
extern float gExteriorOriginYQ1890;
extern float gExteriorOriginZQ1890;
extern int32_t gExteriorWindowGridXQ1890;
extern int32_t gExteriorWindowGridYQ1890;
extern uint64_t gExteriorWindowGenerationQ1890;

]==])
set(Q1880_Q1617_NAMESPACE_MARKER "namespace {\n")
string(FIND "${Q1880_Q1617_NATIVE_SOURCE}" "${Q1880_Q1617_NAMESPACE_MARKER}"
       Q1880_Q1617_NAMESPACE_POS)
if(Q1880_Q1617_NAMESPACE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.17 prerequisite missing: q6h renderer namespace")
endif()
string(REPLACE "${Q1880_Q1617_NAMESPACE_MARKER}"
       "${Q1880_Q1617_NAMESPACE_MARKER}${Q1880_Q1617_FORWARD_DECLS}"
       Q1880_Q1617_NATIVE_SOURCE "${Q1880_Q1617_NATIVE_SOURCE}")
string(APPEND Q1880_Q1617_NATIVE_SOURCE
       "\n// Q16.15 XTEL CACHE: verified by PrimeFo3AuthoredDoorAnchorsQ1870 live hook.\n")
file(WRITE "${Q1880_Q1617_NATIVE_FILE}" "${Q1880_Q1617_NATIVE_SOURCE}")

# Q16.17 starts continuous exterior traversal. Keep Q16.16 presentation intact
# and move only the authored exterior CELL/LAND/collision selection window.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1890-cell-streaming.cmake")
