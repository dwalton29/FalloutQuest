# Q15.7: Fallout3.exe/SP17 legacy world-light colour-domain A/B.
#
# Q15.6 proved the cyan error is already present in RAW, before fog/HDR/cinematic.
# Re-reading the exact PC path shows WTHR RGB is blended as bytes then multiplied
# by 1/255; there is no sRGB->linear decode before AmbientColor / PSLightColor.
# Q11.8/Q13.9 currently modernise both BaseMap and WTHR into a linear-light
# equation. This test reconstructs the legacy encoded-domain diffuse arithmetic
# without inventing any tint or neutral colour.
#
# LEFT Y:
#   A/default = current FalloutQuest linear world diffuse
#   B         = encoded BaseMap * (raw byte/255 Ambient + raw byte/255 Sunlight*NdotL)
#               decoded back to linear only for the Quest scene buffer
#
# LEFT X remains Q15.6's independent FINAL -> RAW -> FOG -> HDR stage isolator.
# Specular/local lights/fog/post are deliberately unchanged so this isolates the
# world-diffuse colour domain only. The same path is applied to statics and LAND.

# Shared A/B state in static renderer, terrain renderer and OpenXR input.
string(REPLACE
    "#include \"fo3-render-stage-q1560.h\""
    "#include \"fo3-render-stage-q1560.h\"\n#include \"fo3-legacy-colour-domain-q1570.h\""
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "#include \"fo3-render-stage-q1560.h\""
    "#include \"fo3-render-stage-q1560.h\"\n#include \"fo3-legacy-colour-domain-q1570.h\""
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# -----------------------------------------------------------------------------
# Static / BSShaderPPLighting shader.
# Keep the existing linear uniforms untouched for the A path and for specular.
# Feed a second pair of raw WTHR constants only to the B diffuse equation.
# -----------------------------------------------------------------------------
set(Q1570_SHADER_UNIFORM_OLD [=[
        uniform float uQ1450FogEnabled;
        uniform int uRenderStageQ1560;
]=])
set(Q1570_SHADER_UNIFORM_NEW [=[
        uniform float uQ1450FogEnabled;
        uniform int uRenderStageQ1560;
        uniform float uLegacyColourDomainQ1570;
        uniform vec3 uLegacyAmbientQ1570;
        uniform vec3 uLegacySunlightQ1570;
]=])

string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1570_SHADER_UNIFORM_OLD}" Q1570_STATIC_UNIFORM_POS)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1570_SHADER_UNIFORM_OLD}" Q1570_LAND_UNIFORM_POS)
if(Q1570_STATIC_UNIFORM_POS EQUAL -1 OR Q1570_LAND_UNIFORM_POS EQUAL -1)
    message(FATAL_ERROR
        "Q15.7 could not find Q15.6 shader uniform anchors: static=${Q1570_STATIC_UNIFORM_POS} land=${Q1570_LAND_UNIFORM_POS}")
