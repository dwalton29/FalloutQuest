# Q16.8: device-visible HUD/loading correction and final generated-source proof.
#
# Q16.6 was present in the shipped libfalloutquest.so, but device testing showed
# that its visual delta was effectively invisible: the loading image had been put
# back at ~86% eye width and the Q16.5 vector Info layout was too close in scale /
# placement to the old development prompt. Q16.8 makes the requested VR design
# unmistakable and explicitly snapshots the final OpenXR source into the native
# target so no later generated-source ambiguity is possible.

set(Q1780_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1780_Q4_INPUT}")
    message(FATAL_ERROR "Q16.8 expected final OpenXR source at ${Q1780_Q4_INPUT}")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fo3-loading-vr-q1780.h")
    message(FATAL_ERROR "Q16.8 VR loading renderer header is missing")
endif()
file(READ "${Q1780_Q4_INPUT}" Q1780_Q4_SOURCE)

# -----------------------------------------------------------------------------
# 1. Route loading through the unmistakable VR presentation: aspect-correct
#    ~62% LSCR, zero forced disparity, visible loading01-style spinner below.
# -----------------------------------------------------------------------------
string(PREPEND Q1780_Q4_SOURCE
       "#include \"fo3-loading-vr-q1780.h\"\n")

set(Q1780_LOADING_OLD
    "RenderFo3LoadingScreenQ1760(framebuffer_, eye.width, eye.height);")
set(Q1780_LOADING_NEW
    "RenderFo3LoadingScreenQ1780(framebuffer_, eye.width, eye.height);")
string(FIND "${Q1780_Q4_SOURCE}" "${Q1780_LOADING_OLD}" Q1780_LOADING_POS)
if(Q1780_LOADING_POS EQUAL -1)
    message(FATAL_ERROR "Q16.8 could not find Q16.6 loading draw dispatch")
endif()
string(REPLACE "${Q1780_LOADING_OLD}" "${Q1780_LOADING_NEW}"
       Q1780_Q4_SOURCE "${Q1780_Q4_SOURCE}")
string(REPLACE
    "ShutdownFo3LoadingScreenQ1760();"
    "ShutdownFo3LoadingScreenQ1780();"
    Q1780_Q4_SOURCE "${Q1780_Q4_SOURCE}")

# -----------------------------------------------------------------------------
# 2. Make the authored Info hierarchy materially legible in-headset. Keep the
#    same HUDMain glow and A / OPEN / DOOR semantics, but enlarge the Q16.5
#    geometry by ~22% and give the two text rows more vertical separation.
# -----------------------------------------------------------------------------
macro(q1780_replace_hud Q1780_OLD Q1780_NEW)
    string(FIND "${Q1780_Q4_SOURCE}" "${Q1780_OLD}" Q1780_FOUND)
    if(Q1780_FOUND EQUAL -1)
        message(FATAL_ERROR "Q16.8 could not find HUD token: ${Q1780_OLD}")
    endif()
    string(REPLACE "${Q1780_OLD}" "${Q1780_NEW}"
           Q1780_Q4_SOURCE "${Q1780_Q4_SOURCE}")
endmacro()

q1780_replace_hud(
    [=[constexpr float q1750CharW = 0.0135f;]=]
    [=[constexpr float q1750CharW = 0.0165f;]=])
q1780_replace_hud(
    [=[constexpr float q1750CharH = 0.0205f;]=]
    [=[constexpr float q1750CharH = 0.0250f;]=])
q1780_replace_hud(
    [=[constexpr float q1750CharGap = 0.0035f;]=]
    [=[constexpr float q1750CharGap = 0.0042f;]=])
q1780_replace_hud(
    [=[constexpr float q1750BottomY = 0.105f;]=]
    [=[constexpr float q1750BottomY = 0.097f;]=])
q1780_replace_hud(
    [=[constexpr float q1750TopY = q1750BottomY + q1750CharH + 0.0065f;]=]
    [=[constexpr float q1750TopY = q1750BottomY + q1750CharH + 0.0090f;]=])
q1780_replace_hud(
    [=[constexpr float q1750TextX = -0.030f;]=]
    [=[constexpr float q1750TextX = -0.025f;]=])
q1780_replace_hud(
    [=[constexpr float q1750ButtonCx = -0.074f;]=]
    [=[constexpr float q1750ButtonCx = -0.087f;]=])
q1780_replace_hud(
    [=[constexpr float q1750ButtonRx = 0.0275f;]=]
    [=[constexpr float q1750ButtonRx = 0.0335f;]=])
q1780_replace_hud(
    [=[constexpr float q1750ButtonRy = 0.0275f;]=]
    [=[constexpr float q1750ButtonRy = 0.0335f;]=])
q1780_replace_hud(
    [=[constexpr float aw = 0.0145f;]=]
    [=[constexpr float aw = 0.0175f;]=])
q1780_replace_hud(
    [=[constexpr float ah = 0.0210f;]=]
    [=[constexpr float ah = 0.0255f;]=])

