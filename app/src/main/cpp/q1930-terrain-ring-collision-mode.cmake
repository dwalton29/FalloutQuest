# Q16.21: widen only the LAND render window and make streamed exterior collision
# an explicit scene property instead of inferring it from the presence of Megaton
# architecture in the current placement set.
#
# Q16.20 device evidence:
#   - visual objects are already the expensive part of a shift, so keep their
#     authored CELL window at radius 1 (3x3);
#   - terrain itself is cheap/cached enough to test radius 2 (5x5), pushing the
#     hard world edge one full CELL farther away in every direction;
#   - Wasteland collision eventually flipped to exterior=0 because Q7.8A inferred
#     exterior mode by looking for architecture\\megaton meshes. Once those left
#     the 3x3 object window, collision filtered the Wasteland and built 0 triangles.
#
# This layer therefore changes no streaming coordinates and no object radius.

# -----------------------------------------------------------------------------
# A. Collision mode: expose a one-shot override consumed by the mature collision
# initializer. Interior/legacy callers retain inference; streamed exterior shifts
# explicitly request exterior authored-bhk semantics.
# -----------------------------------------------------------------------------
set(Q1930_COLLISION_FLAG_OLD "bool gExteriorAllBhksQ78A = false;")
set(Q1930_COLLISION_FLAG_NEW [==[
bool gExteriorAllBhksQ78A = false;
int gNextCollisionExteriorOverrideQ1930 = -1;
]==])
string(FIND "${Q74_COLLISION_SOURCE}" "${Q1930_COLLISION_FLAG_OLD}" Q1930_COLLISION_FLAG_POS)
if(Q1930_COLLISION_FLAG_POS EQUAL -1)
    message(FATAL_ERROR "Q16.21 could not find exterior collision state")
