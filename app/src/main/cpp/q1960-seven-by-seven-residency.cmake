# Q16.24: make the wider exterior world resident before the player can reach its edge.
#
# Q16.23 device evidence proved two remaining continuity problems:
#   - Capital Wasteland entry still used the historical radius-1 selector, so the
#     visible world started 3x3 and then visibly grew to the async 5x5 target;
#   - even after 5x5 was established, one-cell entering strips took long enough
#     that continuous locomotion could still reach the live edge before commit.
#
# Q16.24 therefore makes both initial and streamed Wasteland visuals radius 3
# (7x7), keeps authored collision local at radius 1 (3x3), and grows the retained
# LAND backing ring to radius 4 (9x9). The extra LAND ring is deliberate: a 7x7
# visual window shifted by one cell still fits completely inside a retained 9x9
# terrain window, so adjacent stream commits do not need to rebuild all terrain
# geometry. Prefetch begins as soon as stable movement direction exists anywhere
# in the current 4096-unit CELL; target centres remain adjacent-only.

# -----------------------------------------------------------------------------
# A. Final compiled worldspace selector: default large-worldspace selection is
#    now radius 3. Small child worldspaces still take their full-small-worldspace
#    branch exactly as before. Route the already-mature q1951 CELL TU through a
#    new generated q1960 copy so no older CELL fixes are lost.
# -----------------------------------------------------------------------------
if(NOT DEFINED Q1951_WORLDSPACE_FINAL OR NOT EXISTS "${Q1951_WORLDSPACE_FINAL}")
    message(FATAL_ERROR "Q16.24 expected q1951 final worldspace implementation")
endif()
if(NOT DEFINED Q720_CELL_SOURCE OR NOT EXISTS "${Q720_CELL_SOURCE}")
    message(FATAL_ERROR "Q16.24 expected q1951 final CELL translation unit")
endif()

file(READ "${Q1951_WORLDSPACE_FINAL}" Q1960_WORLDSPACE_SOURCE)
set(Q1960_DEFAULT_RADIUS_OLD [==[
    const int q1950SelectionRadius = gQ1950WorldspaceGridRadiusOverride >= 0
        ? gQ1950WorldspaceGridRadiusOverride : GRID_RADIUS_Q75;
]==])
set(Q1960_DEFAULT_RADIUS_NEW [==[
    const int q1950SelectionRadius = gQ1950WorldspaceGridRadiusOverride >= 0
        ? gQ1950WorldspaceGridRadiusOverride : 3; // Q16.24 initial exterior 7x7
]==])
string(FIND "${Q1960_WORLDSPACE_SOURCE}" "${Q1960_DEFAULT_RADIUS_OLD}" Q1960_DEFAULT_RADIUS_POS)
if(Q1960_DEFAULT_RADIUS_POS EQUAL -1)
    message(FATAL_ERROR "Q16.24 could not find q1951 default worldspace radius")
endif()
string(REPLACE "${Q1960_DEFAULT_RADIUS_OLD}" "${Q1960_DEFAULT_RADIUS_NEW}"
       Q1960_WORLDSPACE_SOURCE "${Q1960_WORLDSPACE_SOURCE}")
string(REPLACE "Q16.23 VISUAL WINDOW:" "Q16.24 VISUAL WINDOW:"
       Q1960_WORLDSPACE_SOURCE "${Q1960_WORLDSPACE_SOURCE}")

set(Q1960_WORLDSPACE_FINAL "${CMAKE_CURRENT_BINARY_DIR}/fo3-worldspace-q1960.cpp")
file(WRITE "${Q1960_WORLDSPACE_FINAL}" "${Q1960_WORLDSPACE_SOURCE}")

