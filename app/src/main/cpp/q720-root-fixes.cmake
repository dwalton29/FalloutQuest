# Q7.20 preserved root-fix layer.
include("${CMAKE_CURRENT_SOURCE_DIR}/q720-root-fixes-base.cmake")

# Q7.21 adds seam-aware traversal for modular walkable Megaton geometry.
include("${CMAKE_CURRENT_SOURCE_DIR}/q721-seam-bridge.cmake")

# Q7.22 extends that seam classification across a full capsule radius and
# validates the raised path to supported ground beyond the endcap.
include("${CMAKE_CURRENT_SOURCE_DIR}/q722-walkable-seam-manifold.cmake")
