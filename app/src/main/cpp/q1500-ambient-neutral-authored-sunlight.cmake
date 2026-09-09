# Q15.0: keep the proven cyan fix while restoring authored directional warmth.
#
# Q14.9 proved that the blue/cyan cast is carried by the world-light RGB path:
# neutralizing Ambient + Sunlight removes it. Q14.9 also necessarily removed the
# warm directional colour that is visible in the reference renderer, leaving the
# scene flatter and more neutral. Q15.0 narrows the correction further.
#
# A / default:
#   current Q13.9/Q14.0 Ambient + Sunlight, unchanged
# B / LEFT Y:
#   Ambient -> same Rec.709 linear luminance, neutral R=G=B
#   Sunlight -> current authored Q13.9/Q14.0 RGB unchanged
#
# This is intentionally still a diagnostic, not an artistic tint. It tests the
# hypothesis that the broad cyan contamination comes from ambient staging while
# the authored directional sunlight is the warm component we need to retain.
# BaseMap, normals, sun direction, local lights, fog, ImageSpace, AO, bloom,
# exposure and geometry remain unchanged on both statics and LAND.

set(Q1500_STATIC_OLD [=[
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
set(Q1500_STATIC_NEW [=[
    const Fo3EnvironmentQ1000& q1000Env = GetFo3EnvironmentQ1000();
    if (q1000Env.valid) {
        float q1500Ambient[3]{q1000Env.ambient[0], q1000Env.ambient[1], q1000Env.ambient[2]};
        if (GetFo3LegacyPpDiffuseDomainQ1470()) {
            const float q1500AmbientLuma =
                0.2126f * q1500Ambient[0] + 0.7152f * q1500Ambient[1] + 0.0722f * q1500Ambient[2];
            q1500Ambient[0] = q1500AmbientLuma;
            q1500Ambient[1] = q1500AmbientLuma;
            q1500Ambient[2] = q1500AmbientLuma;
        }
        glUniform3fv(gAmbientColorLocationQ1000, 1, q1500Ambient);
        glUniform3fv(gSunlightColorLocationQ1000, 1, q1000Env.sunlight);
        glUniform3fv(gSunDirectionLocationQ1000, 1, q1000Env.sunDirection);
    } else {
        glUniform3f(gAmbientColorLocationQ1000, 0.34f, 0.34f, 0.34f);
        glUniform3f(gSunlightColorLocationQ1000, 0.66f, 0.66f, 0.66f);
        glUniform3f(gSunDirectionLocationQ1000, 0.35f, 0.85f, 0.40f);
    }
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1500_STATIC_OLD}" Q1500_STATIC_POS)
if(Q1500_STATIC_POS EQUAL -1)
    message(FATAL_ERROR "Q15.0 could not find Q14.9 static environment upload")
endif()
string(REPLACE "${Q1500_STATIC_OLD}" "${Q1500_STATIC_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1500_LAND_OLD [=[
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
set(Q1500_LAND_NEW [=[
    const Fo3EnvironmentQ1000& q1000Env = GetFo3EnvironmentQ1000();
    if (q1000Env.valid) {
        float q1500Ambient[3]{q1000Env.ambient[0], q1000Env.ambient[1], q1000Env.ambient[2]};
        if (GetFo3LegacyPpDiffuseDomainQ1470()) {
            const float q1500AmbientLuma =
                0.2126f * q1500Ambient[0] + 0.7152f * q1500Ambient[1] + 0.0722f * q1500Ambient[2];
            q1500Ambient[0] = q1500AmbientLuma;
            q1500Ambient[1] = q1500AmbientLuma;
            q1500Ambient[2] = q1500AmbientLuma;
        }
        glUniform3fv(q1000TerrainAmbientLocation, 1, q1500Ambient);
        glUniform3fv(q1000TerrainSunlightLocation, 1, q1000Env.sunlight);
        glUniform3fv(q1000TerrainSunDirectionLocation, 1, q1000Env.sunDirection);
    } else {
        glUniform3f(q1000TerrainAmbientLocation, 0.48f, 0.48f, 0.48f);
        glUniform3f(q1000TerrainSunlightLocation, 0.52f, 0.52f, 0.52f);
        glUniform3f(q1000TerrainSunDirectionLocation, 0.35f, 0.85f, 0.40f);
    }
]=])
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1500_LAND_OLD}" Q1500_LAND_POS)
if(Q1500_LAND_POS EQUAL -1)
    message(FATAL_ERROR "Q15.0 could not find Q14.9 LAND environment upload")
endif()
string(REPLACE "${Q1500_LAND_OLD}" "${Q1500_LAND_NEW}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# Rewrite the active diagnostics. Q14.8/Q14.9 remain in source history only.
string(REPLACE
    "Q14.9 WORLD LIGHT CHROMA A/B READY: mode=%s control=LEFT_Y scope=statics+LAND neutralLuminance=Rec709Linear ambientSunlightOnly=1 baseMapUnchanged=1 localLightsUnchanged=1 fogUnchanged=1 postUnchanged=1"
    "Q15.0 AMBIENT CHROMA A/B READY: mode=%s control=LEFT_Y scope=statics+LAND ambientNeutralLuminance=Rec709Linear authoredSunlightRGB=1 baseMapUnchanged=1 localLightsUnchanged=1 fogUnchanged=1 postUnchanged=1"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1500_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1500_Q4_INPUT}")
    message(FATAL_ERROR "Q15.0 expected final Q14.9 OpenXR source at ${Q1500_Q4_INPUT}")
endif()
file(READ "${Q1500_Q4_INPUT}" Q1500_Q4_SOURCE)
string(REPLACE
    "Q14.9 WORLD LIGHT CHROMA MODE: mode=%s source=LEFT_Y scope=statics+LAND"
    "Q15.0 AMBIENT CHROMA MODE: mode=%s source=LEFT_Y scope=statics+LAND authoredSunlightRGB=1"
    Q1500_Q4_SOURCE "${Q1500_Q4_SOURCE}")
file(WRITE "${Q1500_Q4_INPUT}" "${Q1500_Q4_SOURCE}")

# Hard guards: ambient neutralization must exist in both renderers while sunlight
# is uploaded directly from the authored environment without Q14.9 grayscale.
string(FIND "${Q6H_NATIVE_SOURCE}" "q1500AmbientLuma" Q1500_STATIC_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "q1500AmbientLuma" Q1500_LAND_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "glUniform3fv(gSunlightColorLocationQ1000, 1, q1000Env.sunlight);" Q1500_STATIC_SUN_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "glUniform3fv(q1000TerrainSunlightLocation, 1, q1000Env.sunlight);" Q1500_LAND_SUN_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "q1490SunlightLuma" Q1500_STATIC_OLD_SUN)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "q1490SunlightLuma" Q1500_LAND_OLD_SUN)
string(FIND "${Q1500_Q4_SOURCE}" "Q15.0 AMBIENT CHROMA MODE" Q1500_LOG_OK)
if(Q1500_STATIC_OK EQUAL -1 OR Q1500_LAND_OK EQUAL -1 OR
   Q1500_STATIC_SUN_OK EQUAL -1 OR Q1500_LAND_SUN_OK EQUAL -1 OR
   NOT Q1500_STATIC_OLD_SUN EQUAL -1 OR NOT Q1500_LAND_OLD_SUN EQUAL -1 OR
   Q1500_LOG_OK EQUAL -1)
    message(FATAL_ERROR
        "Q15.0 hook drifted: static=${Q1500_STATIC_OK} land=${Q1500_LAND_OK} staticSun=${Q1500_STATIC_SUN_OK} landSun=${Q1500_LAND_SUN_OK} oldStaticSun=${Q1500_STATIC_OLD_SUN} oldLandSun=${Q1500_LAND_OLD_SUN} log=${Q1500_LOG_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")

message(STATUS "Q15.0 neutral Ambient chroma + authored Sunlight RGB A/B enabled on LEFT Y for statics + LAND")
