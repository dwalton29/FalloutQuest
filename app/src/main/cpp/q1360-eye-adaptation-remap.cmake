# Q13.6: fix Q13.5's saturated exposure mapping.
#
# Device evidence from Megaton showed scene log-luminance moving ~0.12..0.39
# while targetLum/sceneLum always requested >2x exposure. The 2x safety clamp
# therefore became the picture rather than a safety net. GECK documentation
# describes Eye Adapt Speed as adapted scene luminance chasing actual luminance,
# while Target LUM is a fragile legacy control and Upper LUM Clamp is effectively
# unused. Do not interpret Target LUM as a normalized linear output luminance.
#
# This bridge keeps the proven temporal/stereo machinery from Q13.5 but maps the
# legacy ~1.x Target LUM domain into our 0..~1 linear eye-buffer range before
# calculating exposure. A sqrt response intentionally compresses adaptation so
# bright sky can pull below neutral and dark structures can lift above neutral
# without turning the whole frame into a permanently boosted HDR image.

set(Q1360_OLD_EXPOSURE_SHADER [==[
            float sceneLum = exp(sumLog / 16.0);
            float desiredExposure = clamp(uTargetLum / max(sceneLum, 0.02), 0.5, 2.0);
            float previousExposure = clamp(Q1350UnpackExposure(texture(uPrev, vec2(0.5)).rg),
                                           0.5, 2.0);
            float retention = clamp(uEyeAdaptSpeed, 0.0, 0.9995);
]==])
set(Q1360_NEW_EXPOSURE_SHADER [==[
            float sceneLum = exp(sumLog / 16.0);

            // Q13.6: Fallout 3's Target LUM is not a normalized 0..1 target.
            // Bridge the legacy ~1.x authored domain into this renderer's linear
            // eye-buffer luminance range. For WastelandBaseImageSpace 1.140 ->
            // 0.285, which sits inside the measured Megaton day range instead of
            // forcing every view against the maximum exposure clamp.
            float targetBridge = clamp(uTargetLum * 0.25, 0.10, 0.60);
            float desiredExposure = clamp(
                sqrt(targetBridge / max(sceneLum, 0.02)), 0.75, 1.35);
            float previousExposure = clamp(Q1350UnpackExposure(texture(uPrev, vec2(0.5)).rg),
                                           0.75, 1.35);
            float retention = clamp(uEyeAdaptSpeed, 0.0, 0.9995);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1360_OLD_EXPOSURE_SHADER}" Q1360_SHADER_POS)
if(Q1360_SHADER_POS EQUAL -1)
    message(FATAL_ERROR "Q13.6 could not find Q13.5 saturated exposure formula")
endif()
string(REPLACE "${Q1360_OLD_EXPOSURE_SHADER}" "${Q1360_NEW_EXPOSURE_SHADER}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# The final composite must use the same narrower safety window as the adaptation
# pass. Exposure history remains packed exactly as Q13.5 established.
set(Q1360_OLD_COMPOSITE_CLAMP [==[
            float exposureQ1350 = clamp(
                (packedExposureQ1350.r + packedExposureQ1350.g / 255.0) * 4.0,
                0.5, 2.0);
]==])
set(Q1360_NEW_COMPOSITE_CLAMP [==[
            float exposureQ1350 = clamp(
                (packedExposureQ1350.r + packedExposureQ1350.g / 255.0) * 4.0,
                0.75, 1.35);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1360_OLD_COMPOSITE_CLAMP}" Q1360_CLAMP_POS)
if(Q1360_CLAMP_POS EQUAL -1)
    message(FATAL_ERROR "Q13.6 could not find Q13.5 post exposure clamp")
endif()
string(REPLACE "${Q1360_OLD_COMPOSITE_CLAMP}" "${Q1360_NEW_COMPOSITE_CLAMP}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Runtime diagnostics: make the corrected mapping obvious on-device.
set(Q1360_OLD_READY [==[
Q13.5 HDR ADAPT READY: probe=4x4-log-average history=RGBA8-packed16 gpuOnly=1 update=eye0-once-per-stereo-frame exposureClamp=0.500..2.000 semantics=eyeAdapt-retention
]==])
set(Q1360_NEW_READY [==[
Q13.6 HDR ADAPT READY: probe=4x4-log-average history=RGBA8-packed16 gpuOnly=1 update=eye0-once-per-stereo-frame exposureClamp=0.750..1.350 semantics=eyeAdapt-retention targetLumBridge=quarter-scale sqrtResponse=1
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1360_OLD_READY}" Q1360_READY_POS)
if(Q1360_READY_POS EQUAL -1)
    message(FATAL_ERROR "Q13.6 could not find Q13.5 ready diagnostic")
endif()
string(REPLACE "${Q1360_OLD_READY}" "${Q1360_NEW_READY}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1360_OLD_EXPOSURE_LOG [==[
        Q6H_LOGI("Q13.5 HDR EXPOSURE: sceneLum=%.4f targetLum=%.3f desiredExposure=%.4f adaptedExposure=%.4f eyeAdaptSpeed=%.3f update=%llu stereoShared=1 gpuHistory=1 mapping=target-over-logAverage clamp=0.5..2.0",
                 sceneLum, targetLum, desiredExposure, exposure, eyeSpeed,
                 static_cast<unsigned long long>(q1350AdaptFrame));
]==])
set(Q1360_NEW_EXPOSURE_LOG [==[
        const float targetBridge = std::clamp(targetLum * 0.25f, 0.10f, 0.60f);
        Q6H_LOGI("Q13.6 HDR EXPOSURE: sceneLum=%.4f targetLum=%.3f targetBridge=%.4f desiredExposure=%.4f adaptedExposure=%.4f eyeAdaptSpeed=%.3f update=%llu stereoShared=1 gpuHistory=1 mapping=sqrt(targetQuarter/logAverage) clamp=0.75..1.35",
                 sceneLum, targetLum, targetBridge, desiredExposure, exposure, eyeSpeed,
                 static_cast<unsigned long long>(q1350AdaptFrame));
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1360_OLD_EXPOSURE_LOG}" Q1360_LOG_POS)
if(Q1360_LOG_POS EQUAL -1)
    message(FATAL_ERROR "Q13.6 could not find Q13.5 exposure diagnostic")
endif()
string(REPLACE "${Q1360_OLD_EXPOSURE_LOG}" "${Q1360_NEW_EXPOSURE_LOG}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Q13.4 predated the temporal bridge; stop labelling eye adaptation deferred.
string(REPLACE "eyeAdapt=deferred-exact"
               "eyeAdapt=q136-relative-gpu"
               Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Drift guards.
string(FIND "${Q6H_NATIVE_SOURCE}" "targetBridge = clamp(uTargetLum * 0.25" Q1360_BRIDGE_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q13.6 HDR EXPOSURE" Q1360_LOG_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "0.75, 1.35" Q1360_CLAMP_OK)
if(Q1360_BRIDGE_OK EQUAL -1 OR Q1360_LOG_OK EQUAL -1 OR Q1360_CLAMP_OK EQUAL -1)
    message(FATAL_ERROR "Q13.6 exposure remap verification failed: bridge=${Q1360_BRIDGE_OK} log=${Q1360_LOG_OK} clamp=${Q1360_CLAMP_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q13.6 saturated eye-adaptation mapping replaced with bounded relative response")
