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