endif()
string(REPLACE "${Q1930_COLLISION_FLAG_OLD}" "${Q1930_COLLISION_FLAG_NEW}"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

# The collision source is heavily patched by earlier layers, so anchor directly
# on the stable public initializer signature rather than assuming it still sits
# immediately after the anonymous-namespace closing brace.
set(Q1930_COLLISION_SETTER_MARKER [==[
bool InitializeFo3CollisionOverlay(const std::vector<Fo3WorldPlacement>& placements,
]==])
set(Q1930_COLLISION_SETTER_REPLACEMENT [==[
void SetNextFo3CollisionExteriorModeQ1930(bool exterior) {
    gNextCollisionExteriorOverrideQ1930 = exterior ? 1 : 0;
}

bool InitializeFo3CollisionOverlay(const std::vector<Fo3WorldPlacement>& placements,
]==])
string(FIND "${Q74_COLLISION_SOURCE}" "${Q1930_COLLISION_SETTER_MARKER}" Q1930_COLLISION_SETTER_POS)
if(Q1930_COLLISION_SETTER_POS EQUAL -1)
    message(FATAL_ERROR "Q16.21 could not find collision initializer signature")
endif()
string(REPLACE "${Q1930_COLLISION_SETTER_MARKER}" "${Q1930_COLLISION_SETTER_REPLACEMENT}"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

set(Q1930_COLLISION_INFER_OLD [==[
    const bool exteriorAllBhksQ78A = IsExteriorMegatonPlacementSetQ78A(placements);
    gExteriorAllBhksQ78A = exteriorAllBhksQ78A;
]==])
set(Q1930_COLLISION_INFER_NEW [==[
    const bool inferredExteriorQ1930 = IsExteriorMegatonPlacementSetQ78A(placements);
    const bool exteriorAllBhksQ78A =
        gNextCollisionExteriorOverrideQ1930 >= 0
            ? (gNextCollisionExteriorOverrideQ1930 != 0)
            : inferredExteriorQ1930;
    if (gNextCollisionExteriorOverrideQ1930 >= 0) {
        Q6F_LOGI("Q16.21 COLLISION MODE OVERRIDE: exterior=%d inferredMegaton=%d placements=%zu",
                 exteriorAllBhksQ78A ? 1 : 0,
                 inferredExteriorQ1930 ? 1 : 0,
                 placements.size());
    }
    gNextCollisionExteriorOverrideQ1930 = -1;
    gExteriorAllBhksQ78A = exteriorAllBhksQ78A;
]==])
string(FIND "${Q74_COLLISION_SOURCE}" "${Q1930_COLLISION_INFER_OLD}" Q1930_COLLISION_INFER_POS)
if(Q1930_COLLISION_INFER_POS EQUAL -1)
    message(FATAL_ERROR "Q16.21 could not find collision exterior inference")
endif()
string(REPLACE "${Q1930_COLLISION_INFER_OLD}" "${Q1930_COLLISION_INFER_NEW}"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

# -----------------------------------------------------------------------------
# B. Terrain: separate LAND visual radius from Q7.5's radius-1 object window.
# The indexed/decoded LAND and persistent DDS caches from Q16.19 stay intact.
# -----------------------------------------------------------------------------
set(Q1930_TERRAIN_RADIUS_DECL_OLD [==[
    const bool loadWholeWorldspace = gridCells.size() <= SMALL_WORLDSPACE_CELL_LIMIT_Q75;

    std::unordered_set<uint32_t> selectedCells;
]==])
set(Q1930_TERRAIN_RADIUS_DECL_NEW [==[
    const bool loadWholeWorldspace = gridCells.size() <= SMALL_WORLDSPACE_CELL_LIMIT_Q75;
    constexpr int Q1930_TERRAIN_GRID_RADIUS = 2;

    std::unordered_set<uint32_t> selectedCells;
]==])
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "${Q1930_TERRAIN_RADIUS_DECL_OLD}" Q1930_TERRAIN_DECL_POS)
if(Q1930_TERRAIN_DECL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.21 could not find terrain selection declaration")
endif()
string(REPLACE "${Q1930_TERRAIN_RADIUS_DECL_OLD}" "${Q1930_TERRAIN_RADIUS_DECL_NEW}"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")

set(Q1930_TERRAIN_RADIUS_TEST_OLD [==[
            (std::abs(cell->gridX - targetGridX) <= GRID_RADIUS_Q75 &&
             std::abs(cell->gridY - targetGridY) <= GRID_RADIUS_Q75)) {
]==])
set(Q1930_TERRAIN_RADIUS_TEST_NEW [==[
            (std::abs(cell->gridX - targetGridX) <= Q1930_TERRAIN_GRID_RADIUS &&
             std::abs(cell->gridY - targetGridY) <= Q1930_TERRAIN_GRID_RADIUS)) {
]==])
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "${Q1930_TERRAIN_RADIUS_TEST_OLD}" Q1930_TERRAIN_TEST_POS)
if(Q1930_TERRAIN_TEST_POS EQUAL -1)
    message(FATAL_ERROR "Q16.21 could not find terrain radius test")
endif()
string(REPLACE "${Q1930_TERRAIN_RADIUS_TEST_OLD}" "${Q1930_TERRAIN_RADIUS_TEST_NEW}"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")

# Existing Q7.9 LAND READY diagnostics expose the resulting terrainCells count;
# add a dedicated marker so the headset log makes 5x5 vs 3x3 unambiguous.
set(Q1930_TERRAIN_LOG_MARKER [==[
    std::unordered_map<uint32_t, CellInfoQ75> cellsById;
]==])
set(Q1930_TERRAIN_LOG_REPLACEMENT [==[
    Q75_LOGI("Q16.21 TERRAIN WINDOW: worldspace=%08X targetGrid=(%d,%d) radius=%d selectedCells=%zu wholeWorld=%d",
             worldspaceFormId, targetGridX, targetGridY,
             Q1930_TERRAIN_GRID_RADIUS, selectedCells.size(),
             loadWholeWorldspace ? 1 : 0);

    std::unordered_map<uint32_t, CellInfoQ75> cellsById;
]==])
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "${Q1930_TERRAIN_LOG_MARKER}" Q1930_TERRAIN_LOG_POS)
if(Q1930_TERRAIN_LOG_POS EQUAL -1)
    message(FATAL_ERROR "Q16.21 could not find terrain cellsById insertion point")
endif()
string(REPLACE "${Q1930_TERRAIN_LOG_MARKER}" "${Q1930_TERRAIN_LOG_REPLACEMENT}"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")

# Q16.19 already emitted the final generated terrain source before Q16.20/21.
# Rewrite it after widening the selector so the live cell host includes this copy.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-data-q720.cpp"
     "${Q720_TERRAIN_DATA_SOURCE}")

# -----------------------------------------------------------------------------
# C. Streamed Wasteland collision explicitly says it is exterior. This is a
# one-shot override consumed by the next InitializeFo3CollisionOverlay call.
# -----------------------------------------------------------------------------
set(Q1930_NATIVE_FILE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp")
if(NOT EXISTS "${Q1930_NATIVE_FILE}")
    message(FATAL_ERROR "Q16.21 expected Q16.20 generated renderer")
endif()
file(READ "${Q1930_NATIVE_FILE}" Q1930_NATIVE_SOURCE)
string(PREPEND Q1930_NATIVE_SOURCE
       "void SetNextFo3CollisionExteriorModeQ1930(bool exterior);\n")

set(Q1930_STREAM_COLLISION_OLD [==[
    gPendingStreamQ1900.collisionReady = InitializeFo3CollisionOverlay(
        collisionPlacements,
]==])
set(Q1930_STREAM_COLLISION_NEW [==[
    SetNextFo3CollisionExteriorModeQ1930(true);
    gPendingStreamQ1900.collisionReady = InitializeFo3CollisionOverlay(
        collisionPlacements,
]==])
string(FIND "${Q1930_NATIVE_SOURCE}" "${Q1930_STREAM_COLLISION_OLD}" Q1930_STREAM_COLLISION_POS)
if(Q1930_STREAM_COLLISION_POS EQUAL -1)
    message(FATAL_ERROR "Q16.21 could not find staged collision initialization")
endif()
string(REPLACE "${Q1930_STREAM_COLLISION_OLD}" "${Q1930_STREAM_COLLISION_NEW}"
       Q1930_NATIVE_SOURCE "${Q1930_NATIVE_SOURCE}")

# Native streaming diagnostics identify the new layer; Q16.19 cache diagnostics
# intentionally retain their original tag to distinguish cache work.
string(REPLACE "Q16.20" "Q16.21" Q1930_NATIVE_SOURCE "${Q1930_NATIVE_SOURCE}")
file(WRITE "${Q1930_NATIVE_FILE}" "${Q1930_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# D. Visible label Q16.20 -> Q16.21. Interaction HUD and authored door behavior
# remain untouched.
# -----------------------------------------------------------------------------
set(Q1930_Q4_FILE "${CMAKE_CURRENT_BINARY_DIR}/q1800-q4-generated.cpp")
if(NOT EXISTS "${Q1930_Q4_FILE}")
    message(FATAL_ERROR "Q16.21 expected final OpenXR source")
endif()
file(READ "${Q1930_Q4_FILE}" Q1930_Q4_SOURCE)
set(Q1930_LABEL_OLD [==[
        q1600Digit(q1600X, 0x06u); // Q16.20: 2 = A B G E D
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x3Fu); // Q16.20: 0 = A B C D E F
]==])
set(Q1930_LABEL_NEW [==[
        q1600Digit(q1600X, 0x06u); // Q16.21: 2 = A B G E D
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x06u); // Q16.21: 1 = B C
]==])
string(FIND "${Q1930_Q4_SOURCE}" "${Q1930_LABEL_OLD}" Q1930_LABEL_POS)
if(Q1930_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.21 could not find Q16.20 build-label digits")
endif()
string(REPLACE "${Q1930_LABEL_OLD}" "${Q1930_LABEL_NEW}"
       Q1930_Q4_SOURCE "${Q1930_Q4_SOURCE}")
string(REPLACE "Q16.20 BUILD LABEL:" "Q16.21 BUILD LABEL:"
       Q1930_Q4_SOURCE "${Q1930_Q4_SOURCE}")
string(REPLACE "text=Q16.20 anchor=left-hand" "text=Q16.21 anchor=left-hand"
       Q1930_Q4_SOURCE "${Q1930_Q4_SOURCE}")
string(REPLACE "Q16.20 AUTHORED DOOR FACING" "Q16.21 AUTHORED DOOR FACING"
       Q1930_Q4_SOURCE "${Q1930_Q4_SOURCE}")
file(WRITE "${Q1930_Q4_FILE}" "${Q1930_Q4_SOURCE}")

# Configure-time proof of the intended split: radius 2 appears only in terrain,
# while q75's GRID_RADIUS_Q75 object selector is left untouched.
string(FIND "${Q74_COLLISION_SOURCE}" "Q16.21 COLLISION MODE OVERRIDE" Q1930_COLLISION_OK)
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "Q1930_TERRAIN_GRID_RADIUS = 2" Q1930_TERRAIN_RADIUS_OK)
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "Q16.21 TERRAIN WINDOW" Q1930_TERRAIN_LOG_OK)
string(FIND "${Q1930_NATIVE_SOURCE}" "SetNextFo3CollisionExteriorModeQ1930(true);" Q1930_STREAM_MODE_OK)
string(FIND "${Q1930_Q4_SOURCE}" "Q16.21: 1 = B C" Q1930_LABEL_OK)
if(Q1930_COLLISION_OK EQUAL -1 OR Q1930_TERRAIN_RADIUS_OK EQUAL -1 OR
   Q1930_TERRAIN_LOG_OK EQUAL -1 OR Q1930_STREAM_MODE_OK EQUAL -1 OR
   Q1930_LABEL_OK EQUAL -1)
    message(FATAL_ERROR "Q16.21 terrain/collision verification failed")
endif()

message(STATUS "Q16.21 enabled: 3x3 full objects/collision + 5x5 LAND visuals + explicit streamed exterior collision mode")
