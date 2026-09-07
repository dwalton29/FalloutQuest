# Q10.6: restore vanilla Fallout 3 asset fidelity without touching movement.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1060-land-vclr.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/q1060-land-material.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/q1060-nif-coverage.cmake")

# Re-write generated visual sources after Q10.5/Q10.6.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp" "${Q720_TERRAIN_RENDER_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-cell-spawn-q720.cpp" "${Q720_CELL_SOURCE_TEXT}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")

# Fail configure loudly if a prior generated-source marker drifts rather than
# silently shipping another partial-fidelity build.
string(FIND "${Q720_TERRAIN_DATA_SOURCE}" "DecodeVclrQ1060" Q1060_HAS_VCLR)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "uNormalGloss" Q1060_HAS_NORMAL)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "aVertexColor" Q1060_HAS_COLOR_ATTR)
string(FIND "${Q720_CELL_SOURCE_TEXT}" "fo3-terrain-material-q1060.cpp" Q1060_HAS_MATERIAL_INCLUDE)
string(FIND "${Q6H_NIF_SOURCE}" "Q10.6 NIF COVERAGE" Q1060_HAS_NIF_COVERAGE)
if(Q1060_HAS_VCLR LESS 0 OR Q1060_HAS_NORMAL LESS 0 OR Q1060_HAS_COLOR_ATTR LESS 0 OR
   Q1060_HAS_MATERIAL_INCLUDE LESS 0 OR Q1060_HAS_NIF_COVERAGE LESS 0)
    message(FATAL_ERROR "Q10.6 patch drift: VCLR=${Q1060_HAS_VCLR} normal=${Q1060_HAS_NORMAL} color=${Q1060_HAS_COLOR_ATTR} material=${Q1060_HAS_MATERIAL_INCLUDE} nif=${Q1060_HAS_NIF_COVERAGE}")
endif()
