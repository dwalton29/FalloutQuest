# Q15.2: replace the guessed Q13.5/Q13.6 scalar exposure with the shipped
# Fallout 3 Shader Package 17 HDR reduction/final-blend semantics.
#
# Reverse-engineered directly from the user's Fallout3.exe 1.7.0.4 and the
# active shaderpackage017.sdp reported by RendererInfo.txt:
#
#   ISHDRADAPT:
#     current/previous RGB are temporally blended, then the RGB-vector magnitude
#     is clamped to 0.01..UpperLUMClamp.
#
#   ISHDRBRIGHT:
#     bright = max(source.rgb - BrightClamp, 0) * BrightScale
#
#   ISHDRBLENDINSHADER(CIN):
#     denom       = max(adaptedMagnitude, TargetLUM)
#     bloomTerm   = max(blurredBright * (0.5 / denom), 0)
#     sceneTerm   = originalScene * (TargetLUM / denom)
#     output      = bloomTerm + sceneTerm
#
# Q15.2 keeps Q15.1's proven lighting A/B untouched. LEFT Y still means the
# non-cyan 65%-strength neutral Ambient + authored warm Sunlight. The HDR path
# below applies identically to both lighting modes so a Q15.2 LEFT-Y screenshot
# can be compared directly with the prior Q15.1 LEFT-Y screenshot.
#
# The 4x4 full-frame RGB probe is a Quest reduction of the original multi-stage
# DOWN4/DOWN16 chain. It preserves the shipped RGB-vector/magnitude semantics.
# Temporal steady-state is exact; the transient exponent currently uses one
# adaptation step per stereo frame because the exact TimingData.z feed is still
# being traced. This does not change the settled static comparison target.

