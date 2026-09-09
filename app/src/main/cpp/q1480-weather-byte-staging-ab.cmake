# Q14.8: Fallout3.exe-guided WTHR lighting staging A/B.
#
# A / default:
#   linear BaseMap * (Q13.9 sRGB-decoded Ambient + Sunlight * NdotL)
# B / LEFT Y:
#   linear BaseMap * (raw WTHR byte/255 Ambient + Sunlight * NdotL)
#
# Only the WTHR Ambient/Sunlight CPU constants change. BaseMap sampling, normals,
# sun direction, local lights, fog, ImageSpace, post and geometry stay unchanged.
# The raw endpoint cache is refreshed by the native TU, where Q14.0 is already
# available, and consumed through a tiny bridge by both statics and LAND. This
# deliberately avoids pulling Q14.0's ESM parser headers into the LAND/CELL TU.

# Native TU owns the refresh implementation because Q14.0 has already inserted
# fo3-time-of-day-q1400.h earlier in this source. LAND sees only the bridge.
string(REPLACE
    "#include \"fo3-pplighting-domain-q1470.h\""
    "#include \"fo3-pplighting-domain-q1470.h\"\n#define FO3_Q1480_DEFINE_REFRESH 1\n#include \"fo3-weather-byte-staging-q1480.h\"\n#undef FO3_Q1480_DEFINE_REFRESH"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "#include \"fo3-environment-q1000.h\""
    "#include \"fo3-environment-q1000.h\"\n#include \"fo3-weather-byte-staging-q1480.h\""
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

string(FIND "${Q6H_NATIVE_SOURCE}" "FO3_Q1480_DEFINE_REFRESH" Q1480_NATIVE_BRIDGE_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "fo3-weather-byte-staging-q1480.h" Q1480_LAND_BRIDGE_OK)
if(Q1480_NATIVE_BRIDGE_OK EQUAL -1 OR Q1480_LAND_BRIDGE_OK EQUAL -1)
    message(FATAL_ERROR "Q14.8 could not install isolated weather staging bridge")
endif()

# Q14.7's encoded-domain shader branch stays disabled. LEFT Y now controls only
# the CPU-side WTHR constants below.
set(Q1480_OLD_DOMAIN_UPLOAD [=[
        glUniform1f(gPpDiffuseDomainLocationQ1470,
                    GetFo3LegacyPpDiffuseDomainQ1470() ? 1.0f : 0.0f);
]=])
set(Q1480_NEW_DOMAIN_UPLOAD [=[
        glUniform1f(gPpDiffuseDomainLocationQ1470, 0.0f);
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1480_OLD_DOMAIN_UPLOAD}" Q1480_DOMAIN_POS)
if(Q1480_DOMAIN_POS EQUAL -1)
    message(FATAL_ERROR "Q14.8 could not neutralize Q14.7 shader-domain branch")
endif()
string(REPLACE "${Q1480_OLD_DOMAIN_UPLOAD}" "${Q1480_NEW_DOMAIN_UPLOAD}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Static/NIF WTHR constants.
set(Q1480_STATIC_OLD [=[
    const Fo3EnvironmentQ1000& q1000Env = GetFo3EnvironmentQ1000();
    if (q1000Env.valid) {
        glUniform3fv(gAmbientColorLocationQ1000, 1, q1000Env.ambient);
        glUniform3fv(gSunlightColorLocationQ1000, 1, q1000Env.sunlight);
        glUniform3fv(gSunDirectionLocationQ1000, 1, q1000Env.sunDirection);
    } else {
        glUniform3f(gAmbientColorLocationQ1000, 0.34f, 0.34f, 0.34f);
        glUniform3f(gSunlightColorLocationQ1000, 0.66f, 0.66f, 0.66f);
        glUniform3f(gSunDirectionLocationQ1000, 0.35f, 0.85f, 0.40f);
    }
]=])
set(Q1480_STATIC_NEW [=[
    const Fo3EnvironmentQ1000& q1000Env = GetFo3EnvironmentQ1000();
    if (q1000Env.valid) {
        float q1480RawAmbient[3]{};
        float q1480RawSunlight[3]{};
        const bool q1480WantRaw = UseFo3RawWeatherLightingQ1480();
        if (q1480WantRaw) RefreshFo3RawWeatherLightingQ1480();
        const bool q1480UseRaw = q1480WantRaw &&
            GetFo3RawWeatherLightingQ1480(q1480RawAmbient, q1480RawSunlight);
        glUniform3fv(gAmbientColorLocationQ1000, 1,
                     q1480UseRaw ? q1480RawAmbient : q1000Env.ambient);
        glUniform3fv(gSunlightColorLocationQ1000, 1,
                     q1480UseRaw ? q1480RawSunlight : q1000Env.sunlight);
        glUniform3fv(gSunDirectionLocationQ1000, 1, q1000Env.sunDirection);
    } else {
        glUniform3f(gAmbientColorLocationQ1000, 0.34f, 0.34f, 0.34f);
        glUniform3f(gSunlightColorLocationQ1000, 0.66f, 0.66f, 0.66f);
        glUniform3f(gSunDirectionLocationQ1000, 0.35f, 0.85f, 0.40f);
    }
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1480_STATIC_OLD}" Q1480_STATIC_POS)
if(Q1480_STATIC_POS EQUAL -1)
    message(FATAL_ERROR "Q14.8 could not find static Q10.0 WTHR uniform upload")
endif()
string(REPLACE "${Q1480_STATIC_OLD}" "${Q1480_STATIC_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# LAND uses the same switch and same shared raw endpoint cache. Calling refresh
# here is safe: its implementation lives in the native TU and the bridge exposes
# only a normal function declaration to LAND.
set(Q1480_LAND_OLD [=[
    const Fo3EnvironmentQ1000& q1000Env = GetFo3EnvironmentQ1000();
    if (q1000Env.valid) {
        glUniform3fv(q1000TerrainAmbientLocation, 1, q1000Env.ambient);
        glUniform3fv(q1000TerrainSunlightLocation, 1, q1000Env.sunlight);
        glUniform3fv(q1000TerrainSunDirectionLocation, 1, q1000Env.sunDirection);
    } else {
        glUniform3f(q1000TerrainAmbientLocation, 0.48f, 0.48f, 0.48f);
        glUniform3f(q1000TerrainSunlightLocation, 0.52f, 0.52f, 0.52f);
        glUniform3f(q1000TerrainSunDirectionLocation, 0.35f, 0.85f, 0.40f);
    }
]=])
set(Q1480_LAND_NEW [=[
    const Fo3EnvironmentQ1000& q1000Env = GetFo3EnvironmentQ1000();
    if (q1000Env.valid) {
        float q1480RawAmbient[3]{};
        float q1480RawSunlight[3]{};
        const bool q1480WantRaw = UseFo3RawWeatherLightingQ1480();
        if (q1480WantRaw) RefreshFo3RawWeatherLightingQ1480();
        const bool q1480UseRaw = q1480WantRaw &&
            GetFo3RawWeatherLightingQ1480(q1480RawAmbient, q1480RawSunlight);
        glUniform3fv(q1000TerrainAmbientLocation, 1,
                     q1480UseRaw ? q1480RawAmbient : q1000Env.ambient);
        glUniform3fv(q1000TerrainSunlightLocation, 1,
                     q1480UseRaw ? q1480RawSunlight : q1000Env.sunlight);
        glUniform3fv(q1000TerrainSunDirectionLocation, 1, q1000Env.sunDirection);
    } else {
        glUniform3f(q1000TerrainAmbientLocation, 0.48f, 0.48f, 0.48f);
        glUniform3f(q1000TerrainSunlightLocation, 0.52f, 0.52f, 0.52f);
        glUniform3f(q1000TerrainSunDirectionLocation, 0.35f, 0.85f, 0.40f);
    }
]=])
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1480_LAND_OLD}" Q1480_LAND_POS)
if(Q1480_LAND_POS EQUAL -1)
    message(FATAL_ERROR "Q14.8 could not find LAND Q10.0 WTHR uniform upload")
endif()
string(REPLACE "${Q1480_LAND_OLD}" "${Q1480_LAND_NEW}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# Correct Q14.7's obsolete diagnostics so device logs describe the active A/B.
string(REPLACE
    "Q14.7 PPLIGHTING DOMAIN A/B READY: mode=%s control=LEFT_Y scope=static-world-diffuse baseAmbientSunlightTogether=1 outputBackToLinear=1 landUnchanged=1 specularUnchanged=1 localLightsUnchanged=1 fogUnchanged=1 postUnchanged=1"
    "Q14.8 WTHR STAGING A/B READY: mode=%s control=LEFT_Y scope=statics+LAND baseMapLinear=1 rawEndpointsDirect=1 sunlightDimmerPreserved=1 localLightsUnchanged=1 fogUnchanged=1 postUnchanged=1"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1480_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1480_Q4_INPUT}")
    message(FATAL_ERROR "Q14.8 expected final Q14.7 OpenXR source at ${Q1480_Q4_INPUT}")
endif()
file(READ "${Q1480_Q4_INPUT}" Q1480_Q4_SOURCE)
string(REPLACE
    "Q14.7 PPLIGHTING DOMAIN MODE: mode=%s source=LEFT_Y scope=static-world-diffuse"
    "Q14.8 WTHR STAGING MODE: mode=%s source=LEFT_Y scope=statics+LAND baseMapLinear=1"
    Q1480_Q4_SOURCE "${Q1480_Q4_SOURCE}")
file(WRITE "${Q1480_Q4_INPUT}" "${Q1480_Q4_SOURCE}")

# Drift guards prove both renderers consume the bridge and Q14.7's encoded
# BaseMap branch cannot become active from LEFT Y.
string(FIND "${Q6H_NATIVE_SOURCE}" "RefreshFo3RawWeatherLightingQ1480();" Q1480_STATIC_REFRESH_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "GetFo3RawWeatherLightingQ1480(q1480RawAmbient, q1480RawSunlight)" Q1480_STATIC_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "RefreshFo3RawWeatherLightingQ1480();" Q1480_LAND_REFRESH_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "GetFo3RawWeatherLightingQ1480(q1480RawAmbient, q1480RawSunlight)" Q1480_LAND_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "glUniform1f(gPpDiffuseDomainLocationQ1470, 0.0f);" Q1480_OLD_DOMAIN_OFF)
string(FIND "${Q1480_Q4_SOURCE}" "Q14.8 WTHR STAGING MODE" Q1480_LOG_OK)
if(Q1480_STATIC_REFRESH_OK EQUAL -1 OR Q1480_STATIC_OK EQUAL -1 OR
   Q1480_LAND_REFRESH_OK EQUAL -1 OR Q1480_LAND_OK EQUAL -1 OR
   Q1480_OLD_DOMAIN_OFF EQUAL -1 OR Q1480_LOG_OK EQUAL -1)
    message(FATAL_ERROR
        "Q14.8 hook drifted: staticRefresh=${Q1480_STATIC_REFRESH_OK} static=${Q1480_STATIC_OK} landRefresh=${Q1480_LAND_REFRESH_OK} land=${Q1480_LAND_OK} oldDomainOff=${Q1480_OLD_DOMAIN_OFF} log=${Q1480_LOG_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")

message(STATUS "Q14.8 raw byte/255 WTHR Ambient+Sunlight staging A/B enabled on LEFT Y for statics + LAND")
