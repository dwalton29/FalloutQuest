# Q14.3: restore Fallout 3's default exterior shadow semantics.
#
# Q10.5 intentionally added a modern directional shadow map for statics + LAND.
# That was a Quest-side visual enhancement, not Fallout 3's default renderer.
# Vanilla/default FO3 renders directional diffuse shading on world geometry but
# does not normally let architecture/statics cast the Q10.5 world shadow map.
#
# This is a controlled fidelity A/B:
#   - keep WTHR Ambient/Sunlight, CLMT time, ImageSpace, fog, materials and AO;
#   - keep directional Lambert response (faces still turn away from the sun);
#   - disable only Q10.5 static/LAND *cast-shadow visibility*.
#
# Q14.2 is not active in this build, so WTHR Ambient is back on the established
# Q13.9/Q14.1 path. No tint, hue shift or shadow-colour substitute is added.

# Static/NIF receiver: don't build or sample Q10.5's world shadow map.
set(Q1430_STATIC_OLD [=[
    const bool q1050ShadowActive = Q1050UpdateSunShadow();
]=])
set(Q1430_STATIC_NEW [=[
    const bool q1050ShadowActive = false;
    static bool q1430Logged = false;
    if (!q1430Logged) {
        q1430Logged = true;
        Q6H_LOGI("Q14.3 VANILLA EXTERIOR SHADOWS: staticArchCast=0 landCast=0 directionalLambert=1 actorShadowScope=unchanged q1050WorldShadowMap=disabled tint=none source=FO3-default-render-semantics");
    }
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1430_STATIC_OLD}" Q1430_STATIC_POS)
if(Q1430_STATIC_POS EQUAL -1)
    message(FATAL_ERROR "Q14.3 could not find Q10.5 static shadow activation hook")
endif()
string(REPLACE "${Q1430_STATIC_OLD}" "${Q1430_STATIC_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# LAND receiver: force Q10.5 visibility off as an independent hard guard. Scene
# swaps already clear the terrain shadow bridge, but this prevents a stale depth
# texture/state from ever re-enabling it.
set(Q1430_LAND_OLD [=[
    if (q1050TerrainShadowsEnabledLocation >= 0) glUniform1f(q1050TerrainShadowsEnabledLocation, q1050TerrainShadowEnabled && q1050TerrainShadowTexture ? 1.0f : 0.0f);
]=])
set(Q1430_LAND_NEW [=[
    if (q1050TerrainShadowsEnabledLocation >= 0) glUniform1f(q1050TerrainShadowsEnabledLocation, 0.0f);
]=])
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1430_LAND_OLD}" Q1430_LAND_POS)
if(Q1430_LAND_POS EQUAL -1)
    message(FATAL_ERROR "Q14.3 could not find Q10.5 LAND shadow activation hook")
endif()
string(REPLACE "${Q1430_LAND_OLD}" "${Q1430_LAND_NEW}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# Hard guards: Lambert/directional sunlight must remain in both renderers. This
# patch is about cast shadows only, not flattening authored light direction.
string(FIND "${Q6H_NATIVE_SOURCE}"
            "uSunlightColor * lambert * q1050SunVisibility" Q1430_STATIC_LAMBERT_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}"
            "uSunlightColor * lambert * q1050SunVisibility" Q1430_LAND_LAMBERT_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "const bool q1050ShadowActive = false;" Q1430_STATIC_DISABLED_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}"
            "glUniform1f(q1050TerrainShadowsEnabledLocation, 0.0f)" Q1430_LAND_DISABLED_OK)
if(Q1430_STATIC_LAMBERT_OK EQUAL -1 OR Q1430_LAND_LAMBERT_OK EQUAL -1 OR
   Q1430_STATIC_DISABLED_OK EQUAL -1 OR Q1430_LAND_DISABLED_OK EQUAL -1)
    message(FATAL_ERROR
        "Q14.3 shadow-semantics guard failed: staticLambert=${Q1430_STATIC_LAMBERT_OK} landLambert=${Q1430_LAND_LAMBERT_OK} staticOff=${Q1430_STATIC_DISABLED_OK} landOff=${Q1430_LAND_DISABLED_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")

message(STATUS "Q14.3 vanilla FO3 exterior static/LAND cast-shadow semantics enabled; directional shading retained")
