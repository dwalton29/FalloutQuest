# Q7.20 root fixes for the two long-running Megaton issues.
#
# 1) Fallout 3 exterior CELL/XCLC has a third uint32 field whose low four bits
#    are Force Hide Land quadrant flags. Earlier FalloutQuest terrain code read
#    only X/Y, so LAND Bethesda intentionally suppresses could be rendered and
#    grounded on. Generate terrain sources that preserve and obey those flags.
# 2) Q7.19's exact 3D capsule contacts still decided whether a blocker was a
#    step from the highest vertex of the entire triangle. Replace that heuristic
#    with a clearance-first character step: raise by maxStep, test the requested
#    horizontal endpoint with the real capsule, then drop to valid walkable
#    ground. Long ramp/pipe/stair triangles can no longer become walls merely
#    because a distant vertex is high.

# ----- Exterior character controller -----
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fo3-collision-controller-q716.inc"
     Q720_CONTROLLER_SOURCE)
string(REPLACE "Q7.19" "Q7.20"
       Q720_CONTROLLER_SOURCE "${Q720_CONTROLLER_SOURCE}")
string(REPLACE
    "// Q7.19 exterior character-controller response.\n//\n// Q7.20 replaces the old X/Z-projected \"wall\" test with an actual 3D\n// vertical capsule-vs-authored-Havok-triangle closest-distance test. Q7.18's\n// sweep/slide/step and persistent-overlap behavior remains otherwise intact."
    "// Q7.20 exterior character-controller response.\n//\n// Q7.19 introduced exact 3D capsule-vs-Havok-triangle contacts. Q7.20 removes\n// the remaining whole-triangle-height step heuristic: a step is now proven by\n// raising the capsule, testing the requested horizontal endpoint, then dropping\n// onto valid walkable ground."
    Q720_CONTROLLER_SOURCE "${Q720_CONTROLLER_SOURCE}")

set(Q720_OLD_CONTACT_GATE [=[
    const float stepHeight = StepHeightQ78B();
    const float radius2 = collisionRadius * collisionRadius;

    if (std::fabs(tri.normal.y) >= WalkableNormalThresholdQ714(tri)) return false;
    if (gExteriorAllBhksQ78A && tri.maxY <= feetY + stepHeight + PLAYER_SKIN) return false;

    const float lowerY = feetY + collisionRadius;
]=])
set(Q720_NEW_CONTACT_GATE [=[
    const float radius2 = collisionRadius * collisionRadius;

    if (std::fabs(tri.normal.y) >= WalkableNormalThresholdQ714(tri)) return false;

    // Q7.20: do not hide low wall contacts here. The character controller must
    // see the real obstacle first and prove it can step over it with a raised
    // capsule test. Suppressing triangles by their maxY made the raised test
    // circular and could also let obstacles taller than maxStep disappear.
    const float lowerY = feetY + collisionRadius;
]=])
string(REPLACE "${Q720_OLD_CONTACT_GATE}" "${Q720_NEW_CONTACT_GATE}"
       Q720_CONTROLLER_SOURCE "${Q720_CONTROLLER_SOURCE}")

