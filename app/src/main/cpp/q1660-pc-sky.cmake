# Q15.16: replace only the current Q13.9 final-eye sky draw with the standalone
# PC-captured SKY/SKYTEX renderer. Do NOT shadow/redefine any existing headers:
# Q15.15's world/material/HDR/output translation unit stays byte-for-byte on its
# already-proven path apart from this eye-source sky call and build label.

set(Q1660_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1660_Q4_INPUT}")
    message(FATAL_ERROR "Q15.16 expected final OpenXR source at ${Q1660_Q4_INPUT}")
endif()
file(READ "${Q1660_Q4_INPUT}" Q1660_Q4_SOURCE)

# Q13.3 inserted the authored-weather sky include and Q13.9 subsequently wrapped
# that renderer for its colour-domain conversion. Add the independent PC sky
# renderer beside the existing sky include; all weather/runtime types therefore
# stay sourced from the same physical headers with no duplicate definitions.
set(Q1660_INCLUDE_OLD [=[
#include "fo3-weather-sky-q1330.h"
]=])
set(Q1660_INCLUDE_NEW [=[
#include "fo3-weather-sky-q1330.h"
#include "fo3-pc-sky-q1660.h"
]=])
string(FIND "${Q1660_Q4_SOURCE}" "${Q1660_INCLUDE_OLD}" Q1660_INCLUDE_POS)
if(Q1660_INCLUDE_POS EQUAL -1)
    message(FATAL_ERROR "Q15.16 could not find authored sky include in final eye source")
endif()
string(REPLACE "${Q1660_INCLUDE_OLD}" "${Q1660_INCLUDE_NEW}"
       Q1660_Q4_SOURCE "${Q1660_Q4_SOURCE}")

# Q13.9 is the renderer actually present by this point in the milestone chain.
# Replace that wrapper only; world/material/HDR/output code remains untouched.
set(Q1660_CALL_OLD "RenderFo3SkyQ1390(skyMvp.m);")
set(Q1660_CALL_NEW "RenderFo3PcSkyQ1660(skyMvp.m);")
string(FIND "${Q1660_Q4_SOURCE}" "${Q1660_CALL_OLD}" Q1660_CALL_POS)
if(Q1660_CALL_POS EQUAL -1)
    message(FATAL_ERROR "Q15.16 could not find Q13.9 sky draw in final eye source")
endif()
string(REPLACE "${Q1660_CALL_OLD}" "${Q1660_CALL_NEW}"
       Q1660_Q4_SOURCE "${Q1660_Q4_SOURCE}")

# Visible build identity: Q15.15 -> Q15.16. Seven-segment 6 = A F G E D C.
string(REPLACE
    "q1600Digit(q1600X, 0x6Du); // 5 = A F G C D"
    "q1600Digit(q1600X, 0x7Du); // 6 = A F G E D C"
    Q1660_Q4_SOURCE "${Q1660_Q4_SOURCE}")
string(REPLACE "Q15.15" "Q15.16" Q1660_Q4_SOURCE "${Q1660_Q4_SOURCE}")

# Guards: Q15.16 must be present, the Q13.9 wrapper must no longer render, and
# the successful Q15.15 direct-PC output-domain assignment must remain intact.
string(FIND "${Q1660_Q4_SOURCE}" "#include \"fo3-pc-sky-q1660.h\"" Q1660_INCLUDE_OK)
string(FIND "${Q1660_Q4_SOURCE}" "RenderFo3PcSkyQ1660(skyMvp.m);" Q1660_CALL_OK)
string(FIND "${Q1660_Q4_SOURCE}" "RenderFo3SkyQ1390(skyMvp.m);" Q1660_OLD_CALL)
string(FIND "${Q1660_Q4_SOURCE}" "text=Q15.16 anchor=left-hand" Q1660_LABEL_OK)
string(FIND "${Q1660_Q4_SOURCE}" "q1600Digit(q1600X, 0x7Du)" Q1660_SIX_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "colour = q1640PcOutput;" Q1660_Q1515_OK)
if(Q1660_INCLUDE_OK EQUAL -1 OR Q1660_CALL_OK EQUAL -1 OR
   NOT Q1660_OLD_CALL EQUAL -1 OR Q1660_LABEL_OK EQUAL -1 OR
   Q1660_SIX_OK EQUAL -1 OR Q1660_Q1515_OK EQUAL -1)
    message(FATAL_ERROR
        "Q15.16 hook verification failed: include=${Q1660_INCLUDE_OK} call=${Q1660_CALL_OK} old=${Q1660_OLD_CALL} label=${Q1660_LABEL_OK} six=${Q1660_SIX_OK} q1515=${Q1660_Q1515_OK}")
endif()

file(WRITE "${Q1660_Q4_INPUT}" "${Q1660_Q4_SOURCE}")
message(STATUS "Q15.16 standalone PC sky enabled: raw WTHR*1.55, V-scroll SKYTEX clouds, textured additive Sun; Q15.15 world path untouched")
