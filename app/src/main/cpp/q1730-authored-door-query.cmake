# Q16.3: authored XTEL interaction fallback for load doors with no drawable NIF.
#
# Q16.0 targets rendered DOOR meshes and works well for ordinary doors, but some
# Bethesda load-door references (notably large settlement gates) can use an
# invisible/no-model activation ref while separate geometry supplies the visible
# gate. Q16.3 keeps the mesh/AABB hit as the primary path and falls back to the
# enabled XTEL-bearing REFR anchors authored in the current CELL.

if(NOT EXISTS "${Q720_CELL_SOURCE}")
    message(FATAL_ERROR "Q16.3 expected generated transition source at ${Q720_CELL_SOURCE}")
endif()
file(READ "${Q720_CELL_SOURCE}" Q1730_CELL_SOURCE)

# The Q16 interior include already exposes generic CELL-child REFR collection and
# enable-parent resolution in this translation unit. Add the XTEL anchor query
# immediately after it so no second ESM parser is required.
set(Q1730_INTERIOR_INCLUDE "#include \"fo3-cell-interior-q1700.inc\"")
set(Q1730_ANCHOR_INCLUDE
    "#include \"fo3-cell-interior-q1700.inc\"\n#include \"fo3-authored-door-query-q1730.inc\"")
