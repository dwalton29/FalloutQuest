# Q13.9: put Fallout 3's authored RGB-byte colours into the same linear-light
# domain as the sRGB-decoded diffuse/LAND textures established by Q11.8.
#
# This is deliberately NOT an artistic Fallout-green/yellow filter. WTHR sky,
# ambient, sunlight, Sun and fog; placed LIGH colours; and XEMI emittance are
# exact game-authored RGB values with only the standard sRGB transfer decoded.

# -----------------------------------------------------------------------------
# Exterior activation: convert WTHR environment and placed LIGH colours as soon
# as they are loaded from Fallout3.esm.
# -----------------------------------------------------------------------------
string(REPLACE
    "#include \"fo3-visual-depth-q1010.h\""
    "#include \"fo3-visual-depth-q1010.h\"\n#include \"fo3-authored-color-q1390.h\""
    Q720_CELL_SOURCE_TEXT "${Q720_CELL_SOURCE_TEXT}")

string(REPLACE
    "LoadFo3EnvironmentQ1000(gPendingTransitionQ74.worldspaceFormId);"
    "LoadFo3EnvironmentQ1390(gPendingTransitionQ74.worldspaceFormId);"
    Q720_CELL_SOURCE_TEXT "${Q720_CELL_SOURCE_TEXT}")
string(REPLACE
    "LoadFo3PlacedLightsQ1010(gPendingTransitionQ74.worldspaceFormId,"
    "LoadFo3PlacedLightsQ1390(gPendingTransitionQ74.worldspaceFormId,"
    Q720_CELL_SOURCE_TEXT "${Q720_CELL_SOURCE_TEXT}")
string(REPLACE
    "ResetFo3EnvironmentQ1000();"
    "ResetFo3EnvironmentQ1390();"
    Q720_CELL_SOURCE_TEXT "${Q720_CELL_SOURCE_TEXT}")

string(FIND "${Q720_CELL_SOURCE_TEXT}" "LoadFo3EnvironmentQ1390" Q1390_ENV_LOAD_OK)
string(FIND "${Q720_CELL_SOURCE_TEXT}" "LoadFo3PlacedLightsQ1390" Q1390_LIGHT_LOAD_OK)
string(FIND "${Q720_CELL_SOURCE_TEXT}" "ResetFo3EnvironmentQ1390" Q1390_ENV_RESET_OK)
if(Q1390_ENV_LOAD_OK EQUAL -1 OR Q1390_LIGHT_LOAD_OK EQUAL -1 OR Q1390_ENV_RESET_OK EQUAL -1)
    message(FATAL_ERROR "Q13.9 exterior colour-domain hook drifted: env=${Q1390_ENV_LOAD_OK} light=${Q1390_LIGHT_LOAD_OK} reset=${Q1390_ENV_RESET_OK}")
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-cell-spawn-q720.cpp"
     "${Q720_CELL_SOURCE_TEXT}")

# -----------------------------------------------------------------------------
# External emittance: Q13.8 resolves XEMI correctly, but its normalized RGB is
# still encoded colour. Decode it before the material shader receives it.
# -----------------------------------------------------------------------------
string(REPLACE
    "#include \"fo3-external-emittance-q1380.h\""
    "#include \"fo3-external-emittance-q1380.h\"\n#include \"fo3-authored-color-q1390.h\""
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "ResolveFo3ExternalEmittanceQ1380(0x00000A74u,"
    "ResolveFo3ExternalEmittanceQ1390(0x00000A74u,"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(FIND "${Q6H_NATIVE_SOURCE}" "ResolveFo3ExternalEmittanceQ1390(0x00000A74u," Q1390_XEMI_OK)
if(Q1390_XEMI_OK EQUAL -1)
    message(FATAL_ERROR "Q13.9 could not replace Q13.8 external-emittance resolver")
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Sky: Q13.3 reparses WTHR independently of gFo3EnvironmentQ1000, so convert its
# Sun/cloud colour fields separately in the FINAL Q12.8 eye source.
# -----------------------------------------------------------------------------
set(Q1390_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1390_Q4_INPUT}")
    message(FATAL_ERROR "Q13.9 expected final eye source at ${Q1390_Q4_INPUT}")
endif()
file(READ "${Q1390_Q4_INPUT}" Q1390_Q4_SOURCE)

string(REPLACE
    "#include \"fo3-weather-sky-q1330.h\""
    "#include \"fo3-weather-sky-q1330.h\"\n#include \"fo3-weather-sky-color-q1390.h\""
    Q1390_Q4_SOURCE "${Q1390_Q4_SOURCE}")
string(REPLACE
    "RenderFo3SkyQ1330(skyMvp.m);"
    "RenderFo3SkyQ1390(skyMvp.m);"
    Q1390_Q4_SOURCE "${Q1390_Q4_SOURCE}")

string(FIND "${Q1390_Q4_SOURCE}" "fo3-weather-sky-color-q1390.h" Q1390_SKY_INCLUDE_OK)
string(FIND "${Q1390_Q4_SOURCE}" "RenderFo3SkyQ1390(skyMvp.m);" Q1390_SKY_CALL_OK)
if(Q1390_SKY_INCLUDE_OK EQUAL -1 OR Q1390_SKY_CALL_OK EQUAL -1)
    message(FATAL_ERROR "Q13.9 final sky colour-domain hook drifted: include=${Q1390_SKY_INCLUDE_OK} call=${Q1390_SKY_CALL_OK}")
endif()
file(WRITE "${Q1390_Q4_INPUT}" "${Q1390_Q4_SOURCE}")

# Hard verification of the intended behaviour rather than an arbitrary colour.
string(FIND "${Q6H_NATIVE_SOURCE}" "fo3-authored-color-q1390.h" Q1390_NATIVE_INCLUDE_OK)
if(Q1390_NATIVE_INCLUDE_OK EQUAL -1)
    message(FATAL_ERROR "Q13.9 authored colour helper missing from final native source")
endif()

message(STATUS "Q13.9 authored WTHR/LIGH/XEMI sRGB-byte to linear-light correction enabled; no artistic filter")
