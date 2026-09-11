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

# Q15.16 ports the captured PC SKY/SKYTEX semantics without touching Q15.15's
# world colour path: raw WTHR sky RGB * 1.55, PNAM-zero clouds remain visible,
# cloud V scrolling, and the authored Sky\\Sun.dds additive sun treatment.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1660-pc-sky.cmake")

# Q15.17 replaces Q15.2's approximate full-resolution 8-tap bloom source with
# the captured PC SP17 sequence: 640x256 linear downsample -> 256x256 point,
# per-tap bright threshold 0.55 + vertical ISBPBLUR15, then horizontal ISBLUR15.
# Q15.15 output-domain and Q15.16 sky paths remain untouched.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1670-pc-hdr-bloom.cmake")

# Current q4 still contains Q7.1's analog trigger-value proof probe. Normalize
# that live host to a boolean right-A action before Q16.0 adds its HUD/loading UI.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1699-active-input-normalizer.cmake")

# Q16.0 patches the heavily generated Q7.20 transition unit. Put its two stable
# public headers together at the front so the Q16 replacement anchor is
# deterministic even when older generators have inserted source includes between
# the originals. Both headers are guarded, so later duplicate includes are safe.
#
# Q7.20 also inserts transition bookkeeping between the player-reset assignment
# and the terrainReady declaration. Q16's inherited patch predates that insertion,
# so normalize only that declaration's position: remove its original occurrence
# and put it immediately after the stable reset prefix. No executable statements
# are removed or reordered relative to one another.
if(EXISTS "${Q720_CELL_SOURCE}")
    file(READ "${Q720_CELL_SOURCE}" Q1699_CELL_SOURCE)
    string(PREPEND Q1699_CELL_SOURCE
           "#include \"fo3-transition-q74.h\"\n#include \"fo3-terrain-q76.h\"\n")

    set(Q1699_COMPLETE_PREFIX
        "void CompleteFo3CellTransitionQ74(uint32_t cellFormId) {\n    gCurrentCellQ74 = cellFormId;\n    gPlayerResetPendingQ74 = true;\n")
    set(Q1699_TERRAIN_DECL "    bool terrainReady = false;\n")
    string(FIND "${Q1699_CELL_SOURCE}" "${Q1699_COMPLETE_PREFIX}" Q1699_COMPLETE_PREFIX_POS)
    string(FIND "${Q1699_CELL_SOURCE}" "${Q1699_TERRAIN_DECL}" Q1699_TERRAIN_DECL_POS)
    if(Q1699_COMPLETE_PREFIX_POS EQUAL -1 OR Q1699_TERRAIN_DECL_POS EQUAL -1)
        message(FATAL_ERROR
            "Q16.0 pre-normalizer could not find completion prefix/declaration: prefix=${Q1699_COMPLETE_PREFIX_POS} terrain=${Q1699_TERRAIN_DECL_POS}")
    endif()
    string(REPLACE "${Q1699_TERRAIN_DECL}" ""
           Q1699_CELL_SOURCE "${Q1699_CELL_SOURCE}")
    string(REPLACE "${Q1699_COMPLETE_PREFIX}"
           "${Q1699_COMPLETE_PREFIX}\n${Q1699_TERRAIN_DECL}"
           Q1699_CELL_SOURCE "${Q1699_CELL_SOURCE}")

    file(WRITE "${Q720_CELL_SOURCE}" "${Q1699_CELL_SOURCE}")
endif()

# The old q7-runtime renderer transform is no longer in the active chain. Restore
# only the required authored-door surfaces on the live Q6H renderer: record type,
# VR AABB, cached XTEL and the ray helper consumed by Q16.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1698-active-door-metadata.cmake")

# Q16 interior destinations need the same arbitrary-CELL REFR->BASE->MODL path
# that the historical mutable-Q7 renderer had, but the current live transition TU
# only contains the exterior WRLD neighborhood loader. Include the generic Q16
# interior parser immediately after fo3-cell-spawn.cpp, where the proven ESM
# compressed-record/subrecord helpers are already defined, and expose the exact
# legacy function name q1700's interior dispatch expects.
if(EXISTS "${Q720_CELL_SOURCE}")
    file(READ "${Q720_CELL_SOURCE}" Q1697_CELL_SOURCE)
    set(Q1697_SPAWN_END "#undef ProbeMegatonPlayerHouseDoorQ71\n")
    set(Q1697_INTERIOR_BRIDGE [=[
#undef ProbeMegatonPlayerHouseDoorQ71
#include "fo3-cell-interior-q1700.inc"

bool LoadFo3CellPlacements(uint32_t cellFormId,
                           std::vector<Fo3WorldPlacement>& outPlacements) {
    return LoadFo3InteriorCellPlacementsQ1700(cellFormId, outPlacements);
}
]=])
    string(FIND "${Q1697_CELL_SOURCE}" "${Q1697_SPAWN_END}" Q1697_SPAWN_END_POS)
    if(Q1697_SPAWN_END_POS EQUAL -1)
        message(FATAL_ERROR "Q16.0 could not find fo3-cell-spawn include tail for interior loader")
    endif()
    string(REPLACE "${Q1697_SPAWN_END}" "${Q1697_INTERIOR_BRIDGE}"
           Q1697_CELL_SOURCE "${Q1697_CELL_SOURCE}")
    file(WRITE "${Q720_CELL_SOURCE}" "${Q1697_CELL_SOURCE}")
endif()

# Q16.0 promotes the old proof-door path into real authored CELL traversal:
# right-hand aim prompt, right A activation, generic interior/exterior XTEL,
# and destination-aware Fallout3.esm LSCR loading screens presented before swap.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1700-cell-traversal-ui.cmake")

# Q16.1 keeps Q16.0 traversal intact and replaces only the temporary right-hand
# debug prompt with Fallout 3 HUDMainMenu's vanilla Info-widget styling.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1710-vanilla-interaction-hud.cmake")
