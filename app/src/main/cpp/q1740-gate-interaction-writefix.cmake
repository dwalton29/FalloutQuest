# Q16.4: make Q16.3's authored-door renderer changes actually reach the APK,
# and give Megaton's main gate a conservative interaction proxy.
#
# Device logs from Q16.3 proved:
#   - REFR 00003B2C is the real MegatonMainGate01 authored DOOR.
#   - its XTEL resolves correctly to exterior destination 00003B24 / CELL 00002DB4.
#   - the visual NIF contributes only one rendered triangle, while collision has
#     the full gate geometry, so the normal rendered-mesh AABB is not a useful
#     controller target.
#   - Q16.3 modified Q6H_NATIVE_SOURCE after Q16.0 had already written the
#     generated renderer file, so none of the ESM fallback code reached runtime.
#
# Q16.4 fixes both issues without touching CELL/XTEL/loading semantics.

# -----------------------------------------------------------------------------
# 1. Expand only MegatonMainGate01's interaction AABB. The ray still has the
# existing 3 m reach; this simply makes the gate's tiny one-triangle render proxy
# cover the physical gate that the collision system already sees.
# -----------------------------------------------------------------------------
set(Q1740_PAD_OLD [==[
    constexpr float PAD = 0.08f;
]==])
set(Q1740_PAD_NEW [==[
    const bool q1740MegatonMainGate =
        object.editorId == "MegatonMainGate01" ||
        object.modelPath.find("MegatonMainGate01") != std::string::npos ||
        object.modelPath.find("megatonmaingate01") != std::string::npos;
    const float PAD = q1740MegatonMainGate ? 6.50f : 0.08f;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1740_PAD_OLD}" Q1740_PAD_POS)
if(Q1740_PAD_POS EQUAL -1)
    message(FATAL_ERROR "Q16.4 could not find Q16 rendered-door RayAabb padding")
endif()
string(REPLACE "${Q1740_PAD_OLD}" "${Q1740_PAD_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Make runtime logs identify the fixed renderer path.
string(REPLACE "Q16.3 DOOR AIM FALLBACK:" "Q16.4 DOOR AIM FALLBACK:"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# CRITICAL Q16.3 write-order fix: q1730 runs after q1700's renderer file write.
# Persist the now-patched in-memory renderer so the compiler actually sees it.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# 2. Bump the visible headset proof from Q16.3 -> Q16.4.
# -----------------------------------------------------------------------------
set(Q1740_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1740_Q4_INPUT}")
    message(FATAL_ERROR "Q16.4 expected final OpenXR source at ${Q1740_Q4_INPUT}")
endif()
file(READ "${Q1740_Q4_INPUT}" Q1740_Q4_SOURCE)

set(Q1740_LABEL_OLD [==[
        q1600Digit(q1600X, 0x4Fu); // Q16.3: 3 = A B C D G
]==])
set(Q1740_LABEL_NEW [==[
        q1600Digit(q1600X, 0x66u); // Q16.4: 4 = F G B C
]==])
string(FIND "${Q1740_Q4_SOURCE}" "${Q1740_LABEL_OLD}" Q1740_LABEL_POS)
if(Q1740_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.4 could not find Q16.3 final build-label digit")
endif()
string(REPLACE "${Q1740_LABEL_OLD}" "${Q1740_LABEL_NEW}"
       Q1740_Q4_SOURCE "${Q1740_Q4_SOURCE}")
string(REPLACE "Q16.3 BUILD LABEL:" "Q16.4 BUILD LABEL:"
       Q1740_Q4_SOURCE "${Q1740_Q4_SOURCE}")
string(REPLACE "text=Q16.3 anchor=left-hand" "text=Q16.4 anchor=left-hand"
       Q1740_Q4_SOURCE "${Q1740_Q4_SOURCE}")
file(WRITE "${Q1740_Q4_INPUT}" "${Q1740_Q4_SOURCE}")

# Configure-time guards: the generic authored fallback and the gate proxy must
# both be in the generated renderer, and Q16.2 loading / Q16.1 HUD must survive.
string(FIND "${Q6H_NATIVE_SOURCE}" "QueryFo3AuthoredDoorAnchorQ1730" Q1740_FALLBACK_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "q1740MegatonMainGate" Q1740_GATE_PAD_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "6.50f" Q1740_GATE_RADIUS_OK)
string(FIND "${Q1740_Q4_SOURCE}" "Q16.4: 4 = F G B C" Q1740_LABEL_OK)
string(FIND "${Q1740_Q4_SOURCE}" "RenderFo3LoadingScreenQ1720" Q1740_LOADING_OK)
string(FIND "${Q1740_Q4_SOURCE}" "kButtonSegments = 24" Q1740_HUD_OK)
if(Q1740_FALLBACK_OK EQUAL -1 OR Q1740_GATE_PAD_OK EQUAL -1 OR
   Q1740_GATE_RADIUS_OK EQUAL -1 OR Q1740_LABEL_OK EQUAL -1 OR
   Q1740_LOADING_OK EQUAL -1 OR Q1740_HUD_OK EQUAL -1)
    message(FATAL_ERROR
        "Q16.4 verification failed: fallback=${Q1740_FALLBACK_OK} gatePad=${Q1740_GATE_PAD_OK} gateRadius=${Q1740_GATE_RADIUS_OK} label=${Q1740_LABEL_OK} loading=${Q1740_LOADING_OK} hud=${Q1740_HUD_OK}")
endif()

message(STATUS "Q16.4 gate interaction enabled: q1730 renderer persisted + MegatonMainGate01 6.5m proxy")

# Q16.5 replaces only the drawn interaction geometry with the real HUDMainMenu
# Info widget proportions discovered in Fallout - Misc.bsa, mounted above the
# right forearm for VR. Q16.4 interaction targeting remains authoritative.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1750-arm-mounted-vanilla-info.cmake")

# Q16.6 supersedes Q16.2's guessed loading card/clock with the authored
# loading_menu.xml composition recovered from Fallout - Misc.bsa.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1760-vanilla-loading-menu.cmake")
