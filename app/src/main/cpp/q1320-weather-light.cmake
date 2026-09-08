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
