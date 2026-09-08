# Q13.3: replace the procedural-only exterior sky presentation with Fallout 3's
# authored WTHR weather layers while retaining Q10.0's proven gradient base.
# WTHR provides up to four cloud DDS paths, per-layer Day colours and ONAM
# speeds, plus the authored Sun colour and Sun Glare scalar.
#
# Q12.8 rewrites Q10.0's generated eye source into q1280-q4-generated.cpp and
# makes that the translation unit actually included by q6h-native-generated.cpp.
# Patch the FINAL eye source here; touching q1000-q4-generated.cpp at this point
# is a no-op and can produce a byte-identical APK.
set(Q1330_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1330_Q4_INPUT}")
    message(FATAL_ERROR "Q13.3 expected final Q12.8 eye source at ${Q1330_Q4_INPUT}")
endif()
file(READ "${Q1330_Q4_INPUT}" Q1330_Q4_SOURCE)

string(FIND "${Q6H_NATIVE_SOURCE}" "q1280-q4-generated.cpp" Q1330_FINAL_INCLUDE_BEFORE)
if(Q1330_FINAL_INCLUDE_BEFORE EQUAL -1)
    message(FATAL_ERROR "Q13.3 final native source is not including q1280-q4-generated.cpp")
endif()

string(REPLACE
    "#include \"fo3-environment-q1000.h\""
    "#include \"fo3-environment-q1000.h\"\n#include \"fo3-weather-sky-q1330.h\""
    Q1330_Q4_SOURCE "${Q1330_Q4_SOURCE}")

string(REPLACE
    "RenderFo3SkyQ1000(skyMvp.m);"
    "RenderFo3SkyQ1330(skyMvp.m);"
    Q1330_Q4_SOURCE "${Q1330_Q4_SOURCE}")

string(FIND "${Q1330_Q4_SOURCE}" "fo3-weather-sky-q1330.h" Q1330_INCLUDE_OK)
string(FIND "${Q1330_Q4_SOURCE}" "RenderFo3SkyQ1330(skyMvp.m);" Q1330_CALL_OK)
string(FIND "${Q1330_Q4_SOURCE}" "RenderFo3SkyQ1000(skyMvp.m);" Q1330_OLD_CALL_LEFT)
if(Q1330_INCLUDE_OK EQUAL -1 OR Q1330_CALL_OK EQUAL -1 OR NOT Q1330_OLD_CALL_LEFT EQUAL -1)
    message(FATAL_ERROR "Q13.3 final weather-sky hook drifted: include=${Q1330_INCLUDE_OK} call=${Q1330_CALL_OK} oldCall=${Q1330_OLD_CALL_LEFT}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp" "${Q1330_Q4_SOURCE}")

# The final native source already includes q1280-q4-generated.cpp by path, so
# overwriting that generated file is sufficient. Guard that relationship here so
# a future post milestone cannot silently bypass the sky renderer again.
string(FIND "${Q6H_NATIVE_SOURCE}" "q1280-q4-generated.cpp" Q1330_FINAL_INCLUDE_AFTER)
if(Q1330_FINAL_INCLUDE_AFTER EQUAL -1)
    message(FATAL_ERROR "Q13.3 final q4 include disappeared after weather-sky patch")
endif()

message(STATUS "Q13.3 authored WTHR cloud layers + sun treatment enabled in final Q12.8 eye source")
