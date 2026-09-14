# Q16.23 linkage repair: make the already-generated Q16.23 worldspace selector
# an explicit member of the translation unit that CMake actually compiles.
#
# q720-root-fixes builds fo3-cell-spawn-q720.cpp early and stores its path in
# Q720_CELL_SOURCE. Q16.23 later rewrites fo3-worldspace-q720.cpp in place with
# the thread-local 5x5 stream selector. On CI #522 the q6h stream worker compiled
# with calls to SetFo3WorldspaceGridRadiusOverrideQ1950(), but the compiled CELL
# translation unit did not export that definition. Rather than add a dummy
# symbol, route the final CELL translation unit through a uniquely named copy of
# the Q16.23 worldspace source and point Q720_CELL_SOURCE at that final file.

if(NOT DEFINED Q720_CELL_SOURCE OR NOT EXISTS "${Q720_CELL_SOURCE}")
    message(FATAL_ERROR "Q16.23 linkage repair expected generated CELL translation unit")
endif()

set(Q1951_WORLDSPACE_LIVE "${CMAKE_CURRENT_BINARY_DIR}/fo3-worldspace-q720.cpp")
if(NOT EXISTS "${Q1951_WORLDSPACE_LIVE}")
    message(FATAL_ERROR "Q16.23 linkage repair expected Q16.23 worldspace source")
endif()
file(READ "${Q1951_WORLDSPACE_LIVE}" Q1951_WORLDSPACE_SOURCE)

# Fail closed unless q1950 really did inject the stream-only selector and public
# setter. This ensures the linkage repair cannot accidentally route an old 3x3
# implementation back into the build.
string(FIND "${Q1951_WORLDSPACE_SOURCE}"
       "void SetFo3WorldspaceGridRadiusOverrideQ1950(int radius)"
       Q1951_SETTER_OK)
string(FIND "${Q1951_WORLDSPACE_SOURCE}"
       "q1950SelectionRadius"
       Q1951_RADIUS_OK)
if(Q1951_SETTER_OK EQUAL -1 OR Q1951_RADIUS_OK EQUAL -1)
    message(FATAL_ERROR "Q16.23 linkage repair found worldspace source without q1950 selector")
endif()

set(Q1951_WORLDSPACE_FINAL "${CMAKE_CURRENT_BINARY_DIR}/fo3-worldspace-q1951.cpp")
file(WRITE "${Q1951_WORLDSPACE_FINAL}" "${Q1951_WORLDSPACE_SOURCE}")

file(READ "${Q720_CELL_SOURCE}" Q1951_CELL_SOURCE)
set(Q1951_CELL_INCLUDE_OLD
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-worldspace-q720.cpp\"")
set(Q1951_CELL_INCLUDE_NEW
    "#include \"${Q1951_WORLDSPACE_FINAL}\"")
string(FIND "${Q1951_CELL_SOURCE}" "${Q1951_CELL_INCLUDE_OLD}" Q1951_CELL_INCLUDE_POS)
if(Q1951_CELL_INCLUDE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.23 linkage repair could not find generated q720 worldspace include")
endif()
string(REPLACE "${Q1951_CELL_INCLUDE_OLD}" "${Q1951_CELL_INCLUDE_NEW}"
       Q1951_CELL_SOURCE "${Q1951_CELL_SOURCE}")

set(Q1951_CELL_FINAL "${CMAKE_CURRENT_BINARY_DIR}/fo3-cell-spawn-q1951.cpp")
file(WRITE "${Q1951_CELL_FINAL}" "${Q1951_CELL_SOURCE}")
set(Q720_CELL_SOURCE "${Q1951_CELL_FINAL}")

# Configure-time proof: the exact translation unit handed to add_library now
# includes the final q1950-aware worldspace implementation.
file(READ "${Q720_CELL_SOURCE}" Q1951_CELL_VERIFY)
string(FIND "${Q1951_CELL_VERIFY}" "fo3-worldspace-q1951.cpp" Q1951_CELL_ROUTE_OK)
if(Q1951_CELL_ROUTE_OK EQUAL -1)
    message(FATAL_ERROR "Q16.23 linkage repair failed to route compiled CELL source")
endif()

message(STATUS "Q16.23 linkage repaired: compiled CELL translation unit now owns q1950 5x5 selector setter")
