# Q15.1: reduce only the proven non-cyan neutral ambient fill.
#
# Q15.0 showed that keeping Ambient neutral while restoring authored Sunlight
# gives a small improvement, but the Quest scene still reads flatter/washed out
# than the reference. Q15.1 keeps that exact colour split and changes one scalar:
#
# A / default:
#   current authored Ambient + authored Sunlight, unchanged
# B / LEFT Y:
#   Ambient -> neutral Rec.709 linear luminance * 0.65
#   Sunlight -> authored Q13.9/Q14.0 RGB unchanged
#
# No fog, ImageSpace, exposure, contrast, AO, BaseMap, normal, sun-direction,
# local-light, geometry or post-processing values are changed. The same 0.65
# ambient scale is applied to statics and LAND.

set(Q1510_AMBIENT_OLD [=[
            q1500Ambient[0] = q1500AmbientLuma;
            q1500Ambient[1] = q1500AmbientLuma;
            q1500Ambient[2] = q1500AmbientLuma;
]=])
set(Q1510_AMBIENT_NEW [=[
            q1500Ambient[0] = q1500AmbientLuma * 0.65f;
            q1500Ambient[1] = q1500AmbientLuma * 0.65f;
            q1500Ambient[2] = q1500AmbientLuma * 0.65f;
]=])

string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1510_AMBIENT_OLD}" Q1510_STATIC_POS)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1510_AMBIENT_OLD}" Q1510_LAND_POS)
if(Q1510_STATIC_POS EQUAL -1 OR Q1510_LAND_POS EQUAL -1)
    message(FATAL_ERROR
        "Q15.1 could not find Q15.0 ambient neutral assignments: static=${Q1510_STATIC_POS} land=${Q1510_LAND_POS}")
endif()
string(REPLACE "${Q1510_AMBIENT_OLD}" "${Q1510_AMBIENT_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE "${Q1510_AMBIENT_OLD}" "${Q1510_AMBIENT_NEW}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# Rewrite active diagnostics so captures/logcat state exactly what LEFT Y does.
string(REPLACE
    "Q15.0 AMBIENT CHROMA A/B READY: mode=%s control=LEFT_Y scope=statics+LAND ambientNeutralLuminance=Rec709Linear authoredSunlightRGB=1 baseMapUnchanged=1 localLightsUnchanged=1 fogUnchanged=1 postUnchanged=1"
    "Q15.1 AMBIENT STRENGTH A/B READY: mode=%s control=LEFT_Y scope=statics+LAND ambientNeutralLuminance=Rec709Linear ambientScale=0.65 authoredSunlightRGB=1 baseMapUnchanged=1 localLightsUnchanged=1 fogUnchanged=1 postUnchanged=1"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1510_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1510_Q4_INPUT}")
    message(FATAL_ERROR "Q15.1 expected final Q15.0 OpenXR source at ${Q1510_Q4_INPUT}")
endif()
file(READ "${Q1510_Q4_INPUT}" Q1510_Q4_SOURCE)
string(REPLACE
    "Q15.0 AMBIENT CHROMA MODE: mode=%s source=LEFT_Y scope=statics+LAND authoredSunlightRGB=1"
    "Q15.1 AMBIENT STRENGTH MODE: mode=%s source=LEFT_Y scope=statics+LAND ambientScale=0.65 authoredSunlightRGB=1"
    Q1510_Q4_SOURCE "${Q1510_Q4_SOURCE}")
file(WRITE "${Q1510_Q4_INPUT}" "${Q1510_Q4_SOURCE}")

# Hard guards: both world renderers must use the 0.65 neutral ambient and leave
# the authored sunlight uploads from Q15.0 intact.
string(FIND "${Q6H_NATIVE_SOURCE}" "q1500AmbientLuma * 0.65f" Q1510_STATIC_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "q1500AmbientLuma * 0.65f" Q1510_LAND_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "glUniform3fv(gSunlightColorLocationQ1000, 1, q1000Env.sunlight);" Q1510_STATIC_SUN_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "glUniform3fv(q1000TerrainSunlightLocation, 1, q1000Env.sunlight);" Q1510_LAND_SUN_OK)
string(FIND "${Q1510_Q4_SOURCE}" "Q15.1 AMBIENT STRENGTH MODE" Q1510_LOG_OK)
if(Q1510_STATIC_OK EQUAL -1 OR Q1510_LAND_OK EQUAL -1 OR
   Q1510_STATIC_SUN_OK EQUAL -1 OR Q1510_LAND_SUN_OK EQUAL -1 OR
   Q1510_LOG_OK EQUAL -1)
    message(FATAL_ERROR
        "Q15.1 hook drifted: static=${Q1510_STATIC_OK} land=${Q1510_LAND_OK} staticSun=${Q1510_STATIC_SUN_OK} landSun=${Q1510_LAND_SUN_OK} log=${Q1510_LOG_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")

message(STATUS "Q15.1 neutral Ambient at 65% + authored Sunlight RGB A/B enabled on LEFT Y for statics + LAND")
