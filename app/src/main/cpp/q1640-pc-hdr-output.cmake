# Q15.14: reproduce the captured PC Fallout 3 final HDR/ImageSpace output stage.
#
# Ground truth: apitrace call 4618221, immediately before the final 1280x720
# presentation chain. The active PC shader samples two FP16 sources and writes to
# D3DFMT_X8R8G8B8 with D3DRS_SRGBWRITEENABLE=0. Its captured constants are:
#
#   HDRParam.x = 1.2
#   Cinematic  = (0.875, 0.0, 1.02, 1.1)
#   Tint       = (0.7399142, 0.5749559, 0.3128335, 0.6)
#   Fade       = (0,0,0,0)
#   luminance  = dot(rgb, (0.299, 0.587, 0.114))
#
# The captured Src0 alpha is 0.68359375, below HDRParam.x=1.2, so the final
# reciprocal blend leaves the original scene at weight 1.0 and adds blurred HDR
# bright at 0.5/1.2 = 0.4166667. Q15.2 already implements that SP17 structure;
# Q15.14 pins TargetLUM to the observed 1.2 for this comparison and replaces the
# final Q14.6 cinematic block with the literal captured-PC arithmetic/constants.
#
# OpenXR presents through the preferred GL_SRGB8_ALPHA8 swapchain. PC writes its
# final numeric result directly to a non-sRGB X8R8G8B8 target. Therefore we
# sRGB-decode the PC numeric result only at the final Quest write so the hardware
# sRGB attachment re-encodes it back to the same stored/display code values.

