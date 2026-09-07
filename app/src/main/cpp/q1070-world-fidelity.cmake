# Q10.7: retain only the proven Quest texture/terrain fidelity work.
# The NIF parent-property inheritance experiment caused valid child shapes to
# inherit sticky material state (notably NoLighting) and could turn wall/floor
# regions black. SCOL expansion is also disabled: the device scan proved there
# are zero SCOL definitions/placements in the loaded Fallout3.esm Megaton path.
#
# Keep the pieces that were positively verified on-device:
# - authored LAND TX01 + VCLR from Q10.6
# - stable LAND tangent basis
# - 4x anisotropic filtering when the Quest driver exposes it
include("${CMAKE_CURRENT_SOURCE_DIR}/q1070-texture-terrain-fidelity.cmake")

# Final generated outputs for this milestone.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-static-nif-q6h.cpp" "${Q6H_NIF_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-worldspace-q720.cpp" "${Q720_WORLDSPACE_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp" "${Q720_TERRAIN_RENDER_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
