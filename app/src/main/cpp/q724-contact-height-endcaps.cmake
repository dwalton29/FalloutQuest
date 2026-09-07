# Q7.24: use the exact 3D capsule/triangle contact height for modular endcaps.
#
# Q7.23 proved the model-aware rule fires, but a ScrapGroundPlatesReg02 catch
# still reported blockerTop-feetY=0.364m. The controller was classifying height
# from tri.maxY -- the highest vertex anywhere on the whole triangle -- even
# though ClosestCapsuleSegmentTriangleQ719 already computes the local contact
# point beside the capsule. On sloped/large modular faces this overstates the
# obstacle by several centimetres and recreates the snag.
#
# For the known walkable module families only, classify the lip from the exact
# trianglePoint.y and additionally require that this same triangle no longer
# intersects the capsule after raising by the normal step allowance. Thus a
# genuinely tall side wall on a ramp stays solid, while a low local endcap/lip
# can be traversed.

file(READ "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q723.inc"
     Q724_CONTROLLER_SOURCE)
string(REPLACE "Q7.23" "Q7.24"
       Q724_CONTROLLER_SOURCE "${Q724_CONTROLLER_SOURCE}")

set(Q724_OLD_MODULE_RULE [=[
    if (gExteriorAllBhksQ78A && tri.walkableModuleQ723) {
        const float topRise = tri.maxY - feetY;
        const float maxLowTop = StepHeightQ78B() + PLAYER_SKIN + 0.020f;
        if (topRise >= -0.080f && topRise <= maxLowTop) {
            if (gModuleEndcapLogsQ723 < 120u || (gResolveCounter % 240u) == 0u) {
                ++gModuleEndcapLogsQ723;
                const auto sourceIt = gSurfaceSourcesQ722.find(tri.surfaceKeyQ714);
                if (sourceIt != gSurfaceSourcesQ722.end()) {
                    Q6G_LOGI("Q7.24 MODULE ENDCAP PASS: surface=%llu ref=%08X EDID=%s model=%s topRise=%.3f triNormalY=%.3f maxStep=%.3f",
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
]=])

set(Q724_NEW_MODULE_RULE [=[
    if (gExteriorAllBhksQ78A && tri.walkableModuleQ723) {
        const float contactRise = trianglePoint.y - feetY;
        const float triTopRise = tri.maxY - feetY;
        const float maxLowContact = StepHeightQ78B() + PLAYER_SKIN + 0.020f;

        // A real tall side face remains a blocker because it will still touch
        // the capsule after the player body is raised by the normal step height.
        const float raisedFeetYQ724 = feetY + StepHeightQ78B() + PLAYER_SKIN;
        const Vec3 raisedSegAQ724{x, raisedFeetYQ724 + collisionRadius, z};
        const Vec3 raisedSegBQ724{x, raisedFeetYQ724 + PLAYER_HEIGHT - collisionRadius, z};
        Vec3 raisedCapsulePointQ724;
        Vec3 raisedTrianglePointQ724;
        const float raisedD2Q724 = ClosestCapsuleSegmentTriangleQ719(
            raisedSegAQ724, raisedSegBQ724, tri,
            raisedCapsulePointQ724, raisedTrianglePointQ724);
        const bool clearsWhenRaisedQ724 = !std::isfinite(raisedD2Q724) || raisedD2Q724 >= radius2;

        if (contactRise >= -0.080f && contactRise <= maxLowContact && clearsWhenRaisedQ724) {
            if (gModuleEndcapLogsQ723 < 160u || (gResolveCounter % 240u) == 0u) {
                ++gModuleEndcapLogsQ723;
                const auto sourceIt = gSurfaceSourcesQ722.find(tri.surfaceKeyQ714);
                if (sourceIt != gSurfaceSourcesQ722.end()) {
                    Q6G_LOGI("Q7.24 MODULE CONTACT PASS: surface=%llu ref=%08X EDID=%s model=%s contactRise=%.3f triTopRise=%.3f triNormalY=%.3f raisedClear=1 maxStep=%.3f",
                             static_cast<unsigned long long>(tri.surfaceKeyQ714),
                             sourceIt->second.refFormId,
                             sourceIt->second.editorId.empty() ? "<none>" : sourceIt->second.editorId.c_str(),
                             sourceIt->second.modelPath.c_str(),
                             contactRise, triTopRise, tri.normal.y, StepHeightQ78B());
                }
            }
            return false;
        }
    }
]=])

set(Q724_BEFORE_RULE "${Q724_CONTROLLER_SOURCE}")
string(REPLACE "${Q724_OLD_MODULE_RULE}" "${Q724_NEW_MODULE_RULE}"
       Q724_CONTROLLER_SOURCE "${Q724_CONTROLLER_SOURCE}")
if(Q724_CONTROLLER_SOURCE STREQUAL Q724_BEFORE_RULE)
    message(FATAL_ERROR "Q7.24 could not replace Q7.23 module endcap rule")
endif()

string(REPLACE
    "mode=3d-capsule-triangle+sweep-slide+raised-step+walkable-module-endcaps persistentOverlap=depth-aware"
    "mode=3d-capsule-triangle+sweep-slide+raised-step+local-contact-endcaps persistentOverlap=depth-aware"
    Q724_CONTROLLER_SOURCE "${Q724_CONTROLLER_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q724.inc"
     "${Q724_CONTROLLER_SOURCE}")
string(REPLACE
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q723.inc\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q724.inc\""
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")