# -----------------------------------------------------------------------------
# Pin the exact captured HDRParam.x into Q15.2's existing final blend uniform.
# Keep the sampler/uniform plumbing intact so the rest of the SP17 HDR path
# remains unchanged and the shader cannot optimize away required bindings.
# -----------------------------------------------------------------------------
set(Q1640_TARGET_OLD [==[
    glUniform1f(q1520TargetLumLocation,
                image.valid ? std::clamp(image.hdrTargetLum, 0.001f, 4.0f) : 1.0f);
]==])
set(Q1640_TARGET_NEW [==[
    glUniform1f(q1520TargetLumLocation, 1.2f);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1640_TARGET_OLD}" Q1640_TARGET_POS)
if(Q1640_TARGET_POS EQUAL -1)
    message(FATAL_ERROR "Q15.14 could not find Q15.2 TargetLUM upload")
endif()
string(REPLACE "${Q1640_TARGET_OLD}" "${Q1640_TARGET_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Q14.6 already removed Q13.4's guessed explicit gamma hop and changed the active
# cinematic block. Replace THAT final block, not the historical Q13.4 source.
# The Q15.2 HDR reciprocal scene+bloom combine executes immediately before this.
# -----------------------------------------------------------------------------
set(Q1640_CINEMATIC_OLD [==[
            // Q14.6: compiled Fallout 3 ISCinematic semantics. Keep the input in
            // the renderer's native post-buffer domain; the vanilla shader does
            // not perform an explicit gamma encode/decode around this block.
            vec3 cinematic = colour;
            float cinematicLumQ1460 = dot(cinematic, vec3(0.299, 0.587, 0.114));

            // ISCinematic: saturation first, using the original luminance.
            if ((uFlags & 1) != 0) {
                cinematic = mix(vec3(cinematicLumQ1460), cinematic, uSaturation);
            }

            // ISCinematic tint target is luminance * TintRGB, not rgb * TintRGB.
            if ((uFlags & 4) != 0) {
                vec3 tintedQ1460 = cinematicLumQ1460 * max(uTintColor, vec3(0.0));
                cinematic = mix(cinematic, tintedQ1460, clamp(uTintValue, 0.0, 1.0));
            }

            // Vanilla Fade sits here. No authored Fade feed exists in the current
            // FalloutQuest ESM post state, so its neutral identity is intentional.

            // ISCinematic multiplies brightness before applying contrast.
            if ((uFlags & 8) != 0) {
                cinematic *= uBrightness;
            }
            if ((uFlags & 2) != 0) {
                cinematic = (cinematic - vec3(uContrastAvg)) * uContrast + vec3(uContrastAvg);
            }

            colour = max(cinematic, vec3(0.0));
]==])
set(Q1640_CINEMATIC_NEW [==[
            // Q15.14 / PC call 4618221: literal captured final-film arithmetic.
            // The PC performs this directly on the FP16 HDR-combined values.
            vec3 q1640PcOutput = colour;
            float q1640Lum = dot(q1640PcOutput,
                                 vec3(0.298999995, 0.587000012, 0.114));

            // lrp r1.xyz, c19.x, r0, r0.w ; c19.x = 0.875 saturation
            q1640PcOutput = mix(vec3(q1640Lum), q1640PcOutput, 0.875);

            // mad/mad tint pair with c20 =
            // (0.7399142, 0.5749559, 0.3128335, 0.6).
            vec3 q1640TintTarget = q1640Lum *
                vec3(0.7399142, 0.5749559, 0.3128335);
            q1640PcOutput = mix(q1640PcOutput, q1640TintTarget, 0.6);

            // c19.w brightness=1.1, c19.y contrastAverage=0,
            // c19.z contrast=1.02. Fade c22=(0,0,0,0), so Fade is identity.
            q1640PcOutput = (q1640PcOutput * 1.1 - vec3(0.0)) * 1.02 + vec3(0.0);

            // X8R8G8B8 clamps on the PC. Match that numeric result before
            // compensating for Quest's sRGB swapchain storage conversion.
            q1640PcOutput = clamp(q1640PcOutput, vec3(0.0), vec3(1.0));

            // PC: X8R8G8B8 + D3DRS_SRGBWRITEENABLE=0, so its shader numeric
            // value becomes the stored/display code directly. Quest prefers an
            // sRGB OpenXR attachment; inverse-transfer here so attachment encode
            // lands on the identical final code value.
            colour = Q1340SrgbToLinear(q1640PcOutput);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1640_CINEMATIC_OLD}" Q1640_CINEMATIC_POS)
if(Q1640_CINEMATIC_POS EQUAL -1)
    message(FATAL_ERROR "Q15.14 could not find active Q14.6 cinematic block")
endif()
string(REPLACE "${Q1640_CINEMATIC_OLD}" "${Q1640_CINEMATIC_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Runtime proof. Put this beside the existing post-active logging without
# altering any renderer state.
set(Q1640_DRAW_OLD [==[
    glDrawArrays(GL_TRIANGLES, 0, 3);

    static uint32_t lastLoggedImageSpace = 0xFFFFFFFFu;
]==])
set(Q1640_DRAW_NEW [==[
    glDrawArrays(GL_TRIANGLES, 0, 3);

    static bool q1640Logged = false;
    if (!q1640Logged) {
        q1640Logged = true;
        Q6H_LOGI("Q15.14 PC HDR OUTPUT: call=4618221 targetLum=1.200 saturation=0.875 tint=(0.739914 0.574956 0.312834) tintValue=0.600 contrastAvg=0.000 contrast=1.020 brightness=1.100 fade=0 lum=REC601 pcTarget=X8R8G8B8 pcSrgbWrite=0 questSrgbCompensation=1");
    }

    static uint32_t lastLoggedImageSpace = 0xFFFFFFFFu;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1640_DRAW_OLD}" Q1640_DRAW_POS)
if(Q1640_DRAW_POS EQUAL -1)
    message(FATAL_ERROR "Q15.14 could not find final post draw/log anchor")
endif()
string(REPLACE "${Q1640_DRAW_OLD}" "${Q1640_DRAW_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Visible build identity: Q15.13 -> Q15.14. Q15.13's last seven-segment glyph is
# digit 3 (0x4F); digit 4 is B+C+F+G = 0x66.
# -----------------------------------------------------------------------------
set(Q1640_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1640_Q4_INPUT}")
    message(FATAL_ERROR "Q15.14 expected final OpenXR source at ${Q1640_Q4_INPUT}")
endif()
file(READ "${Q1640_Q4_INPUT}" Q1640_Q4_SOURCE)
string(REPLACE
    "q1600Digit(q1600X, 0x4Fu); // 3 = A B C D G"
    "q1600Digit(q1600X, 0x66u); // 4 = B C F G"
    Q1640_Q4_SOURCE "${Q1640_Q4_SOURCE}")
string(REPLACE "Q15.13" "Q15.14" Q1640_Q4_SOURCE "${Q1640_Q4_SOURCE}")
file(WRITE "${Q1640_Q4_INPUT}" "${Q1640_Q4_SOURCE}")

# Hard guards: Q14.6's conditional cinematic block must be gone, exact captured
# constants must exist, Q15.12 sRGB BaseMap must survive, terrain must remain
# untouched, and the visible headset label must prove Q15.14.
string(FIND "${Q6H_NATIVE_SOURCE}" "float cinematicLumQ1460" Q1640_OLD_Q1460)
string(FIND "${Q6H_NATIVE_SOURCE}" "vec3 q1640PcOutput = colour;" Q1640_PC_OUTPUT_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "vec3(0.298999995, 0.587000012, 0.114)" Q1640_LUMA_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "vec3(0.7399142, 0.5749559, 0.3128335)" Q1640_TINT_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "glUniform1f(q1520TargetLumLocation, 1.2f);" Q1640_TARGET_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q15.14 PC HDR OUTPUT:" Q1640_LOG_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "std::string(label) == \"DIFFUSE\" ? GL_SRGB8_ALPHA8 : GL_RGBA8" Q1640_SRGB_BASEMAP_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "Q15.14" Q1640_TERRAIN_CHANGED)
string(FIND "${Q1640_Q4_SOURCE}" "text=Q15.14 anchor=left-hand" Q1640_LABEL_OK)
string(FIND "${Q1640_Q4_SOURCE}" "q1600Digit(q1600X, 0x66u)" Q1640_LABEL_FOUR_OK)
if(NOT Q1640_OLD_Q1460 EQUAL -1 OR Q1640_PC_OUTPUT_OK EQUAL -1 OR
   Q1640_LUMA_OK EQUAL -1 OR Q1640_TINT_OK EQUAL -1 OR
   Q1640_TARGET_OK EQUAL -1 OR Q1640_LOG_OK EQUAL -1 OR
   Q1640_SRGB_BASEMAP_OK EQUAL -1 OR NOT Q1640_TERRAIN_CHANGED EQUAL -1 OR
   Q1640_LABEL_OK EQUAL -1 OR Q1640_LABEL_FOUR_OK EQUAL -1)
    message(FATAL_ERROR
        "Q15.14 verification failed: oldQ1460=${Q1640_OLD_Q1460} pc=${Q1640_PC_OUTPUT_OK} luma=${Q1640_LUMA_OK} tint=${Q1640_TINT_OK} target=${Q1640_TARGET_OK} log=${Q1640_LOG_OK} srgbBase=${Q1640_SRGB_BASEMAP_OK} terrain=${Q1640_TERRAIN_CHANGED} label=${Q1640_LABEL_OK} four=${Q1640_LABEL_FOUR_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q15.14 captured PC SP17 HDR/ImageSpace output transform enabled")
