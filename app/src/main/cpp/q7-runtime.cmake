# Q7 builds on the Q6H/Q6K generated renderer but makes the CELL runtime mutable.
# Keep this separate from the older milestone transforms so the door/transition
# code can be removed or replaced without destabilising the proven renderer.

# ----- Scene renderer: generic CELL + door metadata + reload -----
string(REPLACE
    "bool gLoggedFirstDraw = false;"
    "bool gLoggedFirstDraw = false;\nuint32_t gCurrentCellFormId = 0x000151E3u;\nfloat gSceneCenterX = 0.0f;\nfloat gSceneCenterY = 0.0f;\nfloat gSceneFloorZ = 0.0f;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q7_OLD_GPU_FIELDS [=[
    uint32_t refFormId = 0;
    uint32_t baseFormId = 0;
    std::string editorId;
    std::string modelPath;
};
]=])
set(Q7_NEW_GPU_FIELDS [=[
    uint32_t refFormId = 0;
    uint32_t baseFormId = 0;
    std::string editorId;
    std::string modelPath;
    std::string baseRecordType;
    Fo3DoorTeleport teleport;
    float minX = 0.0f, maxX = 0.0f;
    float minY = 0.0f, maxY = 0.0f;
    float minZ = 0.0f, maxZ = 0.0f;
};
]=])
string(REPLACE "${Q7_OLD_GPU_FIELDS}" "${Q7_NEW_GPU_FIELDS}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q7_OLD_EXPANDED [=[
    std::vector<float> expanded;
    expanded.reserve(cpu.mesh.indices.size() * FLOATS_PER_VERTEX);

    for (uint32_t index : cpu.mesh.indices) {
]=])
set(Q7_NEW_EXPANDED [=[
    std::vector<float> expanded;
    expanded.reserve(cpu.mesh.indices.size() * FLOATS_PER_VERTEX);
    Vec3 objectMinimum{1e30f, 1e30f, 1e30f};
    Vec3 objectMaximum{-1e30f, -1e30f, -1e30f};

    for (uint32_t index : cpu.mesh.indices) {
]=])
string(REPLACE "${Q7_OLD_EXPANDED}" "${Q7_NEW_EXPANDED}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q7_OLD_MAPPED_POINT [=[
        const Vec3 n = GameDirectionToOpenXr(cpu.normalsGame[index]);
]=])
set(Q7_NEW_MAPPED_POINT [=[
        objectMinimum.x = std::min(objectMinimum.x, p.x);
        objectMinimum.y = std::min(objectMinimum.y, p.y);
        objectMinimum.z = std::min(objectMinimum.z, p.z);
        objectMaximum.x = std::max(objectMaximum.x, p.x);
        objectMaximum.y = std::max(objectMaximum.y, p.y);
        objectMaximum.z = std::max(objectMaximum.z, p.z);
        const Vec3 n = GameDirectionToOpenXr(cpu.normalsGame[index]);
]=])
string(REPLACE "${Q7_OLD_MAPPED_POINT}" "${Q7_NEW_MAPPED_POINT}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q7_OLD_GPU_META [=[
    gpu.editorId = cpu.placement.editorId;
    gpu.modelPath = cpu.placement.modelPath;
]=])
set(Q7_NEW_GPU_META [=[
    gpu.editorId = cpu.placement.editorId;
    gpu.modelPath = cpu.placement.modelPath;
    gpu.baseRecordType = cpu.placement.baseRecordType;
    gpu.teleport = cpu.placement.teleport;
    gpu.minX = objectMinimum.x; gpu.maxX = objectMaximum.x;
    gpu.minY = objectMinimum.y; gpu.maxY = objectMaximum.y;
    gpu.minZ = objectMinimum.z; gpu.maxZ = objectMaximum.z;
]=])
string(REPLACE "${Q7_OLD_GPU_META}" "${Q7_NEW_GPU_META}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "bool InitializeScene() {"
    "bool InitializeSceneForCell(uint32_t cellFormId, const Fo3CellArrival* forcedArrival) {"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "LoadMegatonPlayerHousePlacements(placements)"
    "LoadFo3CellPlacements(cellFormId, placements)"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "Q6H CELL SOURCE: cell=000151E3 modelPlacements=%zu"
    "Q7A CELL SOURCE: cell=%08X modelPlacements=%zu"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "placements.size(), centroidX, centroidY, centroidZ"
    "cellFormId, placements.size(), centroidX, centroidY, centroidZ"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q7_OLD_ARRIVAL [=[
    Fo3CellArrival q6kArrival;
    const bool q6kArrivalReady = LoadMegatonPlayerHouseArrival(q6kArrival) && q6kArrival.valid;
]=])
set(Q7_NEW_ARRIVAL [=[
    Fo3CellArrival q7Arrival;
    bool q7ArrivalReady = false;
    if (forcedArrival && forcedArrival->valid) {
        q7Arrival = *forcedArrival;
        q7ArrivalReady = true;
    } else {
        q7ArrivalReady = LoadFo3CellArrival(cellFormId, q7Arrival) && q7Arrival.valid;
    }
]=])
string(REPLACE "${Q7_OLD_ARRIVAL}" "${Q7_NEW_ARRIVAL}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE "q6kArrivalReady" "q7ArrivalReady"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE "q6kArrival" "q7Arrival"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q7_OLD_PROGRAM_CREATE [=[
    gProgram = CreateQ6HProgram();
    if (!gProgram) return false;
]=])
set(Q7_NEW_PROGRAM_CREATE [=[
    if (!gProgram) {
        gProgram = CreateQ6HProgram();
        if (!gProgram) return false;
    }
]=])
string(REPLACE "${Q7_OLD_PROGRAM_CREATE}" "${Q7_NEW_PROGRAM_CREATE}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q7_OLD_COLLISION_INIT [=[
    std::vector<Fo3WorldPlacement> q6hCollisionPlacements;
]=])
set(Q7_NEW_COLLISION_INIT [=[
    gSceneCenterX = centerX;
    gSceneCenterY = centerY;
    gSceneFloorZ = floorZ;
    std::vector<Fo3WorldPlacement> q6hCollisionPlacements;
]=])
string(REPLACE "${Q7_OLD_COLLISION_INIT}" "${Q7_NEW_COLLISION_INIT}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q7_OLD_SCENE_RETURN [=[
    } else {
        Q6H_LOGE("Q6H FAILED: GPU scene objects=%zu", gObjects.size());
    }
    return gSceneReady;
}
]=])
set(Q7_NEW_SCENE_RETURN [=[
    } else {
        Q6H_LOGE("Q6H FAILED: GPU scene objects=%zu", gObjects.size());
    }
    if (gSceneReady) {
        gCurrentCellFormId = cellFormId;
        Q6H_LOGI("Q7A RUNTIME CELL READY: cell=%08X objects=%zu originGame=(%.2f %.2f %.2f) forcedArrival=%d",
                 gCurrentCellFormId, gObjects.size(), gSceneCenterX, gSceneCenterY,
                 gSceneFloorZ, forcedArrival && forcedArrival->valid ? 1 : 0);
    }
    return gSceneReady;
}
]=])
string(REPLACE "${Q7_OLD_SCENE_RETURN}" "${Q7_NEW_SCENE_RETURN}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q7_RUNTIME_HELPERS [=[
void ClearSceneForQ7Reload() {
    ShutdownFo3CollisionOverlay();
    for (GpuObject& object : gObjects) {
        if (object.vbo) glDeleteBuffers(1, &object.vbo);
        if (object.vao) glDeleteVertexArrays(1, &object.vao);
    }
    gObjects.clear();
    gSceneReady = false;
    gLoggedFirstDraw = false;
}

bool RayAabbQ7(float ox, float oy, float oz,
               float dx, float dy, float dz,
               const GpuObject& object, float& outT) {
    constexpr float PAD = 0.08f;
    const float mins[3]{object.minX-PAD, object.minY-PAD, object.minZ-PAD};
    const float maxs[3]{object.maxX+PAD, object.maxY+PAD, object.maxZ+PAD};
    const float origins[3]{ox, oy, oz};
    const float dirs[3]{dx, dy, dz};
    float tMin = 0.0f;
    float tMax = 3.0f;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(dirs[axis]) < 1e-6f) {
            if (origins[axis] < mins[axis] || origins[axis] > maxs[axis]) return false;
            continue;
        }
        float t1 = (mins[axis] - origins[axis]) / dirs[axis];
        float t2 = (maxs[axis] - origins[axis]) / dirs[axis];
        if (t1 > t2) std::swap(t1, t2);
        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax) return false;
    }
    outT = tMin;
    return tMax >= 0.0f && tMin <= 3.0f;
}

