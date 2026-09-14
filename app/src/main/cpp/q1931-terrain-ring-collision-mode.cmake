# Q16.21 robust layer: 3x3 full objects/collision + 5x5 LAND visuals.
# This supersedes q1930, whose exact terrain-expression anchor was too brittle
# after the mature Q7/Q10 terrain patches had transformed the generated source.

# -----------------------------------------------------------------------------
# A. Explicit streamed-exterior collision mode.
# -----------------------------------------------------------------------------
set(Q1931_COLLISION_FLAG_OLD "bool gExteriorAllBhksQ78A = false;")
set(Q1931_COLLISION_FLAG_NEW [==[
bool gExteriorAllBhksQ78A = false;
int gNextCollisionExteriorOverrideQ1931 = -1;
]==])
string(FIND "${Q74_COLLISION_SOURCE}" "${Q1931_COLLISION_FLAG_OLD}" Q1931_COLLISION_FLAG_POS)
if(Q1931_COLLISION_FLAG_POS EQUAL -1)
    message(FATAL_ERROR "Q16.21 could not find exterior collision state")
endif()
string(REPLACE "${Q1931_COLLISION_FLAG_OLD}" "${Q1931_COLLISION_FLAG_NEW}"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

set(Q1931_COLLISION_SETTER_MARKER [==[
bool InitializeFo3CollisionOverlay(const std::vector<Fo3WorldPlacement>& placements,
]==])
set(Q1931_COLLISION_SETTER_REPLACEMENT [==[
void SetNextFo3CollisionExteriorModeQ1931(bool exterior) {
    gNextCollisionExteriorOverrideQ1931 = exterior ? 1 : 0;
}

bool InitializeFo3CollisionOverlay(const std::vector<Fo3WorldPlacement>& placements,
]==])
string(FIND "${Q74_COLLISION_SOURCE}" "${Q1931_COLLISION_SETTER_MARKER}" Q1931_COLLISION_SETTER_POS)
if(Q1931_COLLISION_SETTER_POS EQUAL -1)
    message(FATAL_ERROR "Q16.21 could not find collision initializer signature")
endif()
string(REPLACE "${Q1931_COLLISION_SETTER_MARKER}" "${Q1931_COLLISION_SETTER_REPLACEMENT}"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

set(Q1931_COLLISION_INFER_OLD [==[
    const bool exteriorAllBhksQ78A = IsExteriorMegatonPlacementSetQ78A(placements);
    gExteriorAllBhksQ78A = exteriorAllBhksQ78A;
]==])
set(Q1931_COLLISION_INFER_NEW [==[
    const bool inferredExteriorQ1931 = IsExteriorMegatonPlacementSetQ78A(placements);
    const bool exteriorAllBhksQ78A =
        gNextCollisionExteriorOverrideQ1931 >= 0
            ? (gNextCollisionExteriorOverrideQ1931 != 0)
            : inferredExteriorQ1931;
    if (gNextCollisionExteriorOverrideQ1931 >= 0) {
        Q6F_LOGI("Q16.21 COLLISION MODE OVERRIDE: exterior=%d inferredMegaton=%d placements=%zu",
                 exteriorAllBhksQ78A ? 1 : 0,
                 inferredExteriorQ1931 ? 1 : 0,
                 placements.size());
    }
    gNextCollisionExteriorOverrideQ1931 = -1;
    gExteriorAllBhksQ78A = exteriorAllBhksQ78A;
]==])
string(FIND "${Q74_COLLISION_SOURCE}" "${Q1931_COLLISION_INFER_OLD}" Q1931_COLLISION_INFER_POS)
if(Q1931_COLLISION_INFER_POS EQUAL -1)
    message(FATAL_ERROR "Q16.21 could not find collision exterior inference")
endif()
string(REPLACE "${Q1931_COLLISION_INFER_OLD}" "${Q1931_COLLISION_INFER_NEW}"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

# -----------------------------------------------------------------------------
# B. Widen only the LAND translation unit from radius 1 to radius 2.
# Q720_TERRAIN_DATA_SOURCE is independent of the worldspace/object selector, so
# replacing GRID_RADIUS_Q75 here cannot increase the object placement window.
# -----------------------------------------------------------------------------
set(Q1931_TERRAIN_DECL_OLD [==[
    const bool loadWholeWorldspace = gridCells.size() <= SMALL_WORLDSPACE_CELL_LIMIT_Q75;

    std::unordered_set<uint32_t> selectedCells;
]==])
set(Q1931_TERRAIN_DECL_NEW [==[
    const bool loadWholeWorldspace = gridCells.size() <= SMALL_WORLDSPACE_CELL_LIMIT_Q75;
    constexpr int Q1931_TERRAIN_GRID_RADIUS = 2;

    std::unordered_set<uint32_t> selectedCells;
]==])
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "${Q1931_TERRAIN_DECL_OLD}" Q1931_TERRAIN_DECL_POS)
if(Q1931_TERRAIN_DECL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.21 could not find terrain selection declaration")
endif()
string(REPLACE "${Q1931_TERRAIN_DECL_OLD}" "${Q1931_TERRAIN_DECL_NEW}"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")

# The token occurs only in this terrain source's selected-CELL radius check.
# Use a token substitution rather than matching the fully formatted expression,
# because several mature terrain layers transform whitespace/body around it.
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "GRID_RADIUS_Q75" Q1931_TERRAIN_RADIUS_TOKEN_POS)
if(Q1931_TERRAIN_RADIUS_TOKEN_POS EQUAL -1)
    message(FATAL_ERROR "Q16.21 could not find LAND radius token")
endif()
string(REPLACE "GRID_RADIUS_Q75" "Q1931_TERRAIN_GRID_RADIUS"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")

# Insert an explicit diagnostic immediately before LAND collection. The mature
# Q7.20 force-hide block still reaches this stable declaration afterwards.
set(Q1931_TERRAIN_LOG_MARKER "    std::vector<Fo3TerrainCellQ76> localTerrain;")
set(Q1931_TERRAIN_LOG_REPLACEMENT [==[
    Q75_LOGI("Q16.21 TERRAIN WINDOW: worldspace=%08X targetGrid=(%d,%d) radius=%d selectedCells=%zu wholeWorld=%d",
             worldspaceFormId, targetGridX, targetGridY,
             Q1931_TERRAIN_GRID_RADIUS, selectedCells.size(),
             loadWholeWorldspace ? 1 : 0);
    std::vector<Fo3TerrainCellQ76> localTerrain;
]==])
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "${Q1931_TERRAIN_LOG_MARKER}" Q1931_TERRAIN_LOG_POS)
if(Q1931_TERRAIN_LOG_POS EQUAL -1)
    message(FATAL_ERROR "Q16.21 could not find LAND collection declaration")
endif()
string(REPLACE "${Q1931_TERRAIN_LOG_MARKER}" "${Q1931_TERRAIN_LOG_REPLACEMENT}"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-data-q720.cpp"
     "${Q720_TERRAIN_DATA_SOURCE}")

# -----------------------------------------------------------------------------
# C. Every staged exterior collision rebuild explicitly opts into exterior BHKs.
# -----------------------------------------------------------------------------
set(Q1931_NATIVE_FILE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp")
if(NOT EXISTS "${Q1931_NATIVE_FILE}")
    message(FATAL_ERROR "Q16.21 expected Q16.20 generated renderer")
endif()
file(READ "${Q1931_NATIVE_FILE}" Q1931_NATIVE_SOURCE)
string(PREPEND Q1931_NATIVE_SOURCE
       "void SetNextFo3CollisionExteriorModeQ1931(bool exterior);\n")

set(Q1931_STREAM_COLLISION_OLD [==[
    gPendingStreamQ1900.collisionReady = InitializeFo3CollisionOverlay(
        collisionPlacements,
]==])
set(Q1931_STREAM_COLLISION_NEW [==[
    SetNextFo3CollisionExteriorModeQ1931(true);
    gPendingStreamQ1900.collisionReady = InitializeFo3CollisionOverlay(
        collisionPlacements,
]==])
string(FIND "${Q1931_NATIVE_SOURCE}" "${Q1931_STREAM_COLLISION_OLD}" Q1931_STREAM_COLLISION_POS)
if(Q1931_STREAM_COLLISION_POS EQUAL -1)
    message(FATAL_ERROR "Q16.21 could not find staged collision initialization")
endif()
string(REPLACE "${Q1931_STREAM_COLLISION_OLD}" "${Q1931_STREAM_COLLISION_NEW}"
       Q1931_NATIVE_SOURCE "${Q1931_NATIVE_SOURCE}")
string(REPLACE "Q16.20" "Q16.21" Q1931_NATIVE_SOURCE "${Q1931_NATIVE_SOURCE}")
file(WRITE "${Q1931_NATIVE_FILE}" "${Q1931_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# D. Visible build label Q16.21.
# -----------------------------------------------------------------------------
set(Q1931_Q4_FILE "${CMAKE_CURRENT_BINARY_DIR}/q1800-q4-generated.cpp")
if(NOT EXISTS "${Q1931_Q4_FILE}")
    message(FATAL_ERROR "Q16.21 expected final OpenXR source")
endif()
file(READ "${Q1931_Q4_FILE}" Q1931_Q4_SOURCE)
set(Q1931_LABEL_OLD [==[
        q1600Digit(q1600X, 0x06u); // Q16.20: 2 = A B G E D
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x3Fu); // Q16.20: 0 = A B C D E F
]==])
set(Q1931_LABEL_NEW [==[
        q1600Digit(q1600X, 0x06u); // Q16.21: 2 = A B G E D
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x06u); // Q16.21: 1 = B C
]==])
string(FIND "${Q1931_Q4_SOURCE}" "${Q1931_LABEL_OLD}" Q1931_LABEL_POS)
if(Q1931_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.21 could not find Q16.20 build-label digits")
endif()
string(REPLACE "${Q1931_LABEL_OLD}" "${Q1931_LABEL_NEW}"
       Q1931_Q4_SOURCE "${Q1931_Q4_SOURCE}")
string(REPLACE "Q16.20 BUILD LABEL:" "Q16.21 BUILD LABEL:"
       Q1931_Q4_SOURCE "${Q1931_Q4_SOURCE}")
string(REPLACE "text=Q16.20 anchor=left-hand" "text=Q16.21 anchor=left-hand"
       Q1931_Q4_SOURCE "${Q1931_Q4_SOURCE}")
string(REPLACE "Q16.20 AUTHORED DOOR FACING" "Q16.21 AUTHORED DOOR FACING"
       Q1931_Q4_SOURCE "${Q1931_Q4_SOURCE}")
file(WRITE "${Q1931_Q4_FILE}" "${Q1931_Q4_SOURCE}")

string(FIND "${Q74_COLLISION_SOURCE}" "Q16.21 COLLISION MODE OVERRIDE" Q1931_COLLISION_OK)
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "Q1931_TERRAIN_GRID_RADIUS = 2" Q1931_TERRAIN_OK)
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "Q16.21 TERRAIN WINDOW" Q1931_TERRAIN_LOG_OK)
string(FIND "${Q1931_NATIVE_SOURCE}" "SetNextFo3CollisionExteriorModeQ1931(true);" Q1931_STREAM_OK)
string(FIND "${Q1931_Q4_SOURCE}" "Q16.21: 1 = B C" Q1931_LABEL_OK)
if(Q1931_COLLISION_OK EQUAL -1 OR Q1931_TERRAIN_OK EQUAL -1 OR
   Q1931_TERRAIN_LOG_OK EQUAL -1 OR Q1931_STREAM_OK EQUAL -1 OR
   Q1931_LABEL_OK EQUAL -1)
    message(FATAL_ERROR "Q16.21 robust terrain/collision verification failed")
endif()
message(STATUS "Q16.21 enabled: 3x3 full objects/collision + 5x5 LAND visuals + explicit streamed exterior collision mode")
