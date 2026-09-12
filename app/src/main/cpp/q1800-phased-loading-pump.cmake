# Q16.10: keep OpenXR submitting loading frames while the destination scene is
# prepared. The old Q7.4 swap did placement parsing, every NIF build, every GL
# upload, collision init and final swap in one RenderScene() call; OpenXR could
# therefore only hold the last submitted frame and the spinner visibly froze.
#
# This pass preserves the same authored scene loaders and final swap semantics,
# but slices CPU mesh builds and GL uploads across render calls. Placement parse,
# collision init and terrain finalisation are still single operations and may
# cause a short hitch, but the multi-second monolithic freeze is removed.

set(Q1800_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1790-q4-generated.cpp")
if(NOT EXISTS "${Q1800_Q4_INPUT}")
    message(FATAL_ERROR "Q16.10 expected Q16.9 final OpenXR source at ${Q1800_Q4_INPUT}")
endif()
file(READ "${Q1800_Q4_INPUT}" Q1800_Q4_SOURCE)

# -----------------------------------------------------------------------------
# 1. Visible build proof: Q16.9 -> Q16.10. The original label had one digit after
#    Q16.; append a second seven-segment digit for 10.
# -----------------------------------------------------------------------------
set(Q1800_LABEL_OLD [==[
        q1600Digit(q1600X, 0x6Fu); // Q16.9: 9 = A B C D F G
]==])
set(Q1800_LABEL_NEW [==[
        q1600Digit(q1600X, 0x06u); // Q16.10: 1 = B C
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x3Fu); // Q16.10: 0 = A B C D E F
]==])
string(FIND "${Q1800_Q4_SOURCE}" "${Q1800_LABEL_OLD}" Q1800_LABEL_POS)
if(Q1800_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.10 could not find Q16.9 build-label digit")
endif()
string(REPLACE "${Q1800_LABEL_OLD}" "${Q1800_LABEL_NEW}"
       Q1800_Q4_SOURCE "${Q1800_Q4_SOURCE}")
string(REPLACE "Q16.9 BUILD LABEL:" "Q16.10 BUILD LABEL:"
       Q1800_Q4_SOURCE "${Q1800_Q4_SOURCE}")
string(REPLACE "text=Q16.9 anchor=left-hand" "text=Q16.10 anchor=left-hand"
       Q1800_Q4_SOURCE "${Q1800_Q4_SOURCE}")

# Fallout 3 XTEL records carry the authored destination rotation. Q7's historical
# host reset playerYaw_ to zero after every door, which is why entering Megaton
# could leave the player looking back at the gate. Cache the currently aimed
# destination's authored Z rotation and make the virtual head face that heading
# when activation succeeds. Subtracting the physical HMD yaw means this works
# regardless of which way the user is standing in their real room.
set(Q1800_AIM_OLD [==[
        Fo3DoorAimQ1700 aim;
        doorAimActiveQ1700_ = QueryFo3DoorAimQ1700(
            hand.position.x, hand.position.y, hand.position.z,
            -handMatrix.m[8], -handMatrix.m[9], -handMatrix.m[10], &aim) && aim.valid;
]==])
set(Q1800_AIM_NEW [==[
        Fo3DoorAimQ1700 aim;
        doorAimActiveQ1700_ = QueryFo3DoorAimQ1700(
            hand.position.x, hand.position.y, hand.position.z,
            -handMatrix.m[8], -handMatrix.m[9], -handMatrix.m[10], &aim) && aim.valid;
        if (doorAimActiveQ1700_) {
            doorAimYawQ1800_ = aim.rz;
            doorAimDestinationQ1800_ = aim.destinationDoorRef;
        }
]==])
string(FIND "${Q1800_Q4_SOURCE}" "${Q1800_AIM_OLD}" Q1800_AIM_POS)
if(Q1800_AIM_POS EQUAL -1)
    message(FATAL_ERROR "Q16.10 could not find Q16.9 door-aim update")
endif()
string(REPLACE "${Q1800_AIM_OLD}" "${Q1800_AIM_NEW}"
       Q1800_Q4_SOURCE "${Q1800_Q4_SOURCE}")

set(Q1800_FIELDS_OLD "    bool doorAimActiveQ1700_{false};")
set(Q1800_FIELDS_NEW [==[
    bool doorAimActiveQ1700_{false};
    float doorAimYawQ1800_{0.0f};
    uint32_t doorAimDestinationQ1800_{0u};
]==])
string(FIND "${Q1800_Q4_SOURCE}" "${Q1800_FIELDS_OLD}" Q1800_FIELDS_POS)
if(Q1800_FIELDS_POS EQUAL -1)
    message(FATAL_ERROR "Q16.10 could not find door-aim state field")
endif()
string(REPLACE "${Q1800_FIELDS_OLD}" "${Q1800_FIELDS_NEW}"
       Q1800_Q4_SOURCE "${Q1800_Q4_SOURCE}")

set(Q1800_RESET_OLD [==[
                            playerPosition_ = {0.0f, 0.0f, 0.0f};
                            playerYaw_ = 0.0f;
                            lastFrameTime_ = 0;
                            FQ_LOGI("Q7C player origin reset after authored door transition");
]==])
set(Q1800_RESET_NEW [==[
                            playerPosition_ = {0.0f, 0.0f, 0.0f};
                            const float q1800PhysicalHeadYaw =
                                YawFromQuaternion(views[0].pose.orientation);
                            const float q1800RawPlayerYaw =
                                doorAimYawQ1800_ - q1800PhysicalHeadYaw;
                            playerYaw_ = std::atan2(std::sin(q1800RawPlayerYaw),
                                                   std::cos(q1800RawPlayerYaw));
                            lastFrameTime_ = 0;
                            FQ_LOGI("Q16.10 AUTHORED DOOR FACING: destinationDoor=%08X XTELrz=%.4f physicalHeadYaw=%.4f playerYaw=%.4f degrees=%.1f",
                                    doorAimDestinationQ1800_, doorAimYawQ1800_,
                                    q1800PhysicalHeadYaw, playerYaw_,
                                    playerYaw_ * 180.0f / PI);
]==])
string(FIND "${Q1800_Q4_SOURCE}" "${Q1800_RESET_OLD}" Q1800_RESET_POS)
if(Q1800_RESET_POS EQUAL -1)
    message(FATAL_ERROR "Q16.10 could not find legacy door player-yaw reset")
endif()
string(REPLACE "${Q1800_RESET_OLD}" "${Q1800_RESET_NEW}"
       Q1800_Q4_SOURCE "${Q1800_Q4_SOURCE}")

set(Q1800_Q4_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/q1800-q4-generated.cpp")
file(WRITE "${Q1800_Q4_OUTPUT}" "${Q1800_Q4_SOURCE}")

# -----------------------------------------------------------------------------
# 2. Preserve the complete current renderer and park only the old monolithic
#    transition function under a legacy name. The first Q16.10 attempt sliced
#    from ProcessQ74TransitionRequest all the way to DrawSceneObject, but later
#    milestones had inserted unrelated lighting/blend/door helpers in that span.
#    Renaming + insertion keeps every one of those helpers byte-for-byte.
# -----------------------------------------------------------------------------
string(PREPEND Q6H_NATIVE_SOURCE "#include <chrono>\n")

set(Q1800_PROCESS_OLD "bool ProcessQ74TransitionRequest() {")
set(Q1800_PROCESS_LEGACY "bool ProcessQ74TransitionRequestLegacyQ1800() {")
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1800_PROCESS_OLD}" Q1800_PROCESS_POS)
if(Q1800_PROCESS_POS EQUAL -1)
    message(FATAL_ERROR "Q16.10 could not find Q7.4 monolithic transition function")
endif()
string(REPLACE "${Q1800_PROCESS_OLD}" "${Q1800_PROCESS_LEGACY}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1800_PHASED_PROCESS [==[
enum Q1800TransitionStage {
    Q1800_STAGE_IDLE = 0,
    Q1800_STAGE_LOAD_PLACEMENTS = 1,
    Q1800_STAGE_BUILD_CPU = 2,
    Q1800_STAGE_UPLOAD_GPU = 3,
    Q1800_STAGE_COLLISION = 4,
    Q1800_STAGE_SWAP = 5,
};

struct Q1800TransitionWork {
    Q1800TransitionStage stage = Q1800_STAGE_IDLE;
    Fo3CellTransitionRequestQ74 request{};
    std::vector<Fo3WorldPlacement> placements;
    std::vector<CpuObject> selected;
    std::vector<GpuObject> replacement;
    size_t placementIndex = 0u;
    size_t uploadIndex = 0u;
    size_t skipped = 0u;
    size_t unsupported = 0u;
    bool collisionReady = false;
};

Q1800TransitionWork gQ1800TransitionWork;

void Q1800AbortTransition(const char* reason) {
    Q74DeleteGpuObjects(gQ1800TransitionWork.replacement);
    Q6H_LOGE("Q16.10 PHASED LOAD FAILED: stage=%d cell=%08X reason=%s oldSceneRetained=1",
             static_cast<int>(gQ1800TransitionWork.stage),
             gQ1800TransitionWork.request.cellFormId,
             reason ? reason : "unknown");
    CancelFo3LoadingQ1700();
    gQ1800TransitionWork = {};
}

bool ProcessQ74TransitionRequest() {
    Q1800TransitionWork& work = gQ1800TransitionWork;

    if (work.stage == Q1800_STAGE_IDLE) {
        Fo3CellTransitionRequestQ74 request;
        if (!ConsumeFo3CellTransitionRequestQ74(request) || !request.valid) return false;

        work = {};
        work.request = request;
        work.stage = Q1800_STAGE_LOAD_PLACEMENTS;
        MarkFo3TransitionWorkStartedQ1700();
        Q6H_LOGI("Q16.10 PHASED LOAD BEGIN: door=%08X cell=%08X worldspace=%08X loadingFrames=%u",
                 request.destinationDoorRef, request.cellFormId, request.worldspaceFormId,
                 GetFo3LoadingPresentedFramesQ1700());
        return true;
    }

    if (work.stage == Q1800_STAGE_LOAD_PLACEMENTS) {
        Q6H_LOGI("Q16.10 LOAD STAGE: cell=%08X phase=placements begin",
                 work.request.cellFormId);
        if (!LoadFo3CellPlacementsQ74(work.request.cellFormId, work.placements)) {
            Q1800AbortTransition("cell-loader");
            return false;
        }
        work.selected.reserve(work.placements.size() * 2u);
        work.stage = Q1800_STAGE_BUILD_CPU;
        Q6H_LOGI("Q16.10 LOAD STAGE: cell=%08X phase=placements ready count=%zu",
                 work.request.cellFormId, work.placements.size());
        return true;
    }

    if (work.stage == Q1800_STAGE_BUILD_CPU) {
        const auto sliceStart = std::chrono::steady_clock::now();
        size_t processedThisSlice = 0u;
        while (work.placementIndex < work.placements.size()) {
            const Fo3WorldPlacement& placement = work.placements[work.placementIndex++];
            ++processedThisSlice;

            if (Q74ShouldSkipPlacement(placement)) {
                ++work.skipped;
            } else {
                std::vector<CpuObject> parts;
                if (!BuildCpuObjects(placement, parts)) {
                    ++work.unsupported;
                } else {
                    for (CpuObject& part : parts) work.selected.push_back(std::move(part));
                }
            }

            const float elapsedMs = std::chrono::duration<float, std::milli>(
                std::chrono::steady_clock::now() - sliceStart).count();
            if (processedThisSlice >= 12u || elapsedMs >= 3.0f) break;
        }

        if (work.placementIndex >= work.placements.size()) {
            if (work.selected.size() < 2u) {
                Q1800AbortTransition("cpu-build-too-small");
                return false;
            }
            work.replacement.reserve(work.selected.size());
            work.stage = Q1800_STAGE_UPLOAD_GPU;
            Q6H_LOGI("Q16.10 LOAD STAGE: cell=%08X phase=cpu ready drawShapes=%zu skipped=%zu unsupported=%zu",
                     work.request.cellFormId, work.selected.size(),
                     work.skipped, work.unsupported);
        }
        return true;
    }

    if (work.stage == Q1800_STAGE_UPLOAD_GPU) {
        const auto sliceStart = std::chrono::steady_clock::now();
        size_t uploadedThisSlice = 0u;
        while (work.uploadIndex < work.selected.size()) {
            CpuObject& cpu = work.selected[work.uploadIndex++];
            GpuObject gpu;
            if (UploadCpuObject(cpu,
                                work.request.x, work.request.y, work.request.z,
                                gpu)) {
                work.replacement.push_back(std::move(gpu));
            } else {
                if (gpu.vbo) glDeleteBuffers(1, &gpu.vbo);
                if (gpu.vao) glDeleteVertexArrays(1, &gpu.vao);
            }
            ++uploadedThisSlice;

            const float elapsedMs = std::chrono::duration<float, std::milli>(
                std::chrono::steady_clock::now() - sliceStart).count();
            if (uploadedThisSlice >= 8u || elapsedMs >= 3.0f) break;
        }

        if (work.uploadIndex >= work.selected.size()) {
            if (work.replacement.size() < 2u) {
                Q1800AbortTransition("gpu-upload-too-small");
                return false;
            }
            work.stage = Q1800_STAGE_COLLISION;
            Q6H_LOGI("Q16.10 LOAD STAGE: cell=%08X phase=gpu ready objects=%zu",
                     work.request.cellFormId, work.replacement.size());
        }
        return true;
    }

    if (work.stage == Q1800_STAGE_COLLISION) {
        std::vector<Fo3WorldPlacement> collisionPlacements;
        collisionPlacements.reserve(work.selected.size());
        for (const CpuObject& cpu : work.selected) {
            collisionPlacements.push_back(cpu.placement);
        }

        work.collisionReady = InitializeFo3CollisionOverlay(
            collisionPlacements,
            work.request.x, work.request.y, work.request.z,
            SCENE_FORWARD, FLOOR_Y, FO3_UNITS_PER_METRE);
        work.stage = Q1800_STAGE_SWAP;
        Q6H_LOGI("Q16.10 LOAD STAGE: cell=%08X phase=collision ready=%d",
                 work.request.cellFormId, work.collisionReady ? 1 : 0);
        return true;
    }

    if (work.stage == Q1800_STAGE_SWAP) {
        const Fo3CellTransitionRequestQ74 request = work.request;
        const size_t skipped = work.skipped;
        const size_t unsupported = work.unsupported;
        const bool collisionReady = work.collisionReady;
        const size_t oldObjects = gObjects.size();

        Q74DeleteGpuObjects(gObjects);
        gObjects = std::move(work.replacement);
        gSceneReady = !gObjects.empty();
        gLoggedFirstDraw = false;

        size_t triangles = 0u;
        size_t realDiffuse = 0u;
        size_t realNormal = 0u;
        for (const GpuObject& object : gObjects) {
            triangles += static_cast<size_t>(object.vertexCount / 3);
            if (object.realDiffuse) ++realDiffuse;
            if (object.realNormal) ++realNormal;
        }

        // Q16.0's completion hook advances WORK -> POST and requests the player
        // origin reset. POST remains visible until one destination frame reaches
        // xrEndFrame, so the new world cannot tear through mid-frame.
        CompleteFo3CellTransitionQ74(request.cellFormId);
        gCurrentCellFormId = request.cellFormId;

        Q6H_LOGI("Q16.10 PHASED LOAD COMPLETE: cell=%08X worldspace=%08X oldObjects=%zu newObjects=%zu triangles=%zu realDiffuse=%zu realNormal=%zu skipped=%zu unsupported=%zu collisionReady=%d loading=POST",
                 request.cellFormId, request.worldspaceFormId,
                 oldObjects, gObjects.size(), triangles, realDiffuse, realNormal,
                 skipped, unsupported, collisionReady ? 1 : 0);

        work = {};
        return true;
    }

    Q1800AbortTransition("invalid-stage");
    return false;
}

]==])

