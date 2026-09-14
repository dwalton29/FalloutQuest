# Q16.22: stop normal locomotion from starving the staged exterior streamer and
# expose the full Q16.21 terrain ring through the VR camera.
#
# Q16.21 device evidence showed three separate bottlenecks/correctness issues:
#   - ordinary running could move >1 CELL from a pending target before it finished,
#     so Q16.20's stale guard repeatedly cancelled 15-30 seconds of valid work;
#   - the 1536-unit edge prefetch started too late for the staged CPU/GPU pipeline;
#   - q4-native.cpp still used its original 100 metre far clip, truncating the new
#     5x5 LAND ring well before its authored extent.
#
# Keep the proven 3x3 object/collision + 5x5 LAND split. Start earlier, process a
# modestly larger amount of cached CPU/GPU work per frame, finish normal in-flight
# generations instead of cancelling them, and reserve cancellation for a genuine
# same-worldspace jump of more than four authored cells.

set(Q1940_NATIVE_FILE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp")
if(NOT EXISTS "${Q1940_NATIVE_FILE}")
    message(FATAL_ERROR "Q16.22 expected Q16.21 generated renderer")
endif()
file(READ "${Q1940_NATIVE_FILE}" Q1940_NATIVE_SOURCE)

# -----------------------------------------------------------------------------
# A. Throughput. Q16.21 logs showed one GPU shape/frame consuming 7-23 seconds
#    depending on the entering window. Textures/NIFs are now cached, so a small
#    geometry batch is substantially cheaper than the old synchronous rebuild.
# -----------------------------------------------------------------------------
set(Q1940_CPU_OLD
    "constexpr size_t Q1900_CPU_PLACEMENTS_PER_FRAME = 4u; // Q16.19 cheap CPU catch-up")
set(Q1940_CPU_NEW
    "constexpr size_t Q1900_CPU_PLACEMENTS_PER_FRAME = 8u; // Q16.22 bounded CPU catch-up")
string(FIND "${Q1940_NATIVE_SOURCE}" "${Q1940_CPU_OLD}" Q1940_CPU_POS)
if(Q1940_CPU_POS EQUAL -1)
    message(FATAL_ERROR "Q16.22 could not find Q16.21 CPU stream budget")
endif()
string(REPLACE "${Q1940_CPU_OLD}" "${Q1940_CPU_NEW}"
       Q1940_NATIVE_SOURCE "${Q1940_NATIVE_SOURCE}")

set(Q1940_GPU_OLD "constexpr size_t Q1900_GPU_SHAPES_PER_FRAME = 1u;")
set(Q1940_GPU_NEW
    "constexpr size_t Q1900_GPU_SHAPES_PER_FRAME = 3u; // Q16.22 cached VBO/VAO catch-up")
string(FIND "${Q1940_NATIVE_SOURCE}" "${Q1940_GPU_OLD}" Q1940_GPU_POS)
if(Q1940_GPU_POS EQUAL -1)
    message(FATAL_ERROR "Q16.22 could not find Q16.21 GPU stream budget")
endif()
string(REPLACE "${Q1940_GPU_OLD}" "${Q1940_GPU_NEW}"
       Q1940_NATIVE_SOURCE "${Q1940_NATIVE_SOURCE}")

# Start about three quarters of a CELL before the boundary instead of only 1536
# units out. At Fallout's current run speed this gives the staged loader roughly
# twice the useful lead time while still requiring stable movement toward an edge.
string(FIND "${Q1940_NATIVE_SOURCE}"
       "constexpr float Q1920_PREFETCH_DISTANCE = 1536.0f;"
       Q1940_PREFETCH_POS)
if(Q1940_PREFETCH_POS EQUAL -1)
    message(FATAL_ERROR "Q16.22 could not find Q16.21 prefetch distance")
endif()
string(REPLACE
    "constexpr float Q1920_PREFETCH_DISTANCE = 1536.0f;"
    "constexpr float Q1920_PREFETCH_DISTANCE = 3072.0f; // Q16.22 early adjacent prefetch"
    Q1940_NATIVE_SOURCE "${Q1940_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# B. Starvation repair. A target two cells away is not stale when the player has
#    simply kept running while a valid adjacent generation is finishing. Throwing
#    it away guarantees the old window never advances. Finish normal generations;
#    only cancel a >4 CELL discontinuity, which is effectively a teleport/jump.
# -----------------------------------------------------------------------------
set(Q1940_STALE_OLD [==[
    if (gQ1920LatestGridValid &&
        (std::abs(gPendingStreamQ1900.targetGridX - gQ1920LatestGridX) > 1 ||
         std::abs(gPendingStreamQ1900.targetGridY - gQ1920LatestGridY) > 1)) {
        Q6H_LOGW("Q16.21 STREAM STALE: generation=%llu target=(%d,%d) actual=(%d,%d) action=cancel-before-work",
                 static_cast<unsigned long long>(gPendingStreamQ1900.generation),
                 gPendingStreamQ1900.targetGridX, gPendingStreamQ1900.targetGridY,
                 gQ1920LatestGridX, gQ1920LatestGridY);
        Q1900CancelPending("target-no-longer-overlaps-player");
        return;
    }
]==])
set(Q1940_STALE_NEW [==[
    if (gQ1920LatestGridValid &&
        (std::abs(gPendingStreamQ1900.targetGridX - gQ1920LatestGridX) > 4 ||
         std::abs(gPendingStreamQ1900.targetGridY - gQ1920LatestGridY) > 4)) {
        Q6H_LOGW("Q16.22 STREAM TELEPORT STALE: generation=%llu target=(%d,%d) actual=(%d,%d) action=cancel-five-plus-cells",
                 static_cast<unsigned long long>(gPendingStreamQ1900.generation),
                 gPendingStreamQ1900.targetGridX, gPendingStreamQ1900.targetGridY,
                 gQ1920LatestGridX, gQ1920LatestGridY);
        Q1900CancelPending("target-five-plus-cells-away");
        return;
    }
]==])
string(FIND "${Q1940_NATIVE_SOURCE}" "${Q1940_STALE_OLD}" Q1940_STALE_POS)
if(Q1940_STALE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.22 could not find Q16.21 stale-target cancellation")
endif()
string(REPLACE "${Q1940_STALE_OLD}" "${Q1940_STALE_NEW}"
       Q1940_NATIVE_SOURCE "${Q1940_NATIVE_SOURCE}")

string(REPLACE "rebuildMode=stable-adjacent-prefetch+cached-assets"
       "rebuildMode=early-prefetch+finish-inflight+cached-assets"
       Q1940_NATIVE_SOURCE "${Q1940_NATIVE_SOURCE}")
# All renderer-side stream diagnostics should identify the live build.
string(REPLACE "Q16.21" "Q16.22"
       Q1940_NATIVE_SOURCE "${Q1940_NATIVE_SOURCE}")
file(WRITE "${Q1940_NATIVE_FILE}" "${Q1940_NATIVE_SOURCE}")

# Keep the separate collision/LAND translation-unit diagnostics on the same build
# number so a single adb filter captures the full generation timeline.
string(REPLACE "Q16.21 COLLISION MODE OVERRIDE" "Q16.22 COLLISION MODE OVERRIDE"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")
string(REPLACE "Q16.21 TERRAIN WINDOW" "Q16.22 TERRAIN WINDOW"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-data-q720.cpp"
     "${Q720_TERRAIN_DATA_SOURCE}")

# -----------------------------------------------------------------------------
# C. VR far clip. q4's original 100 m plane predates exterior traversal. A 5x5
#    Fallout LAND ring reaches ~146 m to an edge and ~207 m to a corner from its
#    centre, so 300 m exposes the complete ring without needlessly using a huge
#    depth range against the existing 0.04 m near plane.
# -----------------------------------------------------------------------------
set(Q1940_Q4_FILE "${CMAKE_CURRENT_BINARY_DIR}/q1800-q4-generated.cpp")
if(NOT EXISTS "${Q1940_Q4_FILE}")
    message(FATAL_ERROR "Q16.22 expected final OpenXR source")
endif()
file(READ "${Q1940_Q4_FILE}" Q1940_Q4_SOURCE)

set(Q1940_PROJECTION_OLD [==[
            const Mat4 projection = ProjectionFromFov(view.fov, 0.04f, 100.0f);
]==])
set(Q1940_PROJECTION_NEW [==[
            constexpr float Q1940_FAR_CLIP_METRES = 300.0f;
            const Mat4 projection = ProjectionFromFov(
                view.fov, 0.04f, Q1940_FAR_CLIP_METRES);
            static bool q1940FarClipLogged = false;
            if (!q1940FarClipLogged) {
                q1940FarClipLogged = true;
                FQ_LOGI("Q16.22 FAR CLIP: near=0.04m far=300.0m");
            }
]==])
string(FIND "${Q1940_Q4_SOURCE}" "${Q1940_PROJECTION_OLD}" Q1940_PROJECTION_POS)
if(Q1940_PROJECTION_POS EQUAL -1)
    message(FATAL_ERROR "Q16.22 could not find 100m OpenXR far clip")
endif()
string(REPLACE "${Q1940_PROJECTION_OLD}" "${Q1940_PROJECTION_NEW}"
       Q1940_Q4_SOURCE "${Q1940_Q4_SOURCE}")

# Correctly render Q16.22 on the seven-segment debug label. Earlier Q16.20/21
# comments described the first digit as 2 while retaining the old mask for 1;
# Q16.22 uses the standard 0x5B mask for both 2 digits.
set(Q1940_LABEL_OLD [==[
        q1600Digit(q1600X, 0x06u); // Q16.21: 2 = A B G E D
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x06u); // Q16.21: 1 = B C
]==])
set(Q1940_LABEL_NEW [==[
        q1600Digit(q1600X, 0x5Bu); // Q16.22: 2 = A B G E D
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x5Bu); // Q16.22: 2 = A B G E D
]==])
string(FIND "${Q1940_Q4_SOURCE}" "${Q1940_LABEL_OLD}" Q1940_LABEL_POS)
if(Q1940_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.22 could not find Q16.21 build-label digits")
endif()
string(REPLACE "${Q1940_LABEL_OLD}" "${Q1940_LABEL_NEW}"
       Q1940_Q4_SOURCE "${Q1940_Q4_SOURCE}")
string(REPLACE "Q16.21 BUILD LABEL:" "Q16.22 BUILD LABEL:"
       Q1940_Q4_SOURCE "${Q1940_Q4_SOURCE}")
string(REPLACE "text=Q16.21 anchor=left-hand" "text=Q16.22 anchor=left-hand"
       Q1940_Q4_SOURCE "${Q1940_Q4_SOURCE}")
string(REPLACE "Q16.21 AUTHORED DOOR FACING" "Q16.22 AUTHORED DOOR FACING"
       Q1940_Q4_SOURCE "${Q1940_Q4_SOURCE}")
file(WRITE "${Q1940_Q4_FILE}" "${Q1940_Q4_SOURCE}")

# Configure-time proof: fail loudly if an upstream generated anchor moves.
string(FIND "${Q1940_NATIVE_SOURCE}" "Q1900_CPU_PLACEMENTS_PER_FRAME = 8u" Q1940_CPU_OK)
string(FIND "${Q1940_NATIVE_SOURCE}" "Q1900_GPU_SHAPES_PER_FRAME = 3u" Q1940_GPU_OK)
string(FIND "${Q1940_NATIVE_SOURCE}" "Q1920_PREFETCH_DISTANCE = 3072.0f" Q1940_PREFETCH_OK)
string(FIND "${Q1940_NATIVE_SOURCE}" "cancel-five-plus-cells" Q1940_STALE_OK)
string(FIND "${Q1940_NATIVE_SOURCE}" "rebuildMode=early-prefetch+finish-inflight+cached-assets" Q1940_MODE_OK)
string(FIND "${Q1940_Q4_SOURCE}" "Q1940_FAR_CLIP_METRES = 300.0f" Q1940_CLIP_OK)
string(FIND "${Q1940_Q4_SOURCE}" "Q16.22: 2 = A B G E D" Q1940_LABEL_OK)
if(Q1940_CPU_OK EQUAL -1 OR Q1940_GPU_OK EQUAL -1 OR
   Q1940_PREFETCH_OK EQUAL -1 OR Q1940_STALE_OK EQUAL -1 OR
   Q1940_MODE_OK EQUAL -1 OR Q1940_CLIP_OK EQUAL -1 OR
   Q1940_LABEL_OK EQUAL -1)
    message(FATAL_ERROR "Q16.22 stream/far-clip verification failed")
endif()

message(STATUS "Q16.22 enabled: 8-placement CPU + 3-shape GPU staging, 3072-unit prefetch, finish normal in-flight streams, 300m far clip")
