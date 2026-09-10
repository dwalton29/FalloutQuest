# Q15.6: isolate the remaining Megaton mismatch by render stage.
#
# The clean Q15.5 authored-lighting screenshot proves the synthetic neutral
# Ambient was only masking the cyan problem. Stop changing colour values and
# instead expose where the divergence first appears.
#
# LEFT X cycles, starting from FINAL at launch:
#   RAW   = authored world/material lighting, WTHR fog disabled, post bypassed
#   FOG   = RAW + authored WTHR fog, post bypassed
#   HDR   = FOG + current SP17 HDR/adaptation path, cinematic IMGS disabled
#   FINAL = normal current renderer
#
# Q15.4's tangent-space A/B is retired here. Its shader code is left compiled for
# history, but the selector is pinned to the existing Quest-world-normal Lambert
# path so LEFT X changes render stage only. LEFT Y remains Q15.5's no-op.

# Shared renderer/OpenXR stage state.
string(REPLACE
    "#include \"fo3-fog-ab-q1450.h\""
    "#include \"fo3-fog-ab-q1450.h\"\n#include \"fo3-render-stage-q1560.h\""
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Static/NIF: stage 0 removes only the final WTHR fog blend. The authored
# Ambient/Sunlight/material equation itself is untouched.
# -----------------------------------------------------------------------------
string(REPLACE
    "        uniform float uQ1450FogEnabled;"
    "        uniform float uQ1450FogEnabled;\n        uniform int uRenderStageQ1560;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1560_STATIC_LAMBERT_OLD [=[
            float lambert = uQ1450FogEnabled > 0.5
                ? q1540QuestLambert
                : q1540Sp17Lambert;
]=])
set(Q1560_STATIC_LAMBERT_NEW [=[
            // Q15.6: Q15.4 tangent A/B retired; LEFT X is render-stage only.
            float lambert = q1540QuestLambert;
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1560_STATIC_LAMBERT_OLD}" Q1560_LAMBERT_POS)
if(Q1560_LAMBERT_POS EQUAL -1)
    message(FATAL_ERROR "Q15.6 could not find Q15.4 static Lambert selector")
endif()
string(REPLACE "${Q1560_STATIC_LAMBERT_OLD}" "${Q1560_STATIC_LAMBERT_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1560_FOG_OLD [=[
            float q1532QuestFragmentFog = pow(fogT, max(uFogPower, 0.01));
            float fogFactor = q1532QuestFragmentFog;
            lit = mix(lit, uFogColor, fogFactor);
]=])
set(Q1560_FOG_NEW [=[
            float q1532QuestFragmentFog = pow(fogT, max(uFogPower, 0.01));
            float fogFactor = uRenderStageQ1560 >= 1
                ? q1532QuestFragmentFog
                : 0.0;
            lit = mix(lit, uFogColor, fogFactor);
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1560_FOG_OLD}" Q1560_STATIC_FOG_POS)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1560_FOG_OLD}" Q1560_LAND_FOG_POS)
if(Q1560_STATIC_FOG_POS EQUAL -1 OR Q1560_LAND_FOG_POS EQUAL -1)
    message(FATAL_ERROR
        "Q15.6 could not find Q15.4 fixed fog blocks: static=${Q1560_STATIC_FOG_POS} land=${Q1560_LAND_FOG_POS}")
endif()
string(REPLACE "${Q1560_FOG_OLD}" "${Q1560_FOG_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Static stage uniform plumbing.
string(REPLACE
    "GLint gFogEnabledLocationQ1450 = -1;"
    "GLint gFogEnabledLocationQ1450 = -1;\nGLint gRenderStageLocationQ1560 = -1;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "    gFogEnabledLocationQ1450 = glGetUniformLocation(gProgram, \"uQ1450FogEnabled\");"
    "    gFogEnabledLocationQ1450 = glGetUniformLocation(gProgram, \"uQ1450FogEnabled\");\n    gRenderStageLocationQ1560 = glGetUniformLocation(gProgram, \"uRenderStageQ1560\");"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1560_STATIC_UPLOAD_OLD [=[
    if (gFogEnabledLocationQ1450 >= 0) {
        glUniform1f(gFogEnabledLocationQ1450, GetFo3FogEnabledQ1450() ? 1.0f : 0.0f);
    }
]=])
set(Q1560_STATIC_UPLOAD_NEW [=[
    if (gFogEnabledLocationQ1450 >= 0) {
        glUniform1f(gFogEnabledLocationQ1450, GetFo3FogEnabledQ1450() ? 1.0f : 0.0f);
    }
    if (gRenderStageLocationQ1560 >= 0) {
        glUniform1i(gRenderStageLocationQ1560, GetFo3RenderStageQ1560());
    }
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1560_STATIC_UPLOAD_OLD}" Q1560_STATIC_UPLOAD_POS)
if(Q1560_STATIC_UPLOAD_POS EQUAL -1)
    message(FATAL_ERROR "Q15.6 could not find static Q15.3b selector upload")
endif()
string(REPLACE "${Q1560_STATIC_UPLOAD_OLD}" "${Q1560_STATIC_UPLOAD_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# LAND: receive the same stage through a main-TU bridge.
# -----------------------------------------------------------------------------
string(PREPEND Q720_TERRAIN_RENDER_SOURCE
       "int GetFo3RenderStageQ1560Bridge();\n\n")

string(REPLACE
    "        uniform float uQ1450FogEnabled;"
    "        uniform float uQ1450FogEnabled;\n        uniform int uRenderStageQ1560;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE "${Q1560_FOG_OLD}" "${Q1560_FOG_NEW}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

string(REPLACE
    "GLint q1450TerrainFogEnabledLocation = -1;"
    "GLint q1450TerrainFogEnabledLocation = -1;\nGLint q1560TerrainRenderStageLocation = -1;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    q1450TerrainFogEnabledLocation = -1;"
    "    q1450TerrainFogEnabledLocation = -1;\n    q1560TerrainRenderStageLocation = -1;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    q1450TerrainFogEnabledLocation = glGetUniformLocation(q76bProgram, \"uQ1450FogEnabled\");"
    "    q1450TerrainFogEnabledLocation = glGetUniformLocation(q76bProgram, \"uQ1450FogEnabled\");\n    q1560TerrainRenderStageLocation = glGetUniformLocation(q76bProgram, \"uRenderStageQ1560\");"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

set(Q1560_LAND_UPLOAD_OLD [=[
    if (q1450TerrainFogEnabledLocation >= 0) {
        glUniform1f(q1450TerrainFogEnabledLocation,
                    GetFo3FogEnabledQ1450Bridge() ? 1.0f : 0.0f);
    }
]=])
set(Q1560_LAND_UPLOAD_NEW [=[
    if (q1450TerrainFogEnabledLocation >= 0) {
        glUniform1f(q1450TerrainFogEnabledLocation,
                    GetFo3FogEnabledQ1450Bridge() ? 1.0f : 0.0f);
    }
    if (q1560TerrainRenderStageLocation >= 0) {
        glUniform1i(q1560TerrainRenderStageLocation, GetFo3RenderStageQ1560Bridge());
    }
]=])
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1560_LAND_UPLOAD_OLD}" Q1560_LAND_UPLOAD_POS)
if(Q1560_LAND_UPLOAD_POS EQUAL -1)
    message(FATAL_ERROR "Q15.6 could not find LAND Q15.3b selector upload")
endif()
string(REPLACE "${Q1560_LAND_UPLOAD_OLD}" "${Q1560_LAND_UPLOAD_NEW}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

set(Q1560_BRIDGE_OLD [=[
bool GetFo3FogEnabledQ1450Bridge() {
    return GetFo3FogEnabledQ1450();
}

]=])
set(Q1560_BRIDGE_NEW [=[
bool GetFo3FogEnabledQ1450Bridge() {
    return GetFo3FogEnabledQ1450();
}

int GetFo3RenderStageQ1560Bridge() {
    return GetFo3RenderStageQ1560();
}

]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1560_BRIDGE_OLD}" Q1560_BRIDGE_POS)
if(Q1560_BRIDGE_POS EQUAL -1)
    message(FATAL_ERROR "Q15.6 could not find Q14.5 LAND bridge")
endif()
string(REPLACE "${Q1560_BRIDGE_OLD}" "${Q1560_BRIDGE_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Fullscreen post: RAW/FOG are literal scene-buffer copies. HDR runs the current
# SP17 HDR path but forces cinematic flags off. FINAL retains normal ImageSpace.
# -----------------------------------------------------------------------------
set(Q1560_POST_UNIFORM_OLD [=[
        uniform sampler2D uDepthQ1370;
        uniform float uTargetLumQ1520;
        out vec4 fragColor;
]=])
set(Q1560_POST_UNIFORM_NEW [=[
        uniform sampler2D uDepthQ1370;
        uniform float uTargetLumQ1520;
        uniform int uRenderStageQ1560;
        out vec4 fragColor;
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1560_POST_UNIFORM_OLD}" Q1560_POST_UNIFORM_POS)
if(Q1560_POST_UNIFORM_POS EQUAL -1)
    message(FATAL_ERROR "Q15.6 could not find Q15.2 post uniform block")
endif()
string(REPLACE "${Q1560_POST_UNIFORM_OLD}" "${Q1560_POST_UNIFORM_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1560_POST_ENTRY_OLD [=[
            vec3 colour = texture(uScene, vUv).rgb;
]=])
set(Q1560_POST_ENTRY_NEW [=[
            vec3 colour = texture(uScene, vUv).rgb;

            // RAW and FOG isolate the pre-post scene exactly. Fog itself is
            // already controlled in the world shaders by the same stage value.
            if (uRenderStageQ1560 <= 1) {
                fragColor = vec4(max(colour, vec3(0.0)), 1.0);
                return;
            }
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1560_POST_ENTRY_OLD}" Q1560_POST_ENTRY_POS)
if(Q1560_POST_ENTRY_POS EQUAL -1)
    message(FATAL_ERROR "Q15.6 could not find fullscreen post main entry")
endif()
string(REPLACE "${Q1560_POST_ENTRY_OLD}" "${Q1560_POST_ENTRY_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "GLint q1520TargetLumLocation = -1;"
    "GLint q1520TargetLumLocation = -1;\nGLint q1560PostRenderStageLocation = -1;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1560_POST_LOCATION_OLD [=[
    q1370DepthLocation = glGetUniformLocation(q1280PostProgram, "uDepthQ1370");
    q1520TargetLumLocation = glGetUniformLocation(q1280PostProgram, "uTargetLumQ1520");
    return q1280SceneLocation >= 0 && q1280TexelLocation >= 0 &&
           q1350ExposureLocation >= 0 && q1370DepthLocation >= 0 &&
           q1520TargetLumLocation >= 0;
]=])
set(Q1560_POST_LOCATION_NEW [=[
    q1370DepthLocation = glGetUniformLocation(q1280PostProgram, "uDepthQ1370");
    q1520TargetLumLocation = glGetUniformLocation(q1280PostProgram, "uTargetLumQ1520");
    q1560PostRenderStageLocation = glGetUniformLocation(q1280PostProgram, "uRenderStageQ1560");
    return q1280SceneLocation >= 0 && q1280TexelLocation >= 0 &&
           q1350ExposureLocation >= 0 && q1370DepthLocation >= 0 &&
           q1520TargetLumLocation >= 0 && q1560PostRenderStageLocation >= 0;
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1560_POST_LOCATION_OLD}" Q1560_POST_LOCATION_POS)
if(Q1560_POST_LOCATION_POS EQUAL -1)
    message(FATAL_ERROR "Q15.6 could not find Q15.2 post uniform lookup")
endif()
string(REPLACE "${Q1560_POST_LOCATION_OLD}" "${Q1560_POST_LOCATION_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1560_IMAGE_UPLOAD_OLD [=[
    const Fo3ImageSpaceQ1280& image = GetFo3ImageSpaceQ1280();
    glUniform1f(q1520TargetLumLocation,
                image.valid ? std::clamp(image.hdrTargetLum, 0.001f, 4.0f) : 1.0f);
    const int flags = image.valid ? static_cast<int>(image.cinematicFlags) : 0;
]=])
set(Q1560_IMAGE_UPLOAD_NEW [=[
    const Fo3ImageSpaceQ1280& image = GetFo3ImageSpaceQ1280();
    const int q1560Stage = GetFo3RenderStageQ1560();
    glUniform1i(q1560PostRenderStageLocation, q1560Stage);
    glUniform1f(q1520TargetLumLocation,
                image.valid ? std::clamp(image.hdrTargetLum, 0.001f, 4.0f) : 1.0f);
    // Stage 2 keeps HDR/adaptation but removes only cinematic IMGS controls.
    const int flags = (q1560Stage >= FO3_RENDER_STAGE_FINAL_Q1560 && image.valid)
        ? static_cast<int>(image.cinematicFlags)
        : 0;
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1560_IMAGE_UPLOAD_OLD}" Q1560_IMAGE_UPLOAD_POS)
if(Q1560_IMAGE_UPLOAD_POS EQUAL -1)
    message(FATAL_ERROR "Q15.6 could not find Q15.2 ImageSpace upload")
endif()
string(REPLACE "${Q1560_IMAGE_UPLOAD_OLD}" "${Q1560_IMAGE_UPLOAD_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Runtime diagnostic label for the static renderer.
string(REPLACE "Q15.4 SP17 TANGENT LIGHT A/B READY:"
               "Q15.6 RENDER STAGE READY:"
               Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "fogPath=Q15.2_IDENTICAL_BOTH_SIDES scope=static-PPLighting sp17Light=TANGENT_VERTEX_ENCODED sp17Normal=RAW_PIXEL_DECODE"
    "stages=RAW_FOG_HDR_FINAL authoredLightingOnly=1 tangentABRetired=1"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# OpenXR input: reuse the established LEFT X action/rising-edge plumbing, but
# cycle the four-stage state instead of toggling the old Q14.5/Q15.4 boolean.
# -----------------------------------------------------------------------------
set(Q1560_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1560_Q4_INPUT}")
    message(FATAL_ERROR "Q15.6 expected final Q15.4 OpenXR source at ${Q1560_Q4_INPUT}")
endif()
file(READ "${Q1560_Q4_INPUT}" Q1560_Q4_SOURCE)

string(REPLACE
    "#include \"fo3-fog-ab-q1450.h\""
    "#include \"fo3-fog-ab-q1450.h\"\n#include \"fo3-render-stage-q1560.h\""
    Q1560_Q4_SOURCE "${Q1560_Q4_SOURCE}")
string(REPLACE "ToggleFo3FogEnabledQ1450();"
               "CycleFo3RenderStageQ1560();"
               Q1560_Q4_SOURCE "${Q1560_Q4_SOURCE}")
string(REPLACE "GetFo3FogModeNameQ1450()"
               "GetFo3RenderStageNameQ1560()"
               Q1560_Q4_SOURCE "${Q1560_Q4_SOURCE}")
string(REPLACE "Q15.4 SP17 TANGENT LIGHT MODE:"
               "Q15.6 RENDER STAGE MODE:"
               Q1560_Q4_SOURCE "${Q1560_Q4_SOURCE}")
string(REPLACE
    "fogPath=Q15.2_IDENTICAL_BOTH_SIDES scope=static-PPLighting sp17Light=TANGENT_VERTEX_ENCODED sp17Normal=RAW_PIXEL_DECODE"
    "stages=RAW_FOG_HDR_FINAL authoredLightingOnly=1 tangentABRetired=1"
    Q1560_Q4_SOURCE "${Q1560_Q4_SOURCE}")
file(WRITE "${Q1560_Q4_INPUT}" "${Q1560_Q4_SOURCE}")

# Hard guards: X must cycle only the stage; RAW must disable fog and post; HDR
# must preserve post while suppressing cinematic flags; the Q15.4 Lambert
# selector must no longer be active.
string(FIND "${Q6H_NATIVE_SOURCE}" "uniform int uRenderStageQ1560;" Q1560_STATIC_STAGE_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "uniform int uRenderStageQ1560;" Q1560_LAND_STAGE_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "uRenderStageQ1560 >= 1" Q1560_STATIC_FOG_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "uRenderStageQ1560 >= 1" Q1560_LAND_FOG_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "if (uRenderStageQ1560 <= 1)" Q1560_POST_BYPASS_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "q1560Stage >= FO3_RENDER_STAGE_FINAL_Q1560" Q1560_CINEMATIC_GATE_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "float lambert = q1540QuestLambert;" Q1560_LAMBERT_FIXED_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "? q1540QuestLambert" Q1560_OLD_LAMBERT)
string(FIND "${Q1560_Q4_SOURCE}" "CycleFo3RenderStageQ1560();" Q1560_CYCLE_OK)
string(FIND "${Q1560_Q4_SOURCE}" "ToggleFo3FogEnabledQ1450();" Q1560_OLD_TOGGLE)
string(FIND "${Q1560_Q4_SOURCE}" "Q15.6 RENDER STAGE MODE:" Q1560_LOG_OK)
if(Q1560_STATIC_STAGE_OK EQUAL -1 OR Q1560_LAND_STAGE_OK EQUAL -1 OR
   Q1560_STATIC_FOG_OK EQUAL -1 OR Q1560_LAND_FOG_OK EQUAL -1 OR
   Q1560_POST_BYPASS_OK EQUAL -1 OR Q1560_CINEMATIC_GATE_OK EQUAL -1 OR
   Q1560_LAMBERT_FIXED_OK EQUAL -1 OR NOT Q1560_OLD_LAMBERT EQUAL -1 OR
   Q1560_CYCLE_OK EQUAL -1 OR NOT Q1560_OLD_TOGGLE EQUAL -1 OR
   Q1560_LOG_OK EQUAL -1)
    message(FATAL_ERROR
        "Q15.6 verification failed: staticStage=${Q1560_STATIC_STAGE_OK} landStage=${Q1560_LAND_STAGE_OK} staticFog=${Q1560_STATIC_FOG_OK} landFog=${Q1560_LAND_FOG_OK} postBypass=${Q1560_POST_BYPASS_OK} cinematicGate=${Q1560_CINEMATIC_GATE_OK} lambert=${Q1560_LAMBERT_FIXED_OK} oldLambert=${Q1560_OLD_LAMBERT} cycle=${Q1560_CYCLE_OK} oldToggle=${Q1560_OLD_TOGGLE} log=${Q1560_LOG_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")

message(STATUS "Q15.6 render-stage isolator enabled on LEFT X: FINAL -> RAW -> FOG -> HDR -> FINAL")
