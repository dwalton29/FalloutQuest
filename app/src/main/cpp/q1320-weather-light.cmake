# Q13.2: apply authored IMGS + active WTHR Day IMAD sunlight/sky scale to the
# existing Fallout environment renderer. Q13.1 already composes the final-frame
# cinematic/HDR channels; this milestone consumes the non-imagespace lighting
# channels that FO3 uses to dim directional sunlight and the untextured sky.

string(REPLACE
    "#include \"fo3-weather-imad-q1300.h\""
    "#include \"fo3-weather-imad-q1300.h\"\n#include \"fo3-weather-light-q1320.h\""
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "LoadFo3ImageSpaceQ1300(request.cellFormId, request.worldspaceFormId);"
    "LoadFo3ImageSpaceQ1320(request.cellFormId, request.worldspaceFormId);"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(FIND "${Q6H_NATIVE_SOURCE}" "fo3-weather-light-q1320.h" Q1320_INCLUDE_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "LoadFo3ImageSpaceQ1320" Q1320_CALL_OK)
if(Q1320_INCLUDE_OK EQUAL -1 OR Q1320_CALL_OK EQUAL -1)
    message(FATAL_ERROR "Q13.2 weather-light hook drifted: include=${Q1320_INCLUDE_OK} call=${Q1320_CALL_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q13.2 authored weather sunlight/sky scaling enabled")

# Q13.3 adds the authored WTHR cloud DDS layers, per-layer colours/speeds and
# Sun colour/glare on top of the same proven gradient sky base.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1330-weather-sky.cmake")

# Q13.4 fixes the final-frame ordering mismatch that was crushing Megaton's dark
# values: keep lighting/bloom linear, apply Fallout's cinematic controls in a
# display-like transfer domain, then return to linear for the OpenXR sRGB target.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1340-hdr-display-domain.cmake")

# Q13.5 restores the missing temporal HDR bridge: a tiny GPU log-luminance probe
# updates one exposure history value from eye 0 and both stereo eyes consume it.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1350-hdr-eye-adaptation.cmake")

# Q13.6 fixes Q13.5's first-pass Target-LUM mapping. Device logs proved the old
# target/scene formula saturated at 2x for every Megaton view, so remap the legacy
# target into this renderer's luminance range and use a bounded sqrt response.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1360-eye-adaptation-remap.cmake")

# Q13.7 exposes the HDR eye depth as a sampleable texture and adds a conservative
# 8-tap contact AO pass to ground Bethesda-placed geometry without changing any
# authored ESM lighting, ImageSpace or material values.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1370-contact-ao.cmake")

# Q13.8 consumes the reference-level XEMI records actually authored throughout
# Megaton and gates them with the NIF External Emittance shader flag. Fixed LIGH
# emittance and REGN->WTHR Day endpoints now drive rendered glow/emissive shapes.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1380-external-emittance.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/q1381-external-emittance-diagnostics.cmake")

# Q13.9 fixes the remaining colour-domain mismatch: Fallout's authored WTHR,
# LIGH and XEMI RGB bytes are decoded through the standard sRGB transfer before
# entering the linear-light renderer. This is a transfer correction, not a tint.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1390-authored-color-space.cmake")

# Q14.0 replaces the frozen Day endpoint with the active CLMT/WTHR time-of-day
# state. A temporary left-trigger fast-forward lets us sweep the actual authored
# Sunrise/Day/Sunset/Night colours, IMADs and XEMI emittance in-headset.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1400-time-of-day.cmake")

# Q14.1 follows the spatial Megaton CELL's XCLR/XCIM links to the actual region
# weather and ImageSpace, fixes the Fallout 3 152-byte IMGS cinematic tail, and
# consumes the WTHR FNAM fog power. No hand-authored colour filter is added.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1410-megaton-cell-environment.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/q1411-fog-power-land-bridge.cmake")

# Q14.2's Ambient-domain A/B is retained in the repository as a falsified
# diagnostic but is deliberately not active from Q14.3 onward. Device testing
# showed that changing that transfer did not remove the cyan shadow character.

# Q14.3 removes Q10.5's Quest-added static/LAND cast-shadow map to match Fallout
# 3's default exterior rendering semantics. Directional Lambert shading remains;
# only architecture/world cast-shadow visibility is disabled.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1430-vanilla-exterior-shadow-semantics.cmake")

# Q14.4's model-normal A/B is retained as a falsified diagnostic but is not
# active in this build. Device testing showed essentially no change in the broad
# cyan/green cast when directional diffuse used authored NIF vertex normals.

# Q14.5 now isolates the distance/veiling path visible in the same captures.
# LEFT X toggles only the authored WTHR fog blend on statics + LAND. All lighting,
# materials, TOD, ImageSpace, AO, exposure and bloom remain identical.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1450-fog-ab.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/q1451-fog-ab-link.cmake")

# Q14.6 uses the shipped Fallout 3 ISCinematic shader as the reference rather
# than the earlier guessed display-domain reconstruction. It removes Q13.4's
# active gamma round-trip, switches cinematic luminance to 0.299/0.587/0.114,
# and restores saturation -> tint -> brightness -> contrast ordering.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1460-vanilla-cinematic.cmake")

# Q14.7 retains the proven LEFT Y input/state and its old encoded-domain shader
# branch as a dormant diagnostic implementation.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1470-pplighting-domain-ab.cmake")

# Q14.8 is retained as a falsified raw-WTHR staging diagnostic. Device testing
# showed raw byte/255 Ambient + Sunlight made Megaton brighter but preserved the
# same blue bias, ruling out Q13.9 RGB transfer as the primary cause.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1480-weather-byte-staging-ab.cmake")

# Q14.9 proved the cyan lives in world-light chroma: neutralizing Ambient +
# Sunlight removes the cast. It remains in the pipeline as the proven diagnostic
# layer that Q15.0 narrows further.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1490-world-light-chroma-isolation.cmake")

# Q15.0 keeps Ambient neutral at the same Rec.709 linear luminance but restores
# the authored Q13.9/Q14.0 Sunlight RGB. This isolates whether ambient is the
# broad cyan carrier while recovering Fallout 3's warm directional light.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1500-ambient-neutral-authored-sunlight.cmake")

# Q15.1 keeps Q15.0's successful non-cyan colour split but reduces only the
# neutral ambient fill to 65%. Authored warm sunlight remains unchanged; this
# isolates the remaining flat/washed appearance as an ambient-strength issue.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1510-ambient-strength.cmake")

# Q15.2 replaces the guessed Q13.5/Q13.6 scalar-exposure tone path with the
# equations decoded from the exact PC Shader Package 17 HDR shaders. Q15.1's
# lighting A/B remains untouched; this phase targets the remaining washout.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1520-sp17-hdr-tone.cmake")

# Q15.3b replaces the failed first vertex-fog experiment with the exact SP17
# metric decoded from SLS1011.vso: pre-divide ModelViewProj.xyz distance,
# nonlinear fog evaluated per vertex, then rasterizer interpolation. Q15.2's
# current per-fragment fog remains the default; LEFT X selects the SP17 path.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1532-sp17-projected-vertex-fog.cmake")
