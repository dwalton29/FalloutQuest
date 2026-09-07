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

# Q8.2: low collision contacts may not cancel forward movement unless there is
# no supported walkable landing or the full raised capsule cannot clear them.
# This is geometry-only and retains normal collision as the fallback.
include("${CMAKE_CURRENT_SOURCE_DIR}/q802-validated-low-obstacle-traversal.cmake")

# Q8.3 fixes Q8.2's landing selector so a nearby old floor cannot hide the
# actual raised stair tread. Enumerate all walkable supports at each probe.
include("${CMAKE_CURRENT_SOURCE_DIR}/q803-all-surface-step-landing.cmake")

# Q9.0 reconstructs placed bhk mesh triangles back into coherent concave
# collision shapes. Low faces now transfer support onto a walkable top surface
# on the same/adjacent authored shape instead of being treated as anonymous
# triangle blockers.
include("${CMAKE_CURRENT_SOURCE_DIR}/q900-coherent-collision-shapes.cmake")

# Q9.1 replaces the active character movement path with a fresh conventional
# capsule controller: sweep; if blocked, up -> forward -> down; otherwise wall
# projection. Old low-obstacle/ledge/simplex logic is not used to decide steps.
include("${CMAKE_CURRENT_SOURCE_DIR}/q910-standard-capsule-controller.cmake")

# Q9.2 removes Q9.1's raised-forward clearance veto. Geometry wholly inside a
# one-step lower-body envelope cannot cancel horizontal movement; after moving,
# feet resolve to the highest valid support within one step. Tall geometry stays solid.
include("${CMAKE_CURRENT_SOURCE_DIR}/q920-step-envelope-controller.cmake")