# Add a one-shot runtime proof at the actual interaction draw site. This lets a
# device log distinguish 'door aim never became active' from 'HUD drew but was
# visually misplaced' without relying on configure-time source inspection.
set(Q1780_HUD_DRAW_OLD [==[
                    if (doorAimActiveQ1700_) {
                        // hud_main_menu.xml -> Info -> text_box.xml semantics:
]==])
set(Q1780_HUD_DRAW_NEW [==[
                    if (doorAimActiveQ1700_) {
                        static bool q1780HudDrawLogged = false;
                        if (!q1780HudDrawLogged) {
                            q1780HudDrawLogged = true;
                            __android_log_print(ANDROID_LOG_INFO, "FalloutQuest",
                                                "Q16.8 HUD DRAW: start=%d count=%d layout=A+OPEN/DOOR scale=1.22 HUDMainRGB=(26,255,128)",
                                                interactionStartVertex_, interactionVertexCount_);
                        }
                        // hud_main_menu.xml -> Info -> text_box.xml semantics:
]==])
string(FIND "${Q1780_Q4_SOURCE}" "${Q1780_HUD_DRAW_OLD}" Q1780_HUD_DRAW_POS)
if(Q1780_HUD_DRAW_POS EQUAL -1)
    message(FATAL_ERROR "Q16.8 could not find active HUD draw block")
endif()
string(REPLACE "${Q1780_HUD_DRAW_OLD}" "${Q1780_HUD_DRAW_NEW}"
       Q1780_Q4_SOURCE "${Q1780_Q4_SOURCE}")

# -----------------------------------------------------------------------------
# 3. Visible build proof: Q16.7 -> Q16.8.
# -----------------------------------------------------------------------------
set(Q1780_LABEL_OLD [==[
        q1600Digit(q1600X, 0x07u); // Q16.7: 7 = A B C
]==])
set(Q1780_LABEL_NEW [==[
        q1600Digit(q1600X, 0x7Fu); // Q16.8: 8 = A B C D E F G
]==])
string(FIND "${Q1780_Q4_SOURCE}" "${Q1780_LABEL_OLD}" Q1780_LABEL_POS)
if(Q1780_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.8 could not find Q16.7 build-label digit")
endif()
string(REPLACE "${Q1780_LABEL_OLD}" "${Q1780_LABEL_NEW}"
       Q1780_Q4_SOURCE "${Q1780_Q4_SOURCE}")
string(REPLACE "Q16.7 BUILD LABEL:" "Q16.8 BUILD LABEL:"
       Q1780_Q4_SOURCE "${Q1780_Q4_SOURCE}")
string(REPLACE "text=Q16.7 anchor=left-hand" "text=Q16.8 anchor=left-hand"
       Q1780_Q4_SOURCE "${Q1780_Q4_SOURCE}")

# -----------------------------------------------------------------------------
# 4. Freeze the fully-patched OpenXR host into its own final source and route the
#    actual q6h-native translation unit to that file. This is deliberately more
#    explicit than mutating q1280 in place: the compiler's input now names Q16.8.
# -----------------------------------------------------------------------------
set(Q1780_Q4_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/q1780-q4-generated.cpp")
file(WRITE "${Q1780_Q4_OUTPUT}" "${Q1780_Q4_SOURCE}")

set(Q1780_Q6H_INCLUDE_OLD
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp\"")
set(Q1780_Q6H_INCLUDE_NEW
    "#include \"${Q1780_Q4_OUTPUT}\"")
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1780_Q6H_INCLUDE_OLD}" Q1780_ROUTE_POS)
if(Q1780_ROUTE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.8 could not find q1280 include in final q6h native source")
endif()
string(REPLACE "${Q1780_Q6H_INCLUDE_OLD}" "${Q1780_Q6H_INCLUDE_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")

# Hard configure-time proof plus runtime-visible strings that can be checked in
# the produced libfalloutquest.so.
string(FIND "${Q1780_Q4_SOURCE}" "RenderFo3LoadingScreenQ1780" Q1780_LOADING_OK)
string(FIND "${Q1780_Q4_SOURCE}" "Q16.8 HUD DRAW:" Q1780_HUD_LOG_OK)
string(FIND "${Q1780_Q4_SOURCE}" "q1750CharW = 0.0165f" Q1780_HUD_SCALE_OK)
string(FIND "${Q1780_Q4_SOURCE}" "q1750Text(\"OPEN\"" Q1780_OPEN_OK)
string(FIND "${Q1780_Q4_SOURCE}" "q1750Text(\"DOOR\"" Q1780_DOOR_OK)
string(FIND "${Q1780_Q4_SOURCE}" "Q16.8: 8 = A B C D E F G" Q1780_LABEL_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "q1780-q4-generated.cpp" Q1780_ROUTE_OK)
if(Q1780_LOADING_OK EQUAL -1 OR Q1780_HUD_LOG_OK EQUAL -1 OR
   Q1780_HUD_SCALE_OK EQUAL -1 OR Q1780_OPEN_OK EQUAL -1 OR
   Q1780_DOOR_OK EQUAL -1 OR Q1780_LABEL_OK EQUAL -1 OR
   Q1780_ROUTE_OK EQUAL -1)
    message(FATAL_ERROR "Q16.8 final UI/runtime verification failed")
endif()

message(STATUS "Q16.8 visible UI runtime enabled: 62% VR LSCR + visible loading01 spinner + enlarged A/OPEN/DOOR HUD + explicit final q1780 source")
