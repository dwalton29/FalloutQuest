# Q14.5: controlled authored-fog A/B.
#
# Q14.4 falsified the mapped-normal/TBN path as the cause of Megaton's strong
# cyan/green cast. This test changes ONE stage only: the final WTHR fog blend in
# static/NIF + LAND shaders.
#
# A / default: AUTHORED_FOG - exact Q14.3/Q14.4 fog behaviour, including Q14.1
#                             FNAM near/far/power and active TOD fog colour.
# B / LEFT X: FOG_OFF       - fogFactor is forced to zero.
#
# Deliberately unchanged: WTHR Ambient/Sunlight/Sun/Sky RGB, Q13.9 colour
# transfer, ImageSpace/IMAD, exposure, bloom, AO, local lights, XEMI, materials,
# LAND materials/normals and Q14.3 exterior cast-shadow semantics.

# Shared state for renderer + final OpenXR host.
string(REPLACE
    "#include \"fo3-time-of-day-q1400.h\""
    "#include \"fo3-time-of-day-q1400.h\"\n#include \"fo3-fog-ab-q1450.h\""
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Static/NIF fog switch.
# -----------------------------------------------------------------------------
string(REPLACE
    "        uniform float uFogPower;"
    "        uniform float uFogPower;\n        uniform float uQ1450FogEnabled;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "GLint gFogPowerLocationQ1410 = -1;"
    "GLint gFogPowerLocationQ1410 = -1;\nGLint gFogEnabledLocationQ1450 = -1;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "    gFogPowerLocationQ1410 = glGetUniformLocation(gProgram, \"uFogPower\");"
    "    gFogPowerLocationQ1410 = glGetUniformLocation(gProgram, \"uFogPower\");\n    gFogEnabledLocationQ1450 = glGetUniformLocation(gProgram, \"uQ1450FogEnabled\");"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1450_STATIC_FOG_OLD [=[
            float fogT = clamp((fogDistance - uFogNear) /
                               max(uFogFar - uFogNear, 0.01), 0.0, 1.0);
            float fogFactor = pow(fogT, max(uFogPower, 0.01));
]=])
set(Q1450_STATIC_FOG_NEW [=[
            float fogT = clamp((fogDistance - uFogNear) /
                               max(uFogFar - uFogNear, 0.01), 0.0, 1.0);
            float fogFactor = uQ1450FogEnabled > 0.5
                ? pow(fogT, max(uFogPower, 0.01))
                : 0.0;
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1450_STATIC_FOG_OLD}" Q1450_STATIC_FOG_POS)
if(Q1450_STATIC_FOG_POS EQUAL -1)
    message(FATAL_ERROR "Q14.5 could not find Q14.1 static fog equation")
endif()
string(REPLACE "${Q1450_STATIC_FOG_OLD}" "${Q1450_STATIC_FOG_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1450_STATIC_UPLOAD_OLD [=[
    if (gFogPowerLocationQ1410 >= 0) glUniform1f(gFogPowerLocationQ1410, GetFo3FogPowerQ1410());
]=])
set(Q1450_STATIC_UPLOAD_NEW [=[
    if (gFogPowerLocationQ1410 >= 0) glUniform1f(gFogPowerLocationQ1410, GetFo3FogPowerQ1410());
    if (gFogEnabledLocationQ1450 >= 0) {
        glUniform1f(gFogEnabledLocationQ1450, GetFo3FogEnabledQ1450() ? 1.0f : 0.0f);
    }
    static bool q1450FogReadyLogged = false;
    if (!q1450FogReadyLogged && q1000Env.valid) {
        q1450FogReadyLogged = true;
        Q6H_LOGI("Q14.5 FOG A/B READY: mode=%s control=LEFT_X weather=%08X fogRGB=(%.3f %.3f %.3f) nearGame=%.1f farGame=%.1f nearRender=%.3f farRender=%.3f power=%.3f onlyFogBlendChanges=1",
                 GetFo3FogModeNameQ1450(), q1000Env.weatherFormId,
                 q1000Env.fog[0], q1000Env.fog[1], q1000Env.fog[2],
                 q1000Env.fogNear, q1000Env.fogFar,
                 q1000Env.fogNear / FO3_UNITS_PER_METRE,
                 q1000Env.fogFar / FO3_UNITS_PER_METRE,
                 GetFo3FogPowerQ1410());
    }
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1450_STATIC_UPLOAD_OLD}" Q1450_STATIC_UPLOAD_POS)
if(Q1450_STATIC_UPLOAD_POS EQUAL -1)
    message(FATAL_ERROR "Q14.5 could not find static fog uniform upload")
endif()
string(REPLACE "${Q1450_STATIC_UPLOAD_OLD}" "${Q1450_STATIC_UPLOAD_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# LAND fog switch. The shared state getter lives in the main translation unit,
# so LAND receives a tiny public bridge rather than duplicating runtime state.
# -----------------------------------------------------------------------------
string(PREPEND Q720_TERRAIN_RENDER_SOURCE
       "bool GetFo3FogEnabledQ1450Bridge();\n\n")

string(REPLACE
    "        uniform float uFogPower;"
    "        uniform float uFogPower;\n        uniform float uQ1450FogEnabled;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

string(REPLACE
    "GLint q1410TerrainFogPowerLocation = -1;"
    "GLint q1410TerrainFogPowerLocation = -1;\nGLint q1450TerrainFogEnabledLocation = -1;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

string(REPLACE
    "    q1410TerrainFogPowerLocation = -1;"
    "    q1410TerrainFogPowerLocation = -1;\n    q1450TerrainFogEnabledLocation = -1;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

string(REPLACE
    "    q1410TerrainFogPowerLocation = glGetUniformLocation(q76bProgram, \"uFogPower\");"
    "    q1410TerrainFogPowerLocation = glGetUniformLocation(q76bProgram, \"uFogPower\");\n    q1450TerrainFogEnabledLocation = glGetUniformLocation(q76bProgram, \"uQ1450FogEnabled\");"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

set(Q1450_LAND_FOG_OLD [=[
            float fogT = clamp((fogDistance - uFogNear) /
                               max(uFogFar - uFogNear, 0.01), 0.0, 1.0);
            float fogFactor = pow(fogT, max(uFogPower, 0.01));
]=])
set(Q1450_LAND_FOG_NEW [=[
            float fogT = clamp((fogDistance - uFogNear) /
                               max(uFogFar - uFogNear, 0.01), 0.0, 1.0);
            float fogFactor = uQ1450FogEnabled > 0.5
                ? pow(fogT, max(uFogPower, 0.01))
                : 0.0;
]=])
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1450_LAND_FOG_OLD}" Q1450_LAND_FOG_POS)
if(Q1450_LAND_FOG_POS EQUAL -1)
    message(FATAL_ERROR "Q14.5 could not find Q14.1 LAND fog equation")
endif()
string(REPLACE "${Q1450_LAND_FOG_OLD}" "${Q1450_LAND_FOG_NEW}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

set(Q1450_LAND_UPLOAD_OLD [=[
    if (q1410TerrainFogPowerLocation >= 0) glUniform1f(q1410TerrainFogPowerLocation, GetFo3FogPowerQ1410());
]=])
set(Q1450_LAND_UPLOAD_NEW [=[
    if (q1410TerrainFogPowerLocation >= 0) glUniform1f(q1410TerrainFogPowerLocation, GetFo3FogPowerQ1410());
    if (q1450TerrainFogEnabledLocation >= 0) {
        glUniform1f(q1450TerrainFogEnabledLocation,
                    GetFo3FogEnabledQ1450Bridge() ? 1.0f : 0.0f);
    }
]=])
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1450_LAND_UPLOAD_OLD}" Q1450_LAND_UPLOAD_POS)
if(Q1450_LAND_UPLOAD_POS EQUAL -1)
    message(FATAL_ERROR "Q14.5 could not find LAND fog uniform upload")
endif()
string(REPLACE "${Q1450_LAND_UPLOAD_OLD}" "${Q1450_LAND_UPLOAD_NEW}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# Main-TU implementation for the LAND bridge.
set(Q1450_BRIDGE [=[
bool GetFo3FogEnabledQ1450Bridge() {
    return GetFo3FogEnabledQ1450();
}

]=])
string(REPLACE
    "void Q6HGenFramebuffers(GLsizei n, GLuint* framebuffers) {"
    "${Q1450_BRIDGE}void Q6HGenFramebuffers(GLsizei n, GLuint* framebuffers) {"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Final OpenXR host: LEFT X toggles authored fog on/off. Q14.4's normal A/B is
# intentionally not included in this build. LEFT trigger time advance and RIGHT
# trigger activation remain untouched.
# -----------------------------------------------------------------------------
set(Q1450_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1450_Q4_INPUT}")
    message(FATAL_ERROR "Q14.5 expected final Q14.1 OpenXR source at ${Q1450_Q4_INPUT}")
endif()
file(READ "${Q1450_Q4_INPUT}" Q1450_Q4_SOURCE)

string(REPLACE
    "#include <vector>"
    "#include <vector>\n#include \"fo3-fog-ab-q1450.h\""
    Q1450_Q4_SOURCE "${Q1450_Q4_SOURCE}")

set(Q1450_ACTION_ANCHOR [=[
        if (!CreateAction("time_advance", "Advance Time", XR_ACTION_TYPE_FLOAT_INPUT,
                          1, &handPaths_[0], &timeAdvanceAction_)) return false;
]=])
set(Q1450_ACTION_NEW [=[
        if (!CreateAction("time_advance", "Advance Time", XR_ACTION_TYPE_FLOAT_INPUT,
                          1, &handPaths_[0], &timeAdvanceAction_)) return false;
        if (!CreateAction("fog_toggle", "Fog Toggle", XR_ACTION_TYPE_BOOLEAN_INPUT,
                          1, &handPaths_[0], &fogToggleAction_)) return false;
]=])
string(FIND "${Q1450_Q4_SOURCE}" "${Q1450_ACTION_ANCHOR}" Q1450_ACTION_POS)
if(Q1450_ACTION_POS EQUAL -1)
    message(FATAL_ERROR "Q14.5 could not find Q14.0 time action anchor")
endif()
string(REPLACE "${Q1450_ACTION_ANCHOR}" "${Q1450_ACTION_NEW}"
       Q1450_Q4_SOURCE "${Q1450_Q4_SOURCE}")

string(REPLACE
    "        XrPath leftTimeTriggerValue = XR_NULL_PATH;"
    "        XrPath leftTimeTriggerValue = XR_NULL_PATH;\n        XrPath leftFogX = XR_NULL_PATH;"
    Q1450_Q4_SOURCE "${Q1450_Q4_SOURCE}")
string(REPLACE
    "            !Path(\"/user/hand/left/input/trigger/value\", &leftTimeTriggerValue)) return false;"
    "            !Path(\"/user/hand/left/input/trigger/value\", &leftTimeTriggerValue) ||\n            !Path(\"/user/hand/left/input/x/click\", &leftFogX)) return false;"
    Q1450_Q4_SOURCE "${Q1450_Q4_SOURCE}")
string(REPLACE
    "        const std::array<XrActionSuggestedBinding, 7> bindings{{"
    "        const std::array<XrActionSuggestedBinding, 8> bindings{{"
    Q1450_Q4_SOURCE "${Q1450_Q4_SOURCE}")
string(REPLACE
    "            {timeAdvanceAction_, leftTimeTriggerValue},"
    "            {timeAdvanceAction_, leftTimeTriggerValue},\n            {fogToggleAction_, leftFogX},"
    Q1450_Q4_SOURCE "${Q1450_Q4_SOURCE}")

set(Q1450_SYNC_MARKER [=[
    void SyncInput(XrTime time) {
]=])
set(Q1450_SYNC_HELPER [=[
    bool ReadBooleanActionQ1450(XrAction action, XrPath subaction) {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        getInfo.subactionPath = subaction;
        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
        if (!CheckXr(xrGetActionStateBoolean(session_, &getInfo, &state),
                     "xrGetActionStateBoolean")) return false;
        return state.isActive && state.currentState;
    }

    void SyncInput(XrTime time) {
]=])
string(FIND "${Q1450_Q4_SOURCE}" "${Q1450_SYNC_MARKER}" Q1450_SYNC_POS)
if(Q1450_SYNC_POS EQUAL -1)
    message(FATAL_ERROR "Q14.5 could not find SyncInput anchor")
endif()
string(REPLACE "${Q1450_SYNC_MARKER}" "${Q1450_SYNC_HELPER}"
       Q1450_Q4_SOURCE "${Q1450_Q4_SOURCE}")

set(Q1450_SAMPLE_ANCHOR [=[
        timeAdvanceValue_ = ReadFloatAction(timeAdvanceAction_, handPaths_[0]);
]=])
set(Q1450_SAMPLE_NEW [=[
        timeAdvanceValue_ = ReadFloatAction(timeAdvanceAction_, handPaths_[0]);
        const bool q1450FogPressed = ReadBooleanActionQ1450(fogToggleAction_, handPaths_[0]);
        if (q1450FogPressed && !fogTogglePressed_) {
            ToggleFo3FogEnabledQ1450();
            FQ_LOGI("Q14.5 FOG MODE: mode=%s source=LEFT_X onlyFogBlendChanges=1",
                    GetFo3FogModeNameQ1450());
        }
        fogTogglePressed_ = q1450FogPressed;
]=])
string(FIND "${Q1450_Q4_SOURCE}" "${Q1450_SAMPLE_ANCHOR}" Q1450_SAMPLE_POS)
if(Q1450_SAMPLE_POS EQUAL -1)
    message(FATAL_ERROR "Q14.5 could not find Q14.0 input sample anchor")
endif()
string(REPLACE "${Q1450_SAMPLE_ANCHOR}" "${Q1450_SAMPLE_NEW}"
       Q1450_Q4_SOURCE "${Q1450_Q4_SOURCE}")

string(REPLACE
    "    XrAction timeAdvanceAction_{XR_NULL_HANDLE};"
    "    XrAction timeAdvanceAction_{XR_NULL_HANDLE};\n    XrAction fogToggleAction_{XR_NULL_HANDLE};"
    Q1450_Q4_SOURCE "${Q1450_Q4_SOURCE}")
string(REPLACE
    "    float timeAdvanceValue_{0.0f};"
    "    float timeAdvanceValue_{0.0f};\n    bool fogTogglePressed_{false};"
    Q1450_Q4_SOURCE "${Q1450_Q4_SOURCE}")

# Hard drift guards.
string(FIND "${Q6H_NATIVE_SOURCE}" "uQ1450FogEnabled" Q1450_STATIC_SHADER_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "uQ1450FogEnabled" Q1450_LAND_SHADER_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "GetFo3FogEnabledQ1450Bridge" Q1450_BRIDGE_OK)
string(FIND "${Q1450_Q4_SOURCE}" "fog_toggle" Q1450_ACTION_OK)
string(FIND "${Q1450_Q4_SOURCE}" "/user/hand/left/input/x/click" Q1450_BIND_OK)
string(FIND "${Q1450_Q4_SOURCE}" "ToggleFo3FogEnabledQ1450" Q1450_TOGGLE_OK)
string(FIND "${Q1450_Q4_SOURCE}" "fogTogglePressed_{false}" Q1450_STATE_OK)
if(Q1450_STATIC_SHADER_OK EQUAL -1 OR Q1450_LAND_SHADER_OK EQUAL -1 OR
   Q1450_BRIDGE_OK EQUAL -1 OR Q1450_ACTION_OK EQUAL -1 OR
   Q1450_BIND_OK EQUAL -1 OR Q1450_TOGGLE_OK EQUAL -1 OR
   Q1450_STATE_OK EQUAL -1)
    message(FATAL_ERROR
        "Q14.5 hook drifted: static=${Q1450_STATIC_SHADER_OK} land=${Q1450_LAND_SHADER_OK} bridge=${Q1450_BRIDGE_OK} action=${Q1450_ACTION_OK} bind=${Q1450_BIND_OK} toggle=${Q1450_TOGGLE_OK} state=${Q1450_STATE_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")
file(WRITE "${Q1450_Q4_INPUT}" "${Q1450_Q4_SOURCE}")

message(STATUS "Q14.5 authored exterior fog ON/OFF A/B enabled on LEFT X")
