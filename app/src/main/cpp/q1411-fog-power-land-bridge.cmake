# Q14.1 build bridge: LAND is compiled as its own translation unit, so forward
# declare the Q14.1 fog-power getter used by the generated terrain shader upload.
# The implementation lives in the main native unit via
# fo3-megaton-cell-environment-q1410.h.

string(PREPEND Q720_TERRAIN_RENDER_SOURCE
       "float GetFo3FogPowerQ1410();\n\n")

string(FIND "${Q720_TERRAIN_RENDER_SOURCE}"
            "float GetFo3FogPowerQ1410();" Q1411_LAND_DECL_OK)
if(Q1411_LAND_DECL_OK EQUAL -1)
    message(FATAL_ERROR "Q14.1 LAND fog-power bridge declaration missing")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")

message(STATUS "Q14.1 LAND fog-power bridge enabled")
