# Q14.8: Fallout3.exe-guided WTHR lighting staging A/B.
#
# Fallout 3's shipped PPLighting shader confirms the core world-light equation
# itself is AmbientColor + LightColor * NdotL. The remaining question is how the
# WTHR RGB bytes should be staged into those constants. Q13.9 currently decodes
# WTHR RGB through sRGB before lighting; Fallout3.exe weather code was observed
# normalizing authored bytes with 1/255. This test changes only that staging.
#
# A / default:
#   linear BaseMap * (Q13.9-linear Ambient + Q13.9-linear Sunlight * NdotL)
# B / LEFT Y:
#   linear BaseMap * (raw WTHR byte/255 Ambient + raw WTHR byte/255 Sunlight * NdotL)
#
# The same B state is fed to statics AND LAND. Texture formats, BaseMap sampling,
# normals, sun direction, local lights, fog, ImageSpace, AO, bloom, exposure and
# all geometry remain unchanged. The old Q14.7 BaseMap re-encode branch is held
# off so LEFT Y has exactly one meaning in this build.

# Shared helper/state in both world renderers.
string(REPLACE
    "#include \"fo3-pplighting-domain-q1470.h\""
    "#include \"fo3-pplighting-domain-q1470.h\"\n#include \"fo3-weather-byte-staging-q1480.h\""
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "#include \"fo3-environment-q1000.h\""
    "#include \"fo3-environment-q1000.h\"\n#include \"fo3-weather-byte-staging-q1480.h\""
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# Q14.7's shader-domain branch must stay disabled; LEFT Y now selects only the
# CPU-side WTHR constants uploaded below.
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
        const bool q1480UseRaw = UseFo3RawWeatherLightingQ1480() &&
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

# LAND WTHR constants: exact same toggle and raw endpoint values as statics.
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
        const bool q1480UseRaw = UseFo3RawWeatherLightingQ1480() &&
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

# Correct Q14.7's now-obsolete diagnostics so device logs describe the active A/B.
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

# Hard guards prove that both renderers consume the raw helper and that the old
# Q14.7 encoded-BaseMap branch cannot become active from LEFT Y.
string(FIND "${Q6H_NATIVE_SOURCE}" "GetFo3RawWeatherLightingQ1480(q1480RawAmbient, q1480RawSunlight)" Q1480_STATIC_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "GetFo3RawWeatherLightingQ1480(q1480RawAmbient, q1480RawSunlight)" Q1480_LAND_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "glUniform1f(gPpDiffuseDomainLocationQ1470, 0.0f);" Q1480_OLD_DOMAIN_OFF)
string(FIND "${Q1480_Q4_SOURCE}" "Q14.8 WTHR STAGING MODE" Q1480_LOG_OK)
if(Q1480_STATIC_OK EQUAL -1 OR Q1480_LAND_OK EQUAL -1 OR
   Q1480_OLD_DOMAIN_OFF EQUAL -1 OR Q1480_LOG_OK EQUAL -1)
    message(FATAL_ERROR
        "Q14.8 hook drifted: static=${Q1480_STATIC_OK} land=${Q1480_LAND_OK} oldDomainOff=${Q1480_OLD_DOMAIN_OFF} log=${Q1480_LOG_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")

message(STATUS "Q14.8 raw byte/255 WTHR Ambient+Sunlight staging A/B enabled on LEFT Y for statics + LAND")
