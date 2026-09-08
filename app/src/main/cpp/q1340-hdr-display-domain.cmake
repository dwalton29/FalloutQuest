# Q13.4: fix the ordering/domain of Fallout 3 ImageSpace cinematic controls.
#
# Q12.8 applied saturation/contrast/tint/brightness directly to the linear HDR
# eye buffer. Fallout's cinematic controls are authored as final-frame/film
# controls, and applying contrast around an authored average luminance near 1.0
# directly to dark linear values can drive them negative before the final clamp,
# producing the crushed-black look seen in Megaton.
#
# Keep lighting, bloom and emissive work in the existing linear HDR buffer. For
# only the cinematic block, encode to a display-like sRGB transfer domain, apply
# the authored IMGS/IMAD controls there, then decode back to linear before writing
# to the OpenXR sRGB swapchain. This changes no authored values and adds no colour
# filter. Eye adaptation/Target LUM remain deliberately deferred until their exact
# temporal semantics are implemented rather than guessed.

set(Q1340_OLD_MAIN_MARKER [==[
        void main() {
            vec3 colour = texture(uScene, vUv).rgb;
]==])
set(Q1340_NEW_MAIN_MARKER [==[
        vec3 Q1340LinearToSrgb(vec3 c) {
            c = max(c, vec3(0.0));
            bvec3 low = lessThanEqual(c, vec3(0.0031308));
            vec3 lo = c * 12.92;
            vec3 hi = 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055;
            return mix(hi, lo, vec3(low));
        }

        vec3 Q1340SrgbToLinear(vec3 c) {
            c = max(c, vec3(0.0));
            bvec3 low = lessThanEqual(c, vec3(0.04045));
            vec3 lo = c / 12.92;
            vec3 hi = pow((c + 0.055) / 1.055, vec3(2.4));
            return mix(hi, lo, vec3(low));
        }

        void main() {
            vec3 colour = texture(uScene, vUv).rgb;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1340_OLD_MAIN_MARKER}" Q1340_MAIN_POS)
if(Q1340_MAIN_POS EQUAL -1)
    message(FATAL_ERROR "Q13.4 could not find Q12.8 post-shader main marker")
endif()
string(REPLACE "${Q1340_OLD_MAIN_MARKER}" "${Q1340_NEW_MAIN_MARKER}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1340_OLD_CINEMATIC [==[
            // IMGS flags: 0x01 saturation, 0x02 contrast, 0x04 tint,
            // 0x08 brightness. Values remain data-driven from Fallout3.esm.
            if ((uFlags & 1) != 0) {
                float lum = Q1280Lum(colour);
                colour = mix(vec3(lum), colour, uSaturation);
            }
            if ((uFlags & 2) != 0) {
                colour = (colour - vec3(uContrastAvg)) * uContrast + vec3(uContrastAvg);
            }
            if ((uFlags & 4) != 0) {
                float lum = Q1280Lum(max(colour, vec3(0.0)));
                vec3 tinted = lum * max(uTintColor, vec3(0.0));
                colour = mix(colour, tinted, clamp(uTintValue, 0.0, 1.0));
            }
            if ((uFlags & 8) != 0) {
                colour *= uBrightness;
            }

            // Preserve the renderer's established exposure while allowing HDR
            // bloom/emission to roll into the display range without NaN/negative
            // output. Eye adaptation is intentionally deferred until game time.
            colour = max(colour, vec3(0.0));
            fragColor = vec4(colour, 1.0);
]==])
set(Q1340_NEW_CINEMATIC [==[
            // Q13.4: Fallout's cinematic controls are final-frame controls. Do
            // not run them against raw linear HDR radiance: contrast around an
            // authored average luminance near 1.0 can otherwise send ordinary
            // dark surfaces negative and the old final max() turns them black.
            // Keep bloom/lighting linear, temporarily enter display transfer
            // space for the film controls, then return to linear for the sRGB
            // OpenXR target.
            vec3 cinematic = Q1340LinearToSrgb(colour);
            if ((uFlags & 1) != 0) {
                float lum = Q1280Lum(cinematic);
                cinematic = mix(vec3(lum), cinematic, uSaturation);
            }
            if ((uFlags & 2) != 0) {
                cinematic = (cinematic - vec3(uContrastAvg)) * uContrast + vec3(uContrastAvg);
            }
            if ((uFlags & 4) != 0) {
                float lum = Q1280Lum(max(cinematic, vec3(0.0)));
                vec3 tinted = lum * max(uTintColor, vec3(0.0));
                cinematic = mix(cinematic, tinted, clamp(uTintValue, 0.0, 1.0));
            }
            if ((uFlags & 8) != 0) {
                cinematic *= uBrightness;
            }

            colour = Q1340SrgbToLinear(max(cinematic, vec3(0.0)));
            fragColor = vec4(max(colour, vec3(0.0)), 1.0);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1340_OLD_CINEMATIC}" Q1340_CINEMATIC_POS)
if(Q1340_CINEMATIC_POS EQUAL -1)
    message(FATAL_ERROR "Q13.4 could not find Q12.8 cinematic block")
endif()
string(REPLACE "${Q1340_OLD_CINEMATIC}" "${Q1340_NEW_CINEMATIC}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1340_OLD_LOG [==[
        Q6H_LOGI("Q12.8 POST ACTIVE: IMGS=%08X flags=0x%02X saturation=%.3f contrast=%.3f brightness=%.3f tintValue=%.3f bloomExterior=%.3f bloomApplied=%.3f eyeAdapt=deferred",
                 currentImageSpace, image.valid ? image.cinematicFlags : 0u,
                 image.valid ? image.cinematicSaturation : 1.0f,
                 image.valid ? image.cinematicContrast : 1.0f,
                 image.valid ? image.cinematicBrightness : 1.0f,
                 image.valid ? image.cinematicTintValue : 0.0f,
                 image.valid ? image.bloomAlphaExterior : 0.0f,
                 bloomAlpha * 0.35f);
]==])
set(Q1340_NEW_LOG [==[
        Q6H_LOGI("Q13.4 HDR POST ACTIVE: IMGS=%08X flags=0x%02X saturation=%.3f contrastAvg=%.3f contrast=%.3f brightness=%.3f tintValue=%.3f bloomExterior=%.3f bloomApplied=%.3f targetLum=%.3f upperLum=%.3f eyeAdaptSpeed=%.3f cinematicDomain=srgb eyeAdapt=deferred-exact",
                 currentImageSpace, image.valid ? image.cinematicFlags : 0u,
                 image.valid ? image.cinematicSaturation : 1.0f,
                 image.valid ? image.cinematicContrastAvgLum : 0.5f,
                 image.valid ? image.cinematicContrast : 1.0f,
                 image.valid ? image.cinematicBrightness : 1.0f,
                 image.valid ? image.cinematicTintValue : 0.0f,
                 image.valid ? image.bloomAlphaExterior : 0.0f,
                 bloomAlpha * 0.35f,
                 image.valid ? image.hdrTargetLum : 1.0f,
                 image.valid ? image.hdrUpperLumClamp : 1.0f,
                 image.valid ? image.hdrEyeAdaptSpeed : 0.5f);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1340_OLD_LOG}" Q1340_LOG_POS)
if(Q1340_LOG_POS EQUAL -1)
    message(FATAL_ERROR "Q13.4 could not find Q12.8 post-active log")
endif()
string(REPLACE "${Q1340_OLD_LOG}" "${Q1340_NEW_LOG}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(FIND "${Q6H_NATIVE_SOURCE}" "Q1340LinearToSrgb" Q1340_TRANSFER_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q13.4 HDR POST ACTIVE" Q1340_LOG_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "vec3 cinematic = Q1340LinearToSrgb(colour);" Q1340_DOMAIN_OK)
if(Q1340_TRANSFER_OK EQUAL -1 OR Q1340_LOG_OK EQUAL -1 OR Q1340_DOMAIN_OK EQUAL -1)
    message(FATAL_ERROR "Q13.4 HDR display-domain verification failed")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q13.4 ImageSpace cinematic display-domain ordering enabled")
