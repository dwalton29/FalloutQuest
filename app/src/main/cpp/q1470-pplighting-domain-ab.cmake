# Q14.7: controlled static BSShaderPPLighting world-light colour-domain A/B.
#
# Q14.2 changed only WTHR Ambient back to normalized RGB bytes while BaseMap and
# Sunlight remained in the Q11.8/Q13.9 linear pipeline, so it did not test a
# coherent legacy encoded-domain lighting equation. Q14.7 changes the complete
# static diffuse world-light triplet together:
#
# A / default, LEFT Y unpressed:
#   linear(BaseMap) * (linear(Ambient) + linear(Sunlight) * NdotL)
#
# B / LEFT Y toggle:
#   decode( encode(BaseMap) *
#           (encode(Ambient) + encode(Sunlight) * NdotL) )
#
# The B result is decoded back to FalloutQuest's linear scene buffer immediately
# afterwards. Specular, local LIGH, XEMI/emissive, vertex-colour gating, fog,
# ImageSpace/IMAD, bloom, AO, exposure, LAND and all geometry remain unchanged.
# This is a diagnostic reconstruction of the legacy arithmetic domain, not an
# artistic tint or a claim that the exact D3D9 sampler state is already proven.

# Shared state: Q14.5 already exposes its state header in the active static TU.
string(REPLACE
    "#include \"fo3-fog-ab-q1450.h\""
    "#include \"fo3-fog-ab-q1450.h\"\n#include \"fo3-pplighting-domain-q1470.h\""
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Static/NIF fragment shader.
# -----------------------------------------------------------------------------
string(REPLACE
    "        uniform float uQ1450FogEnabled;"
    "        uniform float uQ1450FogEnabled;\n        uniform float uQ1470LegacyPpDiffuseDomain;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Exact standard-sRGB inverse/forward helpers let us reconstruct the authored
# encoded domain from the already-linear Q11.8/Q13.9 inputs without changing DDS
# upload formats or ESM parsing. Values above 1 are deliberately not upper-
# clamped so existing authored scale multipliers remain observable.
set(Q1470_GLSL_HELPERS [=[
        float Q1470LinearToSrgb1(float value) {
            float c = max(value, 0.0);
            return c <= 0.0031308
                ? c * 12.92
                : 1.055 * pow(c, 1.0 / 2.4) - 0.055;
        }
        vec3 Q1470LinearToSrgb(vec3 value) {
            return vec3(Q1470LinearToSrgb1(value.r),
                        Q1470LinearToSrgb1(value.g),
                        Q1470LinearToSrgb1(value.b));
        }
        float Q1470SrgbToLinear1(float value) {
            float c = max(value, 0.0);
            return c <= 0.04045
                ? c / 12.92
                : pow((c + 0.055) / 1.055, 2.4);
        }
        vec3 Q1470SrgbToLinear(vec3 value) {
            return vec3(Q1470SrgbToLinear1(value.r),
                        Q1470SrgbToLinear1(value.g),
                        Q1470SrgbToLinear1(value.b));
        }

]=])
set(Q1470_SHADOW_FN_ANCHOR [=[
        float Q1050ShadowVisibility(vec4 shadowCoord, vec3 N, vec3 L) {
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1470_SHADOW_FN_ANCHOR}" Q1470_HELPER_POS)
if(Q1470_HELPER_POS EQUAL -1)
    message(FATAL_ERROR "Q14.7 could not find static Q10.5 shadow helper anchor")
endif()
string(REPLACE "${Q1470_SHADOW_FN_ANCHOR}"
       "${Q1470_GLSL_HELPERS}${Q1470_SHADOW_FN_ANCHOR}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1470_OLD_WORLD_LIGHT [=[
            vec3 lit = uNoLighting > 0.5
                ? baseColor
                : baseColor * (uAmbientColor + uSunlightColor * lambert * q1050SunVisibility) + uSunlightColor * specularContribution * q1050SunVisibility;
]=])
set(Q1470_NEW_WORLD_LIGHT [=[
            vec3 q1470WorldDiffuse =
                baseColor * (uAmbientColor + uSunlightColor * lambert * q1050SunVisibility);
            if (uQ1470LegacyPpDiffuseDomain > 0.5 && uNoLighting <= 0.5) {
                vec3 q1470BaseEncoded = Q1470LinearToSrgb(baseColor);
                vec3 q1470AmbientEncoded = Q1470LinearToSrgb(uAmbientColor);
                vec3 q1470SunlightEncoded = Q1470LinearToSrgb(uSunlightColor);
                vec3 q1470EncodedDiffuse = q1470BaseEncoded *
                    (q1470AmbientEncoded +
                     q1470SunlightEncoded * lambert * q1050SunVisibility);
                q1470WorldDiffuse = Q1470SrgbToLinear(q1470EncodedDiffuse);
            }
            vec3 lit = uNoLighting > 0.5
                ? baseColor
                : q1470WorldDiffuse +
                  uSunlightColor * specularContribution * q1050SunVisibility;
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1470_OLD_WORLD_LIGHT}" Q1470_WORLD_LIGHT_POS)
if(Q1470_WORLD_LIGHT_POS EQUAL -1)
    message(FATAL_ERROR "Q14.7 could not find active static PPLighting world-light equation")
endif()
string(REPLACE "${Q1470_OLD_WORLD_LIGHT}" "${Q1470_NEW_WORLD_LIGHT}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Uniform handle / lookup / draw upload.
string(REPLACE
    "GLint gFogEnabledLocationQ1450 = -1;"
    "GLint gFogEnabledLocationQ1450 = -1;\nGLint gPpDiffuseDomainLocationQ1470 = -1;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "    gFogEnabledLocationQ1450 = glGetUniformLocation(gProgram, \"uQ1450FogEnabled\");"
    "    gFogEnabledLocationQ1450 = glGetUniformLocation(gProgram, \"uQ1450FogEnabled\");\n    gPpDiffuseDomainLocationQ1470 = glGetUniformLocation(gProgram, \"uQ1470LegacyPpDiffuseDomain\");"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1470_UPLOAD_ANCHOR [=[
    if (gFogEnabledLocationQ1450 >= 0) {
        glUniform1f(gFogEnabledLocationQ1450, GetFo3FogEnabledQ1450() ? 1.0f : 0.0f);
    }
]=])
set(Q1470_UPLOAD_NEW [=[
    if (gFogEnabledLocationQ1450 >= 0) {
        glUniform1f(gFogEnabledLocationQ1450, GetFo3FogEnabledQ1450() ? 1.0f : 0.0f);
    }
    if (gPpDiffuseDomainLocationQ1470 >= 0) {
        glUniform1f(gPpDiffuseDomainLocationQ1470,
                    GetFo3LegacyPpDiffuseDomainQ1470() ? 1.0f : 0.0f);
    }
    static bool q1470ReadyLogged = false;
    if (!q1470ReadyLogged) {
        q1470ReadyLogged = true;
        Q6H_LOGI("Q14.7 PPLIGHTING DOMAIN A/B READY: mode=%s control=LEFT_Y scope=static-world-diffuse baseAmbientSunlightTogether=1 outputBackToLinear=1 landUnchanged=1 specularUnchanged=1 localLightsUnchanged=1 fogUnchanged=1 postUnchanged=1",
                 GetFo3PpDiffuseDomainNameQ1470());
    }
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1470_UPLOAD_ANCHOR}" Q1470_UPLOAD_POS)
if(Q1470_UPLOAD_POS EQUAL -1)
    message(FATAL_ERROR "Q14.7 could not find Q14.5 static uniform upload anchor")
endif()
string(REPLACE "${Q1470_UPLOAD_ANCHOR}" "${Q1470_UPLOAD_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Final OpenXR host: LEFT Y toggles Q14.7. LEFT X remains Q14.5 fog, LEFT trigger
# remains Q14.0 time advance, and right trigger remains activation.
# -----------------------------------------------------------------------------
set(Q1470_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1470_Q4_INPUT}")
    message(FATAL_ERROR "Q14.7 expected final Q14.5 OpenXR source at ${Q1470_Q4_INPUT}")
endif()
file(READ "${Q1470_Q4_INPUT}" Q1470_Q4_SOURCE)

string(REPLACE
    "#include \"fo3-fog-ab-q1450.h\""
    "#include \"fo3-fog-ab-q1450.h\"\n#include \"fo3-pplighting-domain-q1470.h\""
    Q1470_Q4_SOURCE "${Q1470_Q4_SOURCE}")

set(Q1470_ACTION_ANCHOR [=[
        if (!CreateAction("fog_toggle", "Fog Toggle", XR_ACTION_TYPE_BOOLEAN_INPUT,
                          1, &handPaths_[0], &fogToggleAction_)) return false;
]=])
set(Q1470_ACTION_NEW [=[
        if (!CreateAction("fog_toggle", "Fog Toggle", XR_ACTION_TYPE_BOOLEAN_INPUT,
                          1, &handPaths_[0], &fogToggleAction_)) return false;
        if (!CreateAction("pplighting_domain_toggle", "PPLighting Domain Toggle",
                          XR_ACTION_TYPE_BOOLEAN_INPUT,
                          1, &handPaths_[0], &ppDomainToggleAction_)) return false;
]=])
string(FIND "${Q1470_Q4_SOURCE}" "${Q1470_ACTION_ANCHOR}" Q1470_ACTION_POS)
if(Q1470_ACTION_POS EQUAL -1)
    message(FATAL_ERROR "Q14.7 could not find Q14.5 fog action anchor")
endif()
string(REPLACE "${Q1470_ACTION_ANCHOR}" "${Q1470_ACTION_NEW}"
       Q1470_Q4_SOURCE "${Q1470_Q4_SOURCE}")

string(REPLACE
    "        XrPath leftFogX = XR_NULL_PATH;"
    "        XrPath leftFogX = XR_NULL_PATH;\n        XrPath leftPpDomainY = XR_NULL_PATH;"
    Q1470_Q4_SOURCE "${Q1470_Q4_SOURCE}")
string(REPLACE
    "            !Path(\"/user/hand/left/input/x/click\", &leftFogX)) return false;"
    "            !Path(\"/user/hand/left/input/x/click\", &leftFogX) ||\n            !Path(\"/user/hand/left/input/y/click\", &leftPpDomainY)) return false;"
    Q1470_Q4_SOURCE "${Q1470_Q4_SOURCE}")
string(REPLACE
    "        const std::array<XrActionSuggestedBinding, 8> bindings{{"
    "        const std::array<XrActionSuggestedBinding, 9> bindings{{"
    Q1470_Q4_SOURCE "${Q1470_Q4_SOURCE}")
string(REPLACE
    "            {fogToggleAction_, leftFogX},"
    "            {fogToggleAction_, leftFogX},\n            {ppDomainToggleAction_, leftPpDomainY},"
    Q1470_Q4_SOURCE "${Q1470_Q4_SOURCE}")

set(Q1470_SAMPLE_ANCHOR [=[
        fogTogglePressed_ = q1450FogPressed;
]=])
set(Q1470_SAMPLE_NEW [=[
        fogTogglePressed_ = q1450FogPressed;
        const bool q1470DomainPressed =
            ReadBooleanActionQ1450(ppDomainToggleAction_, handPaths_[0]);
        if (q1470DomainPressed && !ppDomainTogglePressed_) {
            ToggleFo3LegacyPpDiffuseDomainQ1470();
            FQ_LOGI("Q14.7 PPLIGHTING DOMAIN MODE: mode=%s source=LEFT_Y scope=static-world-diffuse",
                    GetFo3PpDiffuseDomainNameQ1470());
        }
        ppDomainTogglePressed_ = q1470DomainPressed;
]=])
string(FIND "${Q1470_Q4_SOURCE}" "${Q1470_SAMPLE_ANCHOR}" Q1470_SAMPLE_POS)
if(Q1470_SAMPLE_POS EQUAL -1)
    message(FATAL_ERROR "Q14.7 could not find Q14.5 input sample anchor")
endif()
string(REPLACE "${Q1470_SAMPLE_ANCHOR}" "${Q1470_SAMPLE_NEW}"
       Q1470_Q4_SOURCE "${Q1470_Q4_SOURCE}")

string(REPLACE
    "    XrAction fogToggleAction_{XR_NULL_HANDLE};"
    "    XrAction fogToggleAction_{XR_NULL_HANDLE};\n    XrAction ppDomainToggleAction_{XR_NULL_HANDLE};"
    Q1470_Q4_SOURCE "${Q1470_Q4_SOURCE}")
string(REPLACE
    "    bool fogTogglePressed_{false};"
    "    bool fogTogglePressed_{false};\n    bool ppDomainTogglePressed_{false};"
    Q1470_Q4_SOURCE "${Q1470_Q4_SOURCE}")

# Hard drift guards. These deliberately prove LAND was not patched by Q14.7.
string(FIND "${Q6H_NATIVE_SOURCE}" "uQ1470LegacyPpDiffuseDomain" Q1470_SHADER_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "q1470BaseEncoded" Q1470_BASE_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "q1470AmbientEncoded" Q1470_AMBIENT_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "q1470SunlightEncoded" Q1470_SUN_OK)
string(FIND "${Q1470_Q4_SOURCE}" "pplighting_domain_toggle" Q1470_ACTION_OK)
string(FIND "${Q1470_Q4_SOURCE}" "/user/hand/left/input/y/click" Q1470_BIND_OK)
string(FIND "${Q1470_Q4_SOURCE}" "ToggleFo3LegacyPpDiffuseDomainQ1470" Q1470_TOGGLE_OK)
string(FIND "${Q1470_Q4_SOURCE}" "ppDomainTogglePressed_{false}" Q1470_STATE_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "uQ1470LegacyPpDiffuseDomain" Q1470_LAND_UNEXPECTED)
if(Q1470_SHADER_OK EQUAL -1 OR Q1470_BASE_OK EQUAL -1 OR
   Q1470_AMBIENT_OK EQUAL -1 OR Q1470_SUN_OK EQUAL -1 OR
   Q1470_ACTION_OK EQUAL -1 OR Q1470_BIND_OK EQUAL -1 OR
   Q1470_TOGGLE_OK EQUAL -1 OR Q1470_STATE_OK EQUAL -1)
    message(FATAL_ERROR
        "Q14.7 hook drifted: shader=${Q1470_SHADER_OK} base=${Q1470_BASE_OK} ambient=${Q1470_AMBIENT_OK} sun=${Q1470_SUN_OK} action=${Q1470_ACTION_OK} bind=${Q1470_BIND_OK} toggle=${Q1470_TOGGLE_OK} state=${Q1470_STATE_OK}")
endif()
if(NOT Q1470_LAND_UNEXPECTED EQUAL -1)
    message(FATAL_ERROR "Q14.7 unexpectedly modified LAND shader")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")
file(WRITE "${Q1470_Q4_INPUT}" "${Q1470_Q4_SOURCE}")

message(STATUS "Q14.7 static PPLighting linear/legacy-encoded diffuse-domain A/B enabled on LEFT Y")
