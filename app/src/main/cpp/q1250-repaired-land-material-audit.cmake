# Q12.5: diagnose material provenance only on LAND cells whose catastrophic
# child VHGT was replaced by the matching parent-worldspace heightfield.
# No visual/material behaviour changes in this stage.

# Record the exact parent LAND used for each repaired child LAND. Keep this
# separate from Fo3TerrainCellQ76 so the diagnostic is visual-only and does not
# change the public terrain/collision structure.
string(REPLACE
    "std::vector<Fo3TerrainCellQ76> gTerrainDataQ76;"
    "std::vector<Fo3TerrainCellQ76> gTerrainDataQ76;\nstd::unordered_map<uint32_t, uint32_t> gQ1250ParentLandByChildLand;\n\nuint32_t GetFo3RepairedParentLandQ1250(uint32_t childLandFormId) {\n    const auto found = gQ1250ParentLandByChildLand.find(childLandFormId);\n    return found == gQ1250ParentLandByChildLand.end() ? 0u : found->second;\n}"
    Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")

string(REPLACE
    "    gTerrainDataQ76.clear();"
    "    gTerrainDataQ76.clear();\n    gQ1250ParentLandByChildLand.clear();"
    Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")

string(REPLACE
    "        localCell.heights = std::move(parentCell.heights);"
    "        localCell.heights = std::move(parentCell.heights);\n        gQ1250ParentLandByChildLand[localCell.landFormId] = parentCell.landFormId;\n        Q75_LOGI(\"Q12.5 REPAIRED LAND LINK: grid=(%d,%d) childLAND=%08X parentLAND=%08X\",\n                 localCell.gridX, localCell.gridY, localCell.landFormId, parentCell.landFormId);"
    Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")

# At renderer build time compare the material payloads attached to exactly those
# repaired child LAND records. This tells us whether the child record is only a
# partial local paint delta while the parent carries the missing base/ATXT data.
set(Q1250_OLD_QUADRANT_RESOLVE [=[
        std::string quadrantTextures[4];
        for (int quadrant = 0; quadrant < 4; ++quadrant) {
            quadrantTextures[quadrant] = ResolveFo3TerrainBaseTextureQ711(cell.landFormId, quadrant);
        }
]=])
set(Q1250_NEW_QUADRANT_RESOLVE [=[
        std::string quadrantTextures[4];
        for (int quadrant = 0; quadrant < 4; ++quadrant) {
            quadrantTextures[quadrant] = ResolveFo3TerrainBaseTextureQ711(cell.landFormId, quadrant);
        }

        const uint32_t repairedParentLandQ1250 = GetFo3RepairedParentLandQ1250(cell.landFormId);
        if (repairedParentLandQ1250 != 0u) {
            uint32_t childBaseMaskQ1250 = 0u;
            uint32_t parentBaseMaskQ1250 = 0u;
            std::string parentQuadrantTexturesQ1250[4];
            for (int quadrant = 0; quadrant < 4; ++quadrant) {
                if (!quadrantTextures[quadrant].empty()) childBaseMaskQ1250 |= (1u << quadrant);
                parentQuadrantTexturesQ1250[quadrant] =
                    ResolveFo3TerrainBaseTextureQ711(repairedParentLandQ1250, quadrant);
                if (!parentQuadrantTexturesQ1250[quadrant].empty()) parentBaseMaskQ1250 |= (1u << quadrant);
            }

            const std::vector<Q718TerrainAlphaLayer>& childAlphaQ1250 =
                GetFo3TerrainAlphaLayersQ718(cell.landFormId);
            const std::vector<Q718TerrainAlphaLayer>& parentAlphaQ1250 =
                GetFo3TerrainAlphaLayersQ718(repairedParentLandQ1250);
            size_t childAlphaVerticesQ1250 = 0u;
            size_t parentAlphaVerticesQ1250 = 0u;
            for (const Q718TerrainAlphaLayer& layer : childAlphaQ1250) {
                childAlphaVerticesQ1250 += layer.authoredVertices;
            }
            for (const Q718TerrainAlphaLayer& layer : parentAlphaQ1250) {
                parentAlphaVerticesQ1250 += layer.authoredVertices;
            }

            Q76B_LOGI("Q12.5 REPAIRED LAND MATERIAL: grid=(%d,%d) childLAND=%08X parentLAND=%08X childBaseMask=0x%X parentBaseMask=0x%X childAlphaLayers=%zu parentAlphaLayers=%zu childAlphaVertices=%zu parentAlphaVertices=%zu",
                      cell.gridX, cell.gridY, cell.landFormId, repairedParentLandQ1250,
                      childBaseMaskQ1250, parentBaseMaskQ1250,
                      childAlphaQ1250.size(), parentAlphaQ1250.size(),
                      childAlphaVerticesQ1250, parentAlphaVerticesQ1250);
            for (int quadrant = 0; quadrant < 4; ++quadrant) {
                Q76B_LOGI("Q12.5 REPAIRED LAND QUAD: grid=(%d,%d) q=%d child=%s parent=%s",
                          cell.gridX, cell.gridY, quadrant,
                          quadrantTextures[quadrant].empty() ? "<none>" : quadrantTextures[quadrant].c_str(),
                          parentQuadrantTexturesQ1250[quadrant].empty() ? "<none>" : parentQuadrantTexturesQ1250[quadrant].c_str());
            }
        }
]=])
string(REPLACE "${Q1250_OLD_QUADRANT_RESOLVE}" "${Q1250_NEW_QUADRANT_RESOLVE}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "Q12.5 REPAIRED LAND LINK" Q1250_DATA_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "Q12.5 REPAIRED LAND MATERIAL" Q1250_RENDER_OK)
if(Q1250_DATA_OK LESS 0 OR Q1250_RENDER_OK LESS 0)
    message(FATAL_ERROR "Q12.5 repaired LAND material audit patch drifted: data=${Q1250_DATA_OK} render=${Q1250_RENDER_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-data-q720.cpp" "${Q720_TERRAIN_DATA_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp" "${Q720_TERRAIN_RENDER_SOURCE}")
