# Q15.15: test the remaining PC-vs-Quest output-domain mismatch.
#
# Q15.14 reproduced the captured PC call-4618221 final shader arithmetic, but
# then sRGB-decoded that numeric PC result before writing it to the preferred
# GL_SRGB8_ALPHA8 OpenXR swapchain, assuming the attachment/runtime would encode
# it back to the same code values. Device screenshots strongly contradict that
# assumption: Q15.14 is approximately as dark as an extra sRGB decode of the
# captured PC framebuffer.
#
# This patch changes ONE renderer operation only: keep the exact captured PC
# numeric output directly in the final colour value. All Q15.12 BaseMap sRGB
# decode, Q15.11/15.13 SP17 lighting, Q15.14 HDR/cinematic constants and fog stay
# intact. The existing final fragColor write downstream remains unchanged.

set(Q1650_OUTPUT_OLD [==[
            colour = Q1340SrgbToLinear(q1640PcOutput);
]==])
set(Q1650_OUTPUT_NEW [==[
            // Q15.15: preserve the exact numeric code value emitted by the PC
            // X8R8G8B8/SRGBWRITE=0 path. Do not apply a second transfer here.
            colour = q1640PcOutput;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1650_OUTPUT_OLD}" Q1650_OUTPUT_POS)
if(Q1650_OUTPUT_POS EQUAL -1)
    message(FATAL_ERROR "Q15.15 could not find Q15.14 inverse-sRGB final colour assignment")
endif()
string(REPLACE "${Q1650_OUTPUT_OLD}" "${Q1650_OUTPUT_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Make runtime log explicitly prove the output-domain change.
string(REPLACE
    "questSrgbCompensation=1"
    "questSrgbCompensation=0 directPcCodeWrite=1"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "Q15.14 PC HDR OUTPUT:"
    "Q15.15 PC HDR OUTPUT DOMAIN:"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Visible build identity: Q15.14 -> Q15.15. Seven-segment 5 = A F G C D = 0x6D.
set(Q1650_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1650_Q4_INPUT}")
    message(FATAL_ERROR "Q15.15 expected final OpenXR source at ${Q1650_Q4_INPUT}")
endif()
file(READ "${Q1650_Q4_INPUT}" Q1650_Q4_SOURCE)
string(REPLACE
    "q1600Digit(q1600X, 0x66u); // 4 = B C F G"
    "q1600Digit(q1600X, 0x6Du); // 5 = A F G C D"
    Q1650_Q4_SOURCE "${Q1650_Q4_SOURCE}")
string(REPLACE "Q15.14" "Q15.15" Q1650_Q4_SOURCE "${Q1650_Q4_SOURCE}")
file(WRITE "${Q1650_Q4_INPUT}" "${Q1650_Q4_SOURCE}")

# Hard guards: preserve every captured Q15.14 constant and Q15.12 BaseMap path,
# remove only the final inverse transfer, leave terrain untouched, and prove the
# headset label is Q15.15.
string(FIND "${Q6H_NATIVE_SOURCE}" "colour = q1640PcOutput;" Q1650_DIRECT_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "colour = Q1340SrgbToLinear(q1640PcOutput);" Q1650_OLD_WRITE)
string(FIND "${Q6H_NATIVE_SOURCE}" "vec3(0.7399142, 0.5749559, 0.3128335)" Q1650_TINT_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "glUniform1f(q1520TargetLumLocation, 1.2f);" Q1650_TARGET_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "std::string(label) == \"DIFFUSE\" ? GL_SRGB8_ALPHA8 : GL_RGBA8" Q1650_BASEMAP_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q15.15 PC HDR OUTPUT DOMAIN:" Q1650_LOG_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "Q15.15" Q1650_TERRAIN_CHANGED)
string(FIND "${Q1650_Q4_SOURCE}" "text=Q15.15 anchor=left-hand" Q1650_LABEL_OK)
string(FIND "${Q1650_Q4_SOURCE}" "q1600Digit(q1600X, 0x6Du)" Q1650_LABEL_FIVE_OK)
if(Q1650_DIRECT_OK EQUAL -1 OR NOT Q1650_OLD_WRITE EQUAL -1 OR
   Q1650_TINT_OK EQUAL -1 OR Q1650_TARGET_OK EQUAL -1 OR
   Q1650_BASEMAP_OK EQUAL -1 OR Q1650_LOG_OK EQUAL -1 OR
   NOT Q1650_TERRAIN_CHANGED EQUAL -1 OR Q1650_LABEL_OK EQUAL -1 OR
   Q1650_LABEL_FIVE_OK EQUAL -1)
    message(FATAL_ERROR
        "Q15.15 verification failed: direct=${Q1650_DIRECT_OK} old=${Q1650_OLD_WRITE} tint=${Q1650_TINT_OK} target=${Q1650_TARGET_OK} basemap=${Q1650_BASEMAP_OK} log=${Q1650_LOG_OK} terrain=${Q1650_TERRAIN_CHANGED} label=${Q1650_LABEL_OK} five=${Q1650_LABEL_FIVE_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q15.15 direct captured-PC output code values enabled; no final inverse sRGB transfer")
