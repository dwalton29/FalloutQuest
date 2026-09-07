# Q10.7: world/asset fidelity repair after the Q10.6 material pass.
# - expand Fallout3.esm SCOL static collections into their authored STAT parts
# - preserve Bethesda NIF node subclasses, parent transforms and inherited props
# - accept NiTriBasedGeom subclasses used by Bethesda statics
# - stabilize TX01 terrain normals and sharpen oblique VR texture sampling
# Movement/collision algorithms are unchanged; newly discovered authored static
# geometry naturally enters the existing visual/collision placement pipeline.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1070-nif-hierarchy.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/q1070-scol-world-assembly.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/q1070-texture-terrain-fidelity.cmake")

# Final generated outputs for this milestone.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-static-nif-q6h.cpp" "${Q6H_NIF_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-worldspace-q720.cpp" "${Q720_WORLDSPACE_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp" "${Q720_TERRAIN_RENDER_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
