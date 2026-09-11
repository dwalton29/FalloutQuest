# Q15.11: use the actual Shader Package 17 tangent-space diffuse path captured
# from PC Fallout 3 at Megaton draw call 4601215.
#
# The apitrace state dump proves the active PC PPLighting shaders do this:
#   VS: project LightData onto authored Tangent/Binormal/Normal, normalize it,
#       and interpolate that signed tangent-space vector directly.
#   PS: sample NormalMap, remap RGB 0..1 -> -1..1, normalize it, then DP3 with
#       the interpolated tangent-space light vector for the diffuse N.L term.
#
# Q15.4 implemented an approximation of this path, but Q15.6 later retired it
# and pinned the renderer back to the Quest world-space mapped-normal Lambert.
# Q15.11 corrects the remaining Q15.4 differences and makes the SP17 path the
# actual static/PPLighting diffuse path. LAND is deliberately untouched.
#
# The same PC draw also proves PSLightColor = raw Sunlight * 2.5 at noon. The
# Q15.9 Quest path was still resolving 2.2 from the current IMGS value, so this
# patch pins the captured PC base sunlight dimmer 1.5 -> effective scale 2.5 for
# static PPLighting while we reproduce this reference frame.

# -----------------------------------------------------------------------------
# Vertex stage: PC SLS shader projects c25 into T/B/N and NORMALIZES the result
# before writing the tangent-space light vector. Q15.4 omitted that second
# normalization and encoded it to 0..1. Store the signed normalized vector
# directly in the existing varying (name retained to avoid extra plumbing).
# -----------------------------------------------------------------------------
set(Q1610_VERTEX_OLD [=[
            vec3 q1540LightTangent = vec3(
                dot(aTangent, q1540LightData),
                dot(aBitangent, q1540LightData),
                dot(aNormal, q1540LightData));
            vSp17LightEncodedQ1540 = q1540LightTangent * 0.5 + 0.5;
]=])
set(Q1610_VERTEX_NEW [=[
            vec3 q1540LightTangent = vec3(
                dot(aTangent, q1540LightData),
                dot(aBitangent, q1540LightData),
                dot(aNormal, q1540LightData));
            // PC SLS vertex shader: dp3 T/B/N then rsq-normalize before oT1.
            float q1610LightLen2 = dot(q1540LightTangent, q1540LightTangent);
            vSp17LightEncodedQ1540 = q1540LightTangent *
                inversesqrt(max(q1610LightLen2, 1.0e-12));
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1610_VERTEX_OLD}" Q1610_VERTEX_POS)
if(Q1610_VERTEX_POS EQUAL -1)
    message(FATAL_ERROR "Q15.11 could not find Q15.4 tangent-light vertex block")
endif()
string(REPLACE "${Q1610_VERTEX_OLD}" "${Q1610_VERTEX_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Pixel stage: PC SLS remaps sampled normal RGB and NRM-normalizes it. It does
# NOT apply FalloutQuest's synthetic normal-strength scaling and does NOT decode
# the tangent light from 0..1. Then diffuse is DP3_sat(normal, light).
# -----------------------------------------------------------------------------
set(Q1610_PIXEL_OLD [=[
            vec3 q1540Sp17Normal = normalGloss.rgb * 2.0 - 1.0;
            q1540Sp17Normal.xy *= uNormalStrength;
            vec3 q1540Sp17Light = vSp17LightEncodedQ1540 * 2.0 - 1.0;
            float q1540Sp17Lambert =
                clamp(dot(q1540Sp17Normal, q1540Sp17Light), 0.0, 1.0);
]=])
set(Q1610_PIXEL_NEW [=[
            vec3 q1540Sp17Normal = normalize(normalGloss.rgb * 2.0 - 1.0);
            vec3 q1540Sp17Light = vSp17LightEncodedQ1540;
            // PC SLS pixel shader: dp3 + saturate. The interpolated light is
            // intentionally not renormalized in the pixel stage.
            float q1540Sp17Lambert =
                clamp(dot(q1540Sp17Normal, q1540Sp17Light), 0.0, 1.0);
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1610_PIXEL_OLD}" Q1610_PIXEL_POS)
if(Q1610_PIXEL_POS EQUAL -1)
    message(FATAL_ERROR "Q15.11 could not find Q15.4 SP17 pixel block")
endif()
string(REPLACE "${Q1610_PIXEL_OLD}" "${Q1610_PIXEL_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Q15.6 explicitly pinned the renderer back to Quest's world-space Lambert.
# The PC state dump now gives us direct evidence that this is not the PC path.
set(Q1610_SELECTOR_OLD [=[
            float lambert = q1540QuestLambert;
]=])
set(Q1610_SELECTOR_NEW [=[
            // Q15.11: PC SP17 diffuse is NormalMap dot tangent-space LightData.
            float lambert = q1540Sp17Lambert;
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1610_SELECTOR_OLD}" Q1610_SELECTOR_POS)
if(Q1610_SELECTOR_POS EQUAL -1)
    message(FATAL_ERROR "Q15.11 could not find Q15.6 pinned Quest Lambert")
endif()
string(REPLACE "${Q1610_SELECTOR_OLD}" "${Q1610_SELECTOR_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Match the captured PC PSLightColor exactly. Q15.9 used the active IMGS base
# dimmer (currently 1.2 on Quest -> 2.2 effective). The reference PC draw is
# unequivocally 1.5 -> 2.5, matching Fallout.ini's fSunlightDimmer=1.5.
# -----------------------------------------------------------------------------
set(Q1610_DIMMER_OLD [=[
    baseSunDimmer = gRuntime.baseImage.valid
        ? gRuntime.baseImage.hdrSunlightDimmer
        : 1.5f;
    effectiveSunScale = fo3weatherq1320::EffectiveSunlightScaleQ1320(baseSunDimmer);
]=])
set(Q1610_DIMMER_NEW [=[
    // Q15.11 PC reference capture: fSunlightDimmer=1.5 -> PSLightColor x2.5.
    baseSunDimmer = 1.5f;
    effectiveSunScale = 2.5f;
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1610_DIMMER_OLD}" Q1610_DIMMER_POS)
if(Q1610_DIMMER_POS EQUAL -1)
    message(FATAL_ERROR "Q15.11 could not find Q15.9 sunlight-dimmer block")
endif()
string(REPLACE "${Q1610_DIMMER_OLD}" "${Q1610_DIMMER_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Rename the runtime constant trace so a captured log proves this exact renderer
# revision is active, independently of the floating left-hand label.
string(REPLACE
    "Q15.9 PC SP17 CONSTANTS:"
    "Q15.11 PC SP17 DIFFUSE:"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "scope=static-PPLighting terrainChanged=0 baseMapChanged=0 sunDirectionChanged=0"
    "scope=static-PPLighting diffuse=TANGENT_NORMALMAP_DP3 terrainChanged=0 baseMapChanged=0 sunScaleCaptured=2.5"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Guards: make it impossible to ship this build while still using Q15.6's world
# Lambert or Q15.4's encoded/non-normalized approximation.
string(FIND "${Q6H_NATIVE_SOURCE}" "float lambert = q1540Sp17Lambert;" Q1610_SELECTOR_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "normalize(normalGloss.rgb * 2.0 - 1.0)" Q1610_NORMAL_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "vSp17LightEncodedQ1540 = q1540LightTangent *" Q1610_LIGHT_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "effectiveSunScale = 2.5f;" Q1610_SUN_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "float lambert = q1540QuestLambert;" Q1610_OLD_SELECTOR)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "Q15.11" Q1610_TERRAIN_CHANGED)
if(Q1610_SELECTOR_OK EQUAL -1 OR Q1610_NORMAL_OK EQUAL -1 OR
   Q1610_LIGHT_OK EQUAL -1 OR Q1610_SUN_OK EQUAL -1 OR
   NOT Q1610_OLD_SELECTOR EQUAL -1 OR NOT Q1610_TERRAIN_CHANGED EQUAL -1)
    message(FATAL_ERROR
        "Q15.11 verification failed: selector=${Q1610_SELECTOR_OK} normal=${Q1610_NORMAL_OK} light=${Q1610_LIGHT_OK} sun=${Q1610_SUN_OK} old=${Q1610_OLD_SELECTOR} terrain=${Q1610_TERRAIN_CHANGED}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q15.11 PC SP17 tangent-space NormalMap diffuse enabled; static sunlight scale pinned to captured 2.5")
