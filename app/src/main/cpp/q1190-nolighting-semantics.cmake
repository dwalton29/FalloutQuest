# Q11.9: match Fallout 3 BSShaderNoLightingProperty colour/falloff semantics.
#
# Q10.2's shared fragment path adds NiMaterial emissive colour to every object.
# That is appropriate for PP-lit material/glow handling, but not Fallout 3's
# BSShaderNoLightingProperty path: NoLighting's sampled texture/vertex colour is
# already the surface colour. Adding material emission on top can clip white FX
# textures (for example Megaton's FXWHITE/soft-glow planes) into solid white.
#
# Also, textureless NoLighting geometry must not consume the four falloff values
# simply because those fields exist in the block. Fallout uses textureless
# NoLighting + vertex colours for overlays/details where the falloff path is not
# active. Keep falloff only when the NoLighting shape actually has a texture.

# -----------------------------------------------------------------------------
# Fragment shader: do not add NiMaterial emission on NoLighting objects.
# -----------------------------------------------------------------------------
set(Q1190_OLD_EMISSIVE [==[
            vec3 emissiveMask = uGlowEnabled > 0.5 ? texture(uGlow, vUv).rgb : vec3(1.0);
            lit += emissiveMask * uEmissiveColor * uEmissiveMult;
]==])
set(Q1190_NEW_EMISSIVE [==[
            if (uNoLighting < 0.5) {
                vec3 emissiveMask = uGlowEnabled > 0.5 ? texture(uGlow, vUv).rgb : vec3(1.0);
                lit += emissiveMask * uEmissiveColor * uEmissiveMult;
            }
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1190_OLD_EMISSIVE}" Q1190_EMISSIVE_POS)
if(Q1190_EMISSIVE_POS EQUAL -1)
    message(FATAL_ERROR "Q11.9 could not find shared emissive shader hook")
endif()
string(REPLACE "${Q1190_OLD_EMISSIVE}" "${Q1190_NEW_EMISSIVE}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# GPU state: textureless NoLighting shapes do not enable view-angle falloff.
# -----------------------------------------------------------------------------
set(Q1190_OLD_FALLOFF_ASSIGN [==[
    gpu.noLightingFalloff = cpu.mesh.noLightingFalloff;
]==])
set(Q1190_NEW_FALLOFF_ASSIGN [==[
    gpu.noLightingFalloff =
        cpu.mesh.noLightingFalloff && !cpu.mesh.diffuseTexturePath.empty();
    if (cpu.mesh.noLightingFalloff && cpu.mesh.diffuseTexturePath.empty()) {
        Q6H_LOGI("Q11.9 NOLIGHT FALLOFF SKIP: ref=%08X model=%s reason=textureless",
                 gpu.refFormId, cpu.placement.modelPath.c_str());
    }
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1190_OLD_FALLOFF_ASSIGN}" Q1190_FALLOFF_ASSIGN_POS)
if(Q1190_FALLOFF_ASSIGN_POS EQUAL -1)
    message(FATAL_ERROR "Q11.9 could not find Q11.6 falloff assignment")
endif()
string(REPLACE "${Q1190_OLD_FALLOFF_ASSIGN}" "${Q1190_NEW_FALLOFF_ASSIGN}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Q11.6 used smoothstep directly on the two authored cosine edges. Fallout NIFs
# may store them in either numerical order; GLSL smoothstep is undefined when
# edge0 >= edge1. Use an explicit normalized interpolation so both directions are
# deterministic while preserving the authored start/stop opacities.
set(Q1190_OLD_FALLOFF_SHADER [==[
                float falloffTQ1160 = smoothstep(uNoLightingFalloffParams.x,
                                                 uNoLightingFalloffParams.y,
                                                 viewAngleQ1160);
                float startOpacityQ1160 = min(uNoLightingFalloffParams.z, 1.0);
                float stopOpacityQ1160 = max(uNoLightingFalloffParams.w, 0.0);
                alpha *= mix(startOpacityQ1160, stopOpacityQ1160, falloffTQ1160);
]==])
set(Q1190_NEW_FALLOFF_SHADER [==[
                float falloffSpanQ1190 = uNoLightingFalloffParams.y - uNoLightingFalloffParams.x;
                float falloffTQ1160 = abs(falloffSpanQ1190) > 0.00001
                    ? clamp((viewAngleQ1160 - uNoLightingFalloffParams.x) / falloffSpanQ1190, 0.0, 1.0)
                    : 0.0;
                float startOpacityQ1160 = clamp(uNoLightingFalloffParams.z, 0.0, 1.0);
                float stopOpacityQ1160 = clamp(uNoLightingFalloffParams.w, 0.0, 1.0);
                alpha *= mix(startOpacityQ1160, stopOpacityQ1160, falloffTQ1160);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1190_OLD_FALLOFF_SHADER}" Q1190_FALLOFF_SHADER_POS)
if(Q1190_FALLOFF_SHADER_POS EQUAL -1)
    message(FATAL_ERROR "Q11.9 could not find Q11.6 falloff shader")
endif()
string(REPLACE "${Q1190_OLD_FALLOFF_SHADER}" "${Q1190_NEW_FALLOFF_SHADER}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(FIND "${Q6H_NATIVE_SOURCE}" "if (uNoLighting < 0.5)" Q1190_EMISSIVE_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q11.9 NOLIGHT FALLOFF SKIP" Q1190_FALLOFF_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "falloffSpanQ1190" Q1190_SHADER_OK)
if(Q1190_EMISSIVE_OK EQUAL -1 OR Q1190_FALLOFF_OK EQUAL -1 OR Q1190_SHADER_OK EQUAL -1)
    message(FATAL_ERROR "Q11.9 NoLighting semantics verification failed")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q11.9 Fallout 3 NoLighting colour/falloff semantics enabled")
