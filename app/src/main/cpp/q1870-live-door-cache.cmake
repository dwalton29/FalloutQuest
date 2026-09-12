# Q16.15: eliminate the live controller-ray hang exposed by the Q16.14 log.
#
# Device proof:
#   HUD TARGET SHOW at 19:36:42.828
#   app FPS immediately falls to 1/90
#   moving off the rendered gate enters Q16.3's authored-anchor fallback
#   Q16.0 DOOR XTEL ABSENT then appears roughly every 0.85 s for a new REFR
#
# Q16.3 built its cache by calling ResolveFo3DoorTeleportQ1700 once for every
# active CELL REFR; that resolver performs a full Fallout3.esm scan. Replace only
# that fallback implementation with Q16.15's bounded authored-data index and
# prime it when a scene becomes active. Live ray misses become memory-only.

if(NOT EXISTS "${Q720_CELL_SOURCE}")
    message(FATAL_ERROR "Q16.15 expected transition source at ${Q720_CELL_SOURCE}")
endif()
file(READ "${Q720_CELL_SOURCE}" Q1870_CELL_SOURCE)

set(Q1870_OLD_INC "#include \"fo3-authored-door-query-q1730.inc\"")
set(Q1870_NEW_INC "#include \"fo3-authored-door-query-q1870.inc\"")
string(FIND "${Q1870_CELL_SOURCE}" "${Q1870_OLD_INC}" Q1870_INC_POS)
if(Q1870_INC_POS EQUAL -1)
    message(FATAL_ERROR "Q16.15 could not find Q16.3 authored-door fallback include")
endif()
string(REPLACE "${Q1870_OLD_INC}" "${Q1870_NEW_INC}"
       Q1870_CELL_SOURCE "${Q1870_CELL_SOURCE}")
file(WRITE "${Q720_CELL_SOURCE}" "${Q1870_CELL_SOURCE}")

# Prime the CELL's authored no-model/load-door anchors after the mature scene has
# completed and current-cell identity is known. This runs under the loading/scene
# transition rather than on the first interactive ray miss.
set(Q1870_NATIVE_FILE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp")
if(NOT EXISTS "${Q1870_NATIVE_FILE}")
    message(FATAL_ERROR "Q16.15 expected mature renderer source")
endif()
file(READ "${Q1870_NATIVE_FILE}" Q1870_NATIVE_SOURCE)
string(PREPEND Q1870_NATIVE_SOURCE "#include \"fo3-authored-door-query-q1870.h\"\n")

set(Q1870_COMPLETE_OLD [==[
    CompleteFo3CellTransitionQ74(request.cellFormId);
    gCurrentCellFormId = request.cellFormId;
]==])
set(Q1870_COMPLETE_NEW [==[
    CompleteFo3CellTransitionQ74(request.cellFormId);
    gCurrentCellFormId = request.cellFormId;
    PrimeFo3AuthoredDoorAnchorsQ1870(gCurrentCellFormId);
]==])
string(FIND "${Q1870_NATIVE_SOURCE}" "${Q1870_COMPLETE_OLD}" Q1870_COMPLETE_POS)
if(Q1870_COMPLETE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.15 could not find mature scene completion/current-CELL hook")
endif()
string(REPLACE "${Q1870_COMPLETE_OLD}" "${Q1870_COMPLETE_NEW}"
       Q1870_NATIVE_SOURCE "${Q1870_NATIVE_SOURCE}")
file(WRITE "${Q1870_NATIVE_FILE}" "${Q1870_NATIVE_SOURCE}")

# Headset-visible proof Q16.14 -> Q16.15. No HUD geometry or presentation changes.
set(Q1870_Q4_FILE "${CMAKE_CURRENT_BINARY_DIR}/q1800-q4-generated.cpp")
if(NOT EXISTS "${Q1870_Q4_FILE}")
    message(FATAL_ERROR "Q16.15 expected final OpenXR source")
endif()
file(READ "${Q1870_Q4_FILE}" Q1870_Q4_SOURCE)
set(Q1870_LABEL_OLD [==[
        q1600Digit(q1600X, 0x06u); // Q16.14: 1 = B C
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x66u); // Q16.14: 4 = F G B C
]==])
set(Q1870_LABEL_NEW [==[
        q1600Digit(q1600X, 0x06u); // Q16.15: 1 = B C
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x6Du); // Q16.15: 5 = A F G C D
]==])
string(FIND "${Q1870_Q4_SOURCE}" "${Q1870_LABEL_OLD}" Q1870_LABEL_POS)
if(Q1870_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.15 could not find Q16.14 build-label digits")
endif()
string(REPLACE "${Q1870_LABEL_OLD}" "${Q1870_LABEL_NEW}"
       Q1870_Q4_SOURCE "${Q1870_Q4_SOURCE}")
string(REPLACE "Q16.14 BUILD LABEL:" "Q16.15 BUILD LABEL:"
       Q1870_Q4_SOURCE "${Q1870_Q4_SOURCE}")
string(REPLACE "text=Q16.14 anchor=left-hand" "text=Q16.15 anchor=left-hand"
       Q1870_Q4_SOURCE "${Q1870_Q4_SOURCE}")
string(REPLACE "Q16.14 AUTHORED DOOR FACING" "Q16.15 AUTHORED DOOR FACING"
       Q1870_Q4_SOURCE "${Q1870_Q4_SOURCE}")
file(WRITE "${Q1870_Q4_FILE}" "${Q1870_Q4_SOURCE}")

# Configure-time proof: the old per-REFR fallback implementation is no longer in
# the compiled transition TU; the cache is primed from the mature loader; all
# current authored environment/HUD paths remain present.
string(FIND "${Q1870_CELL_SOURCE}" "fo3-authored-door-query-q1870.inc" Q1870_INC_OK)
string(FIND "${Q1870_CELL_SOURCE}" "fo3-authored-door-query-q1730.inc" Q1870_OLD_INC_BAD)
string(FIND "${Q1870_NATIVE_SOURCE}" "PrimeFo3AuthoredDoorAnchorsQ1870(gCurrentCellFormId);" Q1870_PRIME_OK)
string(FIND "${Q1870_NATIVE_SOURCE}" "Q16.11 MATURE SCENE SWAP BEGIN:" Q1870_MATURE_OK)
string(FIND "${Q1870_NATIVE_SOURCE}" "LoadFo3CellEnvironmentQ1410(" Q1870_ENV_OK)
string(FIND "${Q1870_Q4_SOURCE}" "Q16.13 HUD TARGET HIDE:" Q1870_HIDE_OK)
string(FIND "${Q1870_Q4_SOURCE}" "Q16.15: 5 = A F G C D" Q1870_LABEL_OK)
if(Q1870_INC_OK EQUAL -1 OR NOT Q1870_OLD_INC_BAD EQUAL -1 OR
   Q1870_PRIME_OK EQUAL -1 OR Q1870_MATURE_OK EQUAL -1 OR
   Q1870_ENV_OK EQUAL -1 OR Q1870_HIDE_OK EQUAL -1 OR
   Q1870_LABEL_OK EQUAL -1)
    message(FATAL_ERROR "Q16.15 live-door cache verification failed")
endif()

message(STATUS "Q16.15 authored door fallback enabled: scene-primed bounded ESM index, live ray miss memory-only")