bool ActivateDoorInternalQ7(float ox, float oy, float oz,
                            float dx, float dy, float dz) {
    const float len = std::sqrt(dx*dx + dy*dy + dz*dz);
    if (len < 1e-5f || !gSceneReady) return false;
    dx /= len; dy /= len; dz /= len;

    const GpuObject* hit = nullptr;
    float bestT = 3.0f;
    for (const GpuObject& object : gObjects) {
        if (object.baseRecordType != "DOOR" || !object.teleport.valid) continue;
        float t = 0.0f;
        if (RayAabbQ7(ox, oy, oz, dx, dy, dz, object, t) && t < bestT) {
            bestT = t;
            hit = &object;
        }
    }
    if (!hit) {
        Q6H_LOGI("Q7B ACTIVATE MISS: cell=%08X maxDistance=3.0m", gCurrentCellFormId);
        return false;
    }

    const Fo3DoorTeleport link = hit->teleport;
    Q6H_LOGI("Q7B DOOR ACTIVATE: sourceCell=%08X sourceDoor=%08X EDID=%s distance=%.2f destinationDoor=%08X destinationCell=%08X XTEL=(%.2f %.2f %.2f)",
             gCurrentCellFormId, hit->refFormId,
             hit->editorId.empty() ? "<none>" : hit->editorId.c_str(), bestT,
             link.destinationDoorRefFormId, link.destinationCellFormId,
             link.x, link.y, link.z);

    Fo3CellArrival arrival;
    arrival.sourceDoorRefFormId = hit->refFormId;
    arrival.destinationDoorRefFormId = link.destinationDoorRefFormId;
    arrival.destinationCellFormId = link.destinationCellFormId;
    arrival.flags = link.flags;
    arrival.x = link.x; arrival.y = link.y; arrival.z = link.z;
    arrival.rx = link.rx; arrival.ry = link.ry; arrival.rz = link.rz;
    arrival.valid = true;

    const uint32_t oldCell = gCurrentCellFormId;
    ClearSceneForQ7Reload();
    if (!InitializeSceneForCell(link.destinationCellFormId, &arrival)) {
        Q6H_LOGE("Q7C TRANSITION FAILED: from=%08X to=%08X; restoring previous cell",
                 oldCell, link.destinationCellFormId);
        ClearSceneForQ7Reload();
        Fo3CellArrival oldArrival;
        const Fo3CellArrival* oldArrivalPtr =
            LoadFo3CellArrival(oldCell, oldArrival) && oldArrival.valid ? &oldArrival : nullptr;
        InitializeSceneForCell(oldCell, oldArrivalPtr);
        return false;
    }

    Q6H_LOGI("Q7C TRANSITION READY: from=%08X to=%08X destinationDoor=%08X authoredXTEL=1 terrainLANDPending=%d",
             oldCell, gCurrentCellFormId, link.destinationDoorRefFormId,
             gCurrentCellFormId == 0x000151E3u ? 0 : 1);
    return true;
}