# -----------------------------------------------------------------------------
# Final post shader: expose TargetLUM and replace the old smoothstep bloom helper
# with the exact ISHDRBRIGHT per-channel threshold/scale operation.
# -----------------------------------------------------------------------------
set(Q1520_OLD_TARGET_UNIFORM [==[
        uniform sampler2D uDepthQ1370;
        out vec4 fragColor;
]==])
set(Q1520_NEW_TARGET_UNIFORM [==[
        uniform sampler2D uDepthQ1370;
        uniform float uTargetLumQ1520;
        out vec4 fragColor;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1520_OLD_TARGET_UNIFORM}" Q1520_TARGET_UNIFORM_POS)
if(Q1520_TARGET_UNIFORM_POS EQUAL -1)
    message(FATAL_ERROR "Q15.2 could not find final Q13.7 post sampler block")
endif()
string(REPLACE "${Q1520_OLD_TARGET_UNIFORM}" "${Q1520_NEW_TARGET_UNIFORM}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1520_OLD_BRIGHT [==[
        vec3 Q1280Bright(vec2 uv) {
            vec3 c = texture(uScene, clamp(uv, vec2(0.0), vec2(1.0))).rgb;
            float lum = Q1280Lum(c);
            float threshold = clamp(uBloomThreshold, 0.05, 0.98);
            float mask = smoothstep(threshold, min(threshold + 0.30, 1.0), lum);
            return c * mask;
        }
]==])
set(Q1520_NEW_BRIGHT [==[
        vec3 Q1280Bright(vec2 uv) {
            vec3 c = texture(uScene, clamp(uv, vec2(0.0), vec2(1.0))).rgb;
            // shaderpackage017 / ISHDRBRIGHT.pso:
            // max(Src0.rgb - HDRParam.x, 0) * HDRParam.y
            return max(c - vec3(max(uBloomThreshold, 0.0)), vec3(0.0)) *
                   max(uBloomScale, 0.0);
        }
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1520_OLD_BRIGHT}" Q1520_BRIGHT_POS)
if(Q1520_BRIGHT_POS EQUAL -1)
    message(FATAL_ERROR "Q15.2 could not find Q12.8 bright helper")
endif()
string(REPLACE "${Q1520_OLD_BRIGHT}" "${Q1520_NEW_BRIGHT}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Preserve a separate bright/blur source instead of adding it to the scene before
# tone mapping. Package 17's final shader samples Src0 (bright/blur chain) and
# DestBlend (the original scene) separately.
set(Q1520_OLD_BLOOM [==[
            // Quest-safe one-pass bloom approximation. The authored IMGS blur
            // radius chooses the sample spread; Bright Scale/Clamp and exterior
            // bloom alpha remain the authored strength controls.
            if (uBloomAlpha > 0.0001 && uBloomScale > 0.0001) {
                vec2 d = uTexel * clamp(uBloomRadius, 1.0, 12.0) * 0.85;
                vec3 bloom = Q1280Bright(vUv + vec2( d.x, 0.0));
                bloom += Q1280Bright(vUv + vec2(-d.x, 0.0));
                bloom += Q1280Bright(vUv + vec2(0.0,  d.y));
                bloom += Q1280Bright(vUv + vec2(0.0, -d.y));
                bloom += Q1280Bright(vUv + vec2( d.x,  d.y));
                bloom += Q1280Bright(vUv + vec2(-d.x,  d.y));
                bloom += Q1280Bright(vUv + vec2( d.x, -d.y));
                bloom += Q1280Bright(vUv + vec2(-d.x, -d.y));
                colour += (bloom * 0.125) * uBloomScale * uBloomAlpha;
            }
]==])
set(Q1520_NEW_BLOOM [==[
            // Q15.2: reduced Quest equivalent of the package-17 BRIGHT -> BLUR
            // source. HDR mode does not gate this on the separate Bloom-lighting
            // switch; BrightClamp/BrightScale are the HDR controls themselves.
            vec3 q1520HdrBright = vec3(0.0);
            if (uBloomScale > 0.0001) {
                vec2 d = uTexel * clamp(uBloomRadius, 1.0, 12.0) * 0.85;
                q1520HdrBright = Q1280Bright(vUv + vec2( d.x, 0.0));
                q1520HdrBright += Q1280Bright(vUv + vec2(-d.x, 0.0));
                q1520HdrBright += Q1280Bright(vUv + vec2(0.0,  d.y));
                q1520HdrBright += Q1280Bright(vUv + vec2(0.0, -d.y));
                q1520HdrBright += Q1280Bright(vUv + vec2( d.x,  d.y));
                q1520HdrBright += Q1280Bright(vUv + vec2(-d.x,  d.y));
                q1520HdrBright += Q1280Bright(vUv + vec2( d.x, -d.y));
                q1520HdrBright += Q1280Bright(vUv + vec2(-d.x, -d.y));
                q1520HdrBright *= 0.125;
            }
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1520_OLD_BLOOM}" Q1520_BLOOM_POS)
if(Q1520_BLOOM_POS EQUAL -1)
    message(FATAL_ERROR "Q15.2 could not find active Q12.8 bloom block")
endif()
string(REPLACE "${Q1520_OLD_BLOOM}" "${Q1520_NEW_BLOOM}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Adaptation history: replace Q13.5/Q13.6 log-luma scalar exposure with the RGB
# vector semantics used by ISHDRADAPT. The existing 1x1 RGBA8 history is enough
# because UpperLUMClamp bounds the adapted RGB vector to the display-range-sized
# domain used by the shipped shader. First frame takes the current average.
# -----------------------------------------------------------------------------
set(Q1520_OLD_ADAPT_MAIN [==[
            float sumLog = 0.0;
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    vec2 uv = (vec2(float(x), float(y)) + vec2(0.5)) / 4.0;
                    float lum = max(Q1350Lum(texture(uScene, uv).rgb), 0.01);
                    sumLog += log(lum);
                }
            }
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
            float adaptedExposure = (uFirstFrame != 0)
                ? desiredExposure
                : mix(desiredExposure, previousExposure, retention);

            // RG = high precision exposure; B = scene luminance / 4; A = desired
            // exposure / 4. B/A exist only for infrequent diagnostic readback.
            fragColor = vec4(Q1350PackExposure(adaptedExposure),
                             clamp(sceneLum / 4.0, 0.0, 1.0),
                             clamp(desiredExposure / 4.0, 0.0, 1.0));
]==])
set(Q1520_NEW_ADAPT_MAIN [==[
            vec3 currentAverageQ1520 = vec3(0.0);
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    vec2 uv = (vec2(float(x), float(y)) + vec2(0.5)) / 4.0;
                    currentAverageQ1520 += max(texture(uScene, uv).rgb, vec3(0.0));
                }
            }
            currentAverageQ1520 *= (1.0 / 16.0);

            vec3 previousAverageQ1520 = max(texture(uPrev, vec2(0.5)).rgb, vec3(0.0));

            // ISHDRADAPT uses p = pow(EyeAdaptSpeed, TimingData.z), then
            // (1-p)*previous + p*current. Q15.2 uses one settled comparison step
            // per stereo frame while TimingData.z's CPU feed is traced; steady
            // state is identical and that is the visual target of this build.
            float pQ1520 = clamp(uEyeAdaptSpeed, 0.0, 1.0);
            vec3 adaptedQ1520 = (uFirstFrame != 0)
                ? currentAverageQ1520
                : mix(previousAverageQ1520, currentAverageQ1520, pQ1520);

            float adaptedMagnitudeQ1520 = length(adaptedQ1520);
            float safeMagnitudeQ1520 = max(0.01, adaptedMagnitudeQ1520);
            float clampedMagnitudeQ1520 = min(safeMagnitudeQ1520, max(uTargetLum, 0.01));
            adaptedQ1520 *= clampedMagnitudeQ1520 / safeMagnitudeQ1520;

            // RGB is the adapted/clamped vector itself, matching ISHDRADAPT.
            // Alpha carries current magnitude only for diagnostics.
            fragColor = vec4(adaptedQ1520,
                             clamp(length(currentAverageQ1520) * 0.25, 0.0, 1.0));
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1520_OLD_ADAPT_MAIN}" Q1520_ADAPT_MAIN_POS)
if(Q1520_ADAPT_MAIN_POS EQUAL -1)
    message(FATAL_ERROR "Q15.2 could not find active Q13.6 adaptation main block")
endif()
string(REPLACE "${Q1520_OLD_ADAPT_MAIN}" "${Q1520_NEW_ADAPT_MAIN}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Initialize history to black; uFirstFrame replaces it with the first current
# RGB average. Q13.5's packed scalar initial value is no longer meaningful.
string(REPLACE
    "const uint8_t initialPixel[4]{63u, 191u, 64u, 64u};"
    "const uint8_t initialPixel[4]{0u, 0u, 0u, 255u};"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# The adaptation shader's historical uTargetLum slot is repurposed to the exact
# ISHDRADAPT upper magnitude clamp. TargetLUM itself is uploaded independently
# to the final package-17 blend below.
set(Q1520_OLD_ADAPT_UPLOAD [==[
    const Fo3ImageSpaceQ1280& image = GetFo3ImageSpaceQ1280();
    const float targetLum = image.valid ? std::clamp(image.hdrTargetLum, 0.05f, 4.0f) : 1.0f;
    const float eyeSpeed = image.valid ? std::clamp(image.hdrEyeAdaptSpeed, 0.0f, 0.9995f) : 0.5f;
    glUniform1f(q1350AdaptTargetLocation, targetLum);
    glUniform1f(q1350AdaptSpeedLocation, eyeSpeed);
]==])
set(Q1520_NEW_ADAPT_UPLOAD [==[
    const Fo3ImageSpaceQ1280& image = GetFo3ImageSpaceQ1280();
    const float targetLum = image.valid ? std::clamp(image.hdrTargetLum, 0.001f, 4.0f) : 1.0f;
    const float upperLum = image.valid ? std::clamp(image.hdrUpperLumClamp, 0.01f, 4.0f) : 1.0f;
    const float eyeSpeed = image.valid ? std::clamp(image.hdrEyeAdaptSpeed, 0.0f, 1.0f) : 0.5f;
    glUniform1f(q1350AdaptTargetLocation, upperLum);
    glUniform1f(q1350AdaptSpeedLocation, eyeSpeed);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1520_OLD_ADAPT_UPLOAD}" Q1520_ADAPT_UPLOAD_POS)
if(Q1520_ADAPT_UPLOAD_POS EQUAL -1)
    message(FATAL_ERROR "Q15.2 could not find Q13.5 adaptation parameter upload")
endif()
string(REPLACE "${Q1520_OLD_ADAPT_UPLOAD}" "${Q1520_NEW_ADAPT_UPLOAD}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Replace Q13.6's whole-frame scalar multiply with the exact Package-17 final
# reciprocal blend equation. q1520HdrBright is the reduced Src0 RGB; colour is
# the original scene/DestBlend after the existing conservative contact AO.
# -----------------------------------------------------------------------------
set(Q1520_OLD_EXPOSURE [==[
            // Q13.5 exposure is packed across RG in a 1x1 RGBA8 history texture.
            // R carries the high byte and G the fractional byte of exposure/4.
            vec2 packedExposureQ1350 = texture(uExposureQ1350, vec2(0.5)).rg;
            float exposureQ1350 = clamp(
                (packedExposureQ1350.r + packedExposureQ1350.g / 255.0) * 4.0,
                0.75, 1.35);
            colour *= exposureQ1350;
]==])
set(Q1520_NEW_EXPOSURE [==[
            // shaderpackage017 / ISHDRBLENDINSHADER(CIN): Src0.a in vanilla
            // carries the adapted HDR magnitude through the blur chain. Quest
            // samples the retained adapted RGB history and reconstructs that
            // same magnitude directly.
            vec3 adaptedRgbQ1520 = max(texture(uExposureQ1350, vec2(0.5)).rgb,
                                       vec3(0.0));
            float adaptedMagnitudeQ1520 = max(length(adaptedRgbQ1520), 0.01);
            float targetLumQ1520 = max(uTargetLumQ1520, 0.001);
            float denomQ1520 = max(adaptedMagnitudeQ1520, targetLumQ1520);
            float bloomWeightQ1520 = 0.5 / denomQ1520;
            float sceneWeightQ1520 = targetLumQ1520 / denomQ1520;
            colour = max(q1520HdrBright * bloomWeightQ1520, vec3(0.0)) +
                     colour * sceneWeightQ1520;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1520_OLD_EXPOSURE}" Q1520_EXPOSURE_POS)
if(Q1520_EXPOSURE_POS EQUAL -1)
    message(FATAL_ERROR "Q15.2 could not find Q13.6 final scalar exposure block")
endif()
string(REPLACE "${Q1520_OLD_EXPOSURE}" "${Q1520_NEW_EXPOSURE}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# TargetLUM uniform plumbing for the final shader.
# -----------------------------------------------------------------------------
string(REPLACE
    "GLint q1370DepthLocation = -1;"
    "GLint q1370DepthLocation = -1;\nGLint q1520TargetLumLocation = -1;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1520_OLD_LOCATION [==[
    q1370DepthLocation = glGetUniformLocation(q1280PostProgram, "uDepthQ1370");
    return q1280SceneLocation >= 0 && q1280TexelLocation >= 0 &&
           q1350ExposureLocation >= 0 && q1370DepthLocation >= 0;
]==])
set(Q1520_NEW_LOCATION [==[
    q1370DepthLocation = glGetUniformLocation(q1280PostProgram, "uDepthQ1370");
    q1520TargetLumLocation = glGetUniformLocation(q1280PostProgram, "uTargetLumQ1520");
    return q1280SceneLocation >= 0 && q1280TexelLocation >= 0 &&
           q1350ExposureLocation >= 0 && q1370DepthLocation >= 0 &&
           q1520TargetLumLocation >= 0;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1520_OLD_LOCATION}" Q1520_LOCATION_POS)
if(Q1520_LOCATION_POS EQUAL -1)
    message(FATAL_ERROR "Q15.2 could not find final Q13.7 uniform lookup")
endif()
string(REPLACE "${Q1520_OLD_LOCATION}" "${Q1520_NEW_LOCATION}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1520_IMAGE_MARKER [==[
    const Fo3ImageSpaceQ1280& image = GetFo3ImageSpaceQ1280();
    const int flags = image.valid ? static_cast<int>(image.cinematicFlags) : 0;
]==])
set(Q1520_IMAGE_UPLOAD [==[
    const Fo3ImageSpaceQ1280& image = GetFo3ImageSpaceQ1280();
    glUniform1f(q1520TargetLumLocation,
                image.valid ? std::clamp(image.hdrTargetLum, 0.001f, 4.0f) : 1.0f);
    const int flags = image.valid ? static_cast<int>(image.cinematicFlags) : 0;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1520_IMAGE_MARKER}" Q1520_IMAGE_POS)
if(Q1520_IMAGE_POS EQUAL -1)
    message(FATAL_ERROR "Q15.2 could not find final ImageSpace uniform upload")
endif()
string(REPLACE "${Q1520_IMAGE_MARKER}" "${Q1520_IMAGE_UPLOAD}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Replace obsolete runtime labels so logcat proves the shipped package-17 path
# is active rather than the Q13.6 quarter-target/sqrt reconstruction.
string(REPLACE "Q13.6 HDR ADAPT READY:"
               "Q15.2 SP17 HDR ADAPT READY:"
               Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE "Q13.6 HDR EXPOSURE:"
               "Q15.2 SP17 HDR HISTORY:"
               Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE "eyeAdapt=q136-relative-gpu"
               "eyeAdapt=q152-sp17-rgb-vector"
               Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Hard guards: guessed Q13.6 tone mapping must be absent and both exact package
# 17 equations must be present in the final generated shader.
string(FIND "${Q6H_NATIVE_SOURCE}" "targetBridge = clamp(uTargetLum * 0.25" Q1520_OLD_TARGET_BRIDGE)
string(FIND "${Q6H_NATIVE_SOURCE}" "sqrt(targetBridge / max(sceneLum, 0.02))" Q1520_OLD_SQRT)
string(FIND "${Q6H_NATIVE_SOURCE}" "return max(c - vec3(max(uBloomThreshold, 0.0))" Q1520_BRIGHT_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "float bloomWeightQ1520 = 0.5 / denomQ1520;" Q1520_BLEND_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "float sceneWeightQ1520 = targetLumQ1520 / denomQ1520;" Q1520_SCENE_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "currentAverageQ1520" Q1520_ADAPT_OK)
if(NOT Q1520_OLD_TARGET_BRIDGE EQUAL -1 OR NOT Q1520_OLD_SQRT EQUAL -1 OR
   Q1520_BRIGHT_OK EQUAL -1 OR Q1520_BLEND_OK EQUAL -1 OR
   Q1520_SCENE_OK EQUAL -1 OR Q1520_ADAPT_OK EQUAL -1)
    message(FATAL_ERROR
        "Q15.2 verification failed: oldBridge=${Q1520_OLD_TARGET_BRIDGE} oldSqrt=${Q1520_OLD_SQRT} bright=${Q1520_BRIGHT_OK} blend=${Q1520_BLEND_OK} scene=${Q1520_SCENE_OK} adapt=${Q1520_ADAPT_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")

message(STATUS "Q15.2 Shader Package 17 HDR RGB adaptation + reciprocal final blend enabled")
