# Q15.13: reproduce the captured PC Shader Package 17 PPLighting core beyond
# diffuse N.L. Q15.11 already fixed tangent-space LightData + normal-map DP3 and
# Q15.12 proved static BaseMap input must use sRGB decode. The remaining concrete
# shader mismatch at Megaton draw 4601215 is the specular path.
#
# Captured PC shader semantics for the dominant static PPLighting variant:
#   N       = normalize(NormalMap.rgb * 2 - 1)
#   L       = interpolated normalized tangent-space LightData
#   H       = normalize(interpolated tangent-space half vector)
#   rawNL   = dot(N, L)
#   NdotL   = saturate(rawNL)
#   NdotH   = saturate(dot(N, H))
#   diffuse = BaseMap * max(AmbientColor + PSLightColor * NdotL, 0)
#   spec0   = NormalMap.a * pow(NdotH, Glossiness)
#   spec    = rawNL <= 0.2 ? spec0 * saturate(rawNL + 0.5) : spec0
#   specRGB = saturate(PSLightColor * spec * LightData.w)
#   colour  = diffuse + specRGB
#
# At the captured draw LightData.w = 1. Vertex colour is separately gated by
# BSShaderFlags2 and is already correct in Q11.2. Local LIGH, emissive/NoLighting,
# alpha and the VR-safe fog/post stack are deliberately retained after this world
# lighting core. LAND is untouched.

