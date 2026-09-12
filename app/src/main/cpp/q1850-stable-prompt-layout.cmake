# Q16.13: stable aim-loss state + exact vanilla whitespace handling.
#
# Q16.12 still crashed when the interaction ray stopped hitting the gate. This
# pass removes heap-backed prompt mutation from that transition entirely and also
# fixes a concrete FNT layout bug discovered from the user's vanilla font:
# Baked-in_Monofonto_Large's non-rendering space has raw advance=0 and stores its
# authored 13px word gap in the whitespace metric slot. The Q16.12 renderer was
# therefore collapsing every word boundary.
#
# Keep the mature Q16.11 scene loader untouched. This file patches only the final
# OpenXR interaction state and routes drawing to fo3-interaction-hud-q1850.h.

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fo3-interaction-hud-q1850.h")
    message(FATAL_ERROR "Q16.13 missing fo3-interaction-hud-q1850.h")
endif()

set(Q1850_Q4_FILE "${CMAKE_CURRENT_BINARY_DIR}/q1800-q4-generated.cpp")
if(NOT EXISTS "${Q1850_Q4_FILE}")
    message(FATAL_ERROR "Q16.13 expected final q1800 OpenXR source")
endif()
file(READ "${Q1850_Q4_FILE}" Q1850_Q4_SOURCE)

