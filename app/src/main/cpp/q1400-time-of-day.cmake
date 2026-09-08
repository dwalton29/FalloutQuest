# Q14.0: authored Fallout 3 time-of-day runtime plus a temporary Quest test clock.
#
# The environment values come from CLMT/WTHR/IMAD/XEMI in Fallout3.esm. For
# testing only, the left Touch trigger advances the otherwise-frozen clock at up
# to two game-hours per real second. Right-trigger activation remains untouched.

# Make the runtime available to the final OpenXR host translation unit.
string(REPLACE
    "#include \"fo3-authored-color-q1390.h\""
    "#include \"fo3-authored-color-q1390.h\"\n#include \"fo3-time-of-day-q1400.h\""
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(FIND "${Q6H_NATIVE_SOURCE}" "fo3-time-of-day-q1400.h" Q1400_NATIVE_INCLUDE_OK)
if(Q1400_NATIVE_INCLUDE_OK EQUAL -1)
    message(FATAL_ERROR "Q14.0 could not insert time-of-day runtime include")
endif()

# Q12.8 owns the final eye/OpenXR source; Q13.3/Q13.9 have already rewritten the
# same generated file. Patch it last so the control cannot silently land on an
# obsolete q4 source.
set(Q1400_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1400_Q4_INPUT}")
    message(FATAL_ERROR "Q14.0 expected final Q12.8 eye source at ${Q1400_Q4_INPUT}")
endif()
file(READ "${Q1400_Q4_INPUT}" Q1400_Q4_SOURCE)

# ----- Create the left-trigger float action. -----
set(Q1400_ACTION_CURRENT [=[
        if (!CreateAction("activate_value", "Activate", XR_ACTION_TYPE_FLOAT_INPUT,
                          1, &handPaths_[1], &activateAction_)) return false;
]=])
set(Q1400_ACTION_CURRENT_NEW [=[
        if (!CreateAction("activate_value", "Activate", XR_ACTION_TYPE_FLOAT_INPUT,
                          1, &handPaths_[1], &activateAction_)) return false;
        if (!CreateAction("time_advance", "Advance Time", XR_ACTION_TYPE_FLOAT_INPUT,
                          1, &handPaths_[0], &timeAdvanceAction_)) return false;
]=])
string(FIND "${Q1400_Q4_SOURCE}" "${Q1400_ACTION_CURRENT}" Q1400_ACTION_CURRENT_POS)
if(NOT Q1400_ACTION_CURRENT_POS EQUAL -1)
    string(REPLACE "${Q1400_ACTION_CURRENT}" "${Q1400_ACTION_CURRENT_NEW}"
           Q1400_Q4_SOURCE "${Q1400_Q4_SOURCE}")
else()
    # Older generated Q7 path used a boolean right-trigger action. Keep a
    # fallback so this milestone fails only for genuine source drift.
    set(Q1400_ACTION_BOOL [=[
        if (!CreateAction("activate", "Activate", XR_ACTION_TYPE_BOOLEAN_INPUT,
                          1, &handPaths_[1], &activateAction_)) return false;
]=])
    set(Q1400_ACTION_BOOL_NEW [=[
        if (!CreateAction("activate", "Activate", XR_ACTION_TYPE_BOOLEAN_INPUT,
                          1, &handPaths_[1], &activateAction_)) return false;
        if (!CreateAction("time_advance", "Advance Time", XR_ACTION_TYPE_FLOAT_INPUT,
                          1, &handPaths_[0], &timeAdvanceAction_)) return false;
]=])
    string(FIND "${Q1400_Q4_SOURCE}" "${Q1400_ACTION_BOOL}" Q1400_ACTION_BOOL_POS)
    if(Q1400_ACTION_BOOL_POS EQUAL -1)
        message(FATAL_ERROR "Q14.0 could not find right-trigger action creation anchor")
    endif()
    string(REPLACE "${Q1400_ACTION_BOOL}" "${Q1400_ACTION_BOOL_NEW}"
           Q1400_Q4_SOURCE "${Q1400_Q4_SOURCE}")
endif()

# ----- Bind /user/hand/left/input/trigger/value alongside existing controls. -----
set(Q1400_BIND_CURRENT [=[
        XrPath rightTriggerValue = XR_NULL_PATH;
        if (!Path("/interaction_profiles/oculus/touch_controller", &profile) ||
            !Path("/user/hand/left/input/aim/pose", &leftAim) ||
            !Path("/user/hand/right/input/aim/pose", &rightAim) ||
            !Path("/user/hand/left/input/thumbstick/x", &leftStickX) ||
            !Path("/user/hand/left/input/thumbstick/y", &leftStickY) ||
            !Path("/user/hand/right/input/thumbstick/x", &rightStickX) ||
            !Path("/user/hand/right/input/trigger/value", &rightTriggerValue)) return false;

        const std::array<XrActionSuggestedBinding, 6> bindings{{
            {poseAction_, leftAim},
            {poseAction_, rightAim},
            {moveXAction_, leftStickX},
            {moveYAction_, leftStickY},
            {turnXAction_, rightStickX},
            {activateAction_, rightTriggerValue},
        }};
]=])
set(Q1400_BIND_CURRENT_NEW [=[
        XrPath rightTriggerValue = XR_NULL_PATH;
        XrPath leftTimeTriggerValue = XR_NULL_PATH;
        if (!Path("/interaction_profiles/oculus/touch_controller", &profile) ||
            !Path("/user/hand/left/input/aim/pose", &leftAim) ||
            !Path("/user/hand/right/input/aim/pose", &rightAim) ||
            !Path("/user/hand/left/input/thumbstick/x", &leftStickX) ||
            !Path("/user/hand/left/input/thumbstick/y", &leftStickY) ||
            !Path("/user/hand/right/input/thumbstick/x", &rightStickX) ||
            !Path("/user/hand/right/input/trigger/value", &rightTriggerValue) ||
            !Path("/user/hand/left/input/trigger/value", &leftTimeTriggerValue)) return false;

        const std::array<XrActionSuggestedBinding, 7> bindings{{
            {poseAction_, leftAim},
            {poseAction_, rightAim},
            {moveXAction_, leftStickX},
            {moveYAction_, leftStickY},
            {turnXAction_, rightStickX},
            {activateAction_, rightTriggerValue},
            {timeAdvanceAction_, leftTimeTriggerValue},
        }};
]=])
string(FIND "${Q1400_Q4_SOURCE}" "${Q1400_BIND_CURRENT}" Q1400_BIND_CURRENT_POS)
if(NOT Q1400_BIND_CURRENT_POS EQUAL -1)
    string(REPLACE "${Q1400_BIND_CURRENT}" "${Q1400_BIND_CURRENT_NEW}"
           Q1400_Q4_SOURCE "${Q1400_Q4_SOURCE}")
else()
    set(Q1400_BIND_BOOL [=[
        XrPath rightStickX = XR_NULL_PATH;
        XrPath rightTrigger = XR_NULL_PATH;
        if (!Path("/interaction_profiles/oculus/touch_controller", &profile) ||
            !Path("/user/hand/left/input/aim/pose", &leftAim) ||
            !Path("/user/hand/right/input/aim/pose", &rightAim) ||
            !Path("/user/hand/left/input/thumbstick/x", &leftStickX) ||
            !Path("/user/hand/left/input/thumbstick/y", &leftStickY) ||
            !Path("/user/hand/right/input/thumbstick/x", &rightStickX) ||
            !Path("/user/hand/right/input/trigger/click", &rightTrigger)) return false;

        const std::array<XrActionSuggestedBinding, 6> bindings{{
            {poseAction_, leftAim},
            {poseAction_, rightAim},
            {moveXAction_, leftStickX},
            {moveYAction_, leftStickY},
            {turnXAction_, rightStickX},
            {activateAction_, rightTrigger},
        }};
]=])
    set(Q1400_BIND_BOOL_NEW [=[
        XrPath rightStickX = XR_NULL_PATH;
        XrPath rightTrigger = XR_NULL_PATH;
        XrPath leftTimeTriggerValue = XR_NULL_PATH;
        if (!Path("/interaction_profiles/oculus/touch_controller", &profile) ||
            !Path("/user/hand/left/input/aim/pose", &leftAim) ||
            !Path("/user/hand/right/input/aim/pose", &rightAim) ||
            !Path("/user/hand/left/input/thumbstick/x", &leftStickX) ||
            !Path("/user/hand/left/input/thumbstick/y", &leftStickY) ||
            !Path("/user/hand/right/input/thumbstick/x", &rightStickX) ||
            !Path("/user/hand/right/input/trigger/click", &rightTrigger) ||
            !Path("/user/hand/left/input/trigger/value", &leftTimeTriggerValue)) return false;

        const std::array<XrActionSuggestedBinding, 7> bindings{{
            {poseAction_, leftAim},
            {poseAction_, rightAim},
            {moveXAction_, leftStickX},
            {moveYAction_, leftStickY},
            {turnXAction_, rightStickX},
            {activateAction_, rightTrigger},
            {timeAdvanceAction_, leftTimeTriggerValue},
        }};
]=])
    string(FIND "${Q1400_Q4_SOURCE}" "${Q1400_BIND_BOOL}" Q1400_BIND_BOOL_POS)
    if(Q1400_BIND_BOOL_POS EQUAL -1)
        message(FATAL_ERROR "Q14.0 could not find Touch binding block")
    endif()
    string(REPLACE "${Q1400_BIND_BOOL}" "${Q1400_BIND_BOOL_NEW}"
           Q1400_Q4_SOURCE "${Q1400_Q4_SOURCE}")
endif()

# ----- Sample the analog trigger once after xrSyncActions. -----
set(Q1400_SYNC_ANCHOR [=[
        turnX_ = Deadzone(ReadFloatAction(turnXAction_, handPaths_[1]));
]=])
set(Q1400_SYNC_NEW [=[
        turnX_ = Deadzone(ReadFloatAction(turnXAction_, handPaths_[1]));
        timeAdvanceValue_ = ReadFloatAction(timeAdvanceAction_, handPaths_[0]);
]=])
string(FIND "${Q1400_Q4_SOURCE}" "${Q1400_SYNC_ANCHOR}" Q1400_SYNC_POS)
if(Q1400_SYNC_POS EQUAL -1)
    message(FATAL_ERROR "Q14.0 could not find SyncInput turn action anchor")
endif()
string(REPLACE "${Q1400_SYNC_ANCHOR}" "${Q1400_SYNC_NEW}"
       Q1400_Q4_SOURCE "${Q1400_Q4_SOURCE}")

# ----- Advance/apply the authored clock once per stereo frame before drawing. -----
set(Q1400_FRAME_ANCHOR [=[
                SyncInput(frameState.predictedDisplayTime);
]=])
set(Q1400_FRAME_NEW [=[
                SyncInput(frameState.predictedDisplayTime);
                UpdateFo3TimeOfDayQ1400(timeAdvanceValue_,
                    static_cast<int64_t>(frameState.predictedDisplayTime));
]=])
string(FIND "${Q1400_Q4_SOURCE}" "${Q1400_FRAME_ANCHOR}" Q1400_FRAME_POS)
if(Q1400_FRAME_POS EQUAL -1)
    message(FATAL_ERROR "Q14.0 could not find per-frame input anchor")
endif()
string(REPLACE "${Q1400_FRAME_ANCHOR}" "${Q1400_FRAME_NEW}"
       Q1400_Q4_SOURCE "${Q1400_Q4_SOURCE}")

# ----- Class state. -----
string(REPLACE
    "    XrAction activateAction_{XR_NULL_HANDLE};"
    "    XrAction activateAction_{XR_NULL_HANDLE};\n    XrAction timeAdvanceAction_{XR_NULL_HANDLE};"
    Q1400_Q4_SOURCE "${Q1400_Q4_SOURCE}")
string(REPLACE
    "    float activateValue_{0.0f};"
    "    float activateValue_{0.0f};\n    float timeAdvanceValue_{0.0f};"
    Q1400_Q4_SOURCE "${Q1400_Q4_SOURCE}")
# Boolean-activation generated sources do not have activateValue_. Fall back to
# the stable turn member if the float member replacement did not fire.
string(FIND "${Q1400_Q4_SOURCE}" "float timeAdvanceValue_{0.0f};" Q1400_VALUE_MEMBER_OK)
if(Q1400_VALUE_MEMBER_OK EQUAL -1)
    string(REPLACE
        "    float turnX_{0.0f};"
        "    float turnX_{0.0f};\n    float timeAdvanceValue_{0.0f};"
        Q1400_Q4_SOURCE "${Q1400_Q4_SOURCE}")
endif()

# Hard drift guards: action creation, Touch binding, analog sampling and the
# once-per-stereo-frame update must all be present together.
string(FIND "${Q1400_Q4_SOURCE}" "time_advance" Q1400_ACTION_OK)
string(FIND "${Q1400_Q4_SOURCE}" "/user/hand/left/input/trigger/value" Q1400_BIND_OK)
string(FIND "${Q1400_Q4_SOURCE}" "timeAdvanceValue_ = ReadFloatAction" Q1400_SAMPLE_OK)
string(FIND "${Q1400_Q4_SOURCE}" "UpdateFo3TimeOfDayQ1400(timeAdvanceValue_" Q1400_UPDATE_OK)
string(FIND "${Q1400_Q4_SOURCE}" "XrAction timeAdvanceAction_{XR_NULL_HANDLE};" Q1400_MEMBER_OK)
string(FIND "${Q1400_Q4_SOURCE}" "float timeAdvanceValue_{0.0f};" Q1400_VALUE_OK)
if(Q1400_ACTION_OK EQUAL -1 OR Q1400_BIND_OK EQUAL -1 OR
   Q1400_SAMPLE_OK EQUAL -1 OR Q1400_UPDATE_OK EQUAL -1 OR
   Q1400_MEMBER_OK EQUAL -1 OR Q1400_VALUE_OK EQUAL -1)
    message(FATAL_ERROR
        "Q14.0 input hook drifted: action=${Q1400_ACTION_OK} bind=${Q1400_BIND_OK} sample=${Q1400_SAMPLE_OK} update=${Q1400_UPDATE_OK} member=${Q1400_MEMBER_OK} value=${Q1400_VALUE_OK}")
endif()

file(WRITE "${Q1400_Q4_INPUT}" "${Q1400_Q4_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")

message(STATUS "Q14.0 authored CLMT/WTHR/IMAD/XEMI time of day + left-trigger fast-forward enabled")