set(Q720_OLD_LOW_STEP [=[
bool TryLowBlockerStepOverQ718(float startX, float startZ, float targetX, float targetZ,
                               float feetY, const std::vector<WallContactQ718>& blockers,
                               float& outFeetY, size_t& outRaisedBlockers) {
    outRaisedBlockers = 0u;
    if (blockers.empty()) return false;
    const float maxStep = StepHeightQ78B();
    float blockerTop = -1e30f;
    for (const WallContactQ718& blocker : blockers) {
        if (!std::isfinite(blocker.maxY)) return false;
        blockerTop = std::max(blockerTop, blocker.maxY);
    }
    const float riseToTop = blockerTop - feetY;
    if (riseToTop <= STEP_MIN_RISE_Q78B ||
        riseToTop > maxStep + PLAYER_SKIN + Q718_LOW_BLOCKER_TOP_EPS) return false;
    const float raisedFeetY = std::min(feetY + maxStep + PLAYER_SKIN,
                                      blockerTop + PLAYER_SKIN + Q718_LOW_BLOCKER_TOP_EPS);
    if (HasOverheadBlock(startX, startZ, raisedFeetY) ||
        HasOverheadBlock(targetX, targetZ, raisedFeetY)) return false;
    const float moveX = targetX - startX;
    const float moveZ = targetZ - startZ;
    std::vector<WallContactQ718> raisedBaseline;
    GatherWallContactsQ718(startX, startZ, raisedFeetY, raisedBaseline);
    std::vector<WallContactQ718> raisedBlockers;
    size_t raw = 0u, persistent = 0u;
    CandidateAllowedQ718(targetX, targetZ, raisedFeetY, moveX, moveZ,
                         raisedBaseline, raisedBlockers, &raw, &persistent);
    outRaisedBlockers = raisedBlockers.size();
    if (!raisedBlockers.empty()) return false;
    outFeetY = raisedFeetY;
    if (gStepOverLogsQ718 < 80u || (gResolveCounter % 240u) == 0u) {
        ++gStepOverLogsQ718;
        Q6G_LOGI("Q7.20 STEP OVER: from=(%.3f %.3f) to=(%.3f %.3f) blockerTop=%.3f rise=%.3f raisedTestY=%.3f blockers=%zu persistent=%zu clear=1",
                 startX, startZ, targetX, targetZ, blockerTop, riseToTop, raisedFeetY,
                 blockers.size(), persistent);
    }
    return true;
}
]=])
set(Q720_NEW_LOW_STEP [=[
bool TryLowBlockerStepOverQ718(float startX, float startZ, float targetX, float targetZ,
                               float feetY, const std::vector<WallContactQ718>& blockers,
                               float& outFeetY, size_t& outRaisedBlockers) {
    outRaisedBlockers = 0u;
    if (blockers.empty()) return false;

    const float maxStep = StepHeightQ78B();
    const float raisedFeetY = feetY + maxStep + PLAYER_SKIN;
    if (HasOverheadBlock(startX, startZ, raisedFeetY) ||
        HasOverheadBlock(targetX, targetZ, raisedFeetY)) return false;

    const float moveX = targetX - startX;
    const float moveZ = targetZ - startZ;
    std::vector<WallContactQ718> raisedBaseline;
    GatherWallContactsQ718(startX, startZ, raisedFeetY, raisedBaseline);
    std::vector<WallContactQ718> raisedBlockers;
    size_t raw = 0u, persistent = 0u;
    CandidateAllowedQ718(targetX, targetZ, raisedFeetY, moveX, moveZ,
                         raisedBaseline, raisedBlockers, &raw, &persistent);
    outRaisedBlockers = raisedBlockers.size();
    if (!raisedBlockers.empty()) return false;

    // The raised horizontal path is clear. Now prove there is sensible ground
    // to land on; do not turn this into a ledge-jump or a hover state.
    float landingY = raisedFeetY;
    if (!FindGround(targetX, targetZ, raisedFeetY, landingY)) return false;
    const float landingRise = landingY - feetY;
    if (landingRise < -0.08f || landingRise > maxStep + PLAYER_SKIN) return false;

    outFeetY = landingY;
    if (gStepOverLogsQ718 < 120u || (gResolveCounter % 240u) == 0u) {
        ++gStepOverLogsQ718;
        Q6G_LOGI("Q7.20 RAISED STEP: from=(%.3f %.3f) to=(%.3f %.3f) raisedTestY=%.3f landingY=%.3f rise=%.3f originalBlockers=%zu raisedBlockers=%zu persistent=%zu clear=1",
                 startX, startZ, targetX, targetZ, raisedFeetY, landingY,
                 landingRise, blockers.size(), raisedBlockers.size(), persistent);
    }
    return true;
}
]=])
string(REPLACE "${Q720_OLD_LOW_STEP}" "${Q720_NEW_LOW_STEP}"
       Q720_CONTROLLER_SOURCE "${Q720_CONTROLLER_SOURCE}")
