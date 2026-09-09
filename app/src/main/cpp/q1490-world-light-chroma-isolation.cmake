# Q14.9: world-light chroma-isolation A/B.
#
# Q14.8 proved that changing WTHR RGB transfer from Q13.9 sRGB-decoded values
# to raw byte/255 does not remove the cyan/blue character; it only raises the
# same coloured world-light energy. This test therefore leaves the active Q13.9
# environment values untouched and removes only their chroma when LEFT Y is on.
#
# A / default:
#   current Q13.9/Q14.0 Ambient + Sunlight, unchanged
# B / LEFT Y:
#   same Ambient and Sunlight linear luminance, neutral R=G=B
#
# The neutral value uses Rec.709 linear luminance (0.2126/0.7152/0.0722), so the
# diagnostic preserves perceived linear light intensity while deleting only RGB
# bias. It applies to statics and LAND together. BaseMap, normals, sun direction,
# local lights, fog, ImageSpace, AO, bloom, exposure and geometry remain unchanged.

# Q14.8 has already rewritten the two Q10.0 environment-upload blocks. Replace
# those final blocks so the raw-WTHR bridge becomes dormant and LEFT Y now means
# exactly one thing: neutralize the current world-light chroma.
set(Q1490_STATIC_OLD [=[
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
set(Q1490_STATIC_NEW [=[
    const Fo3EnvironmentQ1000& q1000Env = GetFo3EnvironmentQ1000();
    if (q1000Env.valid) {
        float q1490Ambient[3]{q1000Env.ambient[0], q1000Env.ambient[1], q1000Env.ambient[2]};
        float q1490Sunlight[3]{q1000Env.sunlight[0], q1000Env.sunlight[1], q1000Env.sunlight[2]};
        if (GetFo3LegacyPpDiffuseDomainQ1470()) {
            const float q1490AmbientLuma =
                0.2126f * q1490Ambient[0] + 0.7152f * q1490Ambient[1] + 0.0722f * q1490Ambient[2];
            const float q1490SunlightLuma =
                0.2126f * q1490Sunlight[0] + 0.7152f * q1490Sunlight[1] + 0.0722f * q1490Sunlight[2];
            for (int c = 0; c < 3; ++c) {
                q1490Ambient[c] = q1490AmbientLuma;
                q1490Sunlight[c] = q1490SunlightLuma;
            }
        }
        glUniform3fv(gAmbientColorLocationQ1000, 1, q1490Ambient);
        glUniform3fv(gSunlightColorLocationQ1000, 1, q1490Sunlight);
        glUniform3fv(gSunDirectionLocationQ1000, 1, q1000Env.sunDirection);
    } else {
        glUniform3f(gAmbientColorLocationQ1000, 0.34f, 0.34f, 0.34f);
        glUniform3f(gSunlightColorLocationQ1000, 0.66f, 0.66f, 0.66f);
        glUniform3f(gSunDirectionLocationQ1000, 0.35f, 0.85f, 0.40f);
    }
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1490_STATIC_OLD}" Q1490_STATIC_POS)
if(Q1490_STATIC_POS EQUAL -1)
    message(FATAL_ERROR "Q14.9 could not find Q14.8 static environment upload")
endif()
string(REPLACE "${Q1490_STATIC_OLD}" "${Q1490_STATIC_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1490_LAND_OLD [=[
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
set(Q1490_LAND_NEW [=[
    const Fo3EnvironmentQ1000& q1000Env = GetFo3EnvironmentQ1000();
    if (q1000Env.valid) {
        float q1490Ambient[3]{q1000Env.ambient[0], q1000Env.ambient[1], q1000Env.ambient[2]};
        float q1490Sunlight[3]{q1000Env.sunlight[0], q1000Env.sunlight[1], q1000Env.sunlight[2]};
        if (GetFo3LegacyPpDiffuseDomainQ1470()) {
            const float q1490AmbientLuma =
                0.2126f * q1490Ambient[0] + 0.7152f * q1490Ambient[1] + 0.0722f * q1490Ambient[2];
            const float q1490SunlightLuma =
                0.2126f * q1490Sunlight[0] + 0.7152f * q1490Sunlight[1] + 0.0722f * q1490Sunlight[2];
            for (int c = 0; c < 3; ++c) {
                q1490Ambient[c] = q1490AmbientLuma;
                q1490Sunlight[c] = q1490SunlightLuma;
            }
        }
        glUniform3fv(q1000TerrainAmbientLocation, 1, q1490Ambient);
        glUniform3fv(q1000TerrainSunlightLocation, 1, q1490Sunlight);
        glUniform3fv(q1000TerrainSunDirectionLocation, 1, q1000Env.sunDirection);
    } else {
        glUniform3f(q1000TerrainAmbientLocation, 0.48f, 0.48f, 0.48f);
        glUniform3f(q1000TerrainSunlightLocation, 0.52f, 0.52f, 0.52f);
        glUniform3f(q1000TerrainSunDirectionLocation, 0.35f, 0.85f, 0.40f);
    }
]=])
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1490_LAND_OLD}" Q1490_LAND_POS)
if(Q1490_LAND_POS EQUAL -1)
    message(FATAL_ERROR "Q14.9 could not find Q14.8 LAND environment upload")
endif()
string(REPLACE "${Q1490_LAND_OLD}" "${Q1490_LAND_NEW}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# Rewrite Q14.8 diagnostics to describe the active test.
string(REPLACE
    "Q14.8 WTHR STAGING A/B READY: mode=%s control=LEFT_Y scope=statics+LAND baseMapLinear=1 rawEndpointsDirect=1 sunlightDimmerPreserved=1 localLightsUnchanged=1 fogUnchanged=1 postUnchanged=1"
    "Q14.9 WORLD LIGHT CHROMA A/B READY: mode=%s control=LEFT_Y scope=statics+LAND neutralLuminance=Rec709Linear ambientSunlightOnly=1 baseMapUnchanged=1 localLightsUnchanged=1 fogUnchanged=1 postUnchanged=1"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1490_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1490_Q4_INPUT}")
    message(FATAL_ERROR "Q14.9 expected final Q14.8 OpenXR source at ${Q1490_Q4_INPUT}")
endif()
file(READ "${Q1490_Q4_INPUT}" Q1490_Q4_SOURCE)
string(REPLACE
    "Q14.8 WTHR STAGING MODE: mode=%s source=LEFT_Y scope=statics+LAND baseMapLinear=1"
    "Q14.9 WORLD LIGHT CHROMA MODE: mode=%s source=LEFT_Y scope=statics+LAND"
    Q1490_Q4_SOURCE "${Q1490_Q4_SOURCE}")
file(WRITE "${Q1490_Q4_INPUT}" "${Q1490_Q4_SOURCE}")

# Hard guards: both renderers must contain the neutral-luminance path, and the
# old Q14.8 raw endpoint switch must no longer control either environment upload.
string(FIND "${Q6H_NATIVE_SOURCE}" "q1490AmbientLuma" Q1490_STATIC_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "q1490AmbientLuma" Q1490_LAND_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "q1480UseRaw ? q1480RawAmbient" Q1490_STATIC_RAW_OLD)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "q1480UseRaw ? q1480RawAmbient" Q1490_LAND_RAW_OLD)
string(FIND "${Q1490_Q4_SOURCE}" "Q14.9 WORLD LIGHT CHROMA MODE" Q1490_LOG_OK)
if(Q1490_STATIC_OK EQUAL -1 OR Q1490_LAND_OK EQUAL -1 OR
   NOT Q1490_STATIC_RAW_OLD EQUAL -1 OR NOT Q1490_LAND_RAW_OLD EQUAL -1 OR
   Q1490_LOG_OK EQUAL -1)
    message(FATAL_ERROR
        "Q14.9 hook drifted: static=${Q1490_STATIC_OK} land=${Q1490_LAND_OK} staticRaw=${Q1490_STATIC_RAW_OLD} landRaw=${Q1490_LAND_RAW_OLD} log=${Q1490_LOG_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")

message(STATUS "Q14.9 neutral-luminance Ambient+Sunlight chroma isolation A/B enabled on LEFT Y for statics + LAND")