endif()
string(REPLACE "${Q1570_SHADER_UNIFORM_OLD}" "${Q1570_SHADER_UNIFORM_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE "${Q1570_SHADER_UNIFORM_OLD}" "${Q1570_SHADER_UNIFORM_NEW}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

set(Q1570_STATIC_WORLD_OLD [=[
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
]=])
set(Q1570_STATIC_WORLD_NEW [=[
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
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1570_STATIC_WORLD_OLD}" Q1570_STATIC_WORLD_POS)
if(Q1570_STATIC_WORLD_POS EQUAL -1)
    message(FATAL_ERROR "Q15.7 could not find dormant Q14.7 static encoded-domain block")
endif()
string(REPLACE "${Q1570_STATIC_WORLD_OLD}" "${Q1570_STATIC_WORLD_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# LAND shader. Terrain batches each sample one authored texture, so converting
# the sampled albedo back to its encoded domain reconstructs the same arithmetic
# without re-uploading textures or altering alpha-layer composition.
# -----------------------------------------------------------------------------
set(Q1570_LAND_HELPERS [=[
        float Q1570LinearToSrgb1(float value) {
            float c = max(value, 0.0);
            return c <= 0.0031308
                ? c * 12.92
                : 1.055 * pow(c, 1.0 / 2.4) - 0.055;
        }
        vec3 Q1570LinearToSrgb(vec3 value) {
            return vec3(Q1570LinearToSrgb1(value.r),
                        Q1570LinearToSrgb1(value.g),
                        Q1570LinearToSrgb1(value.b));
        }
        float Q1570SrgbToLinear1(float value) {
            float c = max(value, 0.0);
            return c <= 0.04045
                ? c / 12.92
                : pow((c + 0.055) / 1.055, 2.4);
        }
        vec3 Q1570SrgbToLinear(vec3 value) {
            return vec3(Q1570SrgbToLinear1(value.r),
                        Q1570SrgbToLinear1(value.g),
                        Q1570SrgbToLinear1(value.b));
        }

]=])
set(Q1570_LAND_HELPER_ANCHOR [=[
        float Q1050ShadowVisibility(vec4 shadowCoord, vec3 N, vec3 L) {
]=])
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1570_LAND_HELPER_ANCHOR}" Q1570_LAND_HELPER_POS)
if(Q1570_LAND_HELPER_POS EQUAL -1)
    message(FATAL_ERROR "Q15.7 could not find LAND shadow-helper insertion point")
endif()
string(REPLACE "${Q1570_LAND_HELPER_ANCHOR}"
       "${Q1570_LAND_HELPERS}${Q1570_LAND_HELPER_ANCHOR}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

set(Q1570_LAND_WORLD_OLD [=[
            vec3 lit = albedo * (uAmbientColor + uSunlightColor * lambert * q1050SunVisibility);
]=])
set(Q1570_LAND_WORLD_NEW [=[
            vec3 q1570LandWorldDiffuse =
                albedo * (uAmbientColor + uSunlightColor * lambert * q1050SunVisibility);
            if (uLegacyColourDomainQ1570 > 0.5) {
                vec3 q1570LandBaseEncoded = Q1570LinearToSrgb(albedo);
                vec3 q1570LandEncodedDiffuse = q1570LandBaseEncoded *
                    (uLegacyAmbientQ1570 +
                     uLegacySunlightQ1570 * lambert * q1050SunVisibility);
                q1570LandWorldDiffuse = Q1570SrgbToLinear(q1570LandEncodedDiffuse);
            }
            vec3 lit = q1570LandWorldDiffuse;
]=])
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1570_LAND_WORLD_OLD}" Q1570_LAND_WORLD_POS)
if(Q1570_LAND_WORLD_POS EQUAL -1)
    message(FATAL_ERROR "Q15.7 could not find active LAND world-diffuse equation")
endif()
string(REPLACE "${Q1570_LAND_WORLD_OLD}" "${Q1570_LAND_WORLD_NEW}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# -----------------------------------------------------------------------------
# Uniform handles / lookups.
# -----------------------------------------------------------------------------
string(REPLACE
    "GLint gRenderStageLocationQ1560 = -1;"
    "GLint gRenderStageLocationQ1560 = -1;\nGLint gLegacyColourDomainLocationQ1570 = -1;\nGLint gLegacyAmbientLocationQ1570 = -1;\nGLint gLegacySunlightLocationQ1570 = -1;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "    gRenderStageLocationQ1560 = glGetUniformLocation(gProgram, \"uRenderStageQ1560\");"
    "    gRenderStageLocationQ1560 = glGetUniformLocation(gProgram, \"uRenderStageQ1560\");\n    gLegacyColourDomainLocationQ1570 = glGetUniformLocation(gProgram, \"uLegacyColourDomainQ1570\");\n    gLegacyAmbientLocationQ1570 = glGetUniformLocation(gProgram, \"uLegacyAmbientQ1570\");\n    gLegacySunlightLocationQ1570 = glGetUniformLocation(gProgram, \"uLegacySunlightQ1570\");"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "GLint q1560TerrainRenderStageLocation = -1;"
    "GLint q1560TerrainRenderStageLocation = -1;\nGLint q1570TerrainLegacyDomainLocation = -1;\nGLint q1570TerrainLegacyAmbientLocation = -1;\nGLint q1570TerrainLegacySunlightLocation = -1;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    q1560TerrainRenderStageLocation = -1;"
    "    q1560TerrainRenderStageLocation = -1;\n    q1570TerrainLegacyDomainLocation = -1;\n    q1570TerrainLegacyAmbientLocation = -1;\n    q1570TerrainLegacySunlightLocation = -1;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    q1560TerrainRenderStageLocation = glGetUniformLocation(q76bProgram, \"uRenderStageQ1560\");"
    "    q1560TerrainRenderStageLocation = glGetUniformLocation(q76bProgram, \"uRenderStageQ1560\");\n    q1570TerrainLegacyDomainLocation = glGetUniformLocation(q76bProgram, \"uLegacyColourDomainQ1570\");\n    q1570TerrainLegacyAmbientLocation = glGetUniformLocation(q76bProgram, \"uLegacyAmbientQ1570\");\n    q1570TerrainLegacySunlightLocation = glGetUniformLocation(q76bProgram, \"uLegacySunlightQ1570\");"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# -----------------------------------------------------------------------------
# Upload raw WTHR byte/255 constants from Q14.8's still-valid endpoint cache.
# Q14.8's selector is dormant; only its data extraction is reused here.
# -----------------------------------------------------------------------------
set(Q1570_STATIC_UPLOAD_OLD [=[
    if (gRenderStageLocationQ1560 >= 0) {
        glUniform1i(gRenderStageLocationQ1560, GetFo3RenderStageQ1560());
    }
]=])
set(Q1570_STATIC_UPLOAD_NEW [=[
    if (gRenderStageLocationQ1560 >= 0) {
        glUniform1i(gRenderStageLocationQ1560, GetFo3RenderStageQ1560());
    }
    if (gLegacyColourDomainLocationQ1570 >= 0) {
        glUniform1f(gLegacyColourDomainLocationQ1570,
                    GetFo3LegacyColourDomainQ1570() ? 1.0f : 0.0f);
    }
    if (gLegacyAmbientLocationQ1570 >= 0 && gLegacySunlightLocationQ1570 >= 0) {
        float q1570RawAmbient[3]{0.34f, 0.34f, 0.34f};
        float q1570RawSunlight[3]{0.66f, 0.66f, 0.66f};
        RefreshFo3RawWeatherLightingQ1480();
        const bool q1570RawReady =
            GetFo3RawWeatherLightingQ1480(q1570RawAmbient, q1570RawSunlight);
        glUniform3fv(gLegacyAmbientLocationQ1570, 1, q1570RawAmbient);
        glUniform3fv(gLegacySunlightLocationQ1570, 1, q1570RawSunlight);

        static int q1570LastLoggedMode = -1;
        const int q1570Mode = GetFo3LegacyColourDomainQ1570() ? 1 : 0;
        if (q1570Mode != q1570LastLoggedMode) {
            q1570LastLoggedMode = q1570Mode;
            Q6H_LOGI("Q15.7 LEGACY DOMAIN: mode=%s rawReady=%d ambient=(%.3f %.3f %.3f) sunlight=(%.3f %.3f %.3f) scope=statics+LAND baseMapEncoded=1 outputLinear=1 fakeLighting=0",
                     GetFo3LegacyColourDomainNameQ1570(), q1570RawReady ? 1 : 0,
                     q1570RawAmbient[0], q1570RawAmbient[1], q1570RawAmbient[2],
                     q1570RawSunlight[0], q1570RawSunlight[1], q1570RawSunlight[2]);
        }
    }
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1570_STATIC_UPLOAD_OLD}" Q1570_STATIC_UPLOAD_POS)
if(Q1570_STATIC_UPLOAD_POS EQUAL -1)
    message(FATAL_ERROR "Q15.7 could not find Q15.6 static stage upload")
endif()
string(REPLACE "${Q1570_STATIC_UPLOAD_OLD}" "${Q1570_STATIC_UPLOAD_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1570_LAND_UPLOAD_OLD [=[
    if (q1560TerrainRenderStageLocation >= 0) {
        glUniform1i(q1560TerrainRenderStageLocation, GetFo3RenderStageQ1560());
    }
]=])
set(Q1570_LAND_UPLOAD_NEW [=[
    if (q1560TerrainRenderStageLocation >= 0) {
        glUniform1i(q1560TerrainRenderStageLocation, GetFo3RenderStageQ1560());
    }
    if (q1570TerrainLegacyDomainLocation >= 0) {
        glUniform1f(q1570TerrainLegacyDomainLocation,
                    GetFo3LegacyColourDomainQ1570() ? 1.0f : 0.0f);
    }
    if (q1570TerrainLegacyAmbientLocation >= 0 &&
        q1570TerrainLegacySunlightLocation >= 0) {
        float q1570RawAmbient[3]{0.48f, 0.48f, 0.48f};
        float q1570RawSunlight[3]{0.52f, 0.52f, 0.52f};
        RefreshFo3RawWeatherLightingQ1480();
        GetFo3RawWeatherLightingQ1480(q1570RawAmbient, q1570RawSunlight);
        glUniform3fv(q1570TerrainLegacyAmbientLocation, 1, q1570RawAmbient);
        glUniform3fv(q1570TerrainLegacySunlightLocation, 1, q1570RawSunlight);
    }
]=])
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1570_LAND_UPLOAD_OLD}" Q1570_LAND_UPLOAD_POS)
if(Q1570_LAND_UPLOAD_POS EQUAL -1)
    message(FATAL_ERROR "Q15.7 could not find Q15.6 LAND stage upload")
endif()
string(REPLACE "${Q1570_LAND_UPLOAD_OLD}" "${Q1570_LAND_UPLOAD_NEW}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# -----------------------------------------------------------------------------
# OpenXR: Q15.5 intentionally made LEFT Y a no-op. Reuse that existing action
# for this legitimate authored-domain A/B while preserving LEFT X stage cycling.
# -----------------------------------------------------------------------------
set(Q1570_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1570_Q4_INPUT}")
    message(FATAL_ERROR "Q15.7 expected Q15.6 OpenXR source at ${Q1570_Q4_INPUT}")
endif()
file(READ "${Q1570_Q4_INPUT}" Q1570_Q4_SOURCE)

string(REPLACE
    "#include \"fo3-render-stage-q1560.h\""
    "#include \"fo3-render-stage-q1560.h\"\n#include \"fo3-legacy-colour-domain-q1570.h\""
    Q1570_Q4_SOURCE "${Q1570_Q4_SOURCE}")
string(REPLACE
    "ToggleFo3LegacyPpDiffuseDomainQ1470();"
    "ToggleFo3LegacyColourDomainQ1570();"
    Q1570_Q4_SOURCE "${Q1570_Q4_SOURCE}")
string(REPLACE
    "GetFo3PpDiffuseDomainNameQ1470()"
    "GetFo3LegacyColourDomainNameQ1570()"
    Q1570_Q4_SOURCE "${Q1570_Q4_SOURCE}")
string(REPLACE
    "Q15.1 AMBIENT STRENGTH MODE:"
    "Q15.7 LEGACY DOMAIN MODE:"
    Q1570_Q4_SOURCE "${Q1570_Q4_SOURCE}")
file(WRITE "${Q1570_Q4_INPUT}" "${Q1570_Q4_SOURCE}")

# Hard guards. These prove the new B side is raw WTHR + encoded BaseMap in both
# world renderers, the synthetic Q15.0/Q15.1 selector remains permanently false,
# and Q15.6's X stage cycle is still present.
string(FIND "${Q6H_NATIVE_SOURCE}" "uLegacyColourDomainQ1570" Q1570_STATIC_SELECTOR_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "uLegacyAmbientQ1570 +" Q1570_STATIC_RAW_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q1470LinearToSrgb(baseColor)" Q1570_STATIC_BASE_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "uLegacyColourDomainQ1570" Q1570_LAND_SELECTOR_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "uLegacyAmbientQ1570 +" Q1570_LAND_RAW_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "Q1570LinearToSrgb(albedo)" Q1570_LAND_BASE_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "q1500AmbientLuma * 0.65f" Q1570_SYNTHETIC_CODE_RETAINED)
string(FIND "${Q1570_Q4_SOURCE}" "ToggleFo3LegacyColourDomainQ1570();" Q1570_Y_TOGGLE_OK)
string(FIND "${Q1570_Q4_SOURCE}" "CycleFo3RenderStageQ1560();" Q1570_X_STAGE_OK)
string(FIND "${Q1570_Q4_SOURCE}" "ToggleFo3LegacyPpDiffuseDomainQ1470();" Q1570_OLD_Y_TOGGLE)
if(Q1570_STATIC_SELECTOR_OK EQUAL -1 OR Q1570_STATIC_RAW_OK EQUAL -1 OR
   Q1570_STATIC_BASE_OK EQUAL -1 OR Q1570_LAND_SELECTOR_OK EQUAL -1 OR
   Q1570_LAND_RAW_OK EQUAL -1 OR Q1570_LAND_BASE_OK EQUAL -1 OR
   Q1570_SYNTHETIC_CODE_RETAINED EQUAL -1 OR Q1570_Y_TOGGLE_OK EQUAL -1 OR
   Q1570_X_STAGE_OK EQUAL -1 OR NOT Q1570_OLD_Y_TOGGLE EQUAL -1)
    message(FATAL_ERROR
        "Q15.7 verification failed: staticSelector=${Q1570_STATIC_SELECTOR_OK} staticRaw=${Q1570_STATIC_RAW_OK} staticBase=${Q1570_STATIC_BASE_OK} landSelector=${Q1570_LAND_SELECTOR_OK} landRaw=${Q1570_LAND_RAW_OK} landBase=${Q1570_LAND_BASE_OK} syntheticDormant=${Q1570_SYNTHETIC_CODE_RETAINED} yToggle=${Q1570_Y_TOGGLE_OK} xStage=${Q1570_X_STAGE_OK} oldY=${Q1570_OLD_Y_TOGGLE}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")
file(WRITE "${Q1570_Q4_INPUT}" "${Q1570_Q4_SOURCE}")

message(STATUS "Q15.7 legacy encoded-domain world diffuse A/B enabled on LEFT Y for statics + LAND; LEFT X stage isolator retained")
