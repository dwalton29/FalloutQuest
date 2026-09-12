# Q16.10 authored door facing, applied one frame after a queued transition starts.
#
# q1699's historical input block resets playerYaw_ to zero immediately after A
# queues a door. Rewriting that old generated block proved brittle because later
# milestones transform the host repeatedly. q1800 already caches the aimed
# destination's XTEL rz and destination REFR. Use the loading generation instead:
# on the first loading frame after BeginFo3LoadingQ1700(), apply the authored
# heading exactly once while the loading presentation covers the world.

set(Q1810_Q4_FILE "${CMAKE_CURRENT_BINARY_DIR}/q1800-q4-generated.cpp")
if(NOT EXISTS "${Q1810_Q4_FILE}")
    message(FATAL_ERROR "Q16.10 facing expected q1800 OpenXR source")
endif()
file(READ "${Q1810_Q4_FILE}" Q1810_Q4_SOURCE)

# Track which loading generation has already consumed the cached destination
# heading. Generation zero is startup and is intentionally ignored.
set(Q1810_FIELD_OLD [==[
    uint32_t doorAimDestinationQ1800_{0u};
]==])
set(Q1810_FIELD_NEW [==[
    uint32_t doorAimDestinationQ1800_{0u};
    uint64_t doorFacingGenerationQ1810_{0u};
]==])
string(FIND "${Q1810_Q4_SOURCE}" "${Q1810_FIELD_OLD}" Q1810_FIELD_POS)
if(Q1810_FIELD_POS EQUAL -1)
    message(FATAL_ERROR "Q16.10 facing could not find q1800 cached destination field")
endif()
string(REPLACE "${Q1810_FIELD_OLD}" "${Q1810_FIELD_NEW}"
       Q1810_Q4_SOURCE "${Q1810_Q4_SOURCE}")

set(Q1810_METHOD [==[
    void ApplyAuthoredDoorFacingQ1810(const XrView& view) {
        if (!IsFo3LoadingVisibleQ1700()) return;

        const uint64_t generation = GetFo3LoadingGenerationQ1700();
        if (generation == 0u || generation == doorFacingGenerationQ1810_) return;

        // Consume this generation first so a malformed/missing cached target
        // cannot repeatedly alter the player's heading every loading frame.
        doorFacingGenerationQ1810_ = generation;
        if (doorAimDestinationQ1800_ == 0u) {
            FQ_LOGI("Q16.10 AUTHORED DOOR FACING SKIP: generation=%llu reason=no-cached-destination",
                    static_cast<unsigned long long>(generation));
            return;
        }

        playerPosition_ = {0.0f, 0.0f, 0.0f};
        const float physicalHeadYaw = YawFromQuaternion(view.pose.orientation);
        const float rawPlayerYaw = doorAimYawQ1800_ - physicalHeadYaw;
        playerYaw_ = std::atan2(std::sin(rawPlayerYaw), std::cos(rawPlayerYaw));
        lastFrameTime_ = 0;

        FQ_LOGI("Q16.10 AUTHORED DOOR FACING: destinationDoor=%08X XTELrz=%.4f physicalHeadYaw=%.4f playerYaw=%.4f degrees=%.1f generation=%llu",
                doorAimDestinationQ1800_, doorAimYawQ1800_,
                physicalHeadYaw, playerYaw_,
                playerYaw_ * 180.0f / PI,
                static_cast<unsigned long long>(generation));
    }

]==])
set(Q1810_RENDERFRAME_MARKER "    void RenderFrame() {")
string(FIND "${Q1810_Q4_SOURCE}" "${Q1810_RENDERFRAME_MARKER}" Q1810_RENDERFRAME_POS)
if(Q1810_RENDERFRAME_POS EQUAL -1)
    message(FATAL_ERROR "Q16.10 facing could not find RenderFrame")
endif()
string(REPLACE "${Q1810_RENDERFRAME_MARKER}"
       "${Q1810_METHOD}${Q1810_RENDERFRAME_MARKER}"
       Q1810_Q4_SOURCE "${Q1810_Q4_SOURCE}")

# q1700 calls UpdateDoorAim before the activation edge. On the activation frame
# generation has not advanced yet, so this is a no-op. On the next frame loading
# is visible, UpdateDoorAim preserves q1800's cached XTEL values, and this call
# sees the new generation and applies the authored heading once.
set(Q1810_AIM_CALL_OLD [==[
                UpdateDoorAimQ1700();
]==])
set(Q1810_AIM_CALL_NEW [==[
                UpdateDoorAimQ1700();
                ApplyAuthoredDoorFacingQ1810(views[0]);
]==])
string(FIND "${Q1810_Q4_SOURCE}" "${Q1810_AIM_CALL_OLD}" Q1810_AIM_CALL_POS)
if(Q1810_AIM_CALL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.10 facing could not find per-frame door-aim call")
endif()
string(REPLACE "${Q1810_AIM_CALL_OLD}" "${Q1810_AIM_CALL_NEW}"
       Q1810_Q4_SOURCE "${Q1810_Q4_SOURCE}")

file(WRITE "${Q1810_Q4_FILE}" "${Q1810_Q4_SOURCE}")

# q1800 already routed q6h-native-generated.cpp to q1800-q4-generated.cpp, so
# modifying that generated file in place is sufficient; no second include route
# is needed. Hard proof that both the phased loader and the live facing hook are
# present in the source the compiler will include.
string(FIND "${Q1810_Q4_SOURCE}" "ApplyAuthoredDoorFacingQ1810(views[0]);" Q1810_CALL_OK)
string(FIND "${Q1810_Q4_SOURCE}" "Q16.10 AUTHORED DOOR FACING:" Q1810_LOG_OK)
string(FIND "${Q1810_Q4_SOURCE}" "RenderFo3LoadingScreenQ1780" Q1810_LOADING_OK)
string(FIND "${Q1810_Q4_SOURCE}" "RenderFo3InteractionHudQ1790" Q1810_HUD_OK)
if(Q1810_CALL_OK EQUAL -1 OR Q1810_LOG_OK EQUAL -1 OR
   Q1810_LOADING_OK EQUAL -1 OR Q1810_HUD_OK EQUAL -1)
    message(FATAL_ERROR "Q16.10 authored door facing verification failed")
endif()

message(STATUS "Q16.10 authored XTEL facing enabled: apply cached destination rz once per loading generation")
