# Q12.7: material inheritance follows WRLD Use Land Data for every overlapping
# child/parent LAND grid, not only cells whose heightfield needed repair.
#
# Q12.6 proved the composition itself works, but its parent lookup came from the
# Q12.5 height-repair map. That omitted valid-height child LAND records even
# though the parent loader had already resolved their matching parent LAND.
# Record that relationship before the height-delta gate, then reuse the proven
# Q12.6 parent-base + child-override compositor for all such overlaps.
#
# Geometry policy is unchanged: VHGT/VNML are still replaced only when the
# existing catastrophic-height threshold says to do so. CELL Force Hide Land
# remains child-authoritative.

# Generic child LAND -> matching parent LAND provenance map for material use.
string(REPLACE
    "std::unordered_map<uint32_t, uint32_t> gQ1250ParentLandByChildLand;"
    "std::unordered_map<uint32_t, uint32_t> gQ1250ParentLandByChildLand;\nstd::unordered_map<uint32_t, uint32_t> gQ1270MaterialParentLandByChildLand;\n\nuint32_t GetFo3MaterialParentLandQ1270(uint32_t childLandFormId) {\n    const auto found = gQ1270MaterialParentLandByChildLand.find(childLandFormId);\n    return found == gQ1270MaterialParentLandByChildLand.end() ? 0u : found->second;\n}"
    Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")

string(REPLACE
    "    gQ1250ParentLandByChildLand.clear();"
    "    gQ1250ParentLandByChildLand.clear();\n    gQ1270MaterialParentLandByChildLand.clear();"
    Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")

# The parent loader already resolves the corresponding parent LAND for every
# overlapping selected grid. Preserve that identity before the height-delta
# early-out so valid-height child records can still inherit parent materials.
set(Q1270_OLD_OVERLAP [=[
        Fo3TerrainCellQ76& localCell = terrain[localIt->second];
        const float localMean = MeanHeightQ79(localCell.heights);
]=])
set(Q1270_NEW_OVERLAP [=[
        Fo3TerrainCellQ76& localCell = terrain[localIt->second];
        gQ1270MaterialParentLandByChildLand[localCell.landFormId] = parentCell.landFormId;
        Q75_LOGI("Q12.7 MATERIAL PARENT LINK: grid=(%d,%d) childLAND=%08X parentLAND=%08X",
                 localCell.gridX, localCell.gridY,
                 localCell.landFormId, parentCell.landFormId);
        const float localMean = MeanHeightQ79(localCell.heights);
]=])
string(REPLACE "${Q1270_OLD_OVERLAP}" "${Q1270_NEW_OVERLAP}"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")

# Q12.6 has three material selectors (base audit/fallback, VCLR and alpha). Point
# those selectors at the generic Use-Land overlap map. This intentionally also
# causes the existing Q12.5 child-vs-parent material audit to print for all
# overlapping child LAND records, which gives on-device proof of the expanded
# inheritance domain without adding another large diagnostic path.
string(REPLACE
    "GetFo3RepairedParentLandQ1250(cell.landFormId)"
    "GetFo3MaterialParentLandQ1270(cell.landFormId)"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "Q12.7 MATERIAL PARENT LINK" Q1270_DATA_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "GetFo3MaterialParentLandQ1270(cell.landFormId)" Q1270_RENDER_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "GetFo3RepairedParentLandQ1250(cell.landFormId)" Q1270_OLD_RENDER_LOOKUP)
if(Q1270_DATA_OK LESS 0 OR Q1270_RENDER_OK LESS 0 OR NOT Q1270_OLD_RENDER_LOOKUP LESS 0)
    message(FATAL_ERROR "Q12.7 parent LAND material inheritance patch drifted: data=${Q1270_DATA_OK} render=${Q1270_RENDER_OK} oldLookup=${Q1270_OLD_RENDER_LOOKUP}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-data-q720.cpp"
     "${Q720_TERRAIN_DATA_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")