string(REPLACE
    "mode=3d-capsule-triangle+sweep-slide-step persistentOverlap=depth-aware"
    "mode=3d-capsule-triangle+sweep-slide+raised-step persistentOverlap=depth-aware"
    Q720_CONTROLLER_SOURCE "${Q720_CONTROLLER_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q720.inc"
     "${Q720_CONTROLLER_SOURCE}")
string(REPLACE
    "#include \"fo3-collision-controller-q716.inc\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q720.inc\""
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

# ----- CELL XCLC Force Hide Land -----
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fo3-worldspace-q75.cpp" Q720_WORLDSPACE_SOURCE)
set(Q720_OLD_CELL_INFO [=[
struct CellInfoQ75 {
    uint32_t formId = 0;
    int32_t gridX = 0;
    int32_t gridY = 0;
    bool hasGrid = false;
    std::string editorId;
};
]=])
set(Q720_NEW_CELL_INFO [=[
struct CellInfoQ75 {
    uint32_t formId = 0;
    int32_t gridX = 0;
    int32_t gridY = 0;
    uint32_t forceHideLandQ720 = 0u;
    bool hasGrid = false;
    std::string editorId;
};
]=])
string(REPLACE "${Q720_OLD_CELL_INFO}" "${Q720_NEW_CELL_INFO}"
       Q720_WORLDSPACE_SOURCE "${Q720_WORLDSPACE_SOURCE}")
set(Q720_OLD_XCLC [=[
            if (std::memcmp(type, "XCLC", 4u) == 0 && size >= 8u) {
                cell.gridX = static_cast<int32_t>(ReadLe32Q75(bytes + 0u));
                cell.gridY = static_cast<int32_t>(ReadLe32Q75(bytes + 4u));
                cell.hasGrid = true;
]=])
set(Q720_NEW_XCLC [=[
            if (std::memcmp(type, "XCLC", 4u) == 0 && size >= 8u) {
                cell.gridX = static_cast<int32_t>(ReadLe32Q75(bytes + 0u));
                cell.gridY = static_cast<int32_t>(ReadLe32Q75(bytes + 4u));
                cell.forceHideLandQ720 = size >= 12u ? (ReadLe32Q75(bytes + 8u) & 0x0fu) : 0u;
                cell.hasGrid = true;
]=])
string(REPLACE "${Q720_OLD_XCLC}" "${Q720_NEW_XCLC}"
       Q720_WORLDSPACE_SOURCE "${Q720_WORLDSPACE_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-worldspace-q720.cpp"
     "${Q720_WORLDSPACE_SOURCE}")

# Terrain data owns the currently loaded worldspace's hide-mask map. Child CELL
# flags remain authoritative even when the LAND itself is inherited from Wasteland.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fo3-terrain-data-q76.cpp" Q720_TERRAIN_DATA_SOURCE)
string(REPLACE
    "std::vector<Fo3TerrainCellQ76> gTerrainDataQ76;"
    "std::vector<Fo3TerrainCellQ76> gTerrainDataQ76;\nstd::unordered_map<uint64_t, uint32_t> gTerrainForceHideQ720;"
    Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
set(Q720_GRID_HELPERS [=[
uint32_t GetFo3TerrainForceHideQ720(int32_t gridX, int32_t gridY) {
    const auto found = gTerrainForceHideQ720.find(GridKeyQ79(gridX, gridY));
    return found == gTerrainForceHideQ720.end() ? 0u : (found->second & 0x0fu);
}

bool IsFo3TerrainQuadrantHiddenQ720(int32_t gridX, int32_t gridY, int quadrant) {
    if (quadrant < 0 || quadrant > 3) return false;
    return (GetFo3TerrainForceHideQ720(gridX, gridY) & (1u << quadrant)) != 0u;
}

]=])
string(REPLACE
    "bool DecodeVhgtQ76(const uint8_t* bytes, uint32_t size, std::vector<float>& heights) {"
    "${Q720_GRID_HELPERS}bool DecodeVhgtQ76(const uint8_t* bytes, uint32_t size, std::vector<float>& heights) {"
    Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
string(REPLACE
    "    gTerrainDataQ76.clear();"
    "    gTerrainDataQ76.clear();\n    gTerrainForceHideQ720.clear();"
    Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
set(Q720_OLD_SELECTED_TERRAIN [=[
    std::vector<Fo3TerrainCellQ76> localTerrain;
    CollectSelectedLandQ76(worldspaceFormId, selectedCells, cellsById, localTerrain);
]=])
set(Q720_NEW_SELECTED_TERRAIN [=[
    size_t forceHiddenCellsQ720 = 0u;
    for (uint32_t cellId : selectedCells) {
        const auto cellIt = cellsById.find(cellId);
        if (cellIt == cellsById.end() || !cellIt->second.hasGrid) continue;
        const uint32_t hide = cellIt->second.forceHideLandQ720 & 0x0fu;
        gTerrainForceHideQ720[GridKeyQ79(cellIt->second.gridX, cellIt->second.gridY)] = hide;
        if (hide != 0u) {
            ++forceHiddenCellsQ720;
            Q75_LOGI("Q7.20 LAND HIDE: cell=%08X grid=(%d,%d) forceHide=0x%X",
                     cellId, cellIt->second.gridX, cellIt->second.gridY, hide);
        }
    }
    Q75_LOGI("Q7.20 LAND HIDE READY: worldspace=%08X selectedGridCells=%zu cellsWithHide=%zu",
             worldspaceFormId, selectedCells.size(), forceHiddenCellsQ720);

    std::vector<Fo3TerrainCellQ76> localTerrain;
    CollectSelectedLandQ76(worldspaceFormId, selectedCells, cellsById, localTerrain);
]=])
string(REPLACE "${Q720_OLD_SELECTED_TERRAIN}" "${Q720_NEW_SELECTED_TERRAIN}"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-data-q720.cpp"
     "${Q720_TERRAIN_DATA_SOURCE}")

# Visual LAND: skip every base quad and paint layer the CELL says Fallout hides.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fo3-terrain-render-q76.cpp" Q720_TERRAIN_RENDER_SOURCE)
string(REPLACE "Q7.18" "Q7.20"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "size_t q718AlphaLayersUnresolved = 0;"
    "size_t q718AlphaLayersUnresolved = 0;\nsize_t q720HiddenBaseQuads = 0;\nsize_t q720HiddenAlphaLayers = 0;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    q718AlphaLayersUnresolved = 0;"
    "    q718AlphaLayersUnresolved = 0;\n    q720HiddenBaseQuads = 0;\n    q720HiddenAlphaLayers = 0;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
set(Q720_OLD_BASE_QUAD [=[
                const int quadrant = Q711QuadrantForQuad(x, y);
                Q711CpuBatch& batch = getBatch(quadrantTextures[quadrant], false, -1);
]=])
set(Q720_NEW_BASE_QUAD [=[
                const int quadrant = Q711QuadrantForQuad(x, y);
                if (IsFo3TerrainQuadrantHiddenQ720(cell.gridX, cell.gridY, quadrant)) {
                    ++q720HiddenBaseQuads;
                    continue;
                }
                Q711CpuBatch& batch = getBatch(quadrantTextures[quadrant], false, -1);
]=])
string(REPLACE "${Q720_OLD_BASE_QUAD}" "${Q720_NEW_BASE_QUAD}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
set(Q720_OLD_ALPHA_LAYER [=[
        for (const Q718TerrainAlphaLayer& layer : alphaLayers) {
            if (layer.quadrant > 3u || layer.authoredVertices == 0u) continue;
            ++q718AlphaLayersSeen;
]=])
set(Q720_NEW_ALPHA_LAYER [=[
        for (const Q718TerrainAlphaLayer& layer : alphaLayers) {
            if (layer.quadrant > 3u || layer.authoredVertices == 0u) continue;
            if (IsFo3TerrainQuadrantHiddenQ720(cell.gridX, cell.gridY,
                                               static_cast<int>(layer.quadrant))) {
                ++q720HiddenAlphaLayers;
                continue;
            }
            ++q718AlphaLayersSeen;
]=])
string(REPLACE "${Q720_OLD_ALPHA_LAYER}" "${Q720_NEW_ALPHA_LAYER}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    q76bReady = true;\n    Q76B_LOGI(\"Q7.20 TERRAIN GPU READY:"
    "    q76bReady = true;\n    Q76B_LOGI(\"Q7.20 LAND HIDE APPLIED: worldspace=%08X hiddenBaseQuads=%zu hiddenAlphaLayers=%zu\", q76bWorldspace, q720HiddenBaseQuads, q720HiddenAlphaLayers);\n    Q76B_LOGI(\"Q7.20 TERRAIN GPU READY:"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")

# Physical LAND: forced-hidden terrain is not a fallback floor either.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fo3-terrain-ground-q77.cpp" Q720_TERRAIN_GROUND_SOURCE)
string(REPLACE "Q7.7" "Q7.20"
       Q720_TERRAIN_GROUND_SOURCE "${Q720_TERRAIN_GROUND_SOURCE}")
set(Q720_OLD_GROUND_LOCAL [=[
        const float localX = gameX - static_cast<float>(gridX) * CELL_SIZE_Q77;
        const float localY = gameY - static_cast<float>(gridY) * CELL_SIZE_Q77;
        const float gx = std::clamp(localX / VERTEX_SPACING_Q77, 0.0f, 32.0f);
]=])
set(Q720_NEW_GROUND_LOCAL [=[
        const float localX = gameX - static_cast<float>(gridX) * CELL_SIZE_Q77;
        const float localY = gameY - static_cast<float>(gridY) * CELL_SIZE_Q77;
        const int quadrant = (localY >= CELL_SIZE_Q77 * 0.5f ? 2 : 0) +
                             (localX >= CELL_SIZE_Q77 * 0.5f ? 1 : 0);
        if (IsFo3TerrainQuadrantHiddenQ720(gridX, gridY, quadrant)) return false;
        const float gx = std::clamp(localX / VERTEX_SPACING_Q77, 0.0f, 32.0f);
]=])
string(REPLACE "${Q720_OLD_GROUND_LOCAL}" "${Q720_NEW_GROUND_LOCAL}"
       Q720_TERRAIN_GROUND_SOURCE "${Q720_TERRAIN_GROUND_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-ground-q720.cpp"
     "${Q720_TERRAIN_GROUND_SOURCE}")

# Rebuild the transition translation unit with the generated Q7.20 components.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fo3-cell-spawn-q74.cpp" Q720_CELL_SOURCE_TEXT)
string(REPLACE
    "#include \"fo3-worldspace-q75.cpp\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-worldspace-q720.cpp\""
    Q720_CELL_SOURCE_TEXT "${Q720_CELL_SOURCE_TEXT}")
string(REPLACE
    "#include \"fo3-terrain-data-q76.cpp\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-data-q720.cpp\""
    Q720_CELL_SOURCE_TEXT "${Q720_CELL_SOURCE_TEXT}")
string(REPLACE
    "#include \"fo3-terrain-render-q76.cpp\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp\""
    Q720_CELL_SOURCE_TEXT "${Q720_CELL_SOURCE_TEXT}")
string(REPLACE
    "#include \"fo3-terrain-ground-q77.cpp\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-ground-q720.cpp\""
    Q720_CELL_SOURCE_TEXT "${Q720_CELL_SOURCE_TEXT}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-cell-spawn-q720.cpp"
     "${Q720_CELL_SOURCE_TEXT}")
set(Q720_CELL_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/fo3-cell-spawn-q720.cpp")