file(READ "${Q720_CELL_SOURCE}" Q1960_CELL_SOURCE)
set(Q1960_CELL_INCLUDE_OLD "#include \"${Q1951_WORLDSPACE_FINAL}\"")
set(Q1960_CELL_INCLUDE_NEW "#include \"${Q1960_WORLDSPACE_FINAL}\"")
string(FIND "${Q1960_CELL_SOURCE}" "${Q1960_CELL_INCLUDE_OLD}" Q1960_CELL_INCLUDE_POS)
if(Q1960_CELL_INCLUDE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.24 could not route q1951 CELL TU through q1960 worldspace")
endif()
string(REPLACE "${Q1960_CELL_INCLUDE_OLD}" "${Q1960_CELL_INCLUDE_NEW}"
       Q1960_CELL_SOURCE "${Q1960_CELL_SOURCE}")
set(Q1960_CELL_FINAL "${CMAKE_CURRENT_BINARY_DIR}/fo3-cell-spawn-q1960.cpp")
file(WRITE "${Q1960_CELL_FINAL}" "${Q1960_CELL_SOURCE}")
set(Q720_CELL_SOURCE "${Q1960_CELL_FINAL}")

# -----------------------------------------------------------------------------
# B. Mature renderer: stream 7x7 visuals too, but never feed the complete visual
#    set to collision. The async path already has q1950's radius-1 collision
#    filter. Add the same policy to the initial Capital Wasteland door transition
#    so the loading-screen build may create 7x7 visuals without a 7x7 BHK spike.
# -----------------------------------------------------------------------------
set(Q1960_NATIVE_FILE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp")
if(NOT EXISTS "${Q1960_NATIVE_FILE}")
    message(FATAL_ERROR "Q16.24 expected Q16.23 generated renderer")
endif()
file(READ "${Q1960_NATIVE_FILE}" Q1960_NATIVE_SOURCE)

set(Q1960_WORKER_RADIUS_OLD "SetFo3WorldspaceGridRadiusOverrideQ1950(2);")
set(Q1960_WORKER_RADIUS_NEW "SetFo3WorldspaceGridRadiusOverrideQ1950(3); // Q16.24 streamed 7x7 visuals")
string(FIND "${Q1960_NATIVE_SOURCE}" "${Q1960_WORKER_RADIUS_OLD}" Q1960_WORKER_RADIUS_POS)
if(Q1960_WORKER_RADIUS_POS EQUAL -1)
    message(FATAL_ERROR "Q16.24 could not find Q16.23 async visual-radius override")
endif()
string(REPLACE "${Q1960_WORKER_RADIUS_OLD}" "${Q1960_WORKER_RADIUS_NEW}"
       Q1960_NATIVE_SOURCE "${Q1960_NATIVE_SOURCE}")

set(Q1960_INITIAL_COLLISION_OLD [==[
    std::vector<Fo3WorldPlacement> collisionPlacements;
    collisionPlacements.reserve(work.selected.size());
    for (const CpuObject& cpu : work.selected) {
        collisionPlacements.push_back(cpu.placement);
    }

    work.collisionReady = InitializeFo3CollisionOverlay(
]==])
set(Q1960_INITIAL_COLLISION_NEW [==[
    std::vector<Fo3WorldPlacement> collisionPlacements;
    collisionPlacements.reserve(work.selected.size());
    size_t q1960OutsideInitialCollisionWindow = 0u;

    // Capital Wasteland visuals are now 7x7 from the door transition itself.
    // Keep physics local: only placements whose authored position lies in the
    // destination CELL +/-1 are allowed into the initial collision overlay.
    if (work.request.worldspaceFormId == 0x0000003Cu) {
        constexpr float Q1960_CELL_SIZE = 4096.0f;
        constexpr int Q1960_INITIAL_COLLISION_RADIUS = 1;
        const int32_t q1960TargetGridX = static_cast<int32_t>(
            std::floor(work.request.x / Q1960_CELL_SIZE));
        const int32_t q1960TargetGridY = static_cast<int32_t>(
            std::floor(work.request.y / Q1960_CELL_SIZE));
        for (const CpuObject& cpu : work.selected) {
            const Fo3WorldPlacement& placement = cpu.placement;
            const int32_t q1960PlacementGridX = static_cast<int32_t>(
                std::floor(placement.x / Q1960_CELL_SIZE));
            const int32_t q1960PlacementGridY = static_cast<int32_t>(
                std::floor(placement.y / Q1960_CELL_SIZE));
            if (std::abs(q1960PlacementGridX - q1960TargetGridX) >
                    Q1960_INITIAL_COLLISION_RADIUS ||
                std::abs(q1960PlacementGridY - q1960TargetGridY) >
                    Q1960_INITIAL_COLLISION_RADIUS) {
                ++q1960OutsideInitialCollisionWindow;
                continue;
            }
            collisionPlacements.push_back(placement);
        }
        SetNextFo3CollisionExteriorModeQ1931(true);
        Q6H_LOGI("Q16.24 INITIAL COLLISION WINDOW: worldspace=%08X targetGrid=(%d,%d) visualShapes=%zu localCollisionPlacements=%zu outside3x3=%zu radius=1",
                 work.request.worldspaceFormId,
                 q1960TargetGridX, q1960TargetGridY,
                 work.selected.size(), collisionPlacements.size(),
                 q1960OutsideInitialCollisionWindow);
    } else {
        for (const CpuObject& cpu : work.selected) {
            collisionPlacements.push_back(cpu.placement);
        }
    }

    work.collisionReady = InitializeFo3CollisionOverlay(
]==])
string(FIND "${Q1960_NATIVE_SOURCE}" "${Q1960_INITIAL_COLLISION_OLD}" Q1960_INITIAL_COLLISION_POS)
if(Q1960_INITIAL_COLLISION_POS EQUAL -1)
    message(FATAL_ERROR "Q16.24 could not find phased initial collision placement block")
endif()
string(REPLACE "${Q1960_INITIAL_COLLISION_OLD}" "${Q1960_INITIAL_COLLISION_NEW}"
       Q1960_NATIVE_SOURCE "${Q1960_NATIVE_SOURCE}")

# Earliest safe adjacent prefetch. This does NOT restore Q16.19 velocity
# extrapolation: Q16.20's stable-direction logic and actual +/-1 target clamp stay
# intact. 4096 simply means any distance inside the authored CELL may qualify once
# direction is stable.
set(Q1960_PREFETCH_OLD
    "constexpr float Q1920_PREFETCH_DISTANCE = 3584.0f; // Q16.23 5x5 visual lead time")
set(Q1960_PREFETCH_NEW
    "constexpr float Q1920_PREFETCH_DISTANCE = 4096.0f; // Q16.24 earliest stable-direction adjacent prefetch")
string(FIND "${Q1960_NATIVE_SOURCE}" "${Q1960_PREFETCH_OLD}" Q1960_PREFETCH_POS)
if(Q1960_PREFETCH_POS EQUAL -1)
    message(FATAL_ERROR "Q16.24 could not find Q16.23 prefetch distance")
endif()
string(REPLACE "${Q1960_PREFETCH_OLD}" "${Q1960_PREFETCH_NEW}"
       Q1960_NATIVE_SOURCE "${Q1960_NATIVE_SOURCE}")

string(REPLACE "rebuildMode=5x5-visual+3x3-collision+7x7-terrain-retain"
       "rebuildMode=7x7-visual+3x3-collision+9x9-terrain-retain"
       Q1960_NATIVE_SOURCE "${Q1960_NATIVE_SOURCE}")
string(REPLACE "radius=1 rebuildMode=7x7-visual+3x3-collision+9x9-terrain-retain"
       "visualRadius=3 collisionRadius=1 terrainRadius=4 rebuildMode=7x7-visual+3x3-collision+9x9-terrain-retain"
       Q1960_NATIVE_SOURCE "${Q1960_NATIVE_SOURCE}")

# Q16.23's retained terrain-centre state remains valid, but its diagnostic radius
# must describe the new 9x9 backing ring.
string(REPLACE "centre=(%d,%d) radius=3" "centre=(%d,%d) radius=4"
       Q1960_NATIVE_SOURCE "${Q1960_NATIVE_SOURCE}")
string(REPLACE "delta=(%d,%d) radius=3 fullGpuRebuild=0"
       "delta=(%d,%d) radius=4 fullGpuRebuild=0"
       Q1960_NATIVE_SOURCE "${Q1960_NATIVE_SOURCE}")
string(REPLACE "centre=(%d,%d) radius=3 fullGpuRebuild=1"
       "centre=(%d,%d) radius=4 fullGpuRebuild=1"
       Q1960_NATIVE_SOURCE "${Q1960_NATIVE_SOURCE}")

# All renderer-side stream diagnostics should identify the live build.
string(REPLACE "Q16.23" "Q16.24" Q1960_NATIVE_SOURCE "${Q1960_NATIVE_SOURCE}")
file(WRITE "${Q1960_NATIVE_FILE}" "${Q1960_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# C. LAND: radius 4 (9x9 backing). This is intentionally one ring wider than the
#    radius-3 visual set so q1950's one-cell TERRAIN RETAIN optimisation remains
#    geometrically correct after each adjacent visual recenter.
# -----------------------------------------------------------------------------
set(Q1960_TERRAIN_RADIUS_OLD
    "constexpr int Q1931_TERRAIN_GRID_RADIUS = 3; // Q16.23 retained 7x7 LAND ring")
set(Q1960_TERRAIN_RADIUS_NEW
    "constexpr int Q1931_TERRAIN_GRID_RADIUS = 4; // Q16.24 retained 9x9 LAND backing ring")
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "${Q1960_TERRAIN_RADIUS_OLD}" Q1960_TERRAIN_RADIUS_POS)
if(Q1960_TERRAIN_RADIUS_POS EQUAL -1)
    message(FATAL_ERROR "Q16.24 could not find Q16.23 terrain radius")
endif()
string(REPLACE "${Q1960_TERRAIN_RADIUS_OLD}" "${Q1960_TERRAIN_RADIUS_NEW}"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
string(REPLACE "Q16.23 TERRAIN WINDOW:" "Q16.24 TERRAIN WINDOW:"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-data-q720.cpp"
     "${Q720_TERRAIN_DATA_SOURCE}")

# Keep separate collision diagnostics aligned with the live build.
string(REPLACE "Q16.23 COLLISION MODE OVERRIDE" "Q16.24 COLLISION MODE OVERRIDE"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

# -----------------------------------------------------------------------------
# D. Visible build label Q16.24. The 300m far clip remains deliberate for this
# test; native Fallout distant LOD is still a separate future layer.
# -----------------------------------------------------------------------------
set(Q1960_Q4_FILE "${CMAKE_CURRENT_BINARY_DIR}/q1800-q4-generated.cpp")
if(NOT EXISTS "${Q1960_Q4_FILE}")
    message(FATAL_ERROR "Q16.24 expected Q16.23 OpenXR source")
endif()
file(READ "${Q1960_Q4_FILE}" Q1960_Q4_SOURCE)
set(Q1960_LABEL_OLD [==[
        q1600Digit(q1600X, 0x5Bu); // Q16.23: 2 = A B G E D
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x4Fu); // Q16.23: 3 = A B C D G
]==])
set(Q1960_LABEL_NEW [==[
        q1600Digit(q1600X, 0x5Bu); // Q16.24: 2 = A B G E D
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x66u); // Q16.24: 4 = F G B C
]==])
string(FIND "${Q1960_Q4_SOURCE}" "${Q1960_LABEL_OLD}" Q1960_LABEL_POS)
if(Q1960_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.24 could not find Q16.23 build-label digits")
endif()
string(REPLACE "${Q1960_LABEL_OLD}" "${Q1960_LABEL_NEW}"
       Q1960_Q4_SOURCE "${Q1960_Q4_SOURCE}")
string(REPLACE "Q16.23 BUILD LABEL:" "Q16.24 BUILD LABEL:"
       Q1960_Q4_SOURCE "${Q1960_Q4_SOURCE}")
string(REPLACE "text=Q16.23 anchor=left-hand" "text=Q16.24 anchor=left-hand"
       Q1960_Q4_SOURCE "${Q1960_Q4_SOURCE}")
string(REPLACE "Q16.23 AUTHORED DOOR FACING" "Q16.24 AUTHORED DOOR FACING"
       Q1960_Q4_SOURCE "${Q1960_Q4_SOURCE}")
string(REPLACE "Q16.23 FAR CLIP:" "Q16.24 FAR CLIP:"
       Q1960_Q4_SOURCE "${Q1960_Q4_SOURCE}")
file(WRITE "${Q1960_Q4_FILE}" "${Q1960_Q4_SOURCE}")

# -----------------------------------------------------------------------------
# E. Configure-time proof. Fail closed rather than silently shipping a mixed
# radius build, especially because initial visuals and collision now intentionally
# use different extents.
# -----------------------------------------------------------------------------
file(READ "${Q720_CELL_SOURCE}" Q1960_CELL_VERIFY)
string(FIND "${Q1960_CELL_VERIFY}" "fo3-worldspace-q1960.cpp" Q1960_CELL_ROUTE_OK)
string(FIND "${Q1960_WORLDSPACE_SOURCE}"
       "gQ1950WorldspaceGridRadiusOverride : 3" Q1960_INITIAL_RADIUS_OK)
string(FIND "${Q1960_NATIVE_SOURCE}"
       "SetFo3WorldspaceGridRadiusOverrideQ1950(3);" Q1960_WORKER_OK)
string(FIND "${Q1960_NATIVE_SOURCE}"
       "Q16.24 INITIAL COLLISION WINDOW:" Q1960_INITIAL_COLLISION_OK)
string(FIND "${Q1960_NATIVE_SOURCE}"
       "Q1950_COLLISION_GRID_RADIUS = 1" Q1960_STREAM_COLLISION_OK)
string(FIND "${Q720_TERRAIN_DATA_SOURCE}"
       "Q1931_TERRAIN_GRID_RADIUS = 4" Q1960_TERRAIN_OK)
string(FIND "${Q1960_NATIVE_SOURCE}"
       "Q1920_PREFETCH_DISTANCE = 4096.0f" Q1960_PREFETCH_OK)
string(FIND "${Q1960_NATIVE_SOURCE}"
       "rebuildMode=7x7-visual+3x3-collision+9x9-terrain-retain" Q1960_MODE_OK)
string(FIND "${Q1960_Q4_SOURCE}"
       "Q16.24: 4 = F G B C" Q1960_LABEL_OK)
if(Q1960_CELL_ROUTE_OK EQUAL -1 OR Q1960_INITIAL_RADIUS_OK EQUAL -1 OR
   Q1960_WORKER_OK EQUAL -1 OR Q1960_INITIAL_COLLISION_OK EQUAL -1 OR
   Q1960_STREAM_COLLISION_OK EQUAL -1 OR Q1960_TERRAIN_OK EQUAL -1 OR
   Q1960_PREFETCH_OK EQUAL -1 OR Q1960_MODE_OK EQUAL -1 OR
   Q1960_LABEL_OK EQUAL -1)
    message(FATAL_ERROR "Q16.24 seven-by-seven residency verification failed")
endif()

message(STATUS "Q16.24 enabled: initial+stream 7x7 visuals + local 3x3 collision + retained 9x9 LAND backing + earliest adjacent prefetch")
