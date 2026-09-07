# Q7.20 preserved root-fix layer.
include("${CMAKE_CURRENT_SOURCE_DIR}/q720-root-fixes-base.cmake")

# Q7.21 adds seam-aware traversal for modular walkable Megaton geometry.
include("${CMAKE_CURRENT_SOURCE_DIR}/q721-seam-bridge.cmake")

# Q7.22 extends that seam classification across a full capsule radius and
# validates the raised path to supported ground beyond the endcap.
include("${CMAKE_CURRENT_SOURCE_DIR}/q722-walkable-seam-manifold.cmake")

# Q7.23 uses the now-known GECK/NIF model identity to suppress only low
# side/endcap contacts on authored walkable modular assets.
include("${CMAKE_CURRENT_SOURCE_DIR}/q723-walkable-module-endcaps.cmake")

# Q7.24 uses the exact local 3D contact height (not whole-triangle maxY) for
# those modular endcaps and requires the same face to clear at raised height.
include("${CMAKE_CURRENT_SOURCE_DIR}/q724-contact-height-endcaps.cmake")

# Q7.25 replaces the asset-specific traversal response with a universal
# proxy-style swept capsule, footprint support manifold and constraint solver.
include("${CMAKE_CURRENT_SOURCE_DIR}/q725-character-proxy.cmake")

# Q8.0 replaces Q7.25's strongest-contact approximation with a clean-room
# Fallout 3/Havok-style character proxy: point/plane manifold, linear casts,
# separate support checks, geometry-only step edge welding and simplex solve.
include("${CMAKE_CURRENT_SOURCE_DIR}/q800-fo3-havok-proxy.cmake")

# Q8.1 preserves the actual per-triangle Havok welding information authored in
# Fallout 3's packed collision and uses exact mesh adjacency to remove welded
# low/internal ghost-edge contacts before they reach the character proxy.
include("${CMAKE_CURRENT_SOURCE_DIR}/q801-havok-welding.cmake")

# Q7.23 changed the CollisionTriangle tail before Q8.1 runs, so apply the Q8.1
# welding members against that current struct shape.
include("${CMAKE_CURRENT_SOURCE_DIR}/q801-welding-struct-fix.cmake")
