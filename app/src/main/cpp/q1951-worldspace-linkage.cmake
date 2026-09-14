# Q16.23 linkage repair: make the 5x5 stream selector part of the worldspace
# implementation that the FINAL generated CELL translation unit actually uses.
#
# CI #522 proved q6h could see the Q16.23 setter declaration but the linker could
# not find its definition. CI #524 then proved the mature CELL source no longer
# includes the early fo3-worldspace-q720.cpp name verbatim. Older patch layers
# can reroute that include, so discover the live include instead of hard-coding
# a historical generated filename.

if(NOT DEFINED Q720_CELL_SOURCE OR NOT EXISTS "${Q720_CELL_SOURCE}")
    message(FATAL_ERROR "Q16.23 linkage repair expected generated CELL translation unit")
endif()

file(READ "${Q720_CELL_SOURCE}" Q1951_CELL_SOURCE)

# Discover whichever worldspace implementation the mature CELL source currently
# owns. CMAKE_MATCH_1 is the quoted include path without the quotes.
string(REGEX MATCH
       "#include[ \t]+\"([^\"]*fo3-worldspace[^\"]*)\""
       Q1951_WORLDSPACE_INCLUDE
       "${Q1951_CELL_SOURCE}")
if(Q1951_WORLDSPACE_INCLUDE STREQUAL "")
    message(FATAL_ERROR
        "Q16.23 linkage repair could not discover worldspace include in ${Q720_CELL_SOURCE}")
endif()
set(Q1951_WORLDSPACE_INCLUDE_PATH "${CMAKE_MATCH_1}")

# Resolve absolute generated includes as-is. For a relative include, prefer the
# binary directory, then the source directory. Never silently fall back to an
# unrelated source revision.
if(IS_ABSOLUTE "${Q1951_WORLDSPACE_INCLUDE_PATH}")
    set(Q1951_WORLDSPACE_ACTIVE "${Q1951_WORLDSPACE_INCLUDE_PATH}")
elseif(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${Q1951_WORLDSPACE_INCLUDE_PATH}")
    set(Q1951_WORLDSPACE_ACTIVE
        "${CMAKE_CURRENT_BINARY_DIR}/${Q1951_WORLDSPACE_INCLUDE_PATH}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${Q1951_WORLDSPACE_INCLUDE_PATH}")
    set(Q1951_WORLDSPACE_ACTIVE
        "${CMAKE_CURRENT_SOURCE_DIR}/${Q1951_WORLDSPACE_INCLUDE_PATH}")
else()
    message(FATAL_ERROR
        "Q16.23 linkage repair discovered unresolved worldspace include: ${Q1951_WORLDSPACE_INCLUDE_PATH}")
endif()

file(READ "${Q1951_WORLDSPACE_ACTIVE}" Q1951_WORLDSPACE_SOURCE)
message(STATUS
    "Q16.23 linkage route: CELL=${Q720_CELL_SOURCE} worldspace=${Q1951_WORLDSPACE_ACTIVE}")

# Patch the ACTIVE worldspace lineage in place conceptually, but write a new
# generated q1951 copy so older stages remain immutable for diagnosis. If q1950
# already reached this lineage, simply reuse its implementation.
string(FIND "${Q1951_WORLDSPACE_SOURCE}"
       "void SetFo3WorldspaceGridRadiusOverrideQ1950(int radius)"
       Q1951_SETTER_POS)
string(FIND "${Q1951_WORLDSPACE_SOURCE}"
       "q1950SelectionRadius"
       Q1951_RADIUS_POS)

if(Q1951_SETTER_POS EQUAL -1 OR Q1951_RADIUS_POS EQUAL -1)
    set(Q1951_PUBLIC_OLD [==[
} // namespace

bool LoadFo3WorldspaceNeighborhoodQ75(uint32_t worldspaceFormId,
]==])
    set(Q1951_PUBLIC_NEW [==[
} // namespace

thread_local int gQ1950WorldspaceGridRadiusOverride = -1;

void SetFo3WorldspaceGridRadiusOverrideQ1950(int radius) {
    gQ1950WorldspaceGridRadiusOverride = radius;
}

bool LoadFo3WorldspaceNeighborhoodQ75(uint32_t worldspaceFormId,
]==])
    string(FIND "${Q1951_WORLDSPACE_SOURCE}" "${Q1951_PUBLIC_OLD}" Q1951_PUBLIC_POS)
    if(Q1951_PUBLIC_POS EQUAL -1)
        message(FATAL_ERROR
            "Q16.23 active worldspace lineage lacks public loader marker: ${Q1951_WORLDSPACE_ACTIVE}")
    endif()
    string(REPLACE "${Q1951_PUBLIC_OLD}" "${Q1951_PUBLIC_NEW}"
           Q1951_WORLDSPACE_SOURCE "${Q1951_WORLDSPACE_SOURCE}")

    set(Q1951_RADIUS_DECL_OLD [==[
    const bool loadWholeWorldspace = gridCells.size() <= SMALL_WORLDSPACE_CELL_LIMIT_Q75;

    std::unordered_set<uint32_t> selectedCells;
]==])
    set(Q1951_RADIUS_DECL_NEW [==[
    const bool loadWholeWorldspace = gridCells.size() <= SMALL_WORLDSPACE_CELL_LIMIT_Q75;
    const int q1950SelectionRadius = gQ1950WorldspaceGridRadiusOverride >= 0
        ? gQ1950WorldspaceGridRadiusOverride : GRID_RADIUS_Q75;

    std::unordered_set<uint32_t> selectedCells;
]==])
    string(FIND "${Q1951_WORLDSPACE_SOURCE}" "${Q1951_RADIUS_DECL_OLD}" Q1951_RADIUS_DECL_POS)
    if(Q1951_RADIUS_DECL_POS EQUAL -1)
        message(FATAL_ERROR
            "Q16.23 active worldspace lineage lacks selection declaration: ${Q1951_WORLDSPACE_ACTIVE}")
    endif()
    string(REPLACE "${Q1951_RADIUS_DECL_OLD}" "${Q1951_RADIUS_DECL_NEW}"
           Q1951_WORLDSPACE_SOURCE "${Q1951_WORLDSPACE_SOURCE}")

    set(Q1951_RADIUS_TEST_OLD [==[
            (std::abs(cell->gridX - targetGridX) <= GRID_RADIUS_Q75 &&
             std::abs(cell->gridY - targetGridY) <= GRID_RADIUS_Q75)) {
]==])
    set(Q1951_RADIUS_TEST_NEW [==[
            (std::abs(cell->gridX - targetGridX) <= q1950SelectionRadius &&
             std::abs(cell->gridY - targetGridY) <= q1950SelectionRadius)) {
]==])
    string(FIND "${Q1951_WORLDSPACE_SOURCE}" "${Q1951_RADIUS_TEST_OLD}" Q1951_RADIUS_TEST_POS)
    if(Q1951_RADIUS_TEST_POS EQUAL -1)
        message(FATAL_ERROR
            "Q16.23 active worldspace lineage lacks radius test: ${Q1951_WORLDSPACE_ACTIVE}")
    endif()
    string(REPLACE "${Q1951_RADIUS_TEST_OLD}" "${Q1951_RADIUS_TEST_NEW}"
           Q1951_WORLDSPACE_SOURCE "${Q1951_WORLDSPACE_SOURCE}")

    # Keep the live diagnostic truthful when this lineage had not yet received
    # q1950's earlier q720-only rewrite.
    string(REPLACE "Q7.5 WORLDSPACE CELLS:" "Q16.23 VISUAL WINDOW:"
           Q1951_WORLDSPACE_SOURCE "${Q1951_WORLDSPACE_SOURCE}")

    set(Q1951_LOG_RADIUS_OLD [==[
             GRID_RADIUS_Q75);
]==])
    set(Q1951_LOG_RADIUS_NEW [==[
             q1950SelectionRadius);
]==])
    string(FIND "${Q1951_WORLDSPACE_SOURCE}" "${Q1951_LOG_RADIUS_OLD}" Q1951_LOG_RADIUS_POS)
    if(NOT Q1951_LOG_RADIUS_POS EQUAL -1)
        string(REPLACE "${Q1951_LOG_RADIUS_OLD}" "${Q1951_LOG_RADIUS_NEW}"
               Q1951_WORLDSPACE_SOURCE "${Q1951_WORLDSPACE_SOURCE}")
    endif()
