# Q16.11: startup stability guard for the Q16.10 phased transition pump.
#
# Q16.10 replaced ProcessQ74TransitionRequest globally. FalloutQuest also uses
# that renderer path during initial scene bootstrap, where the Fallout loading
# state is generation zero. The phased path is intended for user door loads,
# not bootstrap. Keep generation-zero startup on the exact legacy loader that
# was proven through Q16.9; loading generations 1+ retain the Q16.10 experiment.

set(Q1820_NATIVE_FILE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp")
if(NOT EXISTS "${Q1820_NATIVE_FILE}")
    message(FATAL_ERROR "Q16.11 expected final q6h native source")
endif()
file(READ "${Q1820_NATIVE_FILE}" Q1820_NATIVE_SOURCE)

set(Q1820_BOOT_OLD [==[
bool ProcessQ74TransitionRequest() {
    Q1800TransitionWork& work = gQ1800TransitionWork;

    if (work.stage == Q1800_STAGE_IDLE) {
]==])
set(Q1820_BOOT_NEW [==[
bool ProcessQ74TransitionRequest() {
    Q1800TransitionWork& work = gQ1800TransitionWork;

    // Generation zero is initial app bootstrap. Do not run the phased loader
    // here: preserve the Q16.9 startup path byte-for-byte. BeginFo3LoadingQ1700
    // advances generation for real door transitions before this function runs,
    // so generations 1+ continue into the phased state machine below.
    if (work.stage == Q1800_STAGE_IDLE &&
        GetFo3LoadingGenerationQ1700() == 0u) {
        static bool q1820StartupRouteLogged = false;
        if (!q1820StartupRouteLogged) {
            q1820StartupRouteLogged = true;
            Q6H_LOGI("Q16.11 STARTUP LOAD ROUTE: generation=0 loader=legacy-proven");
        }
        return ProcessQ74TransitionRequestLegacyQ1800();
    }

    if (work.stage == Q1800_STAGE_IDLE) {
]==])
string(FIND "${Q1820_NATIVE_SOURCE}" "${Q1820_BOOT_OLD}" Q1820_BOOT_POS)
if(Q1820_BOOT_POS EQUAL -1)
    message(FATAL_ERROR "Q16.11 could not find Q16.10 phased transition entry")
endif()
string(REPLACE "${Q1820_BOOT_OLD}" "${Q1820_BOOT_NEW}"
       Q1820_NATIVE_SOURCE "${Q1820_NATIVE_SOURCE}")
file(WRITE "${Q1820_NATIVE_FILE}" "${Q1820_NATIVE_SOURCE}")

# Visible build proof: Q16.10 -> Q16.11. Only the second seven-segment digit
# changes from 0 to 1; no UI/layout assets are altered.
set(Q1820_Q4_FILE "${CMAKE_CURRENT_BINARY_DIR}/q1800-q4-generated.cpp")
if(NOT EXISTS "${Q1820_Q4_FILE}")
    message(FATAL_ERROR "Q16.11 expected q1800 OpenXR source")
endif()
file(READ "${Q1820_Q4_FILE}" Q1820_Q4_SOURCE)

set(Q1820_LABEL_OLD [==[
        q1600Digit(q1600X, 0x06u); // Q16.10: 1 = B C
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x3Fu); // Q16.10: 0 = A B C D E F
]==])
set(Q1820_LABEL_NEW [==[
        q1600Digit(q1600X, 0x06u); // Q16.11: first 1 = B C
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x06u); // Q16.11: second 1 = B C
]==])
string(FIND "${Q1820_Q4_SOURCE}" "${Q1820_LABEL_OLD}" Q1820_LABEL_POS)
if(Q1820_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.11 could not find Q16.10 build-label digits")
endif()
string(REPLACE "${Q1820_LABEL_OLD}" "${Q1820_LABEL_NEW}"
       Q1820_Q4_SOURCE "${Q1820_Q4_SOURCE}")
string(REPLACE "Q16.10 BUILD LABEL:" "Q16.11 BUILD LABEL:"
       Q1820_Q4_SOURCE "${Q1820_Q4_SOURCE}")
string(REPLACE "text=Q16.10 anchor=left-hand" "text=Q16.11 anchor=left-hand"
       Q1820_Q4_SOURCE "${Q1820_Q4_SOURCE}")
string(REPLACE "Q16.10 AUTHORED DOOR FACING" "Q16.11 AUTHORED DOOR FACING"
       Q1820_Q4_SOURCE "${Q1820_Q4_SOURCE}")
file(WRITE "${Q1820_Q4_FILE}" "${Q1820_Q4_SOURCE}")

# Hard proof: startup bypass exists, the in-game phased path remains, and the
# asset-accurate Q16.9 interaction HUD / Q16.8 loading renderer survive.
string(FIND "${Q1820_NATIVE_SOURCE}" "Q16.11 STARTUP LOAD ROUTE:" Q1820_ROUTE_OK)
string(FIND "${Q1820_NATIVE_SOURCE}" "ProcessQ74TransitionRequestLegacyQ1800();" Q1820_LEGACY_OK)
string(FIND "${Q1820_NATIVE_SOURCE}" "Q16.10 PHASED LOAD BEGIN:" Q1820_PHASED_OK)
string(FIND "${Q1820_Q4_SOURCE}" "Q16.11: second 1 = B C" Q1820_LABEL_OK)
string(FIND "${Q1820_Q4_SOURCE}" "RenderFo3InteractionHudQ1790" Q1820_HUD_OK)
string(FIND "${Q1820_Q4_SOURCE}" "RenderFo3LoadingScreenQ1780" Q1820_LOADING_OK)
if(Q1820_ROUTE_OK EQUAL -1 OR Q1820_LEGACY_OK EQUAL -1 OR
   Q1820_PHASED_OK EQUAL -1 OR Q1820_LABEL_OK EQUAL -1 OR
   Q1820_HUD_OK EQUAL -1 OR Q1820_LOADING_OK EQUAL -1)
    message(FATAL_ERROR "Q16.11 startup-loader guard verification failed")
endif()

message(STATUS "Q16.11 startup stability enabled: generation 0 -> proven legacy loader; door generations -> phased loader")
