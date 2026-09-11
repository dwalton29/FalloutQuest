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
        "Q15.6 terrain stage link fix failed: include=${Q1561_INCLUDE_OK} call=${Q1561_CALL_OK} oldCall=${Q1561_OLD_CALL}")
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

# Q15.10+ makes the active test build visually undeniable in-headset: render the
# exact build label immediately above the left Touch controller.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1600-left-hand-build-label.cmake")

# Q15.11 follows the uploaded PC apitrace state at a real Megaton PPLighting draw:
# restore the signed normalized tangent-space LightData path, normalize the sampled
# normal map before DP3_sat, and match the captured 2.5x sunlight scale.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1610-pc-sp17-diffuse.cmake")

# Q15.12 resolves the BaseMap colour-space fork in the direction that produced a
# major device-side improvement: static DIFFUSE uses GL_SRGB8_ALPHA8 hardware
# input decode; normal/data textures and LAND remain untouched.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1620-static-basemap-linear-upload.cmake")

# Q15.13 finishes the captured SP17 static world-light core: tangent-space half
# vector, normal-map-alpha specular, the low-NdotL spec gate and exact warm
# PSLightColor spec contribution. Q15.12 BaseMap decode remains enabled.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1630-pc-sp17-core-equation.cmake")

# Q15.14 uses the actual PC call-4618221 final HDR/ImageSpace shader state rather
# than the guessed Q13.4 display-domain transform: exact Rec.601 saturation/tint,
# captured TargetLUM 1.2, legacy no-sRGB-write output semantics, and Q15.14 label.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1640-pc-hdr-output.cmake")

# Q15.15 tests the last output-domain assumption in Q15.14: write the captured PC
# numeric result directly to the OpenXR target instead of sRGB-decoding it first.
# Device luminance strongly suggests the inverse transfer was being displayed as-is.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1650-pc-output-domain.cmake")
