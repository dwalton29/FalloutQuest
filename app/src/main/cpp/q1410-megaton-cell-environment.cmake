# Q14.1: use Megaton's CELL-authored region weather + ImageSpace instead of the
# WRLD climate-list fallback. Also restore WTHR FNAM Fog Power in the exterior
# shaders. No artistic tint/grade is introduced here.
#
# ESM chain:
#   spatial CELL XCLR -> REGN RDAT(Weather)/RDWT -> WTHR
#   spatial CELL XCIM -> IMGS
#
# The existing Q14.0 test clock remains intact.

# -----------------------------------------------------------------------------
# Runtime resolver + corrected FO3 152-byte IMGS layout.
# Insert the helper before Q14.0, then temporarily alias Q14.0's base IMGS load
# so its four endpoint images inherit the corrected layout as well.
# -----------------------------------------------------------------------------
set(Q1410_TOD_INCLUDE [=[
#include "fo3-time-of-day-q1400.h"
]=])
set(Q1410_TOD_INCLUDE_NEW [=[
#include "fo3-megaton-cell-environment-q1410.h"
#define LoadFo3ImageSpaceQ1280 LoadFo3ImageSpaceBaseQ1410
#include "fo3-time-of-day-q1400.h"
#undef LoadFo3ImageSpaceQ1280
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1410_TOD_INCLUDE}" Q1410_TOD_INCLUDE_POS)
if(Q1410_TOD_INCLUDE_POS EQUAL -1)
    message(FATAL_ERROR "Q14.1 could not find Q14.0 runtime include")
endif()
string(REPLACE "${Q1410_TOD_INCLUDE}" "${Q1410_TOD_INCLUDE_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Q12.8/Q13.2 currently ask the persistent CELL for XCIM after the exterior swap.
# Replace that call with the spatial-cell resolver using the XTEL arrival XY.
set(Q1410_OLD_IMAGE_LOAD [=[
    LoadFo3ImageSpaceQ1320(request.cellFormId, request.worldspaceFormId);
]=])
set(Q1410_NEW_IMAGE_LOAD [=[
    LoadFo3CellEnvironmentQ1410(request.cellFormId, request.worldspaceFormId,
                                request.x, request.y);
    // Rebuild Q14.0 from the corrected region weather/XCIM on the first frame.
    fo3todq1400::gRuntime = {};
    fo3todq1400::gAppliedOnce = false;
    fo3todq1400::gLastPredictedNs = 0;
    fo3todq1400::gLastLoggedHour = -1;
    fo3todq1400::gLastLoggedPhase.clear();
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1410_OLD_IMAGE_LOAD}" Q1410_IMAGE_LOAD_POS)
if(Q1410_IMAGE_LOAD_POS EQUAL -1)
    message(FATAL_ERROR "Q14.1 could not find final ImageSpace transition call")
endif()
string(REPLACE "${Q1410_OLD_IMAGE_LOAD}" "${Q1410_NEW_IMAGE_LOAD}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Static/NIF fog: consume WTHR FNAM Day/Night Power.
# -----------------------------------------------------------------------------
string(REPLACE
    "        uniform float uFogFar;"
    "        uniform float uFogFar;\n        uniform float uFogPower;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "GLint gFogFarLocationQ1010 = -1;"
    "GLint gFogFarLocationQ1010 = -1;\nGLint gFogPowerLocationQ1410 = -1;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "    gFogFarLocationQ1010 = glGetUniformLocation(gProgram, \"uFogFar\");"
    "    gFogFarLocationQ1010 = glGetUniformLocation(gProgram, \"uFogFar\");\n    gFogPowerLocationQ1410 = glGetUniformLocation(gProgram, \"uFogPower\");"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1410_OLD_STATIC_FOG [=[
            float fogFactor = smoothstep(uFogNear, max(uFogFar, uFogNear + 0.01), fogDistance);
]=])
set(Q1410_NEW_STATIC_FOG [=[
            float fogT = clamp((fogDistance - uFogNear) /
                               max(uFogFar - uFogNear, 0.01), 0.0, 1.0);
            float fogFactor = pow(fogT, max(uFogPower, 0.01));
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1410_OLD_STATIC_FOG}" Q1410_STATIC_FOG_POS)
if(Q1410_STATIC_FOG_POS EQUAL -1)
    message(FATAL_ERROR "Q14.1 could not find static fog equation")
endif()
string(REPLACE "${Q1410_OLD_STATIC_FOG}" "${Q1410_NEW_STATIC_FOG}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "    if (q1000Env.valid && q1000Env.fogFar > q1000Env.fogNear + 1.0f) {"
    "    if (gFogPowerLocationQ1410 >= 0) glUniform1f(gFogPowerLocationQ1410, GetFo3FogPowerQ1410());\n    if (q1000Env.valid && q1000Env.fogFar > q1000Env.fogNear + 1.0f) {"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# LAND fog: same WTHR power as statics.
# -----------------------------------------------------------------------------
string(REPLACE
    "        uniform float uFogFar;"
    "        uniform float uFogFar;\n        uniform float uFogPower;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

string(REPLACE
    "GLint q1010TerrainFogFarLocation = -1;"
    "GLint q1010TerrainFogFarLocation = -1;\nGLint q1410TerrainFogPowerLocation = -1;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

string(REPLACE
    "    q1010TerrainFogFarLocation = -1;"
    "    q1010TerrainFogFarLocation = -1;\n    q1410TerrainFogPowerLocation = -1;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

string(REPLACE
    "    q1010TerrainFogFarLocation = glGetUniformLocation(q76bProgram, \"uFogFar\");"
    "    q1010TerrainFogFarLocation = glGetUniformLocation(q76bProgram, \"uFogFar\");\n    q1410TerrainFogPowerLocation = glGetUniformLocation(q76bProgram, \"uFogPower\");"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

set(Q1410_OLD_TERRAIN_FOG [=[
            float fogFactor = smoothstep(uFogNear, max(uFogFar, uFogNear + 0.01), fogDistance);
]=])
set(Q1410_NEW_TERRAIN_FOG [=[
            float fogT = clamp((fogDistance - uFogNear) /
                               max(uFogFar - uFogNear, 0.01), 0.0, 1.0);
            float fogFactor = pow(fogT, max(uFogPower, 0.01));
]=])
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1410_OLD_TERRAIN_FOG}" Q1410_TERRAIN_FOG_POS)
if(Q1410_TERRAIN_FOG_POS EQUAL -1)
    message(FATAL_ERROR "Q14.1 could not find LAND fog equation")
endif()
string(REPLACE "${Q1410_OLD_TERRAIN_FOG}" "${Q1410_NEW_TERRAIN_FOG}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

string(REPLACE
    "    if (q1000Env.valid && q1000Env.fogFar > q1000Env.fogNear + 1.0f) {"
    "    if (q1410TerrainFogPowerLocation >= 0) glUniform1f(q1410TerrainFogPowerLocation, GetFo3FogPowerQ1410());\n    if (q1000Env.valid && q1000Env.fogFar > q1000Env.fogNear + 1.0f) {"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# -----------------------------------------------------------------------------
# Update Fog Power from the same Q14.0 clock once per stereo frame.
# -----------------------------------------------------------------------------
set(Q1410_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1410_Q4_INPUT}")
    message(FATAL_ERROR "Q14.1 expected final Q14.0 eye source at ${Q1410_Q4_INPUT}")
endif()
file(READ "${Q1410_Q4_INPUT}" Q1410_Q4_SOURCE)

set(Q1410_TIME_UPDATE [=[
                UpdateFo3TimeOfDayQ1400(timeAdvanceValue_,
                    static_cast<int64_t>(frameState.predictedDisplayTime));
]=])
set(Q1410_TIME_UPDATE_NEW [=[
                UpdateFo3TimeOfDayQ1400(timeAdvanceValue_,
                    static_cast<int64_t>(frameState.predictedDisplayTime));
                if (fo3todq1400::gRuntime.ready) {
                    UpdateFo3FogPowerQ1410(
                        fo3todq1400::gTestHour,
                        fo3todq1400::gRuntime.climate.sunriseBegin,
                        fo3todq1400::gRuntime.climate.sunriseEnd,
                        fo3todq1400::gRuntime.climate.sunsetBegin,
                        fo3todq1400::gRuntime.climate.sunsetEnd);
                }
]=])
string(FIND "${Q1410_Q4_SOURCE}" "${Q1410_TIME_UPDATE}" Q1410_TIME_UPDATE_POS)
if(Q1410_TIME_UPDATE_POS EQUAL -1)
    message(FATAL_ERROR "Q14.1 could not find Q14.0 time update")
endif()
string(REPLACE "${Q1410_TIME_UPDATE}" "${Q1410_TIME_UPDATE_NEW}"
       Q1410_Q4_SOURCE "${Q1410_Q4_SOURCE}")
file(WRITE "${Q1410_Q4_INPUT}" "${Q1410_Q4_SOURCE}")

# Hard guards for all fidelity-critical pieces.
string(FIND "${Q6H_NATIVE_SOURCE}" "LoadFo3CellEnvironmentQ1410(" Q1410_CELL_CALL_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "fo3-megaton-cell-environment-q1410.h" Q1410_INCLUDE_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "uniform float uFogPower;" Q1410_STATIC_POWER_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "uniform float uFogPower;" Q1410_TERRAIN_POWER_OK)
string(FIND "${Q1410_Q4_SOURCE}" "UpdateFo3FogPowerQ1410(" Q1410_CLOCK_POWER_OK)
if(Q1410_CELL_CALL_OK EQUAL -1 OR Q1410_INCLUDE_OK EQUAL -1 OR
   Q1410_STATIC_POWER_OK EQUAL -1 OR Q1410_TERRAIN_POWER_OK EQUAL -1 OR
   Q1410_CLOCK_POWER_OK EQUAL -1)
    message(FATAL_ERROR
        "Q14.1 hook drifted: cell=${Q1410_CELL_CALL_OK} include=${Q1410_INCLUDE_OK} staticFog=${Q1410_STATIC_POWER_OK} terrainFog=${Q1410_TERRAIN_POWER_OK} clockFog=${Q1410_CLOCK_POWER_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")

message(STATUS "Q14.1 CELL XCLR->REGN weather + XCIM ImageSpace + FNAM Fog Power enabled")
