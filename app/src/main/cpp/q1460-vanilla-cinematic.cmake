# Q14.6: match Fallout 3's compiled ISCinematic pixel-shader math.
#
# The original shader packages give us a concrete reference for this stage.
# ISCinematic uses BT.601-style luminance weights (0.299, 0.587, 0.114), then
# applies saturation -> tint -> Fade -> brightness -> contrast. FalloutQuest's
# Q13.4 path instead inserted an explicit linear<->sRGB transfer and applied
# contrast before tint/brightness. Q14.6 removes that guessed transfer from the
# active path and restores the compiled-shader order. The legacy Q1340 helper
# functions are left in generated source for downstream patch compatibility but
# are no longer called by the active cinematic path.
#
# ISCinematic also contains a Fade blend between tint and brightness. The current
# ESM-driven FalloutQuest post state has no Fade input and Megaton's normal world
# path does not author one here, so Q14.6 leaves that step neutral rather than
# inventing a renderer-side value.

set(Q1460_OLD_CINEMATIC [==[
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
]==])

set(Q1460_NEW_CINEMATIC [==[
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

string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1460_OLD_CINEMATIC}" Q1460_OLD_POS)
if(Q1460_OLD_POS EQUAL -1)
    message(FATAL_ERROR "Q14.6 could not find active Q13.4 cinematic block")
endif()
string(REPLACE "${Q1460_OLD_CINEMATIC}" "${Q1460_NEW_CINEMATIC}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Correct the runtime diagnostic so screenshots/logs prove which math is active.
string(REPLACE
    "cinematicDomain=srgb"
    "cinematicDomain=vanilla-native isc=ISCinematic weights=0.299/0.587/0.114 order=sat-tint-brightness-contrast fade=neutral"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Drift guards: the active block must use the compiled shader weights/order and
# must not enter Q13.4's guessed transfer helpers.
string(FIND "${Q6H_NATIVE_SOURCE}" "float cinematicLumQ1460 = dot(cinematic, vec3(0.299, 0.587, 0.114));" Q1460_LUM_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "vec3 tintedQ1460 = cinematicLumQ1460 * max(uTintColor" Q1460_TINT_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "vec3 cinematic = Q1340LinearToSrgb(colour);" Q1460_OLD_ENTRY)
string(FIND "${Q6H_NATIVE_SOURCE}" "colour = Q1340SrgbToLinear(max(cinematic" Q1460_OLD_EXIT)
string(FIND "${Q6H_NATIVE_SOURCE}" "cinematicDomain=vanilla-native" Q1460_LOG_OK)
if(Q1460_LUM_OK EQUAL -1 OR Q1460_TINT_OK EQUAL -1 OR Q1460_LOG_OK EQUAL -1)
    message(FATAL_ERROR "Q14.6 vanilla cinematic verification failed: lum=${Q1460_LUM_OK} tint=${Q1460_TINT_OK} log=${Q1460_LOG_OK}")
endif()
if(NOT Q1460_OLD_ENTRY EQUAL -1 OR NOT Q1460_OLD_EXIT EQUAL -1)
    message(FATAL_ERROR "Q14.6 guessed Q13.4 transfer is still active: entry=${Q1460_OLD_ENTRY} exit=${Q1460_OLD_EXIT}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q14.6 compiled ISCinematic ordering and luminance semantics enabled")