set(Q1800_DRAW_MARKER "void DrawSceneObject(const GpuObject& object) {")
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1800_DRAW_MARKER}" Q1800_DRAW_POS)
if(Q1800_DRAW_POS EQUAL -1)
    message(FATAL_ERROR "Q16.10 could not find DrawSceneObject insertion anchor")
endif()
string(REPLACE "${Q1800_DRAW_MARKER}"
       "${Q1800_PHASED_PROCESS}${Q1800_DRAW_MARKER}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# 3. Route the final native TU to the Q16.10 OpenXR host and freeze it.
# -----------------------------------------------------------------------------
set(Q1800_Q6H_INCLUDE_OLD
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/q1790-q4-generated.cpp\"")
set(Q1800_Q6H_INCLUDE_NEW
    "#include \"${Q1800_Q4_OUTPUT}\"")
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1800_Q6H_INCLUDE_OLD}" Q1800_ROUTE_POS)
if(Q1800_ROUTE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.10 could not find q1790 include in final q6h native source")
endif()
string(REPLACE "${Q1800_Q6H_INCLUDE_OLD}" "${Q1800_Q6H_INCLUDE_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")

# Hard configure-time proof. Q16.10 changes transition scheduling/lifetime and
# uses the authored XTEL heading; Q16.9's real interaction assets and Q16.8's
# current loading renderer remain intact.
string(FIND "${Q6H_NATIVE_SOURCE}" "ProcessQ74TransitionRequestLegacyQ1800" Q1800_LEGACY_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q16.10 PHASED LOAD BEGIN:" Q1800_BEGIN_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q16.10 PHASED LOAD COMPLETE:" Q1800_COMPLETE_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "MarkFo3TransitionWorkStartedQ1700" Q1800_WORK_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q1590UploadPcSp17LightConstants" Q1800_LIGHT_HELPER_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q1580LogLightTrace" Q1800_TRACE_HELPER_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q1150BlendFactor" Q1800_BLEND_HELPER_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "QueryDoorInternalQ1700" Q1800_DOOR_HELPER_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "q1800-q4-generated.cpp" Q1800_ROUTE_OK)
string(FIND "${Q1800_Q4_SOURCE}" "Q16.10: 1 = B C" Q1800_LABEL_OK)
string(FIND "${Q1800_Q4_SOURCE}" "Q16.10 AUTHORED DOOR FACING:" Q1800_FACING_OK)
string(FIND "${Q1800_Q4_SOURCE}" "RenderFo3InteractionHudQ1790" Q1800_HUD_OK)
string(FIND "${Q1800_Q4_SOURCE}" "RenderFo3LoadingScreenQ1780" Q1800_LOADING_OK)
if(Q1800_LEGACY_OK EQUAL -1 OR Q1800_BEGIN_OK EQUAL -1 OR
   Q1800_COMPLETE_OK EQUAL -1 OR Q1800_WORK_OK EQUAL -1 OR
   Q1800_LIGHT_HELPER_OK EQUAL -1 OR Q1800_TRACE_HELPER_OK EQUAL -1 OR
   Q1800_BLEND_HELPER_OK EQUAL -1 OR Q1800_DOOR_HELPER_OK EQUAL -1 OR
   Q1800_ROUTE_OK EQUAL -1 OR Q1800_LABEL_OK EQUAL -1 OR
   Q1800_FACING_OK EQUAL -1 OR Q1800_HUD_OK EQUAL -1 OR
   Q1800_LOADING_OK EQUAL -1)
    message(FATAL_ERROR "Q16.10 phased-loading/facing verification failed")
endif()

message(STATUS "Q16.10 phased loading enabled: 30-frame dwell + sliced NIF/GL work + authored XTEL door facing")
