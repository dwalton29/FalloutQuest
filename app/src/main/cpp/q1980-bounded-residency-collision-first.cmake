# Q16.26: bounded resident window + collision-first streaming.
#
# Q16.25 device evidence showed the active-centred draw filter is the right
# direction (subjectively smoother), but the wider 7x7 resident streamer still
# cannot keep pace with sustained locomotion:
#   - resident geometry grew above 7k shapes / 3.3M triangles;
#   - a player could advance 3-4 authored cells beyond the resident centre;
#   - collision was rebuilt only AFTER CPU/GPU visual staging, so a 3x3 physics
#     window could be several cells behind by the time it was published;
#   - direct multi-cell SAFETY CATCHUP produced 2k+ entering placements / ~3k
#     staged GPU shapes, creating exactly the memory/work burst associated with
#     the reported hang/crash.
#
# Keep the proven actual-XCLC authority from Q16.25, but reduce the expensive
# near world to a Quest-oriented hierarchy:
#   5x5 GPU resident objects, 3x3 actual-centred full-detail draw, 3x3 authored
#   collision, 5x5 retained LAND. The staged adjacent target remains a one-cell
#   runway around the active 3x3. Collision is built immediately after metadata,
#   before entering NIF CPU/GPU work. Catch-up is strictly one cell/axis at a
#   time and a generation is cancelled as soon as it can no longer contain the
#   actual-centred active set.
#
# Native Level4 LOD remains probe-only in Q16.26. Q16.25 proved the Bethesda
# block files parse and use authored world coordinates, so distant rendering can
# be enabled as a clean next layer after this stability correction.

# -----------------------------------------------------------------------------
# A. Final compiled worldspace selector: 5x5 resident instead of 7x7.
# -----------------------------------------------------------------------------
if(NOT DEFINED Q1960_WORLDSPACE_FINAL OR NOT EXISTS "${Q1960_WORLDSPACE_FINAL}")
    message(FATAL_ERROR "Q16.26 expected q1960 worldspace implementation")
endif()
if(NOT DEFINED Q720_CELL_SOURCE OR NOT EXISTS "${Q720_CELL_SOURCE}")
    message(FATAL_ERROR "Q16.26 expected routed CELL translation unit")
endif()

file(READ "${Q1960_WORLDSPACE_FINAL}" Q1980_WORLDSPACE_SOURCE)
set(Q1980_DEFAULT_RADIUS_OLD [==[
    const int q1950SelectionRadius = gQ1950WorldspaceGridRadiusOverride >= 0
        ? gQ1950WorldspaceGridRadiusOverride : 3; // Q16.24 initial exterior 7x7
]==])
set(Q1980_DEFAULT_RADIUS_NEW [==[
    const int q1950SelectionRadius = gQ1950WorldspaceGridRadiusOverride >= 0
        ? gQ1950WorldspaceGridRadiusOverride : 2; // Q16.26 initial exterior 5x5 resident
]==])
string(FIND "${Q1980_WORLDSPACE_SOURCE}" "${Q1980_DEFAULT_RADIUS_OLD}" Q1980_DEFAULT_RADIUS_POS)
if(Q1980_DEFAULT_RADIUS_POS EQUAL -1)
    message(FATAL_ERROR "Q16.26 could not find q1960 initial 7x7 selector")
endif()
string(REPLACE "${Q1980_DEFAULT_RADIUS_OLD}" "${Q1980_DEFAULT_RADIUS_NEW}"
       Q1980_WORLDSPACE_SOURCE "${Q1980_WORLDSPACE_SOURCE}")
string(REPLACE "Q16.24 VISUAL WINDOW:" "Q16.26 VISUAL WINDOW:"
       Q1980_WORLDSPACE_SOURCE "${Q1980_WORLDSPACE_SOURCE}")

set(Q1980_WORLDSPACE_FINAL "${CMAKE_CURRENT_BINARY_DIR}/fo3-worldspace-q1980.cpp")
file(WRITE "${Q1980_WORLDSPACE_FINAL}" "${Q1980_WORLDSPACE_SOURCE}")