# -----------------------------------------------------------------------------
# Vertex stage: SP17 constructs a world/view half vector per vertex, projects it
# through authored T/B/N, normalizes that tangent vector, then PS NRM normalizes
# the interpolated value again before N.H.
# -----------------------------------------------------------------------------
set(Q1630_VERTEX_DECL_OLD [=[
        uniform highp vec3 uSunDirectionVertexQ1540;
        out vec3 vNormal;
        out highp float vFogFactorQ1532;
        out highp vec3 vSp17LightEncodedQ1540;
]=])
set(Q1630_VERTEX_DECL_NEW [=[
        uniform highp vec3 uSunDirectionVertexQ1540;
        uniform highp vec3 uEyePositionVertexQ1630;
        out vec3 vNormal;
        out highp float vFogFactorQ1532;
        out highp vec3 vSp17LightEncodedQ1540;
        out highp vec3 vSp17HalfQ1630;
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1630_VERTEX_DECL_OLD}" Q1630_VERTEX_DECL_POS)
if(Q1630_VERTEX_DECL_POS EQUAL -1)
    message(FATAL_ERROR "Q15.13 could not find Q15.11 static vertex declaration")
endif()
string(REPLACE "${Q1630_VERTEX_DECL_OLD}" "${Q1630_VERTEX_DECL_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1630_VERTEX_LIGHT_OLD [=[
            float q1610LightLen2 = dot(q1540LightTangent, q1540LightTangent);
            vSp17LightEncodedQ1540 = q1540LightTangent *
                inversesqrt(max(q1610LightLen2, 1.0e-12));
]=])
set(Q1630_VERTEX_LIGHT_NEW [=[
            float q1610LightLen2 = dot(q1540LightTangent, q1540LightTangent);
            vSp17LightEncodedQ1540 = q1540LightTangent *
                inversesqrt(max(q1610LightLen2, 1.0e-12));

            // PC SLS: normalize(EyePosition - vertex), add LightData, normalize,
            // project the half vector into T/B/N, then normalize before oT3.
            vec3 q1630ViewWorld = normalize(uEyePositionVertexQ1630 - aPosition);
            vec3 q1630HalfWorld = normalize(q1630ViewWorld + q1540LightData);
            vec3 q1630HalfTangent = vec3(
                dot(aTangent, q1630HalfWorld),
                dot(aBitangent, q1630HalfWorld),
                dot(aNormal, q1630HalfWorld));
            float q1630HalfLen2 = dot(q1630HalfTangent, q1630HalfTangent);
            vSp17HalfQ1630 = q1630HalfTangent *
                inversesqrt(max(q1630HalfLen2, 1.0e-12));
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1630_VERTEX_LIGHT_OLD}" Q1630_VERTEX_LIGHT_POS)
if(Q1630_VERTEX_LIGHT_POS EQUAL -1)
    message(FATAL_ERROR "Q15.13 could not find Q15.11 tangent-light normalization block")
endif()
string(REPLACE "${Q1630_VERTEX_LIGHT_OLD}" "${Q1630_VERTEX_LIGHT_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "        in highp vec3 vSp17LightEncodedQ1540;"
    "        in highp vec3 vSp17LightEncodedQ1540;\n        in highp vec3 vSp17HalfQ1630;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Pixel stage: replace the old Quest world-space Blinn approximation. Keep
# mappedNormal alive because authored local LIGH still consumes it later.
# -----------------------------------------------------------------------------
set(Q1630_SPEC_OLD [=[
            vec3 viewDirection = normalize(uEyePosition - vPosition);
            vec3 halfVector = normalize(lightDirection + viewDirection);
            float exponent = clamp(uGlossiness, 2.0, 96.0);
            float specularAmount = pow(max(dot(mappedNormal, halfVector), 0.0), exponent)
                                 * normalGloss.a * 0.32 * uSpecularEnabled;
            vec3 specularContribution = uSpecularColor * specularAmount;
]=])
set(Q1630_SPEC_NEW [=[
            // Q15.13: instruction-for-instruction SP17 specular structure from
            // the captured Megaton shader. No synthetic 0.32 attenuation and no
            // material RGB multiplier exist in this PC permutation.
            vec3 q1630Sp17Half = normalize(vSp17HalfQ1630);
            float q1630RawNdotL = dot(q1540Sp17Normal, q1540Sp17Light);
            float q1630NdotH = clamp(dot(q1540Sp17Normal, q1630Sp17Half), 0.0, 1.0);
            float q1630Exponent = clamp(uGlossiness, 0.0, 128.0);
            float q1630SpecBase = normalGloss.a * pow(q1630NdotH, q1630Exponent);
            float q1630SpecLow = q1630SpecBase * clamp(q1630RawNdotL + 0.5, 0.0, 1.0);
            float q1630Spec = q1630RawNdotL <= 0.2 ? q1630SpecLow : q1630SpecBase;
            vec3 q1630SpecularRgb = clamp(uSunlightColor * q1630Spec, 0.0, 1.0)
                                  * uSpecularEnabled;
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1630_SPEC_OLD}" Q1630_SPEC_POS)
if(Q1630_SPEC_POS EQUAL -1)
    message(FATAL_ERROR "Q15.13 could not find old Quest static specular block")
endif()
string(REPLACE "${Q1630_SPEC_OLD}" "${Q1630_SPEC_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Q15.7 owns the final static world-diffuse staging block. Reproduce SP17's max
# against zero explicitly and replace only the old specular add. Q15.9 pins the
# dormant legacy-domain selector off, but retain it for historical diagnostics.
set(Q1630_WORLD_OLD [=[
            vec3 q1470WorldDiffuse =
                baseColor * (uAmbientColor + uSunlightColor * lambert * q1050SunVisibility);
            if (uLegacyColourDomainQ1570 > 0.5 && uNoLighting <= 0.5) {
                // Fallout3.exe stages WTHR colour as byte/255. Reconstruct the
                // authored BaseMap sample's encoded domain, do the SP17 diffuse
                // multiply there, then return to Quest's linear scene buffer.
                vec3 q1570BaseEncoded = Q1470LinearToSrgb(baseColor);
                vec3 q1570EncodedDiffuse = q1570BaseEncoded *
                    (uLegacyAmbientQ1570 +
                     uLegacySunlightQ1570 * lambert * q1050SunVisibility);
                q1470WorldDiffuse = Q1470SrgbToLinear(q1570EncodedDiffuse);
            }
            vec3 lit = uNoLighting > 0.5
                ? baseColor
                : q1470WorldDiffuse +
                  uSunlightColor * specularContribution * q1050SunVisibility;
]=])
set(Q1630_WORLD_NEW [=[
            vec3 q1630Sp17Lighting = max(
                uAmbientColor + uSunlightColor * lambert, vec3(0.0));
            vec3 q1470WorldDiffuse = baseColor * q1630Sp17Lighting;
            if (uLegacyColourDomainQ1570 > 0.5 && uNoLighting <= 0.5) {
                vec3 q1570BaseEncoded = Q1470LinearToSrgb(baseColor);
                vec3 q1570EncodedDiffuse = q1570BaseEncoded *
                    (uLegacyAmbientQ1570 + uLegacySunlightQ1570 * lambert);
                q1470WorldDiffuse = Q1470SrgbToLinear(q1570EncodedDiffuse);
            }
            vec3 lit = uNoLighting > 0.5
                ? baseColor
                : q1470WorldDiffuse + q1630SpecularRgb;
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1630_WORLD_OLD}" Q1630_WORLD_POS)
if(Q1630_WORLD_POS EQUAL -1)
    message(FATAL_ERROR "Q15.13 could not find final Q15.7/Q15.12 static world-light block")
endif()
string(REPLACE "${Q1630_WORLD_OLD}" "${Q1630_WORLD_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Vertex-only eye position location/upload. This follows the same per-eye value
# already used by the fragment/local-light path while avoiding GLSL cross-stage
# precision/link ambiguity.
# -----------------------------------------------------------------------------
string(REPLACE
    "GLint gSunDirectionVertexLocationQ1540 = -1;"
    "GLint gSunDirectionVertexLocationQ1540 = -1;\nGLint gEyePositionVertexLocationQ1630 = -1;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "    gSunDirectionVertexLocationQ1540 = glGetUniformLocation(gProgram, \"uSunDirectionVertexQ1540\");"
    "    gSunDirectionVertexLocationQ1540 = glGetUniformLocation(gProgram, \"uSunDirectionVertexQ1540\");\n    gEyePositionVertexLocationQ1630 = glGetUniformLocation(gProgram, \"uEyePositionVertexQ1630\");"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1630_SUN_UPLOAD_OLD [=[
    if (gSunDirectionVertexLocationQ1540 >= 0) {
        if (q1000Env.valid) {
            glUniform3fv(gSunDirectionVertexLocationQ1540, 1, q1000Env.sunDirection);
        } else {
            glUniform3f(gSunDirectionVertexLocationQ1540, 0.35f, 0.85f, 0.40f);
        }
    }
]=])
set(Q1630_SUN_UPLOAD_NEW [=[
    if (gSunDirectionVertexLocationQ1540 >= 0) {
        if (q1000Env.valid) {
            glUniform3fv(gSunDirectionVertexLocationQ1540, 1, q1000Env.sunDirection);
        } else {
            glUniform3f(gSunDirectionVertexLocationQ1540, 0.35f, 0.85f, 0.40f);
        }
    }
    if (gEyePositionVertexLocationQ1630 >= 0) {
        glUniform3fv(gEyePositionVertexLocationQ1630, 1, gFo3EyePositionQ1010);
    }
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1630_SUN_UPLOAD_OLD}" Q1630_SUN_UPLOAD_POS)
if(Q1630_SUN_UPLOAD_POS EQUAL -1)
    message(FATAL_ERROR "Q15.13 could not find Q15.4 vertex sunlight upload")
endif()
string(REPLACE "${Q1630_SUN_UPLOAD_OLD}" "${Q1630_SUN_UPLOAD_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Runtime proof line.
string(REPLACE
    "Q15.12 SRGB BASEMAP:"
    "Q15.13 SP17 CORE:"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "scope=static-PPLighting diffuse=TANGENT_NORMALMAP_DP3 terrainChanged=0 baseMap=GL_SRGB8_ALPHA8_DECODE sunScaleCaptured=2.5"
    "scope=static-PPLighting core=PC_DP3_DIFFUSE_TANGENT_HALF_SPEC normalAlphaSpec=1 lowNdotLSpecGate=1 syntheticSpec032=0 baseMap=GL_SRGB8_ALPHA8_DECODE terrainChanged=0 sunScaleCaptured=2.5"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Floating build identity: Q15.12 -> Q15.13.
set(Q1630_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1630_Q4_INPUT}")
    message(FATAL_ERROR "Q15.13 expected final OpenXR source at ${Q1630_Q4_INPUT}")
endif()
file(READ "${Q1630_Q4_INPUT}" Q1630_Q4_SOURCE)
string(REPLACE
    "q1600Digit(q1600X, 0x5Bu); // 2 = A B G E D"
    "q1600Digit(q1600X, 0x4Fu); // 3 = A B C D G"
    Q1630_Q4_SOURCE "${Q1630_Q4_SOURCE}")
string(REPLACE "Q15.12" "Q15.13" Q1630_Q4_SOURCE "${Q1630_Q4_SOURCE}")
file(WRITE "${Q1630_Q4_INPUT}" "${Q1630_Q4_SOURCE}")

# Hard guards: exact new core is in statics, arbitrary Quest spec attenuation is
# gone, Q15.12 sRGB BaseMap survives, LAND is untouched, and the headset label
# independently proves this APK revision.
string(FIND "${Q6H_NATIVE_SOURCE}" "vSp17HalfQ1630" Q1630_HALF_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "q1630RawNdotL <= 0.2" Q1630_SPEC_GATE_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "normalGloss.a * pow(q1630NdotH" Q1630_NORMAL_ALPHA_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "q1470WorldDiffuse + q1630SpecularRgb" Q1630_WORLD_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "* 0.32 * uSpecularEnabled" Q1630_OLD_032)
string(FIND "${Q6H_NATIVE_SOURCE}" "std::string(label) == \"DIFFUSE\" ? GL_SRGB8_ALPHA8 : GL_RGBA8" Q1630_SRGB_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "Q15.13" Q1630_TERRAIN_CHANGED)
string(FIND "${Q1630_Q4_SOURCE}" "text=Q15.13 anchor=left-hand" Q1630_LABEL_OK)
string(FIND "${Q1630_Q4_SOURCE}" "q1600Digit(q1600X, 0x4Fu)" Q1630_LABEL_THREE_OK)
if(Q1630_HALF_OK EQUAL -1 OR Q1630_SPEC_GATE_OK EQUAL -1 OR
   Q1630_NORMAL_ALPHA_OK EQUAL -1 OR Q1630_WORLD_OK EQUAL -1 OR
   NOT Q1630_OLD_032 EQUAL -1 OR Q1630_SRGB_OK EQUAL -1 OR
   NOT Q1630_TERRAIN_CHANGED EQUAL -1 OR Q1630_LABEL_OK EQUAL -1 OR
   Q1630_LABEL_THREE_OK EQUAL -1)
    message(FATAL_ERROR
        "Q15.13 verification failed: half=${Q1630_HALF_OK} gate=${Q1630_SPEC_GATE_OK} normalAlpha=${Q1630_NORMAL_ALPHA_OK} world=${Q1630_WORLD_OK} old032=${Q1630_OLD_032} srgb=${Q1630_SRGB_OK} terrain=${Q1630_TERRAIN_CHANGED} label=${Q1630_LABEL_OK} label3=${Q1630_LABEL_THREE_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q15.13 captured PC SP17 diffuse+specular core enabled for static PPLighting; Q15.12 sRGB BaseMap retained; label=Q15.13")
