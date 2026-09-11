# Q16.0 input normalizer for the actually-live q4 Q7.1 host.
# The current q4 source still owns an analog right-trigger/value door probe.
# Convert that one action to a boolean right-A click and route its edge into the
# Q16.0 generic queued-door API. q1700 runs immediately after this patch and adds
# the aim HUD/loading presentation around the normalized input path.

set(Q1699_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1699_Q4_INPUT}")
    message(FATAL_ERROR "Q16.0 input normalizer expected final OpenXR source")
endif()
file(READ "${Q1699_Q4_INPUT}" Q1699_Q4_SOURCE)

set(Q1699_ACTION_OLD [==[
        if (!CreateAction("activate_value", "Activate", XR_ACTION_TYPE_FLOAT_INPUT,
                          1, &handPaths_[1], &activateAction_)) return false;
]==])
set(Q1699_ACTION_NEW [==[
        if (!CreateAction("activate_q160", "Activate", XR_ACTION_TYPE_BOOLEAN_INPUT,
                          1, &handPaths_[1], &activateAction_)) return false;
]==])
string(FIND "${Q1699_Q4_SOURCE}" "${Q1699_ACTION_OLD}" Q1699_ACTION_POS)
if(Q1699_ACTION_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 could not find live Q7.1 float activate action")
endif()
string(REPLACE "${Q1699_ACTION_OLD}" "${Q1699_ACTION_NEW}"
       Q1699_Q4_SOURCE "${Q1699_Q4_SOURCE}")

string(REPLACE "XrPath rightTriggerValue = XR_NULL_PATH;"
               "XrPath rightA = XR_NULL_PATH;"
               Q1699_Q4_SOURCE "${Q1699_Q4_SOURCE}")
string(REPLACE "/user/hand/right/input/trigger/value"
               "/user/hand/right/input/a/click"
               Q1699_Q4_SOURCE "${Q1699_Q4_SOURCE}")
string(REPLACE "&rightTriggerValue)) return false;"
               "&rightA)) return false;"
               Q1699_Q4_SOURCE "${Q1699_Q4_SOURCE}")
string(REPLACE "{activateAction_, rightTriggerValue},"
               "{activateAction_, rightA},"
               Q1699_Q4_SOURCE "${Q1699_Q4_SOURCE}")
string(REPLACE
    "Q7.1 Touch bindings attached: Q6K controls unchanged + right trigger/value probe"
    "Q16.0 Touch bindings attached: Q6K controls unchanged + right A/click activate"
    Q1699_Q4_SOURCE "${Q1699_Q4_SOURCE}")

set(Q1699_BOOL_READER [==[
    bool ReadBoolActionQ1699(XrAction action, XrPath subaction) {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        getInfo.subactionPath = subaction;
        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
        if (!CheckXr(xrGetActionStateBoolean(session_, &getInfo, &state),
                     "xrGetActionStateBoolean(Q16.0 A)")) return false;
        return state.isActive && state.currentState;
    }

]==])
set(Q1699_SYNC_MARKER "    void SyncInput(XrTime time) {")
string(FIND "${Q1699_Q4_SOURCE}" "${Q1699_SYNC_MARKER}" Q1699_SYNC_POS)
if(Q1699_SYNC_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 could not find live SyncInput")
endif()
string(REPLACE "${Q1699_SYNC_MARKER}"
       "${Q1699_BOOL_READER}${Q1699_SYNC_MARKER}"
       Q1699_Q4_SOURCE "${Q1699_Q4_SOURCE}")

set(Q1699_READ_OLD [==[
        activateValue_ = ReadFloatAction(activateAction_, handPaths_[1]);
]==])
set(Q1699_READ_NEW [==[
        activateDownQ1700_ = ReadBoolActionQ1699(activateAction_, handPaths_[1]);
]==])
string(FIND "${Q1699_Q4_SOURCE}" "${Q1699_READ_OLD}" Q1699_READ_POS)
if(Q1699_READ_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 could not find Q7.1 trigger value read")
endif()
string(REPLACE "${Q1699_READ_OLD}" "${Q1699_READ_NEW}"
       Q1699_Q4_SOURCE "${Q1699_Q4_SOURCE}")

# Replace the live Q7.1 special-house probe call, not merely the archival Q7
# helper. The old ProbeDoorQ71 method remains compiled but unreachable.
set(Q1699_PROBE_CALL_OLD [==[
                ProbeDoorQ71();
]==])
set(Q1699_PROBE_CALL_NEW [==[
                if (activateDownQ1700_ && !activateLatched_) {
                    activateLatched_ = true;
                    if (handPoseValid_[1]) {
                        const XrPosef handQ1700 = ToVirtualPose(handLocalPoses_[1]);
                        const Mat4 handMatrixQ1700 = MatrixFromPose(handQ1700);
                        if (ActivateFo3DoorQ1700(
                                handQ1700.position.x, handQ1700.position.y, handQ1700.position.z,
                                -handMatrixQ1700.m[8], -handMatrixQ1700.m[9], -handMatrixQ1700.m[10])) {
                            playerPosition_ = {0.0f, 0.0f, 0.0f};
                            playerYaw_ = 0.0f;
                            lastFrameTime_ = 0;
                            FQ_LOGI("Q16.0 player origin reset for queued authored XTEL");
                        }
                    }
                } else if (!activateDownQ1700_) {
                    activateLatched_ = false;
                }
]==])
string(FIND "${Q1699_Q4_SOURCE}" "${Q1699_PROBE_CALL_OLD}" Q1699_PROBE_POS)
if(Q1699_PROBE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 could not find live Q7.1 ProbeDoorQ71 call")
endif()
string(REPLACE "${Q1699_PROBE_CALL_OLD}" "${Q1699_PROBE_CALL_NEW}"
       Q1699_Q4_SOURCE "${Q1699_Q4_SOURCE}")

string(REPLACE
    "    float activateValue_{0.0f};"
    "    float activateValue_{0.0f}; // legacy Q7.1 method only; no longer sampled\n    bool activateDownQ1700_{false};"
    Q1699_Q4_SOURCE "${Q1699_Q4_SOURCE}")

string(FIND "${Q1699_Q4_SOURCE}" "XR_ACTION_TYPE_BOOLEAN_INPUT" Q1699_BOOL_OK)
string(FIND "${Q1699_Q4_SOURCE}" "/user/hand/right/input/a/click" Q1699_A_OK)
string(FIND "${Q1699_Q4_SOURCE}" "/user/hand/right/input/trigger/value" Q1699_OLD_TRIGGER)
string(FIND "${Q1699_Q4_SOURCE}" "ActivateFo3DoorQ1700(" Q1699_ACTIVATE_OK)
string(FIND "${Q1699_Q4_SOURCE}" "ProbeDoorQ71();" Q1699_OLD_PROBE)
if(Q1699_BOOL_OK EQUAL -1 OR Q1699_A_OK EQUAL -1 OR
   NOT Q1699_OLD_TRIGGER EQUAL -1 OR Q1699_ACTIVATE_OK EQUAL -1 OR
   NOT Q1699_OLD_PROBE EQUAL -1)
    message(FATAL_ERROR
        "Q16.0 live input verification failed: bool=${Q1699_BOOL_OK} A=${Q1699_A_OK} oldTrigger=${Q1699_OLD_TRIGGER} activate=${Q1699_ACTIVATE_OK} oldProbe=${Q1699_OLD_PROBE}")
endif()

file(WRITE "${Q1699_Q4_INPUT}" "${Q1699_Q4_SOURCE}")
message(STATUS "Q16.0 live OpenXR activation normalized: analog trigger probe -> boolean right A queued door")
