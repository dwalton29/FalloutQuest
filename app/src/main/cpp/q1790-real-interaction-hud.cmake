# Q16.9: replace the diagnostic vector interaction prompt with Fallout 3's
# actual HUDMainMenu/Info assets loaded from the user's Fallout - Textures.bsa.
# The authored FNT/TAI/TEX/DDS data drives the pixels and proportions; only the
# controller-local physical scale/placement is a VR adaptation.

set(Q1790_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1780-q4-generated.cpp")
if(NOT EXISTS "${Q1790_Q4_INPUT}")
    message(FATAL_ERROR "Q16.9 expected Q16.8 final OpenXR source at ${Q1790_Q4_INPUT}")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fo3-interaction-hud-q1790.h")
    message(FATAL_ERROR "Q16.9 real interaction HUD renderer header is missing")
endif()
file(READ "${Q1790_Q4_INPUT}" Q1790_Q4_SOURCE)

string(PREPEND Q1790_Q4_SOURCE
       "#include \"fo3-interaction-hud-q1790.h\"\n")

# Route the actual active interaction branch to the real asset renderer. Keep the
# old vector block compiled but unreachable for one release so this patch does
# not depend on replacing a large generated block. There is intentionally no
# fake fallback: if the user's vanilla assets cannot be read, the runtime logs
# the missing asset instead of inventing a substitute UI.
set(Q1790_HUD_OLD [==[
                    if (doorAimActiveQ1700_) {
                        static bool q1780HudDrawLogged = false;
]==])
set(Q1790_HUD_NEW [==[
                    if (doorAimActiveQ1700_) {
                        RenderFo3InteractionHudQ1790(mvp.m);
                    }
                    if (false) {
                        static bool q1780HudDrawLogged = false;
]==])
string(FIND "${Q1790_Q4_SOURCE}" "${Q1790_HUD_OLD}" Q1790_HUD_POS)
if(Q1790_HUD_POS EQUAL -1)
    message(FATAL_ERROR "Q16.9 could not find Q16.8 interaction draw branch")
endif()
string(REPLACE "${Q1790_HUD_OLD}" "${Q1790_HUD_NEW}"
       Q1790_Q4_SOURCE "${Q1790_Q4_SOURCE}")

# Visible headset proof: Q16.8 -> Q16.9.
set(Q1790_LABEL_OLD [==[
        q1600Digit(q1600X, 0x7Fu); // Q16.8: 8 = A B C D E F G
]==])
set(Q1790_LABEL_NEW [==[
        q1600Digit(q1600X, 0x6Fu); // Q16.9: 9 = A B C D F G
]==])
string(FIND "${Q1790_Q4_SOURCE}" "${Q1790_LABEL_OLD}" Q1790_LABEL_POS)
if(Q1790_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.9 could not find Q16.8 build-label digit")
endif()
string(REPLACE "${Q1790_LABEL_OLD}" "${Q1790_LABEL_NEW}"
       Q1790_Q4_SOURCE "${Q1790_Q4_SOURCE}")
string(REPLACE "Q16.8 BUILD LABEL:" "Q16.9 BUILD LABEL:"
       Q1790_Q4_SOURCE "${Q1790_Q4_SOURCE}")
string(REPLACE "text=Q16.8 anchor=left-hand" "text=Q16.9 anchor=left-hand"
       Q1790_Q4_SOURCE "${Q1790_Q4_SOURCE}")

# Freeze Q16.9 as the final compiled OpenXR source, exactly like Q16.8 did for
# its diagnostic renderer.
set(Q1790_Q4_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/q1790-q4-generated.cpp")
file(WRITE "${Q1790_Q4_OUTPUT}" "${Q1790_Q4_SOURCE}")

set(Q1790_Q6H_INCLUDE_OLD
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/q1780-q4-generated.cpp\"")
set(Q1790_Q6H_INCLUDE_NEW
    "#include \"${Q1790_Q4_OUTPUT}\"")
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1790_Q6H_INCLUDE_OLD}" Q1790_ROUTE_POS)
if(Q1790_ROUTE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.9 could not find q1780 include in final q6h native source")
endif()
string(REPLACE
    "${Q1790_Q6H_INCLUDE_OLD}"
    "${Q1790_Q6H_INCLUDE_NEW}"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")

# Configure-time proof: actual real-asset renderer is the live branch; Q16.8
# loading and the existing interaction targeting/traversal stack remain.
string(FIND "${Q1790_Q4_SOURCE}" "fo3-interaction-hud-q1790.h" Q1790_INCLUDE_OK)
string(FIND "${Q1790_Q4_SOURCE}" "RenderFo3InteractionHudQ1790(mvp.m);" Q1790_DRAW_OK)
string(FIND "${Q1790_Q4_SOURCE}" "Q16.9: 9 = A B C D F G" Q1790_LABEL_OK)
string(FIND "${Q1790_Q4_SOURCE}" "RenderFo3LoadingScreenQ1780" Q1790_LOADING_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "q1790-q4-generated.cpp" Q1790_ROUTE_OK)
if(Q1790_INCLUDE_OK EQUAL -1 OR Q1790_DRAW_OK EQUAL -1 OR
   Q1790_LABEL_OK EQUAL -1 OR Q1790_LOADING_OK EQUAL -1 OR
   Q1790_ROUTE_OK EQUAL -1)
    message(FATAL_ERROR "Q16.9 real HUD/runtime verification failed")
endif()

message(STATUS "Q16.9 real Fallout interaction HUD enabled: HUDMainMenu/Info -> FNT + TAI + TEX/DDS from user Textures BSA; VR transform only")
