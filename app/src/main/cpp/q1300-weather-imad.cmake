# Q13.0: apply the active WTHR Day Image Space Modifier (IMAD) on top of the
# resolved CELL/WRLD base IMGS. Q12.9 proved WastelandClear supplies a Day IMAD;
# this keeps the existing final-frame post path and only fixes authored data
# composition. With no game clock/weather transitions yet, the already-selected
# Day modifier is evaluated at full-strength/end state. Dynamic blending follows
# with the time/weather system.

string(REPLACE
    "#include \"fo3-imagespace-q1280.h\""
    "#include \"fo3-imagespace-q1280.h\"\n#include \"fo3-weather-imad-q1300.h\""
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "LoadFo3ImageSpaceQ1280(request.cellFormId, request.worldspaceFormId);"
    "LoadFo3ImageSpaceQ1300(request.cellFormId, request.worldspaceFormId);"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(FIND "${Q6H_NATIVE_SOURCE}" "fo3-weather-imad-q1300.h" Q1300_INCLUDE_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "LoadFo3ImageSpaceQ1300" Q1300_CALL_OK)
if(Q1300_INCLUDE_OK EQUAL -1 OR Q1300_CALL_OK EQUAL -1)
    message(FATAL_ERROR "Q13.0 weather IMAD hook drifted: include=${Q1300_INCLUDE_OK} call=${Q1300_CALL_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q13.0 active Day weather IMAD composition enabled")

# Q13.2 consumes IMAD's authored non-post sunlight/sky scale channels and feeds
# them into the existing WTHR environment lighting/sky renderer. This include
# chains the complete later renderer/runtime stack through Q16.13.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1320-weather-light.cmake")

# Q16.14 runs only after the full current runtime chain has completed. It keeps
# that mature loader intact, removes Q10.4's repeated ESM rescans, and services
# NativeActivity events during long synchronous authored asset work.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1860-boot-responsiveness.cmake")

# Q16.15 removes Q16.3's per-REFR full-ESM interaction fallback and primes
# one authored XTEL cache while the scene finishes loading. Live ray misses are
# memory-only; invisible/no-model load-door support is retained.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1870-live-door-cache.cmake")

# Q16.16 interprets the actual Bethesda bitmap-FNT glyph metrics correctly:
# left/right kerning plus ascent. This fixes the interaction text baseline and
# spacing without changing the Q16.15 door cache or any authored HUD assets.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1880-font-metrics.cmake")
