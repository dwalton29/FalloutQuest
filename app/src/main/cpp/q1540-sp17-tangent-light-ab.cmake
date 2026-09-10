# Q15.4: isolate Shader Package 17's tangent-space PPLighting light-vector path.
#
# Exact shaderpackage017 SLS1011.vso/.pso shows Bethesda does not reconstruct the
# sampled normal into world space for the diffuse dot. The vertex shader projects
# LightData onto the mesh T/B/N basis, encodes that tangent-space vector to 0..1,
# interpolates it, and the pixel shader decodes both vectors before DP3_sat.
#
# A / default (LEFT X unpressed):
#   Current FalloutQuest world-space mapped-normal Lambert path, unchanged.
#
# B / LEFT X:
#   SP17-style per-vertex tangent-space light vector + pixel DP3_sat against the
#   raw decoded normal-map vector.
#
# Q15.3b's fog A/B is retired here: both states use the exact same Q15.2
# per-fragment authored fog, so LEFT X changes only static/PPLighting diffuse
# direction staging. LAND, WTHR RGB, AmbientColor/SunlightColor, ImageSpace,
# specular, local lights, materials and post processing are unchanged.

# -----------------------------------------------------------------------------
# Static/PPLighting vertex stage: reproduce SLS1011 LightData -> tangent basis.
# -----------------------------------------------------------------------------
set(Q1540_VERTEX_DECL_OLD [=[
        uniform highp float uFogPowerVertexQ1532;
        out vec3 vNormal;
        out highp float vFogFactorQ1532;
]=])
set(Q1540_VERTEX_DECL_NEW [=[
        uniform highp float uFogPowerVertexQ1532;
        uniform highp vec3 uSunDirectionVertexQ1540;
        out vec3 vNormal;
        out highp float vFogFactorQ1532;
        out highp vec3 vSp17LightEncodedQ1540;
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1540_VERTEX_DECL_OLD}" Q1540_VERTEX_DECL_POS)
if(Q1540_VERTEX_DECL_POS EQUAL -1)
    message(FATAL_ERROR "Q15.4 could not find Q15.3b static vertex declaration anchor")
endif()
string(REPLACE "${Q1540_VERTEX_DECL_OLD}" "${Q1540_VERTEX_DECL_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1540_VERTEX_ASSIGN_OLD [=[
            vBitangent = aBitangent;
            vUv = aUv;
]=])
set(Q1540_VERTEX_ASSIGN_NEW [=[
            vBitangent = aBitangent;
            vUv = aUv;
            // SLS1011.vso: LightData is dotted with the authored T/B/N rows in
            // the vertex stage, then encoded for interpolation to the pixel stage.
            vec3 q1540LightData = normalize(uSunDirectionVertexQ1540);
            vec3 q1540LightTangent = vec3(
                dot(aTangent, q1540LightData),
                dot(aBitangent, q1540LightData),
                dot(aNormal, q1540LightData));
            vSp17LightEncodedQ1540 = q1540LightTangent * 0.5 + 0.5;
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1540_VERTEX_ASSIGN_OLD}" Q1540_VERTEX_ASSIGN_POS)
if(Q1540_VERTEX_ASSIGN_POS EQUAL -1)
    message(FATAL_ERROR "Q15.4 could not find static TBN/UV vertex assignment anchor")
endif()
string(REPLACE "${Q1540_VERTEX_ASSIGN_OLD}" "${Q1540_VERTEX_ASSIGN_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "        in highp float vFogFactorQ1532;"
    "        in highp float vFogFactorQ1532;\n        in highp vec3 vSp17LightEncodedQ1540;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Static/PPLighting pixel stage: exact structural SLS1011 diffuse comparison.
# Keep Quest's existing path byte-for-byte as A. B uses raw decoded normal map,
# decoded interpolated tangent light, and DP3 clamped to the ps_2_b saturate range.
# -----------------------------------------------------------------------------
set(Q1540_LAMBERT_OLD [=[
            vec3 lightDirection = normalize(uSunDirection);
            float lambert = max(dot(mappedNormal, lightDirection), 0.0);
]=])
set(Q1540_LAMBERT_NEW [=[
            vec3 lightDirection = normalize(uSunDirection);
            float q1540QuestLambert = max(dot(mappedNormal, lightDirection), 0.0);

            vec3 q1540Sp17Normal = normalGloss.rgb * 2.0 - 1.0;
            q1540Sp17Normal.xy *= uNormalStrength;
            vec3 q1540Sp17Light = vSp17LightEncodedQ1540 * 2.0 - 1.0;
            float q1540Sp17Lambert =
                clamp(dot(q1540Sp17Normal, q1540Sp17Light), 0.0, 1.0);

            float lambert = uQ1450FogEnabled > 0.5
                ? q1540QuestLambert
                : q1540Sp17Lambert;
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1540_LAMBERT_OLD}" Q1540_LAMBERT_POS)
if(Q1540_LAMBERT_POS EQUAL -1)
    message(FATAL_ERROR "Q15.4 could not find current static world-space Lambert block")
endif()
string(REPLACE "${Q1540_LAMBERT_OLD}" "${Q1540_LAMBERT_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Retire Q15.3b's fog selector. Both X states now retain Q15.2 fragment fog.
# Do this to statics and LAND so the visual A/B cannot be contaminated by fog.
# -----------------------------------------------------------------------------
set(Q1540_FOG_OLD [=[
            float q1532QuestFragmentFog = pow(fogT, max(uFogPower, 0.01));
            float fogFactor = uQ1450FogEnabled > 0.5
                ? q1532QuestFragmentFog
                : vFogFactorQ1532;
            lit = mix(lit, uFogColor, fogFactor);
]=])
set(Q1540_FOG_NEW [=[
            float q1532QuestFragmentFog = pow(fogT, max(uFogPower, 0.01));
            float fogFactor = q1532QuestFragmentFog;
            lit = mix(lit, uFogColor, fogFactor);
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1540_FOG_OLD}" Q1540_STATIC_FOG_POS)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1540_FOG_OLD}" Q1540_LAND_FOG_POS)
if(Q1540_STATIC_FOG_POS EQUAL -1 OR Q1540_LAND_FOG_POS EQUAL -1)
    message(FATAL_ERROR
        "Q15.4 could not find Q15.3b fog selectors: static=${Q1540_STATIC_FOG_POS} land=${Q1540_LAND_FOG_POS}")
endif()
string(REPLACE "${Q1540_FOG_OLD}" "${Q1540_FOG_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE "${Q1540_FOG_OLD}" "${Q1540_FOG_NEW}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# Static vertex-only sun direction uniform. Use a unique cross-stage name so the
# Adreno linker never has to reconcile precision for uSunDirection.
string(REPLACE
    "GLint gFogPowerVertexLocationQ1532 = -1;"
    "GLint gFogPowerVertexLocationQ1532 = -1;\nGLint gSunDirectionVertexLocationQ1540 = -1;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "    gFogPowerVertexLocationQ1532 = glGetUniformLocation(gProgram, \"uFogPowerVertexQ1532\");"
    "    gFogPowerVertexLocationQ1532 = glGetUniformLocation(gProgram, \"uFogPowerVertexQ1532\");\n    gSunDirectionVertexLocationQ1540 = glGetUniformLocation(gProgram, \"uSunDirectionVertexQ1540\");"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1540_STATIC_UPLOAD_OLD [=[
    if (gFogPowerVertexLocationQ1532 >= 0)
        glUniform1f(gFogPowerVertexLocationQ1532, GetFo3FogPowerQ1410());
]=])
set(Q1540_STATIC_UPLOAD_NEW [=[
    if (gFogPowerVertexLocationQ1532 >= 0)
        glUniform1f(gFogPowerVertexLocationQ1532, GetFo3FogPowerQ1410());
    if (gSunDirectionVertexLocationQ1540 >= 0) {
        if (q1000Env.valid) {
            glUniform3fv(gSunDirectionVertexLocationQ1540, 1, q1000Env.sunDirection);
        } else {
            glUniform3f(gSunDirectionVertexLocationQ1540, 0.35f, 0.85f, 0.40f);
        }
    }
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1540_STATIC_UPLOAD_OLD}" Q1540_STATIC_UPLOAD_POS)
if(Q1540_STATIC_UPLOAD_POS EQUAL -1)
    message(FATAL_ERROR "Q15.4 could not find Q15.3b static vertex-fog upload tail")
endif()
string(REPLACE "${Q1540_STATIC_UPLOAD_OLD}" "${Q1540_STATIC_UPLOAD_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Diagnostics: retain the proven LEFT X action plumbing but accurately describe
# its new meaning. Legacy variable/function names remain to avoid touching input.
string(REPLACE
    "Q15.3b SP17 FOG A/B READY:"
    "Q15.4 SP17 TANGENT LIGHT A/B READY:"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "fogDataUnchanged=1 sp17Metric=MVP_PRE_DIVIDE_XYZ sp17Placement=VERTEX_INTERPOLATED"
    "fogPath=Q15.2_IDENTICAL_BOTH_SIDES scope=static-PPLighting sp17Light=TANGENT_VERTEX_ENCODED sp17Normal=RAW_PIXEL_DECODE"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1540_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1540_Q4_INPUT}")
    message(FATAL_ERROR "Q15.4 expected final Q15.3b OpenXR source at ${Q1540_Q4_INPUT}")
endif()
file(READ "${Q1540_Q4_INPUT}" Q1540_Q4_SOURCE)
string(REPLACE
    "Q15.3b SP17 FOG MODE:"
    "Q15.4 SP17 TANGENT LIGHT MODE:"
    Q1540_Q4_SOURCE "${Q1540_Q4_SOURCE}")
string(REPLACE
    "fogDataUnchanged=1 sp17Metric=MVP_PRE_DIVIDE_XYZ sp17Placement=VERTEX_INTERPOLATED"
    "fogPath=Q15.2_IDENTICAL_BOTH_SIDES scope=static-PPLighting sp17Light=TANGENT_VERTEX_ENCODED sp17Normal=RAW_PIXEL_DECODE"
    Q1540_Q4_SOURCE "${Q1540_Q4_SOURCE}")
file(WRITE "${Q1540_Q4_INPUT}" "${Q1540_Q4_SOURCE}")

# Hard guards: SP17 light vector exists only in static/PPLighting, X selects only
# the Lambert source, and Q15.3b's fog selector ternary is gone in both renderers.
string(FIND "${Q6H_NATIVE_SOURCE}" "vSp17LightEncodedQ1540" Q1540_VARYING_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "q1540Sp17Lambert" Q1540_LAMBERT_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "uSunDirectionVertexQ1540" Q1540_VERTEX_SUN_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "? q1540QuestLambert" Q1540_SELECTOR_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "? q1532QuestFragmentFog" Q1540_STATIC_OLD_FOG)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "? q1532QuestFragmentFog" Q1540_LAND_OLD_FOG)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "q1540Sp17Lambert" Q1540_LAND_UNEXPECTED)
string(FIND "${Q1540_Q4_SOURCE}" "Q15.4 SP17 TANGENT LIGHT MODE" Q1540_LOG_OK)
if(Q1540_VARYING_OK EQUAL -1 OR Q1540_LAMBERT_OK EQUAL -1 OR
   Q1540_VERTEX_SUN_OK EQUAL -1 OR Q1540_SELECTOR_OK EQUAL -1 OR
   NOT Q1540_STATIC_OLD_FOG EQUAL -1 OR NOT Q1540_LAND_OLD_FOG EQUAL -1 OR
   NOT Q1540_LAND_UNEXPECTED EQUAL -1 OR Q1540_LOG_OK EQUAL -1)
    message(FATAL_ERROR
        "Q15.4 verification failed: varying=${Q1540_VARYING_OK} lambert=${Q1540_LAMBERT_OK} vertexSun=${Q1540_VERTEX_SUN_OK} selector=${Q1540_SELECTOR_OK} oldStaticFog=${Q1540_STATIC_OLD_FOG} oldLandFog=${Q1540_LAND_OLD_FOG} landUnexpected=${Q1540_LAND_UNEXPECTED} log=${Q1540_LOG_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")

message(STATUS "Q15.4 SP17 tangent-space PPLighting A/B enabled on LEFT X; Q15.2 fog fixed on both sides")