# Add the fixed-storage renderer and std::array support after Q16.12's include.
set(Q1850_INCLUDE_OLD [==[
#include "fo3-interaction-hud-q1840.h"
#include <string>
]==])
set(Q1850_INCLUDE_NEW [==[
#include "fo3-interaction-hud-q1840.h"
#include "fo3-interaction-hud-q1850.h"
#include <array>
#include <string>
]==])
string(FIND "${Q1850_Q4_SOURCE}" "${Q1850_INCLUDE_OLD}" Q1850_INCLUDE_POS)
if(Q1850_INCLUDE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.13 could not find Q16.12 HUD include block")
endif()
string(REPLACE "${Q1850_INCLUDE_OLD}" "${Q1850_INCLUDE_NEW}"
       Q1850_Q4_SOURCE "${Q1850_Q4_SOURCE}")

# Replace the host's heap-backed prompt with storage whose address never changes.
set(Q1850_FIELDS_OLD [==[
    uint32_t doorAimDestinationQ1800_{0u};
    uint32_t doorAimSourceQ1840_{0u};
    std::string doorPromptQ1840_;
]==])
set(Q1850_FIELDS_NEW [==[
    uint32_t doorAimDestinationQ1800_{0u};
    uint32_t doorAimSourceQ1840_{0u};
    std::array<char, 512> doorPromptQ1850_{};
    bool doorPromptValidQ1850_{false};
]==])
string(FIND "${Q1850_Q4_SOURCE}" "${Q1850_FIELDS_OLD}" Q1850_FIELDS_POS)
if(Q1850_FIELDS_POS EQUAL -1)
    message(FATAL_ERROR "Q16.13 could not find Q16.12 prompt state fields")
endif()
string(REPLACE "${Q1850_FIELDS_OLD}" "${Q1850_FIELDS_NEW}"
       Q1850_Q4_SOURCE "${Q1850_Q4_SOURCE}")

# Q16.0 reset doorAimActiveQ1700_ at the top of UpdateDoorAimQ1700(). That made
# Q16.12's q1840WasActive always false and prevented a real active->inactive
# transition from being represented coherently. Preserve previous-frame state
# until the new query result exists. Invalid hand/loading exits also perform one
# explicit state-only hide without touching the prompt bytes.
set(Q1850_AIM_ENTRY_OLD [==[
    void UpdateDoorAimQ1700() {
        doorAimActiveQ1700_ = false;
        if (IsFo3LoadingVisibleQ1700() || !handPoseValid_[1]) return;
]==])
set(Q1850_AIM_ENTRY_NEW [==[
    void UpdateDoorAimQ1700() {
        if (IsFo3LoadingVisibleQ1700() || !handPoseValid_[1]) {
            if (doorAimActiveQ1700_) {
                FQ_LOGI("Q16.13 HUD TARGET HIDE: sourceDoor=%08X reason=%s promptStorage=fixed-retained",
                        doorAimSourceQ1840_,
                        IsFo3LoadingVisibleQ1700() ? "loading" : "invalid-hand-pose");
            }
            doorAimActiveQ1700_ = false;
            doorAimSourceQ1840_ = 0u;
            doorPromptValidQ1850_ = false;
            return;
        }
]==])
string(FIND "${Q1850_Q4_SOURCE}" "${Q1850_AIM_ENTRY_OLD}" Q1850_AIM_ENTRY_POS)
if(Q1850_AIM_ENTRY_POS EQUAL -1)
    message(FATAL_ERROR "Q16.13 could not find UpdateDoorAimQ1700 entry reset")
endif()
string(REPLACE "${Q1850_AIM_ENTRY_OLD}" "${Q1850_AIM_ENTRY_NEW}"
       Q1850_Q4_SOURCE "${Q1850_Q4_SOURCE}")

set(Q1850_AIM_OLD [==[
        Fo3DoorAimQ1700 aim;
        const bool q1840WasActive = doorAimActiveQ1700_;
        const bool q1840NextActive = QueryFo3DoorAimQ1700(
            hand.position.x, hand.position.y, hand.position.z,
            -handMatrix.m[8], -handMatrix.m[9], -handMatrix.m[10], &aim) && aim.valid;

        if (q1840NextActive) {
            // Keep the authored destination/yaw cache for the post-A facing fix.
            doorAimYawQ1800_ = aim.rz;
            doorAimDestinationQ1800_ = aim.destinationDoorRef;

            if (doorAimSourceQ1840_ != aim.sourceDoorRef || doorPromptQ1840_.empty()) {
                char q1840Prompt[512]{};
                if (GetFo3DoorPromptQ1840(aim.sourceDoorRef,
                                          q1840Prompt,
                                          sizeof(q1840Prompt))) {
                    doorPromptQ1840_ = q1840Prompt;
                } else {
                    // No invented fallback wording. Missing authored data hides
                    // the text and leaves an explicit diagnostic.
                    doorPromptQ1840_.clear();
                    FQ_LOGE("Q16.12 HUD PROMPT MISS: sourceDoor=%08X destinationDoor=%08X",
                            aim.sourceDoorRef, aim.destinationDoorRef);
                }
                doorAimSourceQ1840_ = aim.sourceDoorRef;
            }

            if (!q1840WasActive) {
                FQ_LOGI("Q16.12 HUD TARGET SHOW: sourceDoor=%08X destinationDoor=%08X text=%s",
                        aim.sourceDoorRef, aim.destinationDoorRef,
                        doorPromptQ1840_.empty() ? "<missing>" : doorPromptQ1840_.c_str());
            }
        } else {
            if (q1840WasActive) {
                FQ_LOGI("Q16.12 HUD TARGET HIDE: sourceDoor=%08X resourcesRetained=1",
                        doorAimSourceQ1840_);
            }
            // Hide is state-only: do not destroy/reinitialize any GL resource.
            doorAimSourceQ1840_ = 0u;
            doorPromptQ1840_.clear();
        }
        doorAimActiveQ1700_ = q1840NextActive;
]==])
set(Q1850_AIM_NEW [==[
        Fo3DoorAimQ1700 aim{};
        const bool q1850WasActive = doorAimActiveQ1700_;
        const bool q1850NextActive = QueryFo3DoorAimQ1700(
            hand.position.x, hand.position.y, hand.position.z,
            -handMatrix.m[8], -handMatrix.m[9], -handMatrix.m[10], &aim) && aim.valid;

        if (q1850NextActive) {
            // Keep the authored destination/yaw cache for the post-A facing fix.
            doorAimYawQ1800_ = aim.rz;
            doorAimDestinationQ1800_ = aim.destinationDoorRef;

            if (doorAimSourceQ1840_ != aim.sourceDoorRef || !doorPromptValidQ1850_) {
                std::array<char, 512> q1850Prompt{};
                if (GetFo3DoorPromptQ1840(aim.sourceDoorRef,
                                          q1850Prompt.data(),
                                          q1850Prompt.size())) {
                    // Fixed-size copy only when the target changes. On aim loss
                    // these bytes are deliberately retained and never mutated.
                    std::memcpy(doorPromptQ1850_.data(),
                                q1850Prompt.data(),
                                doorPromptQ1850_.size());
                    doorPromptValidQ1850_ = true;
                } else {
                    doorPromptValidQ1850_ = false;
                    FQ_LOGE("Q16.13 HUD PROMPT MISS: sourceDoor=%08X destinationDoor=%08X",
                            aim.sourceDoorRef, aim.destinationDoorRef);
                }
                doorAimSourceQ1840_ = aim.sourceDoorRef;
            }

            if (!q1850WasActive) {
                FQ_LOGI("Q16.13 HUD TARGET SHOW: sourceDoor=%08X destinationDoor=%08X text=%s promptStorage=fixed",
                        aim.sourceDoorRef, aim.destinationDoorRef,
                        doorPromptValidQ1850_ ? doorPromptQ1850_.data() : "<missing>");
            }
        } else {
            if (q1850WasActive) {
                FQ_LOGI("Q16.13 HUD TARGET HIDE: sourceDoor=%08X reason=ray-miss promptStorage=fixed-retained",
                        doorAimSourceQ1840_);
            }
            // Aim loss is now three scalar writes. Do NOT clear/memset/free the
            // prompt buffer, and do not touch HUD GL resources.
            doorAimSourceQ1840_ = 0u;
            doorPromptValidQ1850_ = false;
        }
        doorAimActiveQ1700_ = q1850NextActive;
]==])
string(FIND "${Q1850_Q4_SOURCE}" "${Q1850_AIM_OLD}" Q1850_AIM_POS)
if(Q1850_AIM_POS EQUAL -1)
    message(FATAL_ERROR "Q16.13 could not find Q16.12 live aim block")
endif()
string(REPLACE "${Q1850_AIM_OLD}" "${Q1850_AIM_NEW}"
       Q1850_Q4_SOURCE "${Q1850_Q4_SOURCE}")

# Route the live draw to the no-per-frame-string Q16.13 renderer.
set(Q1850_DRAW_OLD "RenderFo3InteractionHudQ1840(mvp.m, doorPromptQ1840_.c_str());")
set(Q1850_DRAW_NEW [==[
if (doorPromptValidQ1850_) {
                            RenderFo3InteractionHudQ1850(mvp.m, doorPromptQ1850_.data());
                        }
]==])
string(FIND "${Q1850_Q4_SOURCE}" "${Q1850_DRAW_OLD}" Q1850_DRAW_POS)
if(Q1850_DRAW_POS EQUAL -1)
    message(FATAL_ERROR "Q16.13 could not find Q16.12 live HUD draw")
endif()
string(REPLACE "${Q1850_DRAW_OLD}" "${Q1850_DRAW_NEW}"
       Q1850_Q4_SOURCE "${Q1850_Q4_SOURCE}")

# Visible headset proof: Q16.12 -> Q16.13.
set(Q1850_LABEL_OLD [==[
        q1600Digit(q1600X, 0x06u); // Q16.12: 1 = B C
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x5Bu); // Q16.12: 2 = A B G E D
]==])
set(Q1850_LABEL_NEW [==[
        q1600Digit(q1600X, 0x06u); // Q16.13: 1 = B C
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x4Fu); // Q16.13: 3 = A B C D G
]==])
string(FIND "${Q1850_Q4_SOURCE}" "${Q1850_LABEL_OLD}" Q1850_LABEL_POS)
if(Q1850_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.13 could not find Q16.12 build-label digits")
endif()
string(REPLACE "${Q1850_LABEL_OLD}" "${Q1850_LABEL_NEW}"
       Q1850_Q4_SOURCE "${Q1850_Q4_SOURCE}")
string(REPLACE "Q16.12 BUILD LABEL:" "Q16.13 BUILD LABEL:"
       Q1850_Q4_SOURCE "${Q1850_Q4_SOURCE}")
string(REPLACE "text=Q16.12 anchor=left-hand" "text=Q16.13 anchor=left-hand"
       Q1850_Q4_SOURCE "${Q1850_Q4_SOURCE}")
string(REPLACE "Q16.12 AUTHORED DOOR FACING" "Q16.13 AUTHORED DOOR FACING"
       Q1850_Q4_SOURCE "${Q1850_Q4_SOURCE}")

file(WRITE "${Q1850_Q4_FILE}" "${Q1850_Q4_SOURCE}")

# Proof that aim loss no longer performs heap-backed text mutation and that the
# mature environment-aware loader survives untouched in the renderer TU.
set(Q1850_NATIVE_FILE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp")
if(NOT EXISTS "${Q1850_NATIVE_FILE}")
    message(FATAL_ERROR "Q16.13 expected final q6h native source")
endif()
file(READ "${Q1850_NATIVE_FILE}" Q1850_NATIVE_SOURCE)

string(FIND "${Q1850_Q4_SOURCE}" "std::array<char, 512> doorPromptQ1850_" Q1850_FIXED_OK)
string(FIND "${Q1850_Q4_SOURCE}" "Q16.13 HUD TARGET HIDE:" Q1850_HIDE_OK)
string(FIND "${Q1850_Q4_SOURCE}" "RenderFo3InteractionHudQ1850" Q1850_DRAW_OK)
string(FIND "${Q1850_Q4_SOURCE}" "doorPromptQ1840_.clear()" Q1850_CLEAR_BAD)
string(FIND "${Q1850_Q4_SOURCE}" "std::string doorPromptQ1840_" Q1850_STRING_BAD)
string(FIND "${Q1850_Q4_SOURCE}" "Q16.13: 3 = A B C D G" Q1850_LABEL_OK)
string(FIND "${Q1850_NATIVE_SOURCE}" "Q16.11 MATURE SCENE SWAP BEGIN:" Q1850_MATURE_OK)
string(FIND "${Q1850_NATIVE_SOURCE}" "LoadFo3CellEnvironmentQ1410(" Q1850_ENV_OK)
if(Q1850_FIXED_OK EQUAL -1 OR Q1850_HIDE_OK EQUAL -1 OR
   Q1850_DRAW_OK EQUAL -1 OR NOT Q1850_CLEAR_BAD EQUAL -1 OR
   NOT Q1850_STRING_BAD EQUAL -1 OR Q1850_LABEL_OK EQUAL -1 OR
   Q1850_MATURE_OK EQUAL -1 OR Q1850_ENV_OK EQUAL -1)
    message(FATAL_ERROR "Q16.13 fixed-prompt/aim-loss verification failed")
endif()

message(STATUS "Q16.13 interaction enabled: fixed prompt storage + coherent active->inactive state + vanilla FNT whitespace metric")
