# Q14.2: controlled WTHR Ambient colour-domain A/B.
#
# Q14.1's cell-region weather, Megaton ImageSpace and fog stay unchanged.
# Q13.9 also remains active for sky/fog/sun/sunlight/LIGH/XEMI. Only NAM0
# Ambient is overwritten after Q14.0's per-frame update with the authored
# normalized RGB bytes, allowing a clean test of the legacy Gamebryo constant
# domain without introducing an artistic tint.

# Make the Q14.2 helper visible to the final OpenXR host TU after Q14.0/Q14.1.
set(Q1420_INCLUDE_ANCHOR [=[
#undef LoadFo3ImageSpaceQ1280
]=])
set(Q1420_INCLUDE_NEW [=[
#undef LoadFo3ImageSpaceQ1280
#include "fo3-ambient-domain-q1420.h"
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1420_INCLUDE_ANCHOR}" Q1420_INCLUDE_POS)
if(Q1420_INCLUDE_POS EQUAL -1)
    message(FATAL_ERROR "Q14.2 could not find Q14.1 time-of-day include tail")
endif()
string(REPLACE "${Q1420_INCLUDE_ANCHOR}" "${Q1420_INCLUDE_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Apply the ambient override immediately after Q14.0 time and Q14.1 Fog Power
# have been updated, still once per stereo frame and before either eye draws.
set(Q1420_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1420_Q4_INPUT}")
    message(FATAL_ERROR "Q14.2 expected final Q14.1 eye source at ${Q1420_Q4_INPUT}")
endif()
file(READ "${Q1420_Q4_INPUT}" Q1420_Q4_SOURCE)

set(Q1420_CLOCK_ANCHOR [=[
                if (fo3todq1400::gRuntime.ready) {
                    UpdateFo3FogPowerQ1410(
                        fo3todq1400::gTestHour,
                        fo3todq1400::gRuntime.climate.sunriseBegin,
                        fo3todq1400::gRuntime.climate.sunriseEnd,
                        fo3todq1400::gRuntime.climate.sunsetBegin,
                        fo3todq1400::gRuntime.climate.sunsetEnd);
                }
]=])
set(Q1420_CLOCK_NEW [=[
                if (fo3todq1400::gRuntime.ready) {
                    UpdateFo3FogPowerQ1410(
                        fo3todq1400::gTestHour,
                        fo3todq1400::gRuntime.climate.sunriseBegin,
                        fo3todq1400::gRuntime.climate.sunriseEnd,
                        fo3todq1400::gRuntime.climate.sunsetBegin,
                        fo3todq1400::gRuntime.climate.sunsetEnd);
                }
                ApplyFo3AmbientDomainQ1420();
]=])
string(FIND "${Q1420_Q4_SOURCE}" "${Q1420_CLOCK_ANCHOR}" Q1420_CLOCK_POS)
if(Q1420_CLOCK_POS EQUAL -1)
    message(FATAL_ERROR "Q14.2 could not find Q14.1 per-frame Fog Power anchor")
endif()
string(REPLACE "${Q1420_CLOCK_ANCHOR}" "${Q1420_CLOCK_NEW}"
       Q1420_Q4_SOURCE "${Q1420_Q4_SOURCE}")

# Hard guards so this test cannot silently ship without the one-variable change.
string(FIND "${Q6H_NATIVE_SOURCE}" "fo3-ambient-domain-q1420.h" Q1420_HEADER_OK)
string(FIND "${Q1420_Q4_SOURCE}" "ApplyFo3AmbientDomainQ1420();" Q1420_APPLY_OK)
if(Q1420_HEADER_OK EQUAL -1 OR Q1420_APPLY_OK EQUAL -1)
    message(FATAL_ERROR
        "Q14.2 ambient-domain hook drifted: header=${Q1420_HEADER_OK} apply=${Q1420_APPLY_OK}")
endif()

file(WRITE "${Q1420_Q4_INPUT}" "${Q1420_Q4_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")

message(STATUS "Q14.2 WTHR Ambient raw normalized-byte domain A/B enabled; all other Q13.9 colour paths retained")
