# Q16.20: stability repair for Q16.19's predictive exterior streamer.
#
# Device log proved two correctness failures in Q16.19:
#   1) a Megaton prefetch could survive a door/worldspace transition after its
#      metadata task was consumed, then commit against the new Wasteland gObjects;
#      one observed stale commit retired 1237 shapes and left liveShapes=0.
#   2) frame-delta prediction interpreted long stalls as enormous player velocity,
#      jumping the requested window several cells around the player and causing
#      zero-overlap rebuilds/thrash.
#
# Keep Q16.19's successful immutable caches, but replace prediction with a
# conservative adjacent-cell prefetch:
#   - never target more than one CELL from the player's actual authored XCLC;
#   - start only when moving toward an edge and within 1536 game units of it;
#   - shift one axis at a time;
#   - reject stale frame deltas/teleports;
#   - cancel every pending generation if WRLD/persistent/original-XTEL changes;
#   - cancel a generation as soon as its target no longer overlaps the player's
#     current 3x3 safety window.

set(Q1920_NATIVE_FILE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp")
if(NOT EXISTS "${Q1920_NATIVE_FILE}")
    message(FATAL_ERROR "Q16.20 expected Q16.19 generated renderer")
endif()
file(READ "${Q1920_NATIVE_FILE}" Q1920_NATIVE_SOURCE)

# -----------------------------------------------------------------------------
# A. Make stream ownership independent of the metadata-task lifetime. Q16.19's
# context check consulted `task`, but task is reset after METADATA READY. That is
# exactly how a stale Megaton generation survived into the Capital Wasteland.
# -----------------------------------------------------------------------------
set(Q1920_PENDING_OLD [==[
    float selectionGameX = 0.0f;
    float selectionGameY = 0.0f;
    Q1900StreamPhase phase = Q1900StreamPhase::Metadata;
]==])
set(Q1920_PENDING_NEW [==[
    float selectionGameX = 0.0f;
    float selectionGameY = 0.0f;
    uint32_t sourceWorldspace = 0u;
    uint32_t sourcePersistentCell = 0u;
    float sourceOriginX = 0.0f;
    float sourceOriginY = 0.0f;
    float sourceOriginZ = 0.0f;
    Q1900StreamPhase phase = Q1900StreamPhase::Metadata;
]==])
string(FIND "${Q1920_NATIVE_SOURCE}" "${Q1920_PENDING_OLD}" Q1920_PENDING_POS)
if(Q1920_PENDING_POS EQUAL -1)
    message(FATAL_ERROR "Q16.20 could not find pending stream context fields")
endif()
string(REPLACE "${Q1920_PENDING_OLD}" "${Q1920_PENDING_NEW}"
       Q1920_NATIVE_SOURCE "${Q1920_NATIVE_SOURCE}")

set(Q1920_BEGIN_CONTEXT_OLD [==[
    gPendingStreamQ1900.selectionGameX = selectionGameX;
    gPendingStreamQ1900.selectionGameY = selectionGameY;
    gPendingStreamQ1900.phase = Q1900StreamPhase::Metadata;
]==])
set(Q1920_BEGIN_CONTEXT_NEW [==[
    gPendingStreamQ1900.selectionGameX = selectionGameX;
    gPendingStreamQ1900.selectionGameY = selectionGameY;
    gPendingStreamQ1900.sourceWorldspace = gExteriorWorldspaceQ1890;
    gPendingStreamQ1900.sourcePersistentCell = gExteriorPersistentCellQ1890;
    gPendingStreamQ1900.sourceOriginX = gExteriorOriginXQ1890;
    gPendingStreamQ1900.sourceOriginY = gExteriorOriginYQ1890;
    gPendingStreamQ1900.sourceOriginZ = gExteriorOriginZQ1890;
    gPendingStreamQ1900.phase = Q1900StreamPhase::Metadata;
]==])
string(FIND "${Q1920_NATIVE_SOURCE}" "${Q1920_BEGIN_CONTEXT_OLD}" Q1920_BEGIN_CONTEXT_POS)
if(Q1920_BEGIN_CONTEXT_POS EQUAL -1)
    message(FATAL_ERROR "Q16.20 could not find begin-stream context assignment")
endif()
string(REPLACE "${Q1920_BEGIN_CONTEXT_OLD}" "${Q1920_BEGIN_CONTEXT_NEW}"
       Q1920_NATIVE_SOURCE "${Q1920_NATIVE_SOURCE}")

# Latest actual XCLC is updated even while a stream is busy. Q1900AdvanceStream
# uses it to kill a request that the player has already outrun/reversed away from.
set(Q1920_GLOBAL_MARKER "Q1900PendingStream gPendingStreamQ1900;")
set(Q1920_GLOBAL_NEW [==[
Q1900PendingStream gPendingStreamQ1900;
bool gQ1920LatestGridValid = false;
int32_t gQ1920LatestGridX = 0;
int32_t gQ1920LatestGridY = 0;
]==])
string(FIND "${Q1920_NATIVE_SOURCE}" "${Q1920_GLOBAL_MARKER}" Q1920_GLOBAL_POS)
if(Q1920_GLOBAL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.20 could not find Q16.19 stream global")
endif()
string(REPLACE "${Q1920_GLOBAL_MARKER}" "${Q1920_GLOBAL_NEW}"
       Q1920_NATIVE_SOURCE "${Q1920_NATIVE_SOURCE}")

set(Q1920_ADVANCE_CHECK_OLD [==[
    const auto task = gPendingStreamQ1900.metadata;
    if (!gExteriorStreamingActiveQ1890 ||
        (task && (task->worldspace != gExteriorWorldspaceQ1890 ||
                  task->persistentCell != gExteriorPersistentCellQ1890))) {
        Q1900CancelPending("scene-context-changed");
        return;
    }

    switch (gPendingStreamQ1900.phase) {
]==])
set(Q1920_ADVANCE_CHECK_NEW [==[
    const bool contextChanged =
        !gExteriorStreamingActiveQ1890 ||
        gPendingStreamQ1900.sourceWorldspace != gExteriorWorldspaceQ1890 ||
        gPendingStreamQ1900.sourcePersistentCell != gExteriorPersistentCellQ1890 ||
        std::fabs(gPendingStreamQ1900.sourceOriginX - gExteriorOriginXQ1890) > 0.01f ||
        std::fabs(gPendingStreamQ1900.sourceOriginY - gExteriorOriginYQ1890) > 0.01f ||
        std::fabs(gPendingStreamQ1900.sourceOriginZ - gExteriorOriginZQ1890) > 0.01f;
    if (contextChanged) {
        Q1900CancelPending("scene-context-changed");
        return;
    }

    if (gQ1920LatestGridValid &&
        (std::abs(gPendingStreamQ1900.targetGridX - gQ1920LatestGridX) > 1 ||
         std::abs(gPendingStreamQ1900.targetGridY - gQ1920LatestGridY) > 1)) {
        Q6H_LOGW("Q16.20 STREAM STALE: generation=%llu target=(%d,%d) actual=(%d,%d) action=cancel-before-work",
                 static_cast<unsigned long long>(gPendingStreamQ1900.generation),
                 gPendingStreamQ1900.targetGridX, gPendingStreamQ1900.targetGridY,
                 gQ1920LatestGridX, gQ1920LatestGridY);
        Q1900CancelPending("target-no-longer-overlaps-player");
        return;
    }

    switch (gPendingStreamQ1900.phase) {
]==])
string(FIND "${Q1920_NATIVE_SOURCE}" "${Q1920_ADVANCE_CHECK_OLD}" Q1920_ADVANCE_CHECK_POS)
if(Q1920_ADVANCE_CHECK_POS EQUAL -1)
    message(FATAL_ERROR "Q16.20 could not find Q16.19 pending-context validation")
endif()
string(REPLACE "${Q1920_ADVANCE_CHECK_OLD}" "${Q1920_ADVANCE_CHECK_NEW}"
       Q1920_NATIVE_SOURCE "${Q1920_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# B. Replace Q16.19's frame-speed predictor with edge proximity + stable movement
# direction. Crucially we calculate actual XCLC before advancing the staged stream,
# so stale-target cancellation remains informed while gExteriorStreamBusy is true.
# -----------------------------------------------------------------------------
set(Q1920_UPDATE_OLD [==[
void UpdateFo3ExteriorStreamingQ1890(float virtualHeadX, float virtualHeadZ) {
    Q1900AdvanceStream();
    if (!gExteriorStreamingActiveQ1890 || gExteriorStreamBusyQ1890 ||
        IsFo3LoadingVisibleQ1700()) return;

    const float gameX = gExteriorOriginXQ1890 +
                        virtualHeadX * FO3_UNITS_PER_METRE;
    const float gameY = gExteriorOriginYQ1890 +
                        (SCENE_FORWARD - virtualHeadZ) * FO3_UNITS_PER_METRE;
    const int32_t actualGridX = static_cast<int32_t>(
        std::floor(gameX / Q1890_EXTERIOR_CELL_SIZE));
    const int32_t actualGridY = static_cast<int32_t>(
        std::floor(gameY / Q1890_EXTERIOR_CELL_SIZE));

    // Q16.19 predicts where the player is travelling and recentres the render
    // window one CELL ahead. This gives the 1-shape/frame uploader almost an
    // entire 4096-unit cell of lead time instead of starting at the boundary.
    static uint32_t q1910MotionWorldspace = 0u;
    static bool q1910MotionReady = false;
    static float q1910PreviousGameX = 0.0f;
    static float q1910PreviousGameY = 0.0f;
    static float q1910VelocityX = 0.0f;
    static float q1910VelocityY = 0.0f;
    if (!q1910MotionReady || q1910MotionWorldspace != gExteriorWorldspaceQ1890) {
        q1910MotionWorldspace = gExteriorWorldspaceQ1890;
        q1910MotionReady = true;
        q1910PreviousGameX = gameX;
        q1910PreviousGameY = gameY;
        q1910VelocityX = 0.0f;
        q1910VelocityY = 0.0f;
        return;
    }

    const float frameDx = gameX - q1910PreviousGameX;
    const float frameDy = gameY - q1910PreviousGameY;
    q1910PreviousGameX = gameX;
    q1910PreviousGameY = gameY;
    q1910VelocityX = q1910VelocityX * 0.86f + frameDx * 0.14f;
    q1910VelocityY = q1910VelocityY * 0.86f + frameDy * 0.14f;

    constexpr float Q1910_PREFETCH_SPEED = 1.15f; // game units/frame; filters head jitter
    int32_t desiredGridX = gExteriorWindowGridXQ1890;
    int32_t desiredGridY = gExteriorWindowGridYQ1890;
    bool predictive = false;
    if (std::fabs(q1910VelocityX) >= Q1910_PREFETCH_SPEED) {
        desiredGridX = actualGridX + (q1910VelocityX > 0.0f ? 1 : -1);
        predictive = true;
    } else {
        desiredGridX = actualGridX;
    }
    if (std::fabs(q1910VelocityY) >= Q1910_PREFETCH_SPEED) {
        desiredGridY = actualGridY + (q1910VelocityY > 0.0f ? 1 : -1);
        predictive = true;
    } else {
        desiredGridY = actualGridY;
    }

    // When stationary after an ahead-of-player prefetch, keep that valid window
    // rather than immediately pulling it backward. If the player has actually
    // escaped the current 3x3 safety margin, force a catch-up recenter.
    if (!predictive) {
        const int dx = std::abs(actualGridX - gExteriorWindowGridXQ1890);
        const int dy = std::abs(actualGridY - gExteriorWindowGridYQ1890);
        if (dx <= 1 && dy <= 1) return;
        desiredGridX = actualGridX;
        desiredGridY = actualGridY;
    }

    if (desiredGridX == gExteriorWindowGridXQ1890 &&
        desiredGridY == gExteriorWindowGridYQ1890) return;

    const float selectionGameX =
        (static_cast<float>(desiredGridX) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
    const float selectionGameY =
        (static_cast<float>(desiredGridY) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;

    Q6H_LOGI("Q16.19 PREFETCH: window=(%d,%d) actual=(%d,%d) desired=(%d,%d) velocity=(%.2f %.2f) playerGame=(%.2f %.2f) selectionCenter=(%.2f %.2f)",
             gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
             actualGridX, actualGridY, desiredGridX, desiredGridY,
             q1910VelocityX, q1910VelocityY, gameX, gameY,
             selectionGameX, selectionGameY);
    Q1900BeginStream(selectionGameX, selectionGameY,
                     desiredGridX, desiredGridY);
}
]==])
set(Q1920_UPDATE_NEW [==[
void UpdateFo3ExteriorStreamingQ1890(float virtualHeadX, float virtualHeadZ) {
    if (!gExteriorStreamingActiveQ1890) {
        gQ1920LatestGridValid = false;
        return;
    }

    const float gameX = gExteriorOriginXQ1890 +
                        virtualHeadX * FO3_UNITS_PER_METRE;
    const float gameY = gExteriorOriginYQ1890 +
                        (SCENE_FORWARD - virtualHeadZ) * FO3_UNITS_PER_METRE;
    const int32_t actualGridX = static_cast<int32_t>(
        std::floor(gameX / Q1890_EXTERIOR_CELL_SIZE));
    const int32_t actualGridY = static_cast<int32_t>(
        std::floor(gameY / Q1890_EXTERIOR_CELL_SIZE));
    gQ1920LatestGridValid = true;
    gQ1920LatestGridX = actualGridX;
    gQ1920LatestGridY = actualGridY;

    // Give a pending generation current player position before it advances.
    Q1900AdvanceStream();
    if (gExteriorStreamBusyQ1890 || IsFo3LoadingVisibleQ1700()) return;

    static uint32_t q1920MotionWorldspace = 0u;
    static uint32_t q1920MotionPersistent = 0u;
    static float q1920MotionOriginX = 0.0f;
    static float q1920MotionOriginY = 0.0f;
    static bool q1920MotionReady = false;
    static float q1920PreviousGameX = 0.0f;
    static float q1920PreviousGameY = 0.0f;
    static float q1920SmoothDx = 0.0f;
    static float q1920SmoothDy = 0.0f;
    static int32_t q1920PreviousActualGridX = 0;
    static int32_t q1920PreviousActualGridY = 0;

    const bool newContext = !q1920MotionReady ||
        q1920MotionWorldspace != gExteriorWorldspaceQ1890 ||
        q1920MotionPersistent != gExteriorPersistentCellQ1890 ||
        std::fabs(q1920MotionOriginX - gExteriorOriginXQ1890) > 0.01f ||
        std::fabs(q1920MotionOriginY - gExteriorOriginYQ1890) > 0.01f;
    if (newContext) {
        q1920MotionWorldspace = gExteriorWorldspaceQ1890;
        q1920MotionPersistent = gExteriorPersistentCellQ1890;
        q1920MotionOriginX = gExteriorOriginXQ1890;
        q1920MotionOriginY = gExteriorOriginYQ1890;
        q1920MotionReady = true;
        q1920PreviousGameX = gameX;
        q1920PreviousGameY = gameY;
        q1920SmoothDx = 0.0f;
        q1920SmoothDy = 0.0f;
        q1920PreviousActualGridX = actualGridX;
        q1920PreviousActualGridY = actualGridY;
        Q6H_LOGI("Q16.20 MOTION RESET: worldspace=%08X persistent=%08X actual=(%d,%d) reason=context",
                 gExteriorWorldspaceQ1890, gExteriorPersistentCellQ1890,
                 actualGridX, actualGridY);
        return;
    }

    const bool actualCellChanged =
        actualGridX != q1920PreviousActualGridX ||
        actualGridY != q1920PreviousActualGridY;
    q1920PreviousActualGridX = actualGridX;
    q1920PreviousActualGridY = actualGridY;

    const float frameDx = gameX - q1920PreviousGameX;
    const float frameDy = gameY - q1920PreviousGameY;
    q1920PreviousGameX = gameX;
    q1920PreviousGameY = gameY;

    // A single rendered-frame delta larger than this is a stall, teleport or
    // scene discontinuity, not a locomotion velocity sample. Do not predict from it.
    constexpr float Q1920_MAX_VALID_FRAME_DELTA = 96.0f;
    if (std::fabs(frameDx) > Q1920_MAX_VALID_FRAME_DELTA ||
        std::fabs(frameDy) > Q1920_MAX_VALID_FRAME_DELTA) {
        q1920SmoothDx = 0.0f;
        q1920SmoothDy = 0.0f;
        Q6H_LOGW("Q16.20 MOTION SAMPLE REJECTED: delta=(%.2f %.2f) actual=(%d,%d) reason=stall-or-teleport",
                 frameDx, frameDy, actualGridX, actualGridY);
    } else {
        q1920SmoothDx = q1920SmoothDx * 0.82f + frameDx * 0.18f;
        q1920SmoothDy = q1920SmoothDy * 0.82f + frameDy * 0.18f;
    }

    // If prefetch was missed and the player actually entered another CELL, make
    // the new actual CELL the centre. This remains only one adjacent recenter.
    if (actualCellChanged &&
        (actualGridX != gExteriorWindowGridXQ1890 ||
         actualGridY != gExteriorWindowGridYQ1890)) {
        const int dx = std::abs(actualGridX - gExteriorWindowGridXQ1890);
        const int dy = std::abs(actualGridY - gExteriorWindowGridYQ1890);
        if (dx <= 1 && dy <= 1) {
            const float selectionX =
                (static_cast<float>(actualGridX) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
            const float selectionY =
                (static_cast<float>(actualGridY) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
            Q6H_LOGI("Q16.20 ADJACENT CATCHUP: window=(%d,%d) actual=(%d,%d) selection=(%.2f %.2f)",
                     gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
                     actualGridX, actualGridY, selectionX, selectionY);
            Q1900BeginStream(selectionX, selectionY, actualGridX, actualGridY);
            return;
        }
    }

    // A player outside the current 3x3 means an older request was missed or
    // cancelled. Recenter exactly on actual position; never extrapolate farther.
    if (std::abs(actualGridX - gExteriorWindowGridXQ1890) > 1 ||
        std::abs(actualGridY - gExteriorWindowGridYQ1890) > 1) {
        const float selectionX =
            (static_cast<float>(actualGridX) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
        const float selectionY =
            (static_cast<float>(actualGridY) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
        Q6H_LOGW("Q16.20 SAFETY CATCHUP: window=(%d,%d) actual=(%d,%d) selection=(%.2f %.2f)",
                 gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
                 actualGridX, actualGridY, selectionX, selectionY);
        Q1900BeginStream(selectionX, selectionY, actualGridX, actualGridY);
        return;
    }

    // MegatonExterior (00000A74) is a small child worldspace whose loader already
    // selects the complete authored worldspace. Recentering it buys nothing and
    // was the source of the stale generation observed during the Megaton exit.
    if (gExteriorWorldspaceQ1890 == 0x00000A74u) return;

    const float cellMinX = static_cast<float>(actualGridX) * Q1890_EXTERIOR_CELL_SIZE;
    const float cellMinY = static_cast<float>(actualGridY) * Q1890_EXTERIOR_CELL_SIZE;
    const float localX = gameX - cellMinX;
    const float localY = gameY - cellMinY;
    const float distWest = localX;
    const float distEast = Q1890_EXTERIOR_CELL_SIZE - localX;
    const float distSouth = localY;
    const float distNorth = Q1890_EXTERIOR_CELL_SIZE - localY;

    constexpr float Q1920_PREFETCH_DISTANCE = 1536.0f;
    constexpr float Q1920_DIRECTION_EPSILON = 0.35f;
    int axis = 0; // 1 = X, 2 = Y
    int step = 0;
    float bestDistance = Q1920_PREFETCH_DISTANCE + 1.0f;

    if (q1920SmoothDx > Q1920_DIRECTION_EPSILON && distEast < bestDistance) {
        axis = 1; step = 1; bestDistance = distEast;
    }
    if (q1920SmoothDx < -Q1920_DIRECTION_EPSILON && distWest < bestDistance) {
        axis = 1; step = -1; bestDistance = distWest;
    }
    if (q1920SmoothDy > Q1920_DIRECTION_EPSILON && distNorth < bestDistance) {
        axis = 2; step = 1; bestDistance = distNorth;
    }
    if (q1920SmoothDy < -Q1920_DIRECTION_EPSILON && distSouth < bestDistance) {
        axis = 2; step = -1; bestDistance = distSouth;
    }
    if (axis == 0 || bestDistance > Q1920_PREFETCH_DISTANCE) return;

    // Preserve any already-useful one-cell lead on the other axis. Only the
    // chosen axis moves, and every target coordinate remains within +/-1 of actual.
    int32_t desiredGridX = gExteriorWindowGridXQ1890;
    int32_t desiredGridY = gExteriorWindowGridYQ1890;
    desiredGridX = std::clamp(desiredGridX, actualGridX - 1, actualGridX + 1);
    desiredGridY = std::clamp(desiredGridY, actualGridY - 1, actualGridY + 1);
    if (axis == 1) desiredGridX = actualGridX + step;
    else desiredGridY = actualGridY + step;

    if (desiredGridX == gExteriorWindowGridXQ1890 &&
        desiredGridY == gExteriorWindowGridYQ1890) return;

    const float selectionGameX =
        (static_cast<float>(desiredGridX) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
    const float selectionGameY =
        (static_cast<float>(desiredGridY) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
    Q6H_LOGI("Q16.20 ADJACENT PREFETCH: window=(%d,%d) actual=(%d,%d) desired=(%d,%d) axis=%c step=%d edgeDistance=%.1f smoothDelta=(%.2f %.2f) selection=(%.2f %.2f)",
             gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
             actualGridX, actualGridY, desiredGridX, desiredGridY,
             axis == 1 ? 'X' : 'Y', step, bestDistance,
             q1920SmoothDx, q1920SmoothDy,
             selectionGameX, selectionGameY);
    Q1900BeginStream(selectionGameX, selectionGameY,
                     desiredGridX, desiredGridY);
}
]==])
string(FIND "${Q1920_NATIVE_SOURCE}" "${Q1920_UPDATE_OLD}" Q1920_UPDATE_POS)
if(Q1920_UPDATE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.20 could not find Q16.19 predictive update function")
endif()
string(REPLACE "${Q1920_UPDATE_OLD}" "${Q1920_UPDATE_NEW}"
       Q1920_NATIVE_SOURCE "${Q1920_NATIVE_SOURCE}")

# Native diagnostics now identify the stability layer. Cache logs emitted from
# terrain/collision TUs intentionally retain their original Q16.19 tags.
string(REPLACE "Q16.19 STREAM" "Q16.20 STREAM" Q1920_NATIVE_SOURCE "${Q1920_NATIVE_SOURCE}")
string(REPLACE "Q16.19 METADATA" "Q16.20 METADATA" Q1920_NATIVE_SOURCE "${Q1920_NATIVE_SOURCE}")
string(REPLACE "Q16.19 CPU READY" "Q16.20 CPU READY" Q1920_NATIVE_SOURCE "${Q1920_NATIVE_SOURCE}")
string(REPLACE "Q16.19 GPU READY" "Q16.20 GPU READY" Q1920_NATIVE_SOURCE "${Q1920_NATIVE_SOURCE}")
string(REPLACE "Q16.19 COLLISION READY" "Q16.20 COLLISION READY" Q1920_NATIVE_SOURCE "${Q1920_NATIVE_SOURCE}")
string(REPLACE "Q16.19 TERRAIN READY" "Q16.20 TERRAIN READY" Q1920_NATIVE_SOURCE "${Q1920_NATIVE_SOURCE}")
string(REPLACE "Q16.19 WINDOW READY" "Q16.20 WINDOW READY" Q1920_NATIVE_SOURCE "${Q1920_NATIVE_SOURCE}")
string(REPLACE "Q16.19 STREAM CONTEXT" "Q16.20 STREAM CONTEXT" Q1920_NATIVE_SOURCE "${Q1920_NATIVE_SOURCE}")
string(REPLACE "rebuildMode=async-metadata+incremental-edge"
       "rebuildMode=stable-adjacent-prefetch+cached-assets"
       Q1920_NATIVE_SOURCE "${Q1920_NATIVE_SOURCE}")
file(WRITE "${Q1920_NATIVE_FILE}" "${Q1920_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# C. Visible label Q16.19 -> Q16.20. No interaction HUD metrics/wording changes.
# -----------------------------------------------------------------------------
set(Q1920_Q4_FILE "${CMAKE_CURRENT_BINARY_DIR}/q1800-q4-generated.cpp")
if(NOT EXISTS "${Q1920_Q4_FILE}")
    message(FATAL_ERROR "Q16.20 expected final OpenXR source")
endif()
file(READ "${Q1920_Q4_FILE}" Q1920_Q4_SOURCE)
set(Q1920_LABEL_OLD [==[
        q1600Digit(q1600X, 0x06u); // Q16.19: 1 = B C
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x6Fu); // Q16.19: 9 = A B C D F G
]==])
set(Q1920_LABEL_NEW [==[
        q1600Digit(q1600X, 0x06u); // Q16.20: 2 = A B G E D
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x3Fu); // Q16.20: 0 = A B C D E F
]==])
string(FIND "${Q1920_Q4_SOURCE}" "${Q1920_LABEL_OLD}" Q1920_LABEL_POS)
if(Q1920_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.20 could not find Q16.19 build-label digits")
endif()
string(REPLACE "${Q1920_LABEL_OLD}" "${Q1920_LABEL_NEW}"
       Q1920_Q4_SOURCE "${Q1920_Q4_SOURCE}")
string(REPLACE "Q16.19 BUILD LABEL:" "Q16.20 BUILD LABEL:"
       Q1920_Q4_SOURCE "${Q1920_Q4_SOURCE}")
string(REPLACE "text=Q16.19 anchor=left-hand" "text=Q16.20 anchor=left-hand"
       Q1920_Q4_SOURCE "${Q1920_Q4_SOURCE}")
string(REPLACE "Q16.19 AUTHORED DOOR FACING" "Q16.20 AUTHORED DOOR FACING"
       Q1920_Q4_SOURCE "${Q1920_Q4_SOURCE}")
file(WRITE "${Q1920_Q4_FILE}" "${Q1920_Q4_SOURCE}")

# Configure-time proof of the two device-log failures.
string(FIND "${Q1920_NATIVE_SOURCE}" "sourceWorldspace != gExteriorWorldspaceQ1890" Q1920_CONTEXT_OK)
string(FIND "${Q1920_NATIVE_SOURCE}" "Q16.20 STREAM STALE:" Q1920_STALE_OK)
string(FIND "${Q1920_NATIVE_SOURCE}" "Q16.20 MOTION SAMPLE REJECTED:" Q1920_DELTA_OK)
string(FIND "${Q1920_NATIVE_SOURCE}" "Q16.20 ADJACENT PREFETCH:" Q1920_PREFETCH_OK)
string(FIND "${Q1920_NATIVE_SOURCE}" "gExteriorWorldspaceQ1890 == 0x00000A74u" Q1920_MEGATON_HOLD_OK)
string(FIND "${Q1920_NATIVE_SOURCE}" "q1910VelocityX" Q1920_OLD_PREDICTOR)
string(FIND "${Q1920_Q4_SOURCE}" "Q16.20: 0 = A B C D E F" Q1920_LABEL_OK)
if(Q1920_CONTEXT_OK EQUAL -1 OR Q1920_STALE_OK EQUAL -1 OR
   Q1920_DELTA_OK EQUAL -1 OR Q1920_PREFETCH_OK EQUAL -1 OR
   Q1920_MEGATON_HOLD_OK EQUAL -1 OR NOT Q1920_OLD_PREDICTOR EQUAL -1 OR
   Q1920_LABEL_OK EQUAL -1)
    message(FATAL_ERROR "Q16.20 stable adjacent streaming verification failed")
endif()

message(STATUS "Q16.20 stable exterior streaming enabled: strict scene ownership + stale-target cancellation + adjacent edge prefetch; Q16.19 caches retained")