file(READ "${Q720_CELL_SOURCE}" Q1980_CELL_SOURCE)
set(Q1980_CELL_INCLUDE_OLD "#include \"${Q1960_WORLDSPACE_FINAL}\"")
set(Q1980_CELL_INCLUDE_NEW "#include \"${Q1980_WORLDSPACE_FINAL}\"")
string(FIND "${Q1980_CELL_SOURCE}" "${Q1980_CELL_INCLUDE_OLD}" Q1980_CELL_INCLUDE_POS)
if(Q1980_CELL_INCLUDE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.26 could not reroute CELL TU through q1980 worldspace")
endif()
string(REPLACE "${Q1980_CELL_INCLUDE_OLD}" "${Q1980_CELL_INCLUDE_NEW}"
       Q1980_CELL_SOURCE "${Q1980_CELL_SOURCE}")
set(Q1980_CELL_FINAL "${CMAKE_CURRENT_BINARY_DIR}/fo3-cell-spawn-q1980.cpp")
file(WRITE "${Q1980_CELL_FINAL}" "${Q1980_CELL_SOURCE}")
set(Q720_CELL_SOURCE "${Q1980_CELL_FINAL}")

# -----------------------------------------------------------------------------
# B. Mature native streamer: 5x5 resident, 3x3 active, collision-first.
# -----------------------------------------------------------------------------
set(Q1980_NATIVE_FILE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp")
if(NOT EXISTS "${Q1980_NATIVE_FILE}")
    message(FATAL_ERROR "Q16.26 expected Q16.25 generated renderer")
endif()
file(READ "${Q1980_NATIVE_FILE}" Q1980_NATIVE_SOURCE)

# Stream metadata selects a radius-2 (5x5) resident target.
set(Q1980_WORKER_RADIUS_OLD
    "SetFo3WorldspaceGridRadiusOverrideQ1950(3); // Q16.25 streamed 7x7 visuals")
set(Q1980_WORKER_RADIUS_NEW
    "SetFo3WorldspaceGridRadiusOverrideQ1950(2); // Q16.26 streamed 5x5 resident")
string(FIND "${Q1980_NATIVE_SOURCE}" "${Q1980_WORKER_RADIUS_OLD}" Q1980_WORKER_RADIUS_POS)
if(Q1980_WORKER_RADIUS_POS EQUAL -1)
    message(FATAL_ERROR "Q16.26 could not find Q16.25 streamed radius override")
endif()
string(REPLACE "${Q1980_WORKER_RADIUS_OLD}" "${Q1980_WORKER_RADIUS_NEW}"
       Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")

# Full-detail statics/shadows use only the player's actual-centred 3x3.
string(FIND "${Q1980_NATIVE_SOURCE}"
       "constexpr int Q1970_ACTIVE_VISUAL_RADIUS = 2; // 5x5 full-detail draw set"
       Q1980_ACTIVE_RADIUS_POS)
if(Q1980_ACTIVE_RADIUS_POS EQUAL -1)
    message(FATAL_ERROR "Q16.26 could not find Q16.25 active draw radius")
endif()
string(REPLACE
    "constexpr int Q1970_ACTIVE_VISUAL_RADIUS = 2; // 5x5 full-detail draw set"
    "constexpr int Q1970_ACTIVE_VISUAL_RADIUS = 1; // Q16.26 3x3 full-detail draw set"
    Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")
string(REPLACE
    "std::abs(object.q1970GridX - q1970ShadowGridX) > 2 ||"
    "std::abs(object.q1970GridX - q1970ShadowGridX) > 1 ||"
    Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")
string(REPLACE
    "std::abs(object.q1970GridY - q1970ShadowGridY) > 2) continue;"
    "std::abs(object.q1970GridY - q1970ShadowGridY) > 1) continue;"
    Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")
string(REPLACE
    "activeRadius=2 residentRadius=3"
    "activeRadius=1 residentRadius=2"
    Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")

# Q16.25 waited for every entering NIF to reach GPU before collision. Reverse the
# pipeline: metadata -> collision -> CPU -> GPU -> terrain -> commit. Collision
# does not depend on visual upload and therefore must not be gated by visibleRefs.
set(Q1980_METADATA_PHASE_OLD [==[
    gPendingStreamQ1900.incomingCpu.reserve(
        gPendingStreamQ1900.incomingPlacements.size() * 2u);
    gPendingStreamQ1900.phase = gPendingStreamQ1900.incomingPlacements.empty()
        ? Q1900StreamPhase::Collision : Q1900StreamPhase::Cpu;
]==])
set(Q1980_METADATA_PHASE_NEW [==[
    gPendingStreamQ1900.incomingCpu.reserve(
        gPendingStreamQ1900.incomingPlacements.size() * 2u);
    // Q16.26: publish authored local physics before expensive entering visuals.
    gPendingStreamQ1900.phase = Q1900StreamPhase::Collision;
]==])
string(FIND "${Q1980_NATIVE_SOURCE}" "${Q1980_METADATA_PHASE_OLD}" Q1980_METADATA_PHASE_POS)
if(Q1980_METADATA_PHASE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.26 could not find metadata -> CPU/GPU phase selection")
endif()
string(REPLACE "${Q1980_METADATA_PHASE_OLD}" "${Q1980_METADATA_PHASE_NEW}"
       Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")

set(Q1980_COLLISION_FILTER_OLD [==[
        if (placement.refFormId == 0u ||
            gPendingStreamQ1900.visibleRefs.find(placement.refFormId) ==
                gPendingStreamQ1900.visibleRefs.end() ||
            !seenRefs.emplace(placement.refFormId, 1u).second ||
            Q74ShouldSkipPlacement(placement)) {
]==])
set(Q1980_COLLISION_FILTER_NEW [==[
        if (placement.refFormId == 0u ||
            !seenRefs.emplace(placement.refFormId, 1u).second ||
            Q74ShouldSkipPlacement(placement)) {
]==])
string(FIND "${Q1980_NATIVE_SOURCE}" "${Q1980_COLLISION_FILTER_OLD}" Q1980_COLLISION_FILTER_POS)
if(Q1980_COLLISION_FILTER_POS EQUAL -1)
    message(FATAL_ERROR "Q16.26 could not find collision visibleRefs gate")
endif()
string(REPLACE "${Q1980_COLLISION_FILTER_OLD}" "${Q1980_COLLISION_FILTER_NEW}"
       Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")

# When the target is the normal adjacent prefetch, centre the 3x3 collision on
# that target. It contains both the current actual cell and one full cell beyond
# the boundary, giving physics useful runway while visuals continue staging.
set(Q1980_COLLISION_CENTER_OLD [==[
    const int32_t q1970CollisionGridX = gQ1920LatestGridValid
        ? gQ1920LatestGridX : gPendingStreamQ1900.targetGridX;
    const int32_t q1970CollisionGridY = gQ1920LatestGridValid
        ? gQ1920LatestGridY : gPendingStreamQ1900.targetGridY;
]==])
set(Q1980_COLLISION_CENTER_NEW [==[
    const int32_t q1970CollisionGridX = gPendingStreamQ1900.targetGridX;
    const int32_t q1970CollisionGridY = gPendingStreamQ1900.targetGridY;
]==])
string(FIND "${Q1980_NATIVE_SOURCE}" "${Q1980_COLLISION_CENTER_OLD}" Q1980_COLLISION_CENTER_POS)
if(Q1980_COLLISION_CENTER_POS EQUAL -1)
    message(FATAL_ERROR "Q16.26 could not find Q16.25 actual-centred staged collision")
endif()
string(REPLACE "${Q1980_COLLISION_CENTER_OLD}" "${Q1980_COLLISION_CENTER_NEW}"
       Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")

# Collision now returns to visual CPU staging rather than jumping straight to LAND.
set(Q1980_COLLISION_NEXT_OLD [==[
    gPendingStreamQ1900.phase = Q1900StreamPhase::Terrain;

    Q6H_LOGI("Q16.25 COLLISION READY:
]==])
set(Q1980_COLLISION_NEXT_NEW [==[
    gPendingStreamQ1900.phase = gPendingStreamQ1900.incomingPlacements.empty()
        ? Q1900StreamPhase::Terrain : Q1900StreamPhase::Cpu;

    Q6H_LOGI("Q16.25 COLLISION READY:
]==])
string(FIND "${Q1980_NATIVE_SOURCE}" "${Q1980_COLLISION_NEXT_OLD}" Q1980_COLLISION_NEXT_POS)
if(Q1980_COLLISION_NEXT_POS EQUAL -1)
    message(FATAL_ERROR "Q16.26 could not find collision -> terrain phase transition")
endif()
string(REPLACE "${Q1980_COLLISION_NEXT_OLD}" "${Q1980_COLLISION_NEXT_NEW}"
       Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")

# CPU no longer loops back to collision; collision has already been published.
set(Q1980_CPU_NEXT_OLD [==[
        gPendingStreamQ1900.phase = gPendingStreamQ1900.incomingCpu.empty()
            ? Q1900StreamPhase::Collision : Q1900StreamPhase::Gpu;
]==])
set(Q1980_CPU_NEXT_NEW [==[
        gPendingStreamQ1900.phase = gPendingStreamQ1900.incomingCpu.empty()
            ? Q1900StreamPhase::Terrain : Q1900StreamPhase::Gpu;
]==])
string(FIND "${Q1980_NATIVE_SOURCE}" "${Q1980_CPU_NEXT_OLD}" Q1980_CPU_NEXT_POS)
if(Q1980_CPU_NEXT_POS EQUAL -1)
    message(FATAL_ERROR "Q16.26 could not find CPU completion phase")
endif()
string(REPLACE "${Q1980_CPU_NEXT_OLD}" "${Q1980_CPU_NEXT_NEW}"
       Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")

set(Q1980_GPU_NEXT_OLD [==[
        gPendingStreamQ1900.phase = Q1900StreamPhase::Collision;
        Q6H_LOGI("Q16.25 GPU READY:
]==])
set(Q1980_GPU_NEXT_NEW [==[
        gPendingStreamQ1900.phase = Q1900StreamPhase::Terrain;
        Q6H_LOGI("Q16.25 GPU READY:
]==])
string(FIND "${Q1980_NATIVE_SOURCE}" "${Q1980_GPU_NEXT_OLD}" Q1980_GPU_NEXT_POS)
if(Q1980_GPU_NEXT_POS EQUAL -1)
    message(FATAL_ERROR "Q16.26 could not find GPU -> collision phase transition")
endif()
string(REPLACE "${Q1980_GPU_NEXT_OLD}" "${Q1980_GPU_NEXT_NEW}"
       Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# C. Cancel stale residency immediately, not after hundreds of GPU uploads.
# Q16.22 deliberately allowed four-cell lag to prevent starvation. That was
# useful for a monolithic visual window but is wrong once radius-2 residency is
# explicitly a one-cell runway around radius-1 active visuals.
# -----------------------------------------------------------------------------
set(Q1980_STALE_OLD [==[
    if (gQ1920LatestGridValid &&
        (std::abs(gPendingStreamQ1900.targetGridX - gQ1920LatestGridX) > 4 ||
         std::abs(gPendingStreamQ1900.targetGridY - gQ1920LatestGridY) > 4)) {
        Q6H_LOGW("Q16.25 STREAM TELEPORT STALE: generation=%llu target=(%d,%d) actual=(%d,%d) action=cancel-five-plus-cells",
                 static_cast<unsigned long long>(gPendingStreamQ1900.generation),
                 gPendingStreamQ1900.targetGridX, gPendingStreamQ1900.targetGridY,
                 gQ1920LatestGridX, gQ1920LatestGridY);
        Q1900CancelPending("target-five-plus-cells-away");
        return;
    }
]==])
set(Q1980_STALE_NEW [==[
    if (gQ1920LatestGridValid &&
        (std::abs(gPendingStreamQ1900.targetGridX - gQ1920LatestGridX) > 1 ||
         std::abs(gPendingStreamQ1900.targetGridY - gQ1920LatestGridY) > 1)) {
        Q6H_LOGW("Q16.26 RESIDENT STALE EARLY: generation=%llu target=(%d,%d) actual=(%d,%d) action=cancel-before-more-work reason=5x5-no-longer-covers-active-3x3",
                 static_cast<unsigned long long>(gPendingStreamQ1900.generation),
                 gPendingStreamQ1900.targetGridX, gPendingStreamQ1900.targetGridY,
                 gQ1920LatestGridX, gQ1920LatestGridY);
        Q1900CancelPending("resident-no-longer-covers-active-3x3");
        return;
    }
]==])
string(FIND "${Q1980_NATIVE_SOURCE}" "${Q1980_STALE_OLD}" Q1980_STALE_POS)
if(Q1980_STALE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.26 could not find Q16.25 four-cell stale guard")
endif()
string(REPLACE "${Q1980_STALE_OLD}" "${Q1980_STALE_NEW}"
       Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")

# Q16.25's collision/commit guards retain the same one-cell geometry rule; update
# their diagnostics/reason so logs describe the new 5x5/3x3 hierarchy.
string(REPLACE "7x7-no-longer-covers-active-5x5"
       "5x5-no-longer-covers-active-3x3"
       Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")
string(REPLACE "resident-no-longer-covers-active-5x5"
       "resident-no-longer-covers-active-3x3"
       Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# D. Never issue a multi-cell safety rebuild. Walk the resident centre toward
# actualGrid one authored cell and one axis per generation. This caps entering
# strip size and prevents Q16.25's 2018-placement / 2970-shape burst.
# -----------------------------------------------------------------------------
set(Q1980_CATCHUP_OLD [==[
        const float selectionX =
            (static_cast<float>(actualGridX) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
        const float selectionY =
            (static_cast<float>(actualGridY) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
        Q6H_LOGW("Q16.25 SAFETY CATCHUP: window=(%d,%d) actual=(%d,%d) selection=(%.2f %.2f)",
                 gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
                 actualGridX, actualGridY, selectionX, selectionY);
        Q1900BeginStream(selectionX, selectionY, actualGridX, actualGridY);
        return;
]==])
set(Q1980_CATCHUP_NEW [==[
        const int q1980CatchupDx = actualGridX - gExteriorWindowGridXQ1890;
        const int q1980CatchupDy = actualGridY - gExteriorWindowGridYQ1890;
        int32_t q1980CatchupGridX = gExteriorWindowGridXQ1890;
        int32_t q1980CatchupGridY = gExteriorWindowGridYQ1890;
        if (std::abs(q1980CatchupDx) >= std::abs(q1980CatchupDy) && q1980CatchupDx != 0) {
            q1980CatchupGridX += q1980CatchupDx > 0 ? 1 : -1;
        } else if (q1980CatchupDy != 0) {
            q1980CatchupGridY += q1980CatchupDy > 0 ? 1 : -1;
        }
        const float selectionX =
            (static_cast<float>(q1980CatchupGridX) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
        const float selectionY =
            (static_cast<float>(q1980CatchupGridY) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
        Q6H_LOGW("Q16.26 STEP CATCHUP: window=(%d,%d) actual=(%d,%d) next=(%d,%d) selection=(%.2f %.2f)",
                 gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
                 actualGridX, actualGridY, q1980CatchupGridX, q1980CatchupGridY,
                 selectionX, selectionY);
        Q1900BeginStream(selectionX, selectionY, q1980CatchupGridX, q1980CatchupGridY);
        return;
]==])
string(FIND "${Q1980_NATIVE_SOURCE}" "${Q1980_CATCHUP_OLD}" Q1980_CATCHUP_POS)
if(Q1980_CATCHUP_POS EQUAL -1)
    message(FATAL_ERROR "Q16.26 could not find Q16.25 safety catchup block")
endif()
string(REPLACE "${Q1980_CATCHUP_OLD}" "${Q1980_CATCHUP_NEW}"
       Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")

# Stream context / diagnostics for the new hierarchy.
string(REPLACE
    "visualRadius=3 collisionRadius=1 terrainRadius=4 rebuildMode=7x7-resident+5x5-active+3x3-actual-collision+9x9-terrain"
    "residentRadius=2 activeRadius=1 collisionRadius=1 terrainRadius=2 rebuildMode=5x5-resident+3x3-active+3x3-collision-first+5x5-terrain"
    Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")
string(REPLACE "Q16.25" "Q16.26"
       Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")
file(WRITE "${Q1980_NATIVE_FILE}" "${Q1980_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# E. LAND backing: 5x5. With a 3x3 active set, a retained radius-2 terrain window
# still covers the player after a one-cell resident prefetch, so the q1950
# one-shift retention rule remains valid without carrying 81 full LAND cells.
# -----------------------------------------------------------------------------
set(Q1980_TERRAIN_RADIUS_OLD
    "constexpr int Q1931_TERRAIN_GRID_RADIUS = 4; // Q16.24 retained 9x9 LAND backing ring")
set(Q1980_TERRAIN_RADIUS_NEW
    "constexpr int Q1931_TERRAIN_GRID_RADIUS = 2; // Q16.26 retained 5x5 LAND backing")
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "${Q1980_TERRAIN_RADIUS_OLD}" Q1980_TERRAIN_RADIUS_POS)
if(Q1980_TERRAIN_RADIUS_POS EQUAL -1)
    message(FATAL_ERROR "Q16.26 could not find Q16.24 9x9 LAND radius")
endif()
string(REPLACE "${Q1980_TERRAIN_RADIUS_OLD}" "${Q1980_TERRAIN_RADIUS_NEW}"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
string(REPLACE "Q16.24 TERRAIN WINDOW:" "Q16.26 TERRAIN WINDOW:"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-data-q720.cpp"
     "${Q720_TERRAIN_DATA_SOURCE}")

# q1950/q1960 terrain-residency logs live in the renderer source and were already
# relabelled above. Correct only their radius text; recenter/retain logic is unchanged.
file(READ "${Q1980_NATIVE_FILE}" Q1980_NATIVE_SOURCE)
string(REPLACE "centre=(%d,%d) radius=4" "centre=(%d,%d) radius=2"
       Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")
string(REPLACE "delta=(%d,%d) radius=4 fullGpuRebuild=0"
       "delta=(%d,%d) radius=2 fullGpuRebuild=0"
       Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")
string(REPLACE "centre=(%d,%d) radius=4 fullGpuRebuild=1"
       "centre=(%d,%d) radius=2 fullGpuRebuild=1"
       Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")
file(WRITE "${Q1980_NATIVE_FILE}" "${Q1980_NATIVE_SOURCE}")

string(REPLACE "Q16.24 COLLISION MODE OVERRIDE" "Q16.26 COLLISION MODE OVERRIDE"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

# -----------------------------------------------------------------------------
# F. Headset-visible build label Q16.26. Keep 300m far clip and native LOD probe.
# -----------------------------------------------------------------------------
set(Q1980_Q4_FILE "${CMAKE_CURRENT_BINARY_DIR}/q1800-q4-generated.cpp")
if(NOT EXISTS "${Q1980_Q4_FILE}")
    message(FATAL_ERROR "Q16.26 expected Q16.25 OpenXR source")
endif()
file(READ "${Q1980_Q4_FILE}" Q1980_Q4_SOURCE)
set(Q1980_LABEL_OLD [==[
        q1600Digit(q1600X, 0x5Bu); // Q16.25: 2 = A B G E D
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x6Du); // Q16.25: 5 = A F G C D
]==])
set(Q1980_LABEL_NEW [==[
        q1600Digit(q1600X, 0x5Bu); // Q16.26: 2 = A B G E D
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x7Du); // Q16.26: 6 = A F G E D C
]==])
string(FIND "${Q1980_Q4_SOURCE}" "${Q1980_LABEL_OLD}" Q1980_LABEL_POS)
if(Q1980_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.26 could not find Q16.25 build-label digits")
endif()
string(REPLACE "${Q1980_LABEL_OLD}" "${Q1980_LABEL_NEW}"
       Q1980_Q4_SOURCE "${Q1980_Q4_SOURCE}")
string(REPLACE "Q16.25 BUILD LABEL:" "Q16.26 BUILD LABEL:"
       Q1980_Q4_SOURCE "${Q1980_Q4_SOURCE}")
string(REPLACE "text=Q16.25 anchor=left-hand" "text=Q16.26 anchor=left-hand"
       Q1980_Q4_SOURCE "${Q1980_Q4_SOURCE}")
string(REPLACE "Q16.25 AUTHORED DOOR FACING" "Q16.26 AUTHORED DOOR FACING"
       Q1980_Q4_SOURCE "${Q1980_Q4_SOURCE}")
string(REPLACE "Q16.25 FAR CLIP:" "Q16.26 FAR CLIP:"
       Q1980_Q4_SOURCE "${Q1980_Q4_SOURCE}")
file(WRITE "${Q1980_Q4_FILE}" "${Q1980_Q4_SOURCE}")

# -----------------------------------------------------------------------------
# G. Configure-time proof. Mixed-radius or old phase-order builds are worse than
# a hard configure failure, so verify every architectural invariant.
# -----------------------------------------------------------------------------
file(READ "${Q720_CELL_SOURCE}" Q1980_CELL_VERIFY)
file(READ "${Q1980_NATIVE_FILE}" Q1980_NATIVE_VERIFY)
string(FIND "${Q1980_CELL_VERIFY}" "fo3-worldspace-q1980.cpp" Q1980_CELL_ROUTE_OK)
string(FIND "${Q1980_WORLDSPACE_SOURCE}"
       "gQ1950WorldspaceGridRadiusOverride : 2" Q1980_INITIAL_RADIUS_OK)
string(FIND "${Q1980_NATIVE_VERIFY}"
       "SetFo3WorldspaceGridRadiusOverrideQ1950(2);" Q1980_WORKER_OK)
string(FIND "${Q1980_NATIVE_VERIFY}"
       "Q1970_ACTIVE_VISUAL_RADIUS = 1" Q1980_ACTIVE_OK)
string(FIND "${Q1980_NATIVE_VERIFY}"
       "gPendingStreamQ1900.phase = Q1900StreamPhase::Collision;" Q1980_COLLISION_FIRST_OK)
string(FIND "${Q1980_NATIVE_VERIFY}"
       "Q16.26 RESIDENT STALE EARLY:" Q1980_STALE_OK)
string(FIND "${Q1980_NATIVE_VERIFY}"
       "Q16.26 STEP CATCHUP:" Q1980_STEP_OK)
string(FIND "${Q1980_NATIVE_VERIFY}"
       "5x5-resident+3x3-active+3x3-collision-first+5x5-terrain" Q1980_MODE_OK)
string(FIND "${Q720_TERRAIN_DATA_SOURCE}"
       "Q1931_TERRAIN_GRID_RADIUS = 2" Q1980_TERRAIN_OK)
string(FIND "${Q1980_Q4_SOURCE}" "Q16.26: 6 = A F G E D C" Q1980_LABEL_OK)
if(Q1980_CELL_ROUTE_OK EQUAL -1 OR Q1980_INITIAL_RADIUS_OK EQUAL -1 OR
   Q1980_WORKER_OK EQUAL -1 OR Q1980_ACTIVE_OK EQUAL -1 OR
   Q1980_COLLISION_FIRST_OK EQUAL -1 OR Q1980_STALE_OK EQUAL -1 OR
   Q1980_STEP_OK EQUAL -1 OR Q1980_MODE_OK EQUAL -1 OR
   Q1980_TERRAIN_OK EQUAL -1 OR Q1980_LABEL_OK EQUAL -1)
    message(FATAL_ERROR "Q16.26 bounded-residency/collision-first verification failed")
endif()

message(STATUS "Q16.26 enabled: 5x5 resident, 3x3 active/shadow, collision-first adjacent runway, one-cell catchup, 5x5 LAND; native Level4 probe retained")
