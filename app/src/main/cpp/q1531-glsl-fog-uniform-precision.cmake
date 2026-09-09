# Q15.3a: runtime GLSL link hotfix for the Q15.3 vertex-fog A/B.
#
# Q15.3 introduced the existing fog uniforms into the vertex shader as well as
# the fragment shader. In GLES 3.0, same-name uniforms shared across stages must
# match type AND precision. The fragment shader inherits `precision mediump float;`
# while the vertex stage defaults floating-point uniforms to highp, which can make
# the program fail to link on Adreno and leave only the fallback grid visible.
#
# Make the four shared fog uniforms explicitly mediump in every generated shader
# stage. No fog values, equations, placement semantics, lighting, or post values
# change here.

foreach(Q1531_SOURCE_VAR Q6H_NATIVE_SOURCE Q720_TERRAIN_RENDER_SOURCE)
    string(REPLACE
        "uniform vec3 uEyePosition;"
        "uniform mediump vec3 uEyePosition;"
        ${Q1531_SOURCE_VAR} "${${Q1531_SOURCE_VAR}}")
    string(REPLACE
        "uniform float uFogNear;"
        "uniform mediump float uFogNear;"
        ${Q1531_SOURCE_VAR} "${${Q1531_SOURCE_VAR}}")
    string(REPLACE
        "uniform float uFogFar;"
        "uniform mediump float uFogFar;"
        ${Q1531_SOURCE_VAR} "${${Q1531_SOURCE_VAR}}")
    string(REPLACE
        "uniform float uFogPower;"
        "uniform mediump float uFogPower;"
        ${Q1531_SOURCE_VAR} "${${Q1531_SOURCE_VAR}}")
endforeach()

string(FIND "${Q6H_NATIVE_SOURCE}" "uniform mediump vec3 uEyePosition;" Q1531_STATIC_EYE_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "uniform mediump float uFogNear;" Q1531_STATIC_NEAR_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "uniform mediump vec3 uEyePosition;" Q1531_LAND_EYE_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "uniform mediump float uFogNear;" Q1531_LAND_NEAR_OK)
if(Q1531_STATIC_EYE_OK EQUAL -1 OR Q1531_STATIC_NEAR_OK EQUAL -1 OR
   Q1531_LAND_EYE_OK EQUAL -1 OR Q1531_LAND_NEAR_OK EQUAL -1)
    message(FATAL_ERROR
        "Q15.3a fog precision hotfix failed: staticEye=${Q1531_STATIC_EYE_OK} staticNear=${Q1531_STATIC_NEAR_OK} landEye=${Q1531_LAND_EYE_OK} landNear=${Q1531_LAND_NEAR_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")

message(STATUS "Q15.3a GLSL shared fog-uniform precision matched across vertex + fragment stages")
