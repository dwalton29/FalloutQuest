# Q12.6: LAND material inheritance for the exact child cells whose unusable
# VHGT was repaired from their parent worldspace. The Q12.5 audit proved those
# child LAND records are sparse overrides: all five repaired cells had no BTXT
# base at all and only 0-2 child alpha layers while the parent had full BTXT and
# 19-24 alpha layers.
#
# Preserve child-world semantics:
# - child BTXT wins when authored; missing quadrants fall back to parent BTXT
# - parent ATXT/VTXT paints the inherited terrain first
# - child ATXT/VTXT paints afterward as the local Megaton override
# - child VCLR wins when authored; otherwise use parent VCLR
# - CELL Force Hide Land remains untouched and therefore child-authoritative

# 1) Fill only missing base quadrants from the parent LAND already identified by
# Q12.5. Keep the Q12.5 child-vs-parent log truthful by applying the fallback
# after its audit lines have been emitted.
set(Q1260_OLD_Q1250_TAIL [=[
            for (int quadrant = 0; quadrant < 4; ++quadrant) {
                Q76B_LOGI("Q12.5 REPAIRED LAND QUAD: grid=(%d,%d) q=%d child=%s parent=%s",
                          cell.gridX, cell.gridY, quadrant,
                          quadrantTextures[quadrant].empty() ? "<none>" : quadrantTextures[quadrant].c_str(),
                          parentQuadrantTexturesQ1250[quadrant].empty() ? "<none>" : parentQuadrantTexturesQ1250[quadrant].c_str());
            }
        }
]=])
set(Q1260_NEW_Q1250_TAIL [=[
            for (int quadrant = 0; quadrant < 4; ++quadrant) {
                Q76B_LOGI("Q12.5 REPAIRED LAND QUAD: grid=(%d,%d) q=%d child=%s parent=%s",
                          cell.gridX, cell.gridY, quadrant,
                          quadrantTextures[quadrant].empty() ? "<none>" : quadrantTextures[quadrant].c_str(),
                          parentQuadrantTexturesQ1250[quadrant].empty() ? "<none>" : parentQuadrantTexturesQ1250[quadrant].c_str());
            }

            uint32_t inheritedBaseMaskQ1260 = 0u;
            for (int quadrant = 0; quadrant < 4; ++quadrant) {
                if (quadrantTextures[quadrant].empty() &&
                    !parentQuadrantTexturesQ1250[quadrant].empty()) {
                    quadrantTextures[quadrant] = parentQuadrantTexturesQ1250[quadrant];
                    inheritedBaseMaskQ1260 |= (1u << quadrant);
                }
            }
            Q76B_LOGI("Q12.6 REPAIRED LAND BASE: grid=(%d,%d) childLAND=%08X parentLAND=%08X inheritedMask=0x%X",
                      cell.gridX, cell.gridY, cell.landFormId,
                      repairedParentLandQ1250, inheritedBaseMaskQ1260);
        }
]=])
string(REPLACE "${Q1260_OLD_Q1250_TAIL}" "${Q1260_NEW_Q1250_TAIL}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# 2) VCLR is a complete 33x33 payload when present. A local child VCLR is an
# authored override and wins wholesale; an absent/malformed child VCLR inherits
# the matching parent VCLR instead of silently becoming white.
set(Q1260_OLD_VCLR_SELECT [=[
        const std::vector<float>& authoredColorsQ1060 = GetFo3TerrainVertexColorsQ1060(cell.landFormId);
        const bool hasAuthoredColorsQ1060 = authoredColorsQ1060.size() == Q76B_HEIGHT_COUNT * 3u;
]=])
set(Q1260_NEW_VCLR_SELECT [=[
        const std::vector<float>& childColorsQ1260 = GetFo3TerrainVertexColorsQ1060(cell.landFormId);
        const bool hasChildColorsQ1260 = childColorsQ1260.size() == Q76B_HEIGHT_COUNT * 3u;
        const uint32_t repairedParentLandForVclrQ1260 = GetFo3RepairedParentLandQ1250(cell.landFormId);
        const std::vector<float>& parentColorsQ1260 =
            GetFo3TerrainVertexColorsQ1060(repairedParentLandForVclrQ1260);
        const bool hasParentColorsQ1260 = parentColorsQ1260.size() == Q76B_HEIGHT_COUNT * 3u;
        const std::vector<float>& authoredColorsQ1060 =
            hasChildColorsQ1260 ? childColorsQ1260 :
            (hasParentColorsQ1260 ? parentColorsQ1260 : childColorsQ1260);
        const bool hasAuthoredColorsQ1060 = authoredColorsQ1060.size() == Q76B_HEIGHT_COUNT * 3u;
        if (repairedParentLandForVclrQ1260 != 0u) {
            Q76B_LOGI("Q12.6 REPAIRED LAND VCLR: grid=(%d,%d) child=%d parent=%d source=%s",
                      cell.gridX, cell.gridY,
                      hasChildColorsQ1260 ? 1 : 0,
                      hasParentColorsQ1260 ? 1 : 0,
                      hasChildColorsQ1260 ? "child" :
                      (hasParentColorsQ1260 ? "parent" : "none"));
        }
]=])
string(REPLACE "${Q1260_OLD_VCLR_SELECT}" "${Q1260_NEW_VCLR_SELECT}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# 3) A repaired child LAND is a delta over the parent, not a replacement for
# the parent's alpha paint. Compose parent layers first, then append copies of
# the child layers with a render-order bias so local Megaton paint is always
# above the inherited parent material even when both records reuse layer IDs.
set(Q1260_OLD_ALPHA_SOURCE [=[
        const std::vector<Q718TerrainAlphaLayer>& alphaLayers =
            GetFo3TerrainAlphaLayersQ718(cell.landFormId);
]=])
set(Q1260_NEW_ALPHA_SOURCE [=[
        const std::vector<Q718TerrainAlphaLayer>& childAlphaLayersQ1260 =
            GetFo3TerrainAlphaLayersQ718(cell.landFormId);
        std::vector<Q718TerrainAlphaLayer> mergedAlphaLayersQ1260;
        const std::vector<Q718TerrainAlphaLayer>* alphaLayersQ1260 = &childAlphaLayersQ1260;
        const uint32_t repairedParentLandForAlphaQ1260 = GetFo3RepairedParentLandQ1250(cell.landFormId);
        if (repairedParentLandForAlphaQ1260 != 0u) {
            const std::vector<Q718TerrainAlphaLayer>& parentAlphaLayersQ1260 =
                GetFo3TerrainAlphaLayersQ718(repairedParentLandForAlphaQ1260);
            mergedAlphaLayersQ1260.reserve(parentAlphaLayersQ1260.size() + childAlphaLayersQ1260.size());
            mergedAlphaLayersQ1260.insert(mergedAlphaLayersQ1260.end(),
                                          parentAlphaLayersQ1260.begin(),
                                          parentAlphaLayersQ1260.end());
            constexpr uint32_t Q1260_CHILD_LAYER_BIAS = 1024u;
            for (const Q718TerrainAlphaLayer& childLayerQ1260 : childAlphaLayersQ1260) {
                Q718TerrainAlphaLayer localLayerQ1260 = childLayerQ1260;
                const uint32_t biasedOrderQ1260 =
                    static_cast<uint32_t>(localLayerQ1260.layer) + Q1260_CHILD_LAYER_BIAS;
                localLayerQ1260.layer = static_cast<uint16_t>(
                    std::min<uint32_t>(biasedOrderQ1260, 65535u));
                mergedAlphaLayersQ1260.push_back(std::move(localLayerQ1260));
            }
            alphaLayersQ1260 = &mergedAlphaLayersQ1260;
            Q76B_LOGI("Q12.6 REPAIRED LAND ALPHA: grid=(%d,%d) childLAND=%08X parentLAND=%08X parentLayers=%zu childLayers=%zu mergedLayers=%zu childOrderBias=%u",
                      cell.gridX, cell.gridY, cell.landFormId,
                      repairedParentLandForAlphaQ1260,
                      parentAlphaLayersQ1260.size(), childAlphaLayersQ1260.size(),
                      mergedAlphaLayersQ1260.size(), Q1260_CHILD_LAYER_BIAS);
        }
        const std::vector<Q718TerrainAlphaLayer>& alphaLayers = *alphaLayersQ1260;
]=])
string(REPLACE "${Q1260_OLD_ALPHA_SOURCE}" "${Q1260_NEW_ALPHA_SOURCE}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# Drift guards: fail the build rather than silently shipping a no-op inheritance
# patch if an earlier generated-source transformation changes its anchors.
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "Q12.6 REPAIRED LAND BASE" Q1260_BASE_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "Q12.6 REPAIRED LAND VCLR" Q1260_VCLR_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "Q12.6 REPAIRED LAND ALPHA" Q1260_ALPHA_OK)
if(Q1260_BASE_OK LESS 0 OR Q1260_VCLR_OK LESS 0 OR Q1260_ALPHA_OK LESS 0)
    message(FATAL_ERROR "Q12.6 repaired LAND material inheritance patch drifted: base=${Q1260_BASE_OK} vclr=${Q1260_VCLR_OK} alpha=${Q1260_ALPHA_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")
