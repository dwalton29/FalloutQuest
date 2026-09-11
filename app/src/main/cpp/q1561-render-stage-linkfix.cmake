# Q15.6 link fix: terrain is a separate translation unit. Include the shared
# C++17 inline stage state directly instead of relying on a generated bridge
# symbol from the main renderer translation unit.
string(REPLACE
    "int GetFo3RenderStageQ1560Bridge();\n\n"
    "#include \"fo3-render-stage-q1560.h\"\n\n"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

string(REPLACE
    "GetFo3RenderStageQ1560Bridge()"
    "GetFo3RenderStageQ1560()"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "#include \"fo3-render-stage-q1560.h\"" Q1561_INCLUDE_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "GetFo3RenderStageQ1560()" Q1561_CALL_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "GetFo3RenderStageQ1560Bridge()" Q1561_OLD_CALL)
if(Q1561_INCLUDE_OK EQUAL -1 OR Q1561_CALL_OK EQUAL -1 OR NOT Q1561_OLD_CALL EQUAL -1)
    message(FATAL_ERROR
        "Q15.6 terrain stage link fix failed: include=${Q1561_INCLUDE_OK} call=${Q1561_CALL_OK} oldCall=${Q1561_OLD_CALL")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")
message(STATUS "Q15.6 terrain render-stage link fixed via shared inline state")

# Q15.7 runs after the Q15.6 terrain link rewrite so both generated render
# translation units and the final OpenXR source can share the new domain toggle.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1570-legacy-colour-domain-ab.cmake")

# Q15.8 is deliberately non-visual. It runs after every active renderer patch so
# the sampled normals and sun vector are exactly the inputs used by the final
# static/NIF shader, then emits one Megaton NdotL/coordinate-space trace.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1580-light-direction-trace.cmake")

# Q15.9 consumes the PC D3D9 capture result: SP17 AmbientColor/PSLightColor are
# raw normalized WTHR bytes (sunlight at the captured 1+base-dimmer scale), not
# Q13.9-decoded RGB. Keep this correction static/PPLighting-only for isolation.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1590-pc-sp17-light-constants.cmake")

# Q15.10 makes the active test build visually undeniable in-headset: render the
# exact build label immediately above the left Touch controller.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1600-left-hand-build-label.cmake")
