# Q7.23: model-aware suppression of low authored endcaps on walkable modular pieces.
#
# Q7.22 provenance proved a hard catch on MegatonRampTurn90Sml itself: the
# blocker triangle ended only ~0.184m above the player's feet and the raised
# capsule was completely clear. Generic seam probing still failed to classify
# that contact. Fallout/Havok character motion treats these low perimeter faces
# as traversable step/floor boundaries rather than full walls.
#
# Restrict this behavior to the authored modular walkable families the GECK test
# identified: MegatonRamp*, ScrapGroundPlate*, and WoodPlankGroup*. Only side/
# endcap triangles whose *entire* vertical extent tops out within the normal
# exterior step allowance are suppressed. Taller faces remain normal blockers.

file(READ "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q722.inc"
     Q723_CONTROLLER_SOURCE)
string(REPLACE "Q7.22" "Q7.23"
       Q723_CONTROLLER_SOURCE "${Q723_CONTROLLER_SOURCE}")

# Tag collision triangles from the known walkable modular model families.
set(Q723_OLD_TRI_FLAGS [=[
    bool stairsQ714 = false;
    bool platformQ714 = false;
};
]=])
set(Q723_NEW_TRI_FLAGS [=[
    bool stairsQ714 = false;
    bool platformQ714 = false;
    bool walkableModuleQ723 = false;
};
]=])
string(REPLACE "${Q723_OLD_TRI_FLAGS}" "${Q723_NEW_TRI_FLAGS}"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

set(Q723_MODULE_HELPER [=[
bool IsWalkableModuleQ723(const Fo3WorldPlacement& placement) {
    std::string edid = placement.editorId;
    for (char& ch : edid) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    const std::string model = NormalizeModelPathQ78A(placement.modelPath);
    return edid.rfind("megatonramp", 0) == 0 ||
           edid.rfind("scrapgroundplate", 0) == 0 ||
           edid.rfind("woodplankgroup", 0) == 0 ||
           model.find("megatonramp") != std::string::npos ||
           model.find("scrapgroundplate") != std::string::npos ||
           model.find("woodplankgroup") != std::string::npos;
}

]=])
string(REPLACE
    "bool IsExteriorMegatonPlacementSetQ78A(const std::vector<Fo3WorldPlacement>& placements) {"
    "${Q723_MODULE_HELPER}bool IsExteriorMegatonPlacementSetQ78A(const std::vector<Fo3WorldPlacement>& placements) {"
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

set(Q723_OLD_TRI_BUILD [=[
                CollisionTriangle worldTriangle;
                if (!BuildTriangle(a, b, c, worldTriangle)) continue;
                worldTriangle.surfaceKeyQ714 = MakeSurfaceKeyQ714(
]=])
set(Q723_NEW_TRI_BUILD [=[
                CollisionTriangle worldTriangle;
                if (!BuildTriangle(a, b, c, worldTriangle)) continue;
                worldTriangle.walkableModuleQ723 = IsWalkableModuleQ723(placement);
                worldTriangle.surfaceKeyQ714 = MakeSurfaceKeyQ714(
]=])
string(REPLACE "${Q723_OLD_TRI_BUILD}" "${Q723_NEW_TRI_BUILD}"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

# Counter for evidence that the model-aware rule is actually firing.
string(REPLACE
    "uint64_t gCapsuleContactLogsQ719 = 0u;"
    "uint64_t gCapsuleContactLogsQ719 = 0u;\nuint64_t gModuleEndcapLogsQ723 = 0u;"
    Q723_CONTROLLER_SOURCE "${Q723_CONTROLLER_SOURCE}")

# Exact 3D capsule contact is still computed first. Then, for only the tagged
# walkable modules, discard low non-walkable triangles whose whole top edge is
# within the normal authored step allowance. This avoids the old global maxY
# heuristic: untagged geometry is unchanged, and tall faces on tagged models
# remain walls.
set(Q723_OLD_CLOSEST [=[
    const float d2 = ClosestCapsuleSegmentTriangleQ719(segA, segB, tri, capsulePoint, trianglePoint);
    if (!std::isfinite(d2) || d2 >= radius2) return false;

    const float distance = std::sqrt(std::max(d2, 0.0f));
]=])
set(Q723_NEW_CLOSEST [=[
    const float d2 = ClosestCapsuleSegmentTriangleQ719(segA, segB, tri, capsulePoint, trianglePoint);
    if (!std::isfinite(d2) || d2 >= radius2) return false;

    if (gExteriorAllBhksQ78A && tri.walkableModuleQ723) {
        const float topRise = tri.maxY - feetY;
        const float maxLowTop = StepHeightQ78B() + PLAYER_SKIN + 0.020f;
        if (topRise >= -0.080f && topRise <= maxLowTop) {
            if (gModuleEndcapLogsQ723 < 120u || (gResolveCounter % 240u) == 0u) {
                ++gModuleEndcapLogsQ723;
                const auto sourceIt = gSurfaceSourcesQ722.find(tri.surfaceKeyQ714);
                if (sourceIt != gSurfaceSourcesQ722.end()) {
                    Q6G_LOGI("Q7.23 MODULE ENDCAP PASS: surface=%llu ref=%08X EDID=%s model=%s topRise=%.3f triNormalY=%.3f maxStep=%.3f",
                             static_cast<unsigned long long>(tri.surfaceKeyQ714),
                             sourceIt->second.refFormId,
                             sourceIt->second.editorId.empty() ? "<none>" : sourceIt->second.editorId.c_str(),
                             sourceIt->second.modelPath.c_str(),
                             topRise, tri.normal.y, StepHeightQ78B());
                }
            }
            return false;
        }
    }

    const float distance = std::sqrt(std::max(d2, 0.0f));
]=])
string(REPLACE "${Q723_OLD_CLOSEST}" "${Q723_NEW_CLOSEST}"
       Q723_CONTROLLER_SOURCE "${Q723_CONTROLLER_SOURCE}")

string(REPLACE
    "mode=3d-capsule-triangle+sweep-slide+raised-step+walkable-seam-manifold persistentOverlap=depth-aware"
    "mode=3d-capsule-triangle+sweep-slide+raised-step+walkable-module-endcaps persistentOverlap=depth-aware"
    Q723_CONTROLLER_SOURCE "${Q723_CONTROLLER_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q723.inc"
     "${Q723_CONTROLLER_SOURCE}")
string(REPLACE
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q722.inc\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q723.inc\""
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")
