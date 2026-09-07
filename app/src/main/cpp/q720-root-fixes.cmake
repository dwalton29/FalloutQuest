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