bool InitializeScene() {
    return InitializeSceneForCell(gCurrentCellFormId, nullptr);
}

]=])
string(REPLACE
    "void Q6HGenFramebuffers(GLsizei n, GLuint* framebuffers) {"
    "${Q7_RUNTIME_HELPERS}void Q6HGenFramebuffers(GLsizei n, GLuint* framebuffers) {"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q7_OLD_NAMESPACE_END [=[
} // namespace

#define glGenFramebuffers Q6HGenFramebuffers
]=])
set(Q7_NEW_NAMESPACE_END [=[
} // namespace

bool ActivateFo3DoorQ7(float originX, float originY, float originZ,
                       float dirX, float dirY, float dirZ) {
    return ActivateDoorInternalQ7(originX, originY, originZ, dirX, dirY, dirZ);
}

#define glGenFramebuffers Q6HGenFramebuffers
]=])
string(REPLACE "${Q7_OLD_NAMESPACE_END}" "${Q7_NEW_NAMESPACE_END}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# ----- Q4 OpenXR host: right-trigger activation -----
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/q4-native.cpp" Q7_Q4_SOURCE)
string(REPLACE
    "#include \"fo3-collision-overlay.h\""
    "#include \"fo3-collision-overlay.h\"\n#include \"fo3-door-runtime.h\""
    Q7_Q4_SOURCE "${Q7_Q4_SOURCE}")

set(Q7_OLD_ACTION_CREATE [=[
        if (!CreateAction("turn_x", "Turn X", XR_ACTION_TYPE_FLOAT_INPUT,
                          1, &handPaths_[1], &turnXAction_)) return false;
]=])
set(Q7_NEW_ACTION_CREATE [=[
        if (!CreateAction("turn_x", "Turn X", XR_ACTION_TYPE_FLOAT_INPUT,
                          1, &handPaths_[1], &turnXAction_)) return false;
        if (!CreateAction("activate", "Activate", XR_ACTION_TYPE_BOOLEAN_INPUT,
                          1, &handPaths_[1], &activateAction_)) return false;
]=])
string(REPLACE "${Q7_OLD_ACTION_CREATE}" "${Q7_NEW_ACTION_CREATE}"
       Q7_Q4_SOURCE "${Q7_Q4_SOURCE}")

set(Q7_OLD_PATHS [=[
        XrPath rightStickX = XR_NULL_PATH;
        if (!Path("/interaction_profiles/oculus/touch_controller", &profile) ||
            !Path("/user/hand/left/input/aim/pose", &leftAim) ||
            !Path("/user/hand/right/input/aim/pose", &rightAim) ||
            !Path("/user/hand/left/input/thumbstick/x", &leftStickX) ||
            !Path("/user/hand/left/input/thumbstick/y", &leftStickY) ||
            !Path("/user/hand/right/input/thumbstick/x", &rightStickX)) return false;

        const std::array<XrActionSuggestedBinding, 5> bindings{{
            {poseAction_, leftAim},
            {poseAction_, rightAim},
            {moveXAction_, leftStickX},
            {moveYAction_, leftStickY},
            {turnXAction_, rightStickX},
        }};
]=])
set(Q7_NEW_PATHS [=[
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
string(REPLACE "${Q7_OLD_PATHS}" "${Q7_NEW_PATHS}"
       Q7_Q4_SOURCE "${Q7_Q4_SOURCE}")

set(Q7_BOOL_READER [=[
    bool ReadBoolAction(XrAction action, XrPath subaction) {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        getInfo.subactionPath = subaction;
        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
        if (!CheckXr(xrGetActionStateBoolean(session_, &getInfo, &state),
                     "xrGetActionStateBoolean")) return false;
        return state.isActive && state.currentState;
    }

]=])
string(REPLACE
    "    void SyncInput(XrTime time) {"
    "${Q7_BOOL_READER}    void SyncInput(XrTime time) {"
    Q7_Q4_SOURCE "${Q7_Q4_SOURCE}")
string(REPLACE
    "        turnX_ = Deadzone(ReadFloatAction(turnXAction_, handPaths_[1]));"
    "        turnX_ = Deadzone(ReadFloatAction(turnXAction_, handPaths_[1]));\n        activateDown_ = ReadBoolAction(activateAction_, handPaths_[1]);"
    Q7_Q4_SOURCE "${Q7_Q4_SOURCE}")

set(Q7_ACTIVATE_FRAME [=[
                UpdatePlayer(views[0], frameState.predictedDisplayTime);

                if (activateDown_ && !activateLatched_) {
                    activateLatched_ = true;
                    if (handPoseValid_[1]) {
                        const XrPosef hand = ToVirtualPose(handLocalPoses_[1]);
                        const Mat4 handMatrix = MatrixFromPose(hand);
                        const float dx = -handMatrix.m[8];
                        const float dy = -handMatrix.m[9];
                        const float dz = -handMatrix.m[10];
                        if (ActivateFo3DoorQ7(hand.position.x, hand.position.y, hand.position.z,
                                             dx, dy, dz)) {
                            playerPosition_ = {0.0f, 0.0f, 0.0f};
                            playerYaw_ = 0.0f;
                            lastFrameTime_ = 0;
                            FQ_LOGI("Q7C player origin reset after authored door transition");
                        }
                    }
                } else if (!activateDown_) {
                    activateLatched_ = false;
                }
]=])
string(REPLACE
    "                UpdatePlayer(views[0], frameState.predictedDisplayTime);"
    "${Q7_ACTIVATE_FRAME}"
    Q7_Q4_SOURCE "${Q7_Q4_SOURCE}")

string(REPLACE
    "    XrAction turnXAction_{XR_NULL_HANDLE};"
    "    XrAction turnXAction_{XR_NULL_HANDLE};\n    XrAction activateAction_{XR_NULL_HANDLE};"
    Q7_Q4_SOURCE "${Q7_Q4_SOURCE}")
string(REPLACE
    "    float turnX_{0.0f};"
    "    float turnX_{0.0f};\n    bool activateDown_{false};\n    bool activateLatched_{false};"
    Q7_Q4_SOURCE "${Q7_Q4_SOURCE}")
string(REPLACE
    "Q4 Touch bindings attached: left stick move, right stick snap turn, both aim poses"
    "Q7 Touch bindings attached: left stick move, right stick snap turn, right trigger activate, both aim poses"
    Q7_Q4_SOURCE "${Q7_Q4_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q7-q4-generated.cpp" "${Q7_Q4_SOURCE}")
string(REPLACE
    "#include \"q4-native.cpp\""
    "#include \"q7-q4-generated.cpp\""
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
