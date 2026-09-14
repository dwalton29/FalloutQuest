# Q16.25: separate the live full-detail active set from the wider resident set.
#
# Q16.24 device evidence proved that increasing the whole full-detail window to
# 7x7 does not make traversal seamless on Quest:
#   - a 7x7 resident target can take 25-45 seconds to stage while the player can
#     cross an authored 4096-unit CELL in about 12 seconds at full run speed;
#   - predictive resident recentres therefore put the player visibly off-centre;
#   - rendering/shadowing ~6500 full-detail shapes at once causes severe judder;
#   - the 300m camera can still see past a 7x7 high-detail grid (~205m cardinal).
#
# Keep Q16.24's useful 7x7 GPU residency as a one-cell runway around a 5x5
# active high-detail set, but make the PLAYER'S ACTUAL XCLC authoritative for
# what is drawn and for local collision. Prediction may warm/recentre residency;
# it no longer moves the visible high-detail centre. Keep 9x9 LAND backing until
# native Fallout LOD is proven. Also probe the real Level4 Wasteland LOD NIFs
# from Fallout - Meshes.bsa under the loading screen and report parse/bounds.

set(Q1970_NATIVE_FILE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp")
if(NOT EXISTS "${Q1970_NATIVE_FILE}")
    message(FATAL_ERROR "Q16.25 expected Q16.24 generated renderer")
endif()
file(READ "${Q1970_NATIVE_FILE}" Q1970_NATIVE_SOURCE)

# -----------------------------------------------------------------------------
# A. Make the actual authored player CELL available to the renderer helpers.
# q1920 defines these later in the same anonymous namespace; declarations here
# let the draw/shadow paths use them without changing that proven ownership.
# -----------------------------------------------------------------------------
set(Q1970_FORWARD_OLD [==[
extern uint64_t gExteriorWindowGenerationQ1890;
]==])
set(Q1970_FORWARD_NEW [==[
extern uint64_t gExteriorWindowGenerationQ1890;
extern bool gQ1920LatestGridValid;
extern int32_t gQ1920LatestGridX;
extern int32_t gQ1920LatestGridY;
]==])
string(FIND "${Q1970_NATIVE_SOURCE}" "${Q1970_FORWARD_OLD}" Q1970_FORWARD_POS)
if(Q1970_FORWARD_POS EQUAL -1)
    message(FATAL_ERROR "Q16.25 could not find exterior stream forward declarations")
endif()
string(REPLACE "${Q1970_FORWARD_OLD}" "${Q1970_FORWARD_NEW}"
       Q1970_NATIVE_SOURCE "${Q1970_NATIVE_SOURCE}")

# Every resident GPU shape remembers the authored exterior CELL containing its
# REFR placement. This is tiny immutable metadata; VAO/VBO ownership is unchanged.
# Anchor only to modelPath itself because later material patches legitimately add
# fields after it in the mature GpuObject struct.
set(Q1970_GPU_FIELD_OLD [==[
    std::string modelPath;
]==])
set(Q1970_GPU_FIELD_NEW [==[
    std::string modelPath;
    int32_t q1970GridX = 0;
    int32_t q1970GridY = 0;
]==])
string(FIND "${Q1970_NATIVE_SOURCE}" "${Q1970_GPU_FIELD_OLD}" Q1970_GPU_FIELD_POS)
if(Q1970_GPU_FIELD_POS EQUAL -1)
    message(FATAL_ERROR "Q16.25 could not find GpuObject modelPath field")
endif()
string(REPLACE "${Q1970_GPU_FIELD_OLD}" "${Q1970_GPU_FIELD_NEW}"
       Q1970_NATIVE_SOURCE "${Q1970_NATIVE_SOURCE}")

set(Q1970_GPU_ASSIGN_OLD [==[
    gpu.modelPath = cpu.placement.modelPath;
]==])
set(Q1970_GPU_ASSIGN_NEW [==[
    gpu.modelPath = cpu.placement.modelPath;
    gpu.q1970GridX = static_cast<int32_t>(std::floor(cpu.placement.x / Q1890_EXTERIOR_CELL_SIZE));
    gpu.q1970GridY = static_cast<int32_t>(std::floor(cpu.placement.y / Q1890_EXTERIOR_CELL_SIZE));
]==])
string(FIND "${Q1970_NATIVE_SOURCE}" "${Q1970_GPU_ASSIGN_OLD}" Q1970_GPU_ASSIGN_POS)
if(Q1970_GPU_ASSIGN_POS EQUAL -1)
    message(FATAL_ERROR "Q16.25 could not find GPU placement identity assignment")
endif()
string(REPLACE "${Q1970_GPU_ASSIGN_OLD}" "${Q1970_GPU_ASSIGN_NEW}"
       Q1970_NATIVE_SOURCE "${Q1970_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# B. 7x7 RESIDENT, 5x5 ACTIVE. Do not render all resident geometry.
# CapitalWasteland uses actualGrid radius 2; child/interior worlds retain their
# established behaviour. The ordinary eye draw helper uses one common predicate.
# The Q10.5 shadow runtime is defined earlier than DrawSceneObject, so its same
# 5x5 test is intentionally inlined rather than introducing an ordering hazard.
# -----------------------------------------------------------------------------
set(Q1970_DRAW_MARKER [==[
void DrawSceneObject(const GpuObject& object) {
]==])
set(Q1970_DRAW_HELPERS [==[
bool Q1970GetActiveGrid(int32_t& gridX, int32_t& gridY) {
    if (gExteriorWorldspaceQ1890 != 0x0000003Cu) return false;
    if (gQ1920LatestGridValid) {
        gridX = gQ1920LatestGridX;
        gridY = gQ1920LatestGridY;
    } else {
        gridX = gExteriorWindowGridXQ1890;
        gridY = gExteriorWindowGridYQ1890;
    }
    return true;
}

bool Q1970ShouldRenderFullDetail(const GpuObject& object) {
    int32_t activeGridX = 0;
    int32_t activeGridY = 0;
    if (!Q1970GetActiveGrid(activeGridX, activeGridY)) return true;
    constexpr int Q1970_ACTIVE_VISUAL_RADIUS = 2; // 5x5 full-detail draw set
    return std::abs(object.q1970GridX - activeGridX) <= Q1970_ACTIVE_VISUAL_RADIUS &&
           std::abs(object.q1970GridY - activeGridY) <= Q1970_ACTIVE_VISUAL_RADIUS;
}

void DrawSceneObject(const GpuObject& object) {
    if (!Q1970ShouldRenderFullDetail(object)) return;
]==])
string(FIND "${Q1970_NATIVE_SOURCE}" "${Q1970_DRAW_MARKER}" Q1970_DRAW_POS)
if(Q1970_DRAW_POS EQUAL -1)
    message(FATAL_ERROR "Q16.25 could not find static draw helper")
endif()
string(REPLACE "${Q1970_DRAW_MARKER}" "${Q1970_DRAW_HELPERS}"
       Q1970_NATIVE_SOURCE "${Q1970_NATIVE_SOURCE}")

set(Q1970_SHADOW_LOOP_OLD [==[
    for (const GpuObject& object:gObjects) {
        if (object.alphaBlend || !object.vao || object.vertexCount<=0) continue;
]==])
set(Q1970_SHADOW_LOOP_NEW [==[
    for (const GpuObject& object:gObjects) {
        if (gExteriorWorldspaceQ1890 == 0x0000003Cu) {
            const int32_t q1970ShadowGridX = gQ1920LatestGridValid
                ? gQ1920LatestGridX : gExteriorWindowGridXQ1890;
            const int32_t q1970ShadowGridY = gQ1920LatestGridValid
                ? gQ1920LatestGridY : gExteriorWindowGridYQ1890;
            if (std::abs(object.q1970GridX - q1970ShadowGridX) > 2 ||
                std::abs(object.q1970GridY - q1970ShadowGridY) > 2) continue;
        }
        if (object.alphaBlend || !object.vao || object.vertexCount<=0) continue;
]==])
string(FIND "${Q1970_NATIVE_SOURCE}" "${Q1970_SHADOW_LOOP_OLD}" Q1970_SHADOW_POS)
if(Q1970_SHADOW_POS EQUAL -1)
    message(FATAL_ERROR "Q16.25 could not find Q10.5 shadow caster loop")
endif()
string(REPLACE "${Q1970_SHADOW_LOOP_OLD}" "${Q1970_SHADOW_LOOP_NEW}"
       Q1970_NATIVE_SOURCE "${Q1970_NATIVE_SOURCE}")

# Emit one compact proof whenever actual XCLC changes. Resident shapes remain in
# gObjects, but activeShapes/activeTriangles are the high-detail workload drawn.
set(Q1970_LATEST_GRID_OLD [==[
    gQ1920LatestGridValid = true;
    gQ1920LatestGridX = actualGridX;
    gQ1920LatestGridY = actualGridY;

    // Give a pending generation current player position before it advances.
]==])
set(Q1970_LATEST_GRID_NEW [==[
    gQ1920LatestGridValid = true;
    gQ1920LatestGridX = actualGridX;
    gQ1920LatestGridY = actualGridY;

    static bool q1970ActiveLogReady = false;
    static int32_t q1970LastActiveGridX = 0;
    static int32_t q1970LastActiveGridY = 0;
    if (!q1970ActiveLogReady || actualGridX != q1970LastActiveGridX ||
        actualGridY != q1970LastActiveGridY) {
        q1970ActiveLogReady = true;
        q1970LastActiveGridX = actualGridX;
        q1970LastActiveGridY = actualGridY;
        size_t q1970ActiveShapes = 0u;
        size_t q1970ActiveTriangles = 0u;
        for (const GpuObject& q1970Object : gObjects) {
            if (!Q1970ShouldRenderFullDetail(q1970Object)) continue;
            ++q1970ActiveShapes;
            q1970ActiveTriangles += static_cast<size_t>(q1970Object.vertexCount / 3);
        }
        Q6H_LOGI("Q16.25 ACTIVE DRAW: actual=(%d,%d) residentCentre=(%d,%d) residentShapes=%zu activeShapes=%zu activeTriangles=%zu activeRadius=2 residentRadius=3",
                 actualGridX, actualGridY,
                 gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
                 gObjects.size(), q1970ActiveShapes, q1970ActiveTriangles);
    }

    // Give a pending generation current player position before it advances.
]==])
string(FIND "${Q1970_NATIVE_SOURCE}" "${Q1970_LATEST_GRID_OLD}" Q1970_LATEST_GRID_POS)
if(Q1970_LATEST_GRID_POS EQUAL -1)
    message(FATAL_ERROR "Q16.25 could not find q1920 actual-grid publication")
endif()
string(REPLACE "${Q1970_LATEST_GRID_OLD}" "${Q1970_LATEST_GRID_NEW}"
       Q1970_NATIVE_SOURCE "${Q1970_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# C. Collision follows actualGrid, not a predictive resident centre. A radius-3
# resident window offset by at most one cell fully contains the actual-centred
# radius-2 active visual set and actual-centred radius-1 collision set.
# -----------------------------------------------------------------------------
set(Q1970_COLLISION_DECL_OLD [==[
    size_t q1950OutsideCollisionWindow = 0u;
    constexpr int Q1950_COLLISION_GRID_RADIUS = 1;
    for (const Fo3WorldPlacement& placement : gPendingStreamQ1900.targetPlacements) {
]==])
set(Q1970_COLLISION_DECL_NEW [==[
    size_t q1950OutsideCollisionWindow = 0u;
    constexpr int Q1950_COLLISION_GRID_RADIUS = 1;
    const int32_t q1970CollisionGridX = gQ1920LatestGridValid
        ? gQ1920LatestGridX : gPendingStreamQ1900.targetGridX;
    const int32_t q1970CollisionGridY = gQ1920LatestGridValid
        ? gQ1920LatestGridY : gPendingStreamQ1900.targetGridY;
    for (const Fo3WorldPlacement& placement : gPendingStreamQ1900.targetPlacements) {
]==])
string(FIND "${Q1970_NATIVE_SOURCE}" "${Q1970_COLLISION_DECL_OLD}" Q1970_COLLISION_DECL_POS)
if(Q1970_COLLISION_DECL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.25 could not find streamed collision window declaration")
endif()
string(REPLACE "${Q1970_COLLISION_DECL_OLD}" "${Q1970_COLLISION_DECL_NEW}"
       Q1970_NATIVE_SOURCE "${Q1970_NATIVE_SOURCE}")

set(Q1970_COLLISION_TEST_OLD [==[
        if (std::abs(placementGridX - gPendingStreamQ1900.targetGridX) > Q1950_COLLISION_GRID_RADIUS ||
            std::abs(placementGridY - gPendingStreamQ1900.targetGridY) > Q1950_COLLISION_GRID_RADIUS) {
]==])
set(Q1970_COLLISION_TEST_NEW [==[
        if (std::abs(placementGridX - q1970CollisionGridX) > Q1950_COLLISION_GRID_RADIUS ||
            std::abs(placementGridY - q1970CollisionGridY) > Q1950_COLLISION_GRID_RADIUS) {
]==])
string(FIND "${Q1970_NATIVE_SOURCE}" "${Q1970_COLLISION_TEST_OLD}" Q1970_COLLISION_TEST_POS)
if(Q1970_COLLISION_TEST_POS EQUAL -1)
    message(FATAL_ERROR "Q16.25 could not find streamed collision target-grid test")
endif()
string(REPLACE "${Q1970_COLLISION_TEST_OLD}" "${Q1970_COLLISION_TEST_NEW}"
       Q1970_NATIVE_SOURCE "${Q1970_NATIVE_SOURCE}")

# Do not let a completed predictive generation modify collision/terrain if its
# 7x7 resident target can no longer fully cover the actual-centred 5x5 active set.
set(Q1970_COLLISION_FN_OLD [==[
void Q1900AdvanceCollision() {
    std::vector<Fo3WorldPlacement> collisionPlacements;
]==])
set(Q1970_COLLISION_FN_NEW [==[
void Q1900AdvanceCollision() {
    if (gExteriorWorldspaceQ1890 == 0x0000003Cu && gQ1920LatestGridValid &&
        (std::abs(gPendingStreamQ1900.targetGridX - gQ1920LatestGridX) > 1 ||
         std::abs(gPendingStreamQ1900.targetGridY - gQ1920LatestGridY) > 1)) {
        Q6H_LOGW("Q16.25 RESIDENT PREP STALE: generation=%llu target=(%d,%d) actual=(%d,%d) action=cancel-before-collision reason=7x7-no-longer-covers-active-5x5",
                 static_cast<unsigned long long>(gPendingStreamQ1900.generation),
                 gPendingStreamQ1900.targetGridX, gPendingStreamQ1900.targetGridY,
                 gQ1920LatestGridX, gQ1920LatestGridY);
        Q1900CancelPending("resident-no-longer-covers-active-5x5");
        return;
    }
    std::vector<Fo3WorldPlacement> collisionPlacements;
]==])
string(FIND "${Q1970_NATIVE_SOURCE}" "${Q1970_COLLISION_FN_OLD}" Q1970_COLLISION_FN_POS)
if(Q1970_COLLISION_FN_POS EQUAL -1)
    message(FATAL_ERROR "Q16.25 could not find staged collision function")
endif()
string(REPLACE "${Q1970_COLLISION_FN_OLD}" "${Q1970_COLLISION_FN_NEW}"
       Q1970_NATIVE_SOURCE "${Q1970_NATIVE_SOURCE}")

# Final guard because the player can cross a CELL during the isolated collision
# frame. Never publish a resident ring that cannot contain the active 5x5.
set(Q1970_COMMIT_FN_OLD [==[
void Q1900CommitWindow() {
    const size_t oldShapes = gObjects.size();
]==])
set(Q1970_COMMIT_FN_NEW [==[
void Q1900CommitWindow() {
    if (gExteriorWorldspaceQ1890 == 0x0000003Cu && gQ1920LatestGridValid &&
        (std::abs(gPendingStreamQ1900.targetGridX - gQ1920LatestGridX) > 1 ||
         std::abs(gPendingStreamQ1900.targetGridY - gQ1920LatestGridY) > 1)) {
        Q6H_LOGW("Q16.25 RESIDENT COMMIT STALE: generation=%llu target=(%d,%d) actual=(%d,%d) action=cancel reason=7x7-no-longer-covers-active-5x5",
                 static_cast<unsigned long long>(gPendingStreamQ1900.generation),
                 gPendingStreamQ1900.targetGridX, gPendingStreamQ1900.targetGridY,
                 gQ1920LatestGridX, gQ1920LatestGridY);
        Q1900CancelPending("resident-no-longer-covers-active-5x5-at-commit");
        return;
    }
    const size_t oldShapes = gObjects.size();
]==])
string(FIND "${Q1970_NATIVE_SOURCE}" "${Q1970_COMMIT_FN_OLD}" Q1970_COMMIT_FN_POS)
if(Q1970_COMMIT_FN_POS EQUAL -1)
    message(FATAL_ERROR "Q16.25 could not find stream commit function")
endif()
string(REPLACE "${Q1970_COMMIT_FN_OLD}" "${Q1970_COMMIT_FN_NEW}"
       Q1970_NATIVE_SOURCE "${Q1970_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# D. Native Fallout LOD discovery/probe. The first Capital Wasteland transition
# runs under the loading screen, so inspect the real Level4 terrain/object block
# for that authored 4x4-cell block there. Do not render it yet: log raw BSA hit,
# parser result and transformed mesh bounds first so the next build can place it
# without guessing Bethesda's macromesh coordinate convention.
# -----------------------------------------------------------------------------
set(Q1970_INCLUDE_OLD [==[#include "fo3-static-nif.h"]==])
set(Q1970_INCLUDE_NEW [==[#include "fo3-static-nif.h"
#include "fo3-bsa-reader.h"]==])
string(FIND "${Q1970_NATIVE_SOURCE}" "${Q1970_INCLUDE_OLD}" Q1970_INCLUDE_POS)
if(Q1970_INCLUDE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.25 could not find static NIF include")
endif()
string(REPLACE "${Q1970_INCLUDE_OLD}" "${Q1970_INCLUDE_NEW}"
       Q1970_NATIVE_SOURCE "${Q1970_NATIVE_SOURCE}")

set(Q1970_LOD_HELPER_MARKER [==[
bool Q1970GetActiveGrid(int32_t& gridX, int32_t& gridY) {
]==])
set(Q1970_LOD_HELPERS [==[
int32_t Q1970FloorToLevel4Block(int32_t cell) {
    int32_t quotient = cell / 4;
    if (cell < 0 && (cell % 4) != 0) --quotient;
    return quotient * 4;
}

void Q1970ProbeLodPath(const char* kind, const std::string& path,
                       int32_t blockX, int32_t blockY) {
    std::vector<uint8_t> raw;
    std::string resolved;
    const bool found = LoadFalloutMeshFile(path, raw, &resolved);
    std::vector<Fo3StaticNifMesh> meshes;
    const bool parsed = found && LoadFo3StaticNifMeshes(path, meshes) && !meshes.empty();

    float minX = 1e30f, minY = 1e30f, minZ = 1e30f;
    float maxX = -1e30f, maxY = -1e30f, maxZ = -1e30f;
    size_t vertices = 0u;
    size_t triangles = 0u;
    if (parsed) {
        for (const Fo3StaticNifMesh& mesh : meshes) {
            vertices += mesh.positions.size() / 3u;
            triangles += mesh.indices.size() / 3u;
            for (size_t i = 0u; i + 2u < mesh.positions.size(); i += 3u) {
                minX = std::min(minX, mesh.positions[i]);
                minY = std::min(minY, mesh.positions[i + 1u]);
                minZ = std::min(minZ, mesh.positions[i + 2u]);
                maxX = std::max(maxX, mesh.positions[i]);
                maxY = std::max(maxY, mesh.positions[i + 1u]);
                maxZ = std::max(maxZ, mesh.positions[i + 2u]);
            }
        }
    }

    Q6H_LOGI("Q16.25 NATIVE LOD PROBE: kind=%s block=(%d,%d) found=%d bytes=%zu parsed=%d shapes=%zu vertices=%zu triangles=%zu bounds=[%.1f %.1f %.1f]-[%.1f %.1f %.1f] path=%s resolved=%s",
             kind, blockX, blockY, found ? 1 : 0, raw.size(), parsed ? 1 : 0,
             meshes.size(), vertices, triangles,
             parsed ? minX : 0.0f, parsed ? minY : 0.0f, parsed ? minZ : 0.0f,
             parsed ? maxX : 0.0f, parsed ? maxY : 0.0f, parsed ? maxZ : 0.0f,
             path.c_str(), resolved.empty() ? "<none>" : resolved.c_str());
}

void Q1970ProbeNativeLod(float gameX, float gameY) {
    static bool q1970Probed = false;
    if (q1970Probed) return;
    q1970Probed = true;

    const int32_t cellX = static_cast<int32_t>(std::floor(gameX / Q1890_EXTERIOR_CELL_SIZE));
    const int32_t cellY = static_cast<int32_t>(std::floor(gameY / Q1890_EXTERIOR_CELL_SIZE));
    const int32_t blockX = Q1970FloorToLevel4Block(cellX);
    const int32_t blockY = Q1970FloorToLevel4Block(cellY);
    const std::string suffix = "Wasteland.Level4.X" + std::to_string(blockX) +
                               ".Y" + std::to_string(blockY) + ".NIF";
    const std::string terrainPath = "Landscape\\LOD\\Wasteland\\" + suffix;
    const std::string objectPath = "Landscape\\LOD\\Wasteland\\Blocks\\" + suffix;
    Q6H_LOGI("Q16.25 NATIVE LOD EXPECTED: playerCell=(%d,%d) level4Block=(%d,%d)",
             cellX, cellY, blockX, blockY);
    Q1970ProbeLodPath("terrain", terrainPath, blockX, blockY);
    Q1970ProbeLodPath("objects", objectPath, blockX, blockY);
}

bool Q1970GetActiveGrid(int32_t& gridX, int32_t& gridY) {
]==])
string(FIND "${Q1970_NATIVE_SOURCE}" "${Q1970_LOD_HELPER_MARKER}" Q1970_LOD_HELPER_POS)
if(Q1970_LOD_HELPER_POS EQUAL -1)
    message(FATAL_ERROR "Q16.25 could not find active-grid helper for LOD probe insertion")
endif()
string(REPLACE "${Q1970_LOD_HELPER_MARKER}" "${Q1970_LOD_HELPERS}"
       Q1970_NATIVE_SOURCE "${Q1970_NATIVE_SOURCE}")

set(Q1970_INITIAL_WASTELAND_OLD [==[
    if (request.worldspaceFormId == 0x0000003Cu) {
        constexpr float Q1960_CELL_SIZE = 4096.0f;
]==])
set(Q1970_INITIAL_WASTELAND_NEW [==[
    if (request.worldspaceFormId == 0x0000003Cu) {
        Q1970ProbeNativeLod(request.x, request.y);
        constexpr float Q1960_CELL_SIZE = 4096.0f;
]==])
string(FIND "${Q1970_NATIVE_SOURCE}" "${Q1970_INITIAL_WASTELAND_OLD}" Q1970_INITIAL_WASTELAND_POS)
if(Q1970_INITIAL_WASTELAND_POS EQUAL -1)
    message(FATAL_ERROR "Q16.25 could not find initial Capital Wasteland collision block")
endif()
string(REPLACE "${Q1970_INITIAL_WASTELAND_OLD}" "${Q1970_INITIAL_WASTELAND_NEW}"
       Q1970_NATIVE_SOURCE "${Q1970_NATIVE_SOURCE}")

# Renderer diagnostics now describe resident-vs-active semantics rather than a
# monolithic 7x7 full-detail window.
string(REPLACE "rebuildMode=7x7-visual+3x3-collision+9x9-terrain-retain"
       "rebuildMode=7x7-resident+5x5-active+3x3-actual-collision+9x9-terrain"
       Q1970_NATIVE_SOURCE "${Q1970_NATIVE_SOURCE}")
string(REPLACE "Q16.24" "Q16.25" Q1970_NATIVE_SOURCE "${Q1970_NATIVE_SOURCE}")
file(WRITE "${Q1970_NATIVE_FILE}" "${Q1970_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# E. Teach the generalized NIF probe about Bethesda BSSegmentedTriShape. It is a
# NiTriShape-derived block with a trailing segment table; geometry/material data
# is still in the ordinary NiTriShape prefix/dataRef. This affects parser support
# only; no LOD mesh is submitted to GL in Q16.25.
# -----------------------------------------------------------------------------
set(Q1970_NIF_FILE "${CMAKE_CURRENT_BINARY_DIR}/fo3-static-nif-q6h.cpp")
if(NOT EXISTS "${Q1970_NIF_FILE}")
    message(FATAL_ERROR "Q16.25 expected final generated NIF parser")
endif()
file(READ "${Q1970_NIF_FILE}" Q1970_NIF_SOURCE)

set(Q1970_SHAPE_TYPE_OLD [==[
bool Q970IsShapeType(const std::string& type) {
    return type == "NiTriStrips" || type == "NiTriShape";
}
]==])
set(Q1970_SHAPE_TYPE_NEW [==[
bool Q970IsShapeType(const std::string& type) {
    return type == "NiTriStrips" || type == "NiTriShape" ||
           type == "BSSegmentedTriShape";
}
]==])
string(FIND "${Q1970_NIF_SOURCE}" "${Q1970_SHAPE_TYPE_OLD}" Q1970_SHAPE_TYPE_POS)
if(Q1970_SHAPE_TYPE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.25 could not find Q9.70 shape-type helper")
endif()
string(REPLACE "${Q1970_SHAPE_TYPE_OLD}" "${Q1970_SHAPE_TYPE_NEW}"
       Q1970_NIF_SOURCE "${Q1970_NIF_SOURCE}")

set(Q1970_SHAPE_LOOP_OLD [==[
        if (type != "NiTriStrips" && type != "NiTriShape") continue;
        ++q1060ShapeBlocks;
]==])
set(Q1970_SHAPE_LOOP_NEW [==[
        if (type != "NiTriStrips" && type != "NiTriShape" &&
            type != "BSSegmentedTriShape") continue;
        ++q1060ShapeBlocks;
]==])
string(FIND "${Q1970_NIF_SOURCE}" "${Q1970_SHAPE_LOOP_OLD}" Q1970_SHAPE_LOOP_POS)
if(Q1970_SHAPE_LOOP_POS EQUAL -1)
    message(FATAL_ERROR "Q16.25 could not find final NIF shape loop")
endif()
string(REPLACE "${Q1970_SHAPE_LOOP_OLD}" "${Q1970_SHAPE_LOOP_NEW}"
       Q1970_NIF_SOURCE "${Q1970_NIF_SOURCE}")

# The base NiTriShape fields are complete before BSSegmentedTriShape's derived
# segment vector. Accepting a remaining derived payload is required for that
# block while all reads remain bounded by the NIF block size.
set(Q1970_SHAPE_PARSE_TAIL_OLD [==[
    uint32_t activeMaterial = 0;
    uint8_t dirtyFlag = 0;
    if (!c.U32(activeMaterial) || !c.U8(dirtyFlag)) return false;
    return c.remaining() == 0u;
}
]==])
set(Q1970_SHAPE_PARSE_TAIL_NEW [==[
    uint32_t activeMaterial = 0;
    uint8_t dirtyFlag = 0;
    if (!c.U32(activeMaterial) || !c.U8(dirtyFlag)) return false;
    // Derived Bethesda geometry such as BSSegmentedTriShape appends bounded
    // segment metadata after the NiTriShape prefix. The geometry dataRef and
    // material properties above are already complete for rendering/probing.
    return true;
}
]==])
string(FIND "${Q1970_NIF_SOURCE}" "${Q1970_SHAPE_PARSE_TAIL_OLD}" Q1970_SHAPE_PARSE_TAIL_POS)
if(Q1970_SHAPE_PARSE_TAIL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.25 could not find ShapeObject parse tail")
endif()
string(REPLACE "${Q1970_SHAPE_PARSE_TAIL_OLD}" "${Q1970_SHAPE_PARSE_TAIL_NEW}"
       Q1970_NIF_SOURCE "${Q1970_NIF_SOURCE}")
file(WRITE "${Q1970_NIF_FILE}" "${Q1970_NIF_SOURCE}")

# -----------------------------------------------------------------------------
# F. Visible build label Q16.25; retain Q16.22's 300m camera for the LOD probe.
# -----------------------------------------------------------------------------
set(Q1970_Q4_FILE "${CMAKE_CURRENT_BINARY_DIR}/q1800-q4-generated.cpp")
if(NOT EXISTS "${Q1970_Q4_FILE}")
    message(FATAL_ERROR "Q16.25 expected Q16.24 OpenXR source")
endif()
file(READ "${Q1970_Q4_FILE}" Q1970_Q4_SOURCE)
set(Q1970_LABEL_OLD [==[
        q1600Digit(q1600X, 0x5Bu); // Q16.24: 2 = A B G E D
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x66u); // Q16.24: 4 = F G B C
]==])
set(Q1970_LABEL_NEW [==[
        q1600Digit(q1600X, 0x5Bu); // Q16.25: 2 = A B G E D
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x6Du); // Q16.25: 5 = A F G C D
]==])
string(FIND "${Q1970_Q4_SOURCE}" "${Q1970_LABEL_OLD}" Q1970_LABEL_POS)
if(Q1970_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.25 could not find Q16.24 build-label digits")
endif()
string(REPLACE "${Q1970_LABEL_OLD}" "${Q1970_LABEL_NEW}"
       Q1970_Q4_SOURCE "${Q1970_Q4_SOURCE}")
string(REPLACE "Q16.24 BUILD LABEL:" "Q16.25 BUILD LABEL:"
       Q1970_Q4_SOURCE "${Q1970_Q4_SOURCE}")
string(REPLACE "text=Q16.24 anchor=left-hand" "text=Q16.25 anchor=left-hand"
       Q1970_Q4_SOURCE "${Q1970_Q4_SOURCE}")
string(REPLACE "Q16.24 AUTHORED DOOR FACING" "Q16.25 AUTHORED DOOR FACING"
       Q1970_Q4_SOURCE "${Q1970_Q4_SOURCE}")
string(REPLACE "Q16.24 FAR CLIP:" "Q16.25 FAR CLIP:"
       Q1970_Q4_SOURCE "${Q1970_Q4_SOURCE}")
file(WRITE "${Q1970_Q4_FILE}" "${Q1970_Q4_SOURCE}")

# Keep terrain/collision translation-unit diagnostics on the live build number.
string(REPLACE "Q16.24 TERRAIN WINDOW" "Q16.25 TERRAIN WINDOW"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-data-q720.cpp"
     "${Q720_TERRAIN_DATA_SOURCE}")
string(REPLACE "Q16.24 COLLISION MODE OVERRIDE" "Q16.25 COLLISION MODE OVERRIDE"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

# -----------------------------------------------------------------------------
# G. Configure-time proof. Fail closed if an older patch moves an anchor.
# -----------------------------------------------------------------------------
file(READ "${Q1970_NATIVE_FILE}" Q1970_NATIVE_VERIFY)
file(READ "${Q1970_NIF_FILE}" Q1970_NIF_VERIFY)
string(FIND "${Q1970_NATIVE_VERIFY}" "Q1970_ACTIVE_VISUAL_RADIUS = 2" Q1970_ACTIVE_OK)
string(FIND "${Q1970_NATIVE_VERIFY}" "Q1970ShouldRenderFullDetail(object)" Q1970_DRAW_OK)
string(FIND "${Q1970_NATIVE_VERIFY}" "q1970ShadowGridX" Q1970_SHADOW_OK)
string(FIND "${Q1970_NATIVE_VERIFY}" "q1970CollisionGridX" Q1970_COLLISION_OK)
string(FIND "${Q1970_NATIVE_VERIFY}" "resident-no-longer-covers-active-5x5" Q1970_COMMIT_OK)
string(FIND "${Q1970_NATIVE_VERIFY}" "Q16.25 NATIVE LOD PROBE:" Q1970_LOD_OK)
string(FIND "${Q1970_NIF_VERIFY}" "BSSegmentedTriShape" Q1970_SEGMENTED_OK)
string(FIND "${Q1970_Q4_SOURCE}" "Q16.25: 5 = A F G C D" Q1970_LABEL_OK)
if(Q1970_ACTIVE_OK EQUAL -1 OR Q1970_DRAW_OK EQUAL -1 OR
   Q1970_SHADOW_OK EQUAL -1 OR Q1970_COLLISION_OK EQUAL -1 OR
   Q1970_COMMIT_OK EQUAL -1 OR Q1970_LOD_OK EQUAL -1 OR
   Q1970_SEGMENTED_OK EQUAL -1 OR Q1970_LABEL_OK EQUAL -1)
    message(FATAL_ERROR "Q16.25 centred active/resident + native LOD probe verification failed")
endif()

message(STATUS "Q16.25 enabled: actual-centred 5x5 active draw inside 7x7 resident GPU ring; 3x3 actual collision; 9x9 LAND; Level4 native LOD BSA/parser probe")
