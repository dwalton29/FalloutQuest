# Q16.11: restore the mature transition function as the live loader.
#
# Q16.10 rewrote ProcessQ74TransitionRequest as a phased state machine. That
# broke an important project rule: later milestones had already patched the
# mature function with authored environment/weather/ImageSpace/fog/material
# hooks. The rewritten copy did not carry those behaviours, so the final scene
# could differ visually and in runtime state.
#
# Do not recreate those hooks here. Park the Q16.10 experiment under a separate
# name and restore the exact fully-patched pre-Q16.10 function as the public
# ProcessQ74TransitionRequest used by startup and door loads.

set(Q1820_NATIVE_FILE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp")
if(NOT EXISTS "${Q1820_NATIVE_FILE}")
    message(FATAL_ERROR "Q16.11 expected final q6h native source")
endif()
file(READ "${Q1820_NATIVE_FILE}" Q1820_NATIVE_SOURCE)

# Rename the new phased implementation first, then restore the mature legacy
# declaration to the public symbol. Ordering matters so string replacement does
# not rename the restored declaration a second time.
set(Q1820_PHASED_DECL "bool ProcessQ74TransitionRequest() {")
set(Q1820_PHASED_PARKED "bool ProcessQ74TransitionRequestExperimentalQ1800() {")
string(FIND "${Q1820_NATIVE_SOURCE}" "${Q1820_PHASED_DECL}" Q1820_PHASED_POS)
if(Q1820_PHASED_POS EQUAL -1)
    message(FATAL_ERROR "Q16.11 could not find Q16.10 phased transition declaration")
endif()
string(REPLACE "${Q1820_PHASED_DECL}" "${Q1820_PHASED_PARKED}"
       Q1820_NATIVE_SOURCE "${Q1820_NATIVE_SOURCE}")

set(Q1820_LEGACY_DECL "bool ProcessQ74TransitionRequestLegacyQ1800() {")
set(Q1820_LIVE_DECL "bool ProcessQ74TransitionRequest() {")
string(FIND "${Q1820_NATIVE_SOURCE}" "${Q1820_LEGACY_DECL}" Q1820_LEGACY_POS)
if(Q1820_LEGACY_POS EQUAL -1)
    message(FATAL_ERROR "Q16.11 could not find mature Q16.10 legacy transition")
endif()
string(REPLACE "${Q1820_LEGACY_DECL}" "${Q1820_LIVE_DECL}"
       Q1820_NATIVE_SOURCE "${Q1820_NATIVE_SOURCE}")

# Runtime proof. This sits in the live mature function's existing scene-swap
# begin log by changing only the message text, not any behaviour.
string(REPLACE "Q7.4 SCENE SWAP BEGIN:"
               "Q16.11 MATURE SCENE SWAP BEGIN:"
               Q1820_NATIVE_SOURCE "${Q1820_NATIVE_SOURCE}")

file(WRITE "${Q1820_NATIVE_FILE}" "${Q1820_NATIVE_SOURCE}")

# Visible build proof: Q16.10 -> Q16.11. Only the second seven-segment digit
# changes from 0 to 1; authored UI/layout remains elsewhere.
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

# Hard proof that the live loader is the mature implementation, not the phased
# copy, and that critical authored-environment logic patched into it is present.
string(FIND "${Q1820_NATIVE_SOURCE}" "bool ProcessQ74TransitionRequest() {" Q1820_LIVE_OK)
string(FIND "${Q1820_NATIVE_SOURCE}" "ProcessQ74TransitionRequestExperimentalQ1800" Q1820_PARKED_OK)
string(FIND "${Q1820_NATIVE_SOURCE}" "Q16.11 MATURE SCENE SWAP BEGIN:" Q1820_RUNTIME_OK)
string(FIND "${Q1820_NATIVE_SOURCE}" "LoadFo3CellEnvironmentQ1410(" Q1820_ENV_OK)
string(FIND "${Q1820_NATIVE_SOURCE}" "Q16.10 PHASED LOAD BEGIN:" Q1820_EXPERIMENT_OK)
string(FIND "${Q1820_Q4_SOURCE}" "Q16.11: second 1 = B C" Q1820_LABEL_OK)
string(FIND "${Q1820_Q4_SOURCE}" "RenderFo3InteractionHudQ1790" Q1820_HUD_OK)
string(FIND "${Q1820_Q4_SOURCE}" "RenderFo3LoadingScreenQ1780" Q1820_LOADING_OK)
if(Q1820_LIVE_OK EQUAL -1 OR Q1820_PARKED_OK EQUAL -1 OR
   Q1820_RUNTIME_OK EQUAL -1 OR Q1820_ENV_OK EQUAL -1 OR
   Q1820_EXPERIMENT_OK EQUAL -1 OR Q1820_LABEL_OK EQUAL -1 OR
   Q1820_HUD_OK EQUAL -1 OR Q1820_LOADING_OK EQUAL -1)
    message(FATAL_ERROR "Q16.11 mature-loader restore verification failed")
endif()

message(STATUS "Q16.11 mature transition restored: authored environment hooks preserved; Q16.10 phased loader parked")
