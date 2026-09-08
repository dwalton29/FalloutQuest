# Q12.4: preserve PP-lit texture detail when NiMaterialProperty supplies emission.
#
# Q12.3 proved the remaining FOOD/Church failures share one deterministic shader
# bug rather than a DDS/UV/light-selection problem. MegatonBrassLanternSign's
# 14-triangle MegatonSignScrap03 surface has emissive=(1,1,1), multiplier=2 and
# no glow map; MegatonChurchofAtom has PP-lit surfaces with the same no-glow
# arrangement at multiplier=1. The current shader substitutes vec3(1) when no
# glow map exists, adding a flat white constant to every fragment and clipping
# those materials white/washed-out.
#
# A material without a separate glow texture self-illuminates its sampled base
# colour instead. Authored glow maps remain authoritative. No lighting, colour
# space, alpha, culling, depth or texture state is changed here.

set(Q1240_OLD_EMISSIVE [==[
            if (uNoLighting < 0.5) {
                vec3 emissiveMask = uGlowEnabled > 0.5 ? texture(uGlow, vUv).rgb : vec3(1.0);
                lit += emissiveMask * uEmissiveColor * uEmissiveMult;
            }
]==])
set(Q1240_NEW_EMISSIVE [==[
            if (uNoLighting < 0.5) {
                vec3 emissiveMask = uGlowEnabled > 0.5
                    ? texture(uGlow, vUv).rgb
                    : baseColor;
                lit += emissiveMask * uEmissiveColor * uEmissiveMult;
            }
]==])

string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1240_OLD_EMISSIVE}" Q1240_EMISSIVE_POS)
if(Q1240_EMISSIVE_POS EQUAL -1)
    message(FATAL_ERROR "Q12.4 could not find Q11.9 PP-lit emissive block")
endif()
string(REPLACE "${Q1240_OLD_EMISSIVE}" "${Q1240_NEW_EMISSIVE}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(FIND "${Q6H_NATIVE_SOURCE}" ": baseColor;" Q1240_BASE_OK)
if(Q1240_BASE_OK EQUAL -1)
    message(FATAL_ERROR "Q12.4 base-colour emissive modulation verification failed")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q12.4 PP-lit base-colour emissive modulation enabled")