endif()

# Final proof before routing: the exact active lineage must now own both the
# public setter and the radius expression used by the loader.
string(FIND "${Q1951_WORLDSPACE_SOURCE}"
       "void SetFo3WorldspaceGridRadiusOverrideQ1950(int radius)"
       Q1951_FINAL_SETTER_OK)
string(FIND "${Q1951_WORLDSPACE_SOURCE}"
       "q1950SelectionRadius"
       Q1951_FINAL_RADIUS_OK)
if(Q1951_FINAL_SETTER_OK EQUAL -1 OR Q1951_FINAL_RADIUS_OK EQUAL -1)
    message(FATAL_ERROR "Q16.23 final worldspace selector verification failed")
endif()

set(Q1951_WORLDSPACE_FINAL "${CMAKE_CURRENT_BINARY_DIR}/fo3-worldspace-q1951.cpp")
file(WRITE "${Q1951_WORLDSPACE_FINAL}" "${Q1951_WORLDSPACE_SOURCE}")

# Redirect only the one discovered worldspace include in a copy of the mature
# CELL source. This preserves every other later CELL patch and then updates the
# variable CMake passes to add_library.
set(Q1951_CELL_INCLUDE_NEW "#include \"${Q1951_WORLDSPACE_FINAL}\"")
string(REPLACE "${Q1951_WORLDSPACE_INCLUDE}" "${Q1951_CELL_INCLUDE_NEW}"
       Q1951_CELL_SOURCE "${Q1951_CELL_SOURCE}")

set(Q1951_CELL_FINAL "${CMAKE_CURRENT_BINARY_DIR}/fo3-cell-spawn-q1951.cpp")
file(WRITE "${Q1951_CELL_FINAL}" "${Q1951_CELL_SOURCE}")
set(Q720_CELL_SOURCE "${Q1951_CELL_FINAL}")

file(READ "${Q720_CELL_SOURCE}" Q1951_CELL_VERIFY)
string(FIND "${Q1951_CELL_VERIFY}" "fo3-worldspace-q1951.cpp" Q1951_CELL_ROUTE_OK)
if(Q1951_CELL_ROUTE_OK EQUAL -1)
    message(FATAL_ERROR "Q16.23 linkage repair failed to route compiled CELL source")
endif()

message(STATUS
    "Q16.23 linkage repaired: final CELL translation unit owns q1950 5x5 stream selector")
