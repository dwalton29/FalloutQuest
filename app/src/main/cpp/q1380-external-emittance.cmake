# Q13.8: Fallout 3 reference External Emittance.
#
# Direct inspection of the mounted vanilla Fallout3.esm showed Megaton does not
# place ordinary REFR->LIGH point lights in its WRLD group. Instead, 101 REFRs
# carry XEMI: 30 use fixed LIGH colours and 71 use REGN daylight emittance.
# This pass reproduces that authored path for shapes whose NIF explicitly sets
# BSShader External Emittance (Shader Flags 1 bit 0x20000000).
#
# At this milestone the renderer is still on the Day endpoint, so REGN emittance
# resolves REGN->RDWT->WTHR NAM0 Sunlight/Day. Time interpolation remains Q14.x.
# No hand-authored Fallout-green tint or invented light source is introduced.

# Runtime ESM resolver.
string(REPLACE
    "#include \"fo3-weather-light-q1320.h\""
    "#include \"fo3-weather-light-q1320.h\"\n#include \"fo3-external-emittance-q1380.h\""
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# GPU object state: preserve whether the NIF asked for external emittance and
# the ESM-resolved reference colour. External emittance is per REFR, not model.
# -----------------------------------------------------------------------------
set(Q1380_OLD_GPU_FIELDS [==[
    float emissiveMult = 1.0f;
]==])
set(Q1380_NEW_GPU_FIELDS [==[
    float emissiveMult = 1.0f;
    bool externalEmittanceFlagQ1380 = false;
    bool externalEmittanceEnabledQ1380 = false;
    bool externalEmittanceRegionQ1380 = false;
    float externalEmittanceColorQ1380[3]{0.0f, 0.0f, 0.0f};
    uint32_t externalEmittanceFormIdQ1380 = 0u;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1380_OLD_GPU_FIELDS}" Q1380_GPU_POS)
if(Q1380_GPU_POS EQUAL -1)
    message(FATAL_ERROR "Q13.8 could not find Q10.2 material GPU fields")
endif()
string(REPLACE "${Q1380_OLD_GPU_FIELDS}" "${Q1380_NEW_GPU_FIELDS}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Counters make the ESM+NIF gate visible on-device.
set(Q1380_OLD_SCENE_GLOBAL [==[
bool gSceneReady = false;
bool gLoggedFirstDraw = false;
]==])
set(Q1380_NEW_SCENE_GLOBAL [==[
bool gSceneReady = false;
bool gLoggedFirstDraw = false;
size_t gQ1380ExternalFlagShapes = 0u;
size_t gQ1380ExternalResolvedShapes = 0u;
size_t gQ1380ExternalFixedShapes = 0u;
size_t gQ1380ExternalRegionShapes = 0u;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1380_OLD_SCENE_GLOBAL}" Q1380_SCENE_GLOBAL_POS)
if(Q1380_SCENE_GLOBAL_POS EQUAL -1)
    message(FATAL_ERROR "Q13.8 could not find scene-ready globals")
endif()
string(REPLACE "${Q1380_OLD_SCENE_GLOBAL}" "${Q1380_NEW_SCENE_GLOBAL}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Shader state. GECK/NIF semantics: External Emittance replaces the material's
# own emissive colour and is modulated by the shape's diffuse/glow texture.
# NoLighting shapes use the external colour as their self-lit surface colour;
# PP-lit shapes keep normal lighting and receive the externally-authored emit.
# -----------------------------------------------------------------------------
set(Q1380_OLD_SHADER_UNIFORMS [==[
        uniform float uGlowEnabled;
]==])
set(Q1380_NEW_SHADER_UNIFORMS [==[
        uniform float uGlowEnabled;
        uniform float uExternalEmittanceEnabledQ1380;
        uniform vec3 uExternalEmittanceColorQ1380;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1380_OLD_SHADER_UNIFORMS}" Q1380_SHADER_UNIFORM_POS)
if(Q1380_SHADER_UNIFORM_POS EQUAL -1)
    message(FATAL_ERROR "Q13.8 could not find Q10.2 material uniforms")
endif()
string(REPLACE "${Q1380_OLD_SHADER_UNIFORMS}" "${Q1380_NEW_SHADER_UNIFORMS}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1380_OLD_EMISSIVE [==[
            if (uNoLighting < 0.5) {
                vec3 emissiveMask = uGlowEnabled > 0.5
                    ? texture(uGlow, vUv).rgb
                    : baseColor;
                lit += emissiveMask * uEmissiveColor * uEmissiveMult;
            }
]==])
set(Q1380_NEW_EMISSIVE [==[
            if (uExternalEmittanceEnabledQ1380 > 0.5) {
                vec3 externalMaskQ1380 = uGlowEnabled > 0.5
                    ? texture(uGlow, vUv).rgb
                    : baseColor;
                if (uNoLighting > 0.5) {
                    // NoLighting FX/statics are already self-lit. External
                    // emittance is their authored surface colour, not another
                    // additive light layered on top of the texture.
                    lit = externalMaskQ1380 * uExternalEmittanceColorQ1380;
                } else {
                    // PP-lit geometry keeps ambient/sun/local lighting while its
                    // emitted component comes from the reference's XEMI source.
                    lit += externalMaskQ1380 * uExternalEmittanceColorQ1380;
                }
            } else if (uNoLighting < 0.5) {
                vec3 emissiveMask = uGlowEnabled > 0.5
                    ? texture(uGlow, vUv).rgb
                    : baseColor;
                lit += emissiveMask * uEmissiveColor * uEmissiveMult;
            }
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1380_OLD_EMISSIVE}" Q1380_EMISSIVE_POS)
if(Q1380_EMISSIVE_POS EQUAL -1)
    message(FATAL_ERROR "Q13.8 could not find Q12.4 final emissive block")
endif()
string(REPLACE "${Q1380_OLD_EMISSIVE}" "${Q1380_NEW_EMISSIVE}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Uniform handles/lookups.
set(Q1380_OLD_HANDLE [==[
GLint gGlowEnabledLocationQ1020 = -1;
]==])
set(Q1380_NEW_HANDLE [==[
GLint gGlowEnabledLocationQ1020 = -1;
GLint gExternalEmittanceEnabledLocationQ1380 = -1;
GLint gExternalEmittanceColorLocationQ1380 = -1;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1380_OLD_HANDLE}" Q1380_HANDLE_POS)
if(Q1380_HANDLE_POS EQUAL -1)
    message(FATAL_ERROR "Q13.8 could not find glow uniform handle")
endif()
string(REPLACE "${Q1380_OLD_HANDLE}" "${Q1380_NEW_HANDLE}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1380_OLD_LOOKUP [==[
    gGlowEnabledLocationQ1020 = glGetUniformLocation(gProgram, "uGlowEnabled");
]==])
set(Q1380_NEW_LOOKUP [==[
    gGlowEnabledLocationQ1020 = glGetUniformLocation(gProgram, "uGlowEnabled");
    gExternalEmittanceEnabledLocationQ1380 =
        glGetUniformLocation(gProgram, "uExternalEmittanceEnabledQ1380");
    gExternalEmittanceColorLocationQ1380 =
        glGetUniformLocation(gProgram, "uExternalEmittanceColorQ1380");
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1380_OLD_LOOKUP}" Q1380_LOOKUP_POS)
if(Q1380_LOOKUP_POS EQUAL -1)
    message(FATAL_ERROR "Q13.8 could not find material uniform lookup")
endif()
string(REPLACE "${Q1380_OLD_LOOKUP}" "${Q1380_NEW_LOOKUP}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Per-shape ESM+NIF resolution during upload. A reference only receives XEMI if
# its NIF explicitly sets External Emittance; this prevents a reference-level
# XEMI assignment from tinting unrelated shapes in a multi-shape NIF.
# -----------------------------------------------------------------------------
set(Q1380_OLD_ASSIGN [==[
    gpu.emissiveMult = std::max(0.0f, cpu.mesh.emissiveMult);
]==])
set(Q1380_NEW_ASSIGN [==[
    gpu.emissiveMult = std::max(0.0f, cpu.mesh.emissiveMult);
    gpu.externalEmittanceFlagQ1380 =
        (cpu.mesh.shaderFlags1 & fo3emittanceq1380::EXTERNAL_EMITTANCE_SHADER_FLAG) != 0u;
    if (gpu.externalEmittanceFlagQ1380) {
        ++gQ1380ExternalFlagShapes;
        Fo3ExternalEmittanceQ1380 q1380;
        if (ResolveFo3ExternalEmittanceQ1380(0x00000A74u,
                                             cpu.placement.refFormId, q1380)) {
            gpu.externalEmittanceEnabledQ1380 = true;
            gpu.externalEmittanceRegionQ1380 = q1380.regionDriven;
            gpu.externalEmittanceFormIdQ1380 = q1380.emittanceFormId;
            for (int i = 0; i < 3; ++i) {
                gpu.externalEmittanceColorQ1380[i] = q1380.color[i];
            }
            ++gQ1380ExternalResolvedShapes;
            if (q1380.regionDriven) ++gQ1380ExternalRegionShapes;
            else ++gQ1380ExternalFixedShapes;
        }
    }
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1380_OLD_ASSIGN}" Q1380_ASSIGN_POS)
if(Q1380_ASSIGN_POS EQUAL -1)
    message(FATAL_ERROR "Q13.8 could not find material upload assignment")
endif()
string(REPLACE "${Q1380_OLD_ASSIGN}" "${Q1380_NEW_ASSIGN}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Per-draw state.
set(Q1380_OLD_DRAW [==[
    glUniform1f(gGlowEnabledLocationQ1020, object.realGlow ? 1.0f : 0.0f);
]==])
set(Q1380_NEW_DRAW [==[
    glUniform1f(gGlowEnabledLocationQ1020, object.realGlow ? 1.0f : 0.0f);
    glUniform1f(gExternalEmittanceEnabledLocationQ1380,
                object.externalEmittanceEnabledQ1380 ? 1.0f : 0.0f);
    glUniform3fv(gExternalEmittanceColorLocationQ1380, 1,
                 object.externalEmittanceColorQ1380);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1380_OLD_DRAW}" Q1380_DRAW_POS)
if(Q1380_DRAW_POS EQUAL -1)
    message(FATAL_ERROR "Q13.8 could not find per-draw glow state")
endif()
string(REPLACE "${Q1380_OLD_DRAW}" "${Q1380_NEW_DRAW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Emit a compact renderer-side summary after the scene has uploaded. The ESM
# resolver separately logs the 101 XEMI assignments and examples.
set(Q1380_READY_LOG [==[
        Q6H_LOGI("Q13.8 EMITTANCE GPU: nifFlagShapes=%zu resolvedShapes=%zu fixedLIGHShapes=%zu regionDayShapes=%zu shaderFlag=0x%08X dayEndpoint=1 authoredOnly=1 effectsFolderStillDeferred=1",
                 gQ1380ExternalFlagShapes, gQ1380ExternalResolvedShapes,
                 gQ1380ExternalFixedShapes, gQ1380ExternalRegionShapes,
                 fo3emittanceq1380::EXTERNAL_EMITTANCE_SHADER_FLAG);
]==])
set(Q1380_READY_MARKER [==[
        Q6H_LOGI("Q6H READY: ESM->REFR->BASE->MODL->BSA->NIF objects=%zu triangles=%zu realDiffuse=%zu realNormal=%zu uniqueTextures=%zu alphaBlend=%zu alphaTest=%zu cell=MegatonPlayerHouse",
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1380_READY_MARKER}" Q1380_READY_POS)
if(Q1380_READY_POS EQUAL -1)
    message(FATAL_ERROR "Q13.8 could not find scene ready log")
endif()
string(REPLACE "${Q1380_READY_MARKER}"
       "${Q1380_READY_LOG}\n${Q1380_READY_MARKER}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Reset counters on scene teardown/rebuild.
set(Q1380_OLD_RESET [==[
    gSceneReady = false;
    gLoggedFirstDraw = false;
]==])
set(Q1380_NEW_RESET [==[
    gSceneReady = false;
    gLoggedFirstDraw = false;
    gQ1380ExternalFlagShapes = 0u;
    gQ1380ExternalResolvedShapes = 0u;
    gQ1380ExternalFixedShapes = 0u;
    gQ1380ExternalRegionShapes = 0u;
    ResetFo3ExternalEmittanceQ1380();
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1380_OLD_RESET}" Q1380_RESET_POS)
if(Q1380_RESET_POS EQUAL -1)
    message(FATAL_ERROR "Q13.8 could not find scene reset tail")
endif()
string(REPLACE "${Q1380_OLD_RESET}" "${Q1380_NEW_RESET}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Hard drift guards.
string(FIND "${Q6H_NATIVE_SOURCE}" "fo3-external-emittance-q1380.h" Q1380_INCLUDE_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "uExternalEmittanceColorQ1380" Q1380_SHADER_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "EXTERNAL_EMITTANCE_SHADER_FLAG" Q1380_FLAG_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q13.8 EMITTANCE GPU" Q1380_LOG_OK)
if(Q1380_INCLUDE_OK EQUAL -1 OR Q1380_SHADER_OK EQUAL -1 OR
   Q1380_FLAG_OK EQUAL -1 OR Q1380_LOG_OK EQUAL -1)
    message(FATAL_ERROR "Q13.8 external-emittance verification failed: include=${Q1380_INCLUDE_OK} shader=${Q1380_SHADER_OK} flag=${Q1380_FLAG_OK} log=${Q1380_LOG_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q13.8 Fallout3.esm + NIF external emittance enabled")
