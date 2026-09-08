# Q13.3: replace the procedural-only exterior sky presentation with Fallout 3's
# authored WTHR weather layers while retaining Q10.0's proven gradient base.
# WTHR provides up to four cloud DDS paths, per-layer Day colours and ONAM
# speeds, plus the authored Sun colour and Sun Glare scalar.

# Q10.0 materialised a generated q4 eye-pass file. Patch that file directly so
# later Q12.x/Q13.x native-source transforms remain untouched.
file(READ "${CMAKE_CURRENT_BINARY_DIR}/q1000-q4-generated.cpp" Q1330_Q4_SOURCE)

string(REPLACE
    "#include \"fo3-environment-q1000.h\""
    "#include \"fo3-environment-q1000.h\"\n#include \"fo3-weather-sky-q1330.h\""
    Q1330_Q4_SOURCE "${Q1330_Q4_SOURCE}")

string(REPLACE
    "RenderFo3SkyQ1000(skyMvp.m);"
    "RenderFo3SkyQ1330(skyMvp.m);"
    Q1330_Q4_SOURCE "${Q1330_Q4_SOURCE}")

string(FIND "${Q1330_Q4_SOURCE}" "fo3-weather-sky-q1330.h" Q1330_INCLUDE_OK)
string(FIND "${Q1330_Q4_SOURCE}" "RenderFo3SkyQ1330" Q1330_CALL_OK)
if(Q1330_INCLUDE_OK EQUAL -1 OR Q1330_CALL_OK EQUAL -1)
    message(FATAL_ERROR "Q13.3 weather-sky hook drifted: include=${Q1330_INCLUDE_OK} call=${Q1330_CALL_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q1000-q4-generated.cpp" "${Q1330_Q4_SOURCE}")
message(STATUS "Q13.3 authored WTHR cloud layers + sun treatment enabled")