string(FIND "${Q1730_CELL_SOURCE}" "${Q1730_INTERIOR_INCLUDE}" Q1730_CELL_INCLUDE_POS)
if(Q1730_CELL_INCLUDE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.3 could not find Q16 interior parser include")
endif()
string(REPLACE "${Q1730_INTERIOR_INCLUDE}" "${Q1730_ANCHOR_INCLUDE}"
       Q1730_CELL_SOURCE "${Q1730_CELL_SOURCE}")
file(WRITE "${Q720_CELL_SOURCE}" "${Q1730_CELL_SOURCE}")

# Renderer declaration and scene-origin state. The authored REFR DATA point must
# be mapped with exactly the same center/floor transform used by UploadCpuObject.
string(PREPEND Q6H_NATIVE_SOURCE
       "#include \"fo3-authored-door-query-q1730.h\"\n")

set(Q1730_CELL_STATE_OLD [==[
uint32_t gCurrentCellFormId = 0x00002DBDu;
]==])
set(Q1730_CELL_STATE_NEW [==[
uint32_t gCurrentCellFormId = 0x00002DBDu;
float gSceneCenterXQ1730 = 0.0f;
float gSceneCenterYQ1730 = 0.0f;
float gSceneFloorZQ1730 = 0.0f;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1730_CELL_STATE_OLD}" Q1730_STATE_POS)
if(Q1730_STATE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.3 could not find current CELL renderer state")
endif()
string(REPLACE "${Q1730_CELL_STATE_OLD}" "${Q1730_CELL_STATE_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1730_INITIAL_ORIGIN_OLD [==[
    const float floorZ = q6kArrivalReady ? q6kArrival.z : minimum.z;
]==])
set(Q1730_INITIAL_ORIGIN_NEW [==[
    const float floorZ = q6kArrivalReady ? q6kArrival.z : minimum.z;
    gSceneCenterXQ1730 = centerX;
    gSceneCenterYQ1730 = centerY;
    gSceneFloorZQ1730 = floorZ;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1730_INITIAL_ORIGIN_OLD}" Q1730_INITIAL_ORIGIN_POS)
if(Q1730_INITIAL_ORIGIN_POS EQUAL -1)
    message(FATAL_ERROR "Q16.3 could not find initial scene origin")
endif()
string(REPLACE "${Q1730_INITIAL_ORIGIN_OLD}" "${Q1730_INITIAL_ORIGIN_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1730_SWAP_ORIGIN_OLD [==[
    gSceneReady = !gObjects.empty();
    gLoggedFirstDraw = false;
]==])
set(Q1730_SWAP_ORIGIN_NEW [==[
    gSceneReady = !gObjects.empty();
    gSceneCenterXQ1730 = request.x;
    gSceneCenterYQ1730 = request.y;
    gSceneFloorZQ1730 = request.z;
    gLoggedFirstDraw = false;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1730_SWAP_ORIGIN_OLD}" Q1730_SWAP_ORIGIN_POS)
if(Q1730_SWAP_ORIGIN_POS EQUAL -1)
    message(FATAL_ERROR "Q16.3 could not find Q7.4 scene-swap origin update")
endif()
string(REPLACE "${Q1730_SWAP_ORIGIN_OLD}" "${Q1730_SWAP_ORIGIN_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Q16.0's rendered DOOR AABB remains authoritative. Only if no rendered load
# door was hit do we query ESM-authored XTEL anchors for the current CELL.
set(Q1730_QUERY_OLD [==[
    if (!hit) return false;

    if (outAim) {
        outAim->valid = true;
]==])
set(Q1730_QUERY_NEW [==[
    if (!hit) {
        Fo3DoorAimQ1700 authoredAim;
        if (QueryFo3AuthoredDoorAnchorQ1730(
                gCurrentCellFormId,
                gSceneCenterXQ1730, gSceneCenterYQ1730, gSceneFloorZQ1730,
                FO3_UNITS_PER_METRE, FLOOR_Y, SCENE_FORWARD,
                ox, oy, oz, dx, dy, dz, &authoredAim) && authoredAim.valid) {
            if (outAim) *outAim = authoredAim;
            Q6H_LOGI("Q16.3 DOOR AIM FALLBACK: cell=%08X sourceDoor=%08X destinationDoor=%08X distance=%.2f source=ESM_XTEL_ANCHOR",
                     gCurrentCellFormId, authoredAim.sourceDoorRef,
                     authoredAim.destinationDoorRef, authoredAim.distance);
            return true;
        }
        return false;
    }

    if (outAim) {
        outAim->valid = true;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1730_QUERY_OLD}" Q1730_QUERY_POS)
if(Q1730_QUERY_POS EQUAL -1)
    message(FATAL_ERROR "Q16.3 could not find Q16.0 rendered-door miss path")
endif()
string(REPLACE "${Q1730_QUERY_OLD}" "${Q1730_QUERY_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Visible headset proof: Q16.2 -> Q16.3. Q16.2 loading comfort and Q16.1 HUD are
# otherwise byte-for-byte untouched.
set(Q1730_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1730_Q4_INPUT}")
    message(FATAL_ERROR "Q16.3 expected final OpenXR source at ${Q1730_Q4_INPUT}")
endif()
file(READ "${Q1730_Q4_INPUT}" Q1730_Q4_SOURCE)
set(Q1730_LABEL_OLD [==[
        q1600Digit(q1600X, 0x5Bu); // Q16.2: 2 = A B G E D
]==])
set(Q1730_LABEL_NEW [==[
        q1600Digit(q1600X, 0x4Fu); // Q16.3: 3 = A B C D G
]==])
string(FIND "${Q1730_Q4_SOURCE}" "${Q1730_LABEL_OLD}" Q1730_LABEL_POS)
if(Q1730_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.3 could not find Q16.2 final build-label digit")
endif()
string(REPLACE "${Q1730_LABEL_OLD}" "${Q1730_LABEL_NEW}"
       Q1730_Q4_SOURCE "${Q1730_Q4_SOURCE}")
string(REPLACE "Q16.2 BUILD LABEL:" "Q16.3 BUILD LABEL:"
       Q1730_Q4_SOURCE "${Q1730_Q4_SOURCE}")
string(REPLACE "text=Q16.2 anchor=left-hand" "text=Q16.3 anchor=left-hand"
       Q1730_Q4_SOURCE "${Q1730_Q4_SOURCE}")
file(WRITE "${Q1730_Q4_INPUT}" "${Q1730_Q4_SOURCE}")

# Hard guards: fallback must exist while both prior presentation layers survive.
string(FIND "${Q1730_CELL_SOURCE}" "fo3-authored-door-query-q1730.inc" Q1730_CELL_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "QueryFo3AuthoredDoorAnchorQ1730" Q1730_QUERY_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q16.3 DOOR AIM FALLBACK:" Q1730_LOG_OK)
string(FIND "${Q1730_Q4_SOURCE}" "Q16.3: 3 = A B C D G" Q1730_LABEL_OK)
string(FIND "${Q1730_Q4_SOURCE}" "RenderFo3LoadingScreenQ1720" Q1730_LOADING_OK)
string(FIND "${Q1730_Q4_SOURCE}" "kButtonSegments = 24" Q1730_HUD_OK)
if(Q1730_CELL_OK EQUAL -1 OR Q1730_QUERY_OK EQUAL -1 OR Q1730_LOG_OK EQUAL -1 OR
   Q1730_LABEL_OK EQUAL -1 OR Q1730_LOADING_OK EQUAL -1 OR Q1730_HUD_OK EQUAL -1)
    message(FATAL_ERROR
        "Q16.3 verification failed: cell=${Q1730_CELL_OK} query=${Q1730_QUERY_OK} log=${Q1730_LOG_OK} label=${Q1730_LABEL_OK} loading=${Q1730_LOADING_OK} hud=${Q1730_HUD_OK}")
endif()

message(STATUS "Q16.3 authored XTEL interaction fallback enabled: rendered DOOR AABB first, current-CELL ESM anchors second")
