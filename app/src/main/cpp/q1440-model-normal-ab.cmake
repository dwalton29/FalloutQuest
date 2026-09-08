# Q14.4: controlled static-model directional-light normal A/B.
#
# Purpose: isolate whether FalloutQuest's tangent-space normal-map reconstruction
# is exaggerating the cool WTHR Ambient by driving too many Megaton surfaces out
# of the warm directional-sun Lambert term.
#
# A / default: MAPPED_NORMAL      - exact Q14.3 static-model behaviour.
# B / X toggle: NIF_VERTEX_NORMAL - authored NIF vertex normal is used ONLY for
#                                   directional diffuse Lambert.
#
# Deliberately unchanged: LAND, ESM WTHR colours, ImageSpace, fog, Q14.3 cast-
# shadow semantics, normal-map loading, specular, local lights, XEMI, AO, bloom
# and eye adaptation.

# Shared state is consumed by both the static renderer and the final OpenXR host.
string(REPLACE
    "#include \"fo3-time-of-day-q1400.h\""
    "#include \"fo3-time-of-day-q1400.h\"\n#include \"fo3-model-normal-test-q1440.h\""
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Static shader: expose one diagnostic switch and change ONLY the normal feeding
# the directional diffuse Lambert term. mappedNormal remains intact for all
# existing specular/local-light calculations.
string(REPLACE
    "        uniform vec3 uSunDirection;"
    "        uniform vec3 uSunDirection;\n        uniform float uQ1440UseVertexNormalForSun;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1440_OLD_LAMBERT [=[
            float lambert = max(dot(mappedNormal, lightDirection), 0.0);
]=])
set(Q1440_NEW_LAMBERT [=[
            vec3 q1440SunNormal = uQ1440UseVertexNormalForSun > 0.5
                ? normalize(vNormal)
                : mappedNormal;
            float lambert = max(dot(q1440SunNormal, lightDirection), 0.0);
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1440_OLD_LAMBERT}" Q1440_LAMBERT_POS)
if(Q1440_LAMBERT_POS EQUAL -1)
    message(FATAL_ERROR "Q14.4 could not find static mapped-normal Lambert anchor")
endif()
string(REPLACE "${Q1440_OLD_LAMBERT}" "${Q1440_NEW_LAMBERT}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "GLint gSunDirectionLocationQ1000 = -1;"
    "GLint gSunDirectionLocationQ1000 = -1;\nGLint gModelNormalModeLocationQ1440 = -1;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "    gSunDirectionLocationQ1000 = glGetUniformLocation(gProgram, \"uSunDirection\");"
    "    gSunDirectionLocationQ1000 = glGetUniformLocation(gProgram, \"uSunDirection\");\n    gModelNormalModeLocationQ1440 = glGetUniformLocation(gProgram, \"uQ1440UseVertexNormalForSun\");"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1440_STATIC_UNIFORM_ANCHOR [=[
    glUniform3fv(gEyePositionLocationQ1010, 1, gFo3EyePositionQ1010);
]=])
set(Q1440_STATIC_UNIFORM_NEW [=[
    if (gModelNormalModeLocationQ1440 >= 0) {
        glUniform1f(gModelNormalModeLocationQ1440,
                    GetFo3UseVertexNormalForSunQ1440() ? 1.0f : 0.0f);
    }
    static bool q1440InitialModeLogged = false;
    if (!q1440InitialModeLogged) {
        q1440InitialModeLogged = true;
        Q6H_LOGI("Q14.4 MODEL NORMAL A/B READY: mode=%s control=LEFT_X directionalDiffuseOnly=1 landUnchanged=1 normalMapLoaded=1 specularMappedNormal=1 localLightsMappedNormal=1",
                 GetFo3ModelNormalModeNameQ1440());
    }
    glUniform3fv(gEyePositionLocationQ1010, 1, gFo3EyePositionQ1010);
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1440_STATIC_UNIFORM_ANCHOR}" Q1440_UNIFORM_ANCHOR_POS)
if(Q1440_UNIFORM_ANCHOR_POS EQUAL -1)
    message(FATAL_ERROR "Q14.4 could not find static uniform upload anchor")
endif()
string(REPLACE "${Q1440_STATIC_UNIFORM_ANCHOR}" "${Q1440_STATIC_UNIFORM_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Final OpenXR host: LEFT X click toggles the A/B state. LEFT trigger retains
# Q14.0 time advance and RIGHT trigger retains activation.
# -----------------------------------------------------------------------------
set(Q1440_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1440_Q4_INPUT}")
    message(FATAL_ERROR "Q14.4 expected final Q14.1 OpenXR source at ${Q1440_Q4_INPUT}")
endif()
file(READ "${Q1440_Q4_INPUT}" Q1440_Q4_SOURCE)

# Header access for the toggle state.
string(REPLACE
    "#include <vector>"
    "#include <vector>\n#include \"fo3-model-normal-test-q1440.h\""
    Q1440_Q4_SOURCE "${Q1440_Q4_SOURCE}")

# Create a left-hand boolean action after the existing Q14.0 time action.
set(Q1440_ACTION_ANCHOR [=[
        if (!CreateAction("time_advance", "Advance Time", XR_ACTION_TYPE_FLOAT_INPUT,
                          1, &handPaths_[0], &timeAdvanceAction_)) return false;
]=])
set(Q1440_ACTION_NEW [=[
        if (!CreateAction("time_advance", "Advance Time", XR_ACTION_TYPE_FLOAT_INPUT,
                          1, &handPaths_[0], &timeAdvanceAction_)) return false;
        if (!CreateAction("model_normal_toggle", "Model Normal Toggle", XR_ACTION_TYPE_BOOLEAN_INPUT,
                          1, &handPaths_[0], &modelNormalToggleAction_)) return false;
]=])
string(FIND "${Q1440_Q4_SOURCE}" "${Q1440_ACTION_ANCHOR}" Q1440_ACTION_POS)
if(Q1440_ACTION_POS EQUAL -1)
    message(FATAL_ERROR "Q14.4 could not find Q14.0 time-action anchor")
endif()
string(REPLACE "${Q1440_ACTION_ANCHOR}" "${Q1440_ACTION_NEW}"
       Q1440_Q4_SOURCE "${Q1440_Q4_SOURCE}")

# Add Touch X click to the Q14.0 seven-binding block.
string(REPLACE
    "        XrPath leftTimeTriggerValue = XR_NULL_PATH;"
    "        XrPath leftTimeTriggerValue = XR_NULL_PATH;\n        XrPath leftModelNormalX = XR_NULL_PATH;"
    Q1440_Q4_SOURCE "${Q1440_Q4_SOURCE}")
string(REPLACE
    "            !Path(\"/user/hand/left/input/trigger/value\", &leftTimeTriggerValue)) return false;"
    "            !Path(\"/user/hand/left/input/trigger/value\", &leftTimeTriggerValue) ||\n            !Path(\"/user/hand/left/input/x/click\", &leftModelNormalX)) return false;"
    Q1440_Q4_SOURCE "${Q1440_Q4_SOURCE}")
string(REPLACE
    "        const std::array<XrActionSuggestedBinding, 7> bindings{{"
    "        const std::array<XrActionSuggestedBinding, 8> bindings{{"
    Q1440_Q4_SOURCE "${Q1440_Q4_SOURCE}")
string(REPLACE
    "            {timeAdvanceAction_, leftTimeTriggerValue},"
    "            {timeAdvanceAction_, leftTimeTriggerValue},\n            {modelNormalToggleAction_, leftModelNormalX},"
    Q1440_Q4_SOURCE "${Q1440_Q4_SOURCE}")

# q4 currently only needs float actions; add one small boolean reader immediately
# before SyncInput rather than changing any existing input helper.
set(Q1440_SYNC_MARKER [=[
    void SyncInput(XrTime time) {
]=])
set(Q1440_SYNC_HELPER [=[
    bool ReadBooleanActionQ1440(XrAction action, XrPath subaction) {
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
string(FIND "${Q1440_Q4_SOURCE}" "${Q1440_SYNC_MARKER}" Q1440_SYNC_MARKER_POS)
if(Q1440_SYNC_MARKER_POS EQUAL -1)
    message(FATAL_ERROR "Q14.4 could not find SyncInput anchor")
endif()
string(REPLACE "${Q1440_SYNC_MARKER}" "${Q1440_SYNC_HELPER}"
       Q1440_Q4_SOURCE "${Q1440_Q4_SOURCE}")

# Rising-edge toggle after Q14.0 samples the left trigger. No hold-repeat.
set(Q1440_SAMPLE_ANCHOR [=[
        timeAdvanceValue_ = ReadFloatAction(timeAdvanceAction_, handPaths_[0]);
]=])
set(Q1440_SAMPLE_NEW [=[
        timeAdvanceValue_ = ReadFloatAction(timeAdvanceAction_, handPaths_[0]);
        const bool q1440ModelNormalPressed =
            ReadBooleanActionQ1440(modelNormalToggleAction_, handPaths_[0]);
        if (q1440ModelNormalPressed && !modelNormalTogglePressed_) {
            ToggleFo3UseVertexNormalForSunQ1440();
            FQ_LOGI("Q14.4 MODEL NORMAL MODE: mode=%s source=LEFT_X directionalDiffuseOnly=1",
                    GetFo3ModelNormalModeNameQ1440());
        }
        modelNormalTogglePressed_ = q1440ModelNormalPressed;
]=])
string(FIND "${Q1440_Q4_SOURCE}" "${Q1440_SAMPLE_ANCHOR}" Q1440_SAMPLE_POS)
if(Q1440_SAMPLE_POS EQUAL -1)
    message(FATAL_ERROR "Q14.4 could not find Q14.0 input-sample anchor")
endif()
string(REPLACE "${Q1440_SAMPLE_ANCHOR}" "${Q1440_SAMPLE_NEW}"
       Q1440_Q4_SOURCE "${Q1440_Q4_SOURCE}")

# Class state.
string(REPLACE
    "    XrAction timeAdvanceAction_{XR_NULL_HANDLE};"
    "    XrAction timeAdvanceAction_{XR_NULL_HANDLE};\n    XrAction modelNormalToggleAction_{XR_NULL_HANDLE};"
    Q1440_Q4_SOURCE "${Q1440_Q4_SOURCE}")
string(REPLACE
    "    float timeAdvanceValue_{0.0f};"
    "    float timeAdvanceValue_{0.0f};\n    bool modelNormalTogglePressed_{false};"
    Q1440_Q4_SOURCE "${Q1440_Q4_SOURCE}")

# Hard guards: prove the static test changes only its Lambert normal and that the
# X-button action/binding/sample/state all landed in the final OpenXR source.
string(FIND "${Q6H_NATIVE_SOURCE}" "q1440SunNormal" Q1440_SHADER_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "uQ1440UseVertexNormalForSun" Q1440_UNIFORM_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "uSunlightColor * lambert * q1050SunVisibility" Q1440_Q143_LIT_OK)
string(FIND "${Q1440_Q4_SOURCE}" "model_normal_toggle" Q1440_ACTION_OK)
string(FIND "${Q1440_Q4_SOURCE}" "/user/hand/left/input/x/click" Q1440_BIND_OK)
string(FIND "${Q1440_Q4_SOURCE}" "ReadBooleanActionQ1440" Q1440_BOOL_OK)
string(FIND "${Q1440_Q4_SOURCE}" "ToggleFo3UseVertexNormalForSunQ1440" Q1440_TOGGLE_OK)
string(FIND "${Q1440_Q4_SOURCE}" "modelNormalTogglePressed_{false}" Q1440_STATE_OK)
if(Q1440_SHADER_OK EQUAL -1 OR Q1440_UNIFORM_OK EQUAL -1 OR
   Q1440_Q143_LIT_OK EQUAL -1 OR Q1440_ACTION_OK EQUAL -1 OR
   Q1440_BIND_OK EQUAL -1 OR Q1440_BOOL_OK EQUAL -1 OR
   Q1440_TOGGLE_OK EQUAL -1 OR Q1440_STATE_OK EQUAL -1)
    message(FATAL_ERROR
        "Q14.4 hook drifted: shader=${Q1440_SHADER_OK} uniform=${Q1440_UNIFORM_OK} q143lit=${Q1440_Q143_LIT_OK} action=${Q1440_ACTION_OK} bind=${Q1440_BIND_OK} bool=${Q1440_BOOL_OK} toggle=${Q1440_TOGGLE_OK} state=${Q1440_STATE_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")
file(WRITE "${Q1440_Q4_INPUT}" "${Q1440_Q4_SOURCE}")

message(STATUS "Q14.4 static model mapped-normal/NIF-vertex-normal sunlight A/B enabled on LEFT X")
