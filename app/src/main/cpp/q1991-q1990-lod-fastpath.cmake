# Q16.27 follow-up: Q1990EnsureNativeLodForCell is called from the actual-grid
# publication path. Once the nine Level4 blocks for the current 4x4 macroblock
# are resident, ordinary frames must return immediately rather than rescanning
# the block list or writing another Android log line.

set(Q1991_NATIVE_FILE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp")
if(NOT EXISTS "${Q1991_NATIVE_FILE}")
    message(FATAL_ERROR "Q16.27 LOD fastpath expected q1990 generated renderer")
endif()
file(READ "${Q1991_NATIVE_FILE}" Q1991_NATIVE_SOURCE)

set(Q1991_OLD [==[
    const int32_t centreBlockX = Q1990FloorToLevel4Block(cellX);
    const int32_t centreBlockY = Q1990FloorToLevel4Block(cellY);
    gQ1990NativeLodCentreBlockX = centreBlockX;
    gQ1990NativeLodCentreBlockY = centreBlockY;
    ++gQ1990NativeLodSerial;
]==])
set(Q1991_NEW [==[
    const int32_t centreBlockX = Q1990FloorToLevel4Block(cellX);
    const int32_t centreBlockY = Q1990FloorToLevel4Block(cellY);
    if (sameOrigin && !gQ1990NativeLodBlocks.empty() &&
        centreBlockX == gQ1990NativeLodCentreBlockX &&
        centreBlockY == gQ1990NativeLodCentreBlockY) {
        return;
    }
    gQ1990NativeLodCentreBlockX = centreBlockX;
    gQ1990NativeLodCentreBlockY = centreBlockY;
    ++gQ1990NativeLodSerial;
]==])
string(FIND "${Q1991_NATIVE_SOURCE}" "${Q1991_OLD}" Q1991_POS)
if(Q1991_POS EQUAL -1)
    message(FATAL_ERROR "Q16.27 LOD fastpath could not find macroblock publication")
endif()
string(REPLACE "${Q1991_OLD}" "${Q1991_NEW}"
       Q1991_NATIVE_SOURCE "${Q1991_NATIVE_SOURCE}")
file(WRITE "${Q1991_NATIVE_FILE}" "${Q1991_NATIVE_SOURCE}")

string(FIND "${Q1991_NATIVE_SOURCE}" "sameOrigin && !gQ1990NativeLodBlocks.empty()" Q1991_VERIFY)
if(Q1991_VERIFY EQUAL -1)
    message(FATAL_ERROR "Q16.27 LOD fastpath verification failed")
endif()
message(STATUS "Q16.27 Level4 same-macroblock fastpath enabled")
