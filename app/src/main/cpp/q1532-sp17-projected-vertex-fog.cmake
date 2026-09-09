# Q15.3b: reproduce Shader Package 17's actual exterior fog distance/evaluation.
#
# The previous Q15.3 test was intentionally rolled back after it exposed only the
# fallback grid on Quest. It also used the wrong metric: local-space aPosition
# was subtracted from a world-space eye position. This replacement starts from
# the known-good Q15.2 renderer and follows the shipped SLS1011.vso sequence:
#
#   clip.xyz = ModelViewProj * position
#   distance = length(clip.xyz)                  (before perspective divide)
#   t = 1 - clamp((far - distance)/(far-near), 0, 1)
#   fog = pow(t, power)
#
# The fog factor is evaluated in the vertex shader and interpolated by the
# rasterizer before the pixel-stage FogColor blend. LEFT X is reused strictly as
# an A/B selector:
#   true/default = current Q15.2 per-fragment eye/world-distance fog
#   false        = SP17 projected-xyz per-vertex/interpolated fog
#
# WTHR fog RGB, near/far, power, lighting, ImageSpace, HDR and materials do not
# change. Vertex-only fog uniforms use unique names so GLES never has to link a
# same-name uniform across stages with potentially different precision defaults.

set(Q1532_VERTEX_DECL_OLD [=[
        uniform mat4 uMvp;
        out vec3 vNormal;
]=])
set(Q1532_VERTEX_DECL_NEW [=[
        uniform mat4 uMvp;
        uniform highp float uFogNearVertexQ1532;
        uniform highp float uFogFarVertexQ1532;
        uniform highp float uFogPowerVertexQ1532;
        out vec3 vNormal;
        out highp float vFogFactorQ1532;
]=])

string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1532_VERTEX_DECL_OLD}" Q1532_STATIC_DECL_POS)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1532_VERTEX_DECL_OLD}" Q1532_LAND_DECL_POS)
if(Q1532_STATIC_DECL_POS EQUAL -1 OR Q1532_LAND_DECL_POS EQUAL -1)
    message(FATAL_ERROR
        "Q15.3b could not find vertex declaration anchors: static=${Q1532_STATIC_DECL_POS} land=${Q1532_LAND_DECL_POS}")
endif()
string(REPLACE "${Q1532_VERTEX_DECL_OLD}" "${Q1532_VERTEX_DECL_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE "${Q1532_VERTEX_DECL_OLD}" "${Q1532_VERTEX_DECL_NEW}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# Exact SP17 metric: projected clip XYZ prior to the perspective divide. Keep the
# existing MVP assignment but retain the clip vector for the vertex fog equation.
set(Q1532_POSITION_OLD [=[
            gl_Position = uMvp * vec4(aPosition, 1.0);
]=])
set(Q1532_POSITION_NEW [=[
            vec4 q1532Clip = uMvp * vec4(aPosition, 1.0);
            gl_Position = q1532Clip;
            float q1532FogDistance = length(q1532Clip.xyz);
            float q1532FogT = 1.0 - clamp(
                (uFogFarVertexQ1532 - q1532FogDistance) /
                (uFogFarVertexQ1532 - uFogNearVertexQ1532), 0.0, 1.0);
            vFogFactorQ1532 = pow(q1532FogT, uFogPowerVertexQ1532);
]=])

string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1532_POSITION_OLD}" Q1532_STATIC_POSITION_POS)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1532_POSITION_OLD}" Q1532_LAND_POSITION_POS)
if(Q1532_STATIC_POSITION_POS EQUAL -1 OR Q1532_LAND_POSITION_POS EQUAL -1)
    message(FATAL_ERROR
        "Q15.3b could not find MVP position anchors: static=${Q1532_STATIC_POSITION_POS} land=${Q1532_LAND_POSITION_POS}")
endif()
string(REPLACE "${Q1532_POSITION_OLD}" "${Q1532_POSITION_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE "${Q1532_POSITION_OLD}" "${Q1532_POSITION_NEW}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# Explicit highp on both sides avoids the precision/interface ambiguity that the
# first Q15.3 experiment created on GLES/Adreno.
string(REPLACE
    "        in vec3 vPosition;"
    "        in vec3 vPosition;\n        in highp float vFogFactorQ1532;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "        in vec3 vPosition;"
    "        in vec3 vPosition;\n        in highp float vFogFactorQ1532;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# Q14.5's one-bit uniform becomes only the A/B selector. The Q15.2 path is left
# byte-for-byte equivalent when the selector is true/default.
set(Q1532_FRAGMENT_OLD [=[
            float fogDistance = length(vPosition - uEyePosition);
            float fogT = clamp((fogDistance - uFogNear) /
                               max(uFogFar - uFogNear, 0.01), 0.0, 1.0);
            float fogFactor = uQ1450FogEnabled > 0.5
                ? pow(fogT, max(uFogPower, 0.01))
                : 0.0;
            lit = mix(lit, uFogColor, fogFactor);
]=])
set(Q1532_FRAGMENT_NEW [=[
            float fogDistance = length(vPosition - uEyePosition);
            float fogT = clamp((fogDistance - uFogNear) /
                               max(uFogFar - uFogNear, 0.01), 0.0, 1.0);
            float q1532QuestFragmentFog = pow(fogT, max(uFogPower, 0.01));
            float fogFactor = uQ1450FogEnabled > 0.5
                ? q1532QuestFragmentFog
                : vFogFactorQ1532;
            lit = mix(lit, uFogColor, fogFactor);
]=])

string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1532_FRAGMENT_OLD}" Q1532_STATIC_FRAGMENT_POS)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1532_FRAGMENT_OLD}" Q1532_LAND_FRAGMENT_POS)
if(Q1532_STATIC_FRAGMENT_POS EQUAL -1 OR Q1532_LAND_FRAGMENT_POS EQUAL -1)
    message(FATAL_ERROR
        "Q15.3b could not find Q14.5 fog selector blocks: static=${Q1532_STATIC_FRAGMENT_POS} land=${Q1532_LAND_FRAGMENT_POS}")
endif()
string(REPLACE "${Q1532_FRAGMENT_OLD}" "${Q1532_FRAGMENT_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE "${Q1532_FRAGMENT_OLD}" "${Q1532_FRAGMENT_NEW}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# -----------------------------------------------------------------------------
# Static/NIF vertex-only uniform locations and uploads.
# -----------------------------------------------------------------------------
string(REPLACE
    "GLint gFogEnabledLocationQ1450 = -1;"
    "GLint gFogEnabledLocationQ1450 = -1;\nGLint gFogNearVertexLocationQ1532 = -1;\nGLint gFogFarVertexLocationQ1532 = -1;\nGLint gFogPowerVertexLocationQ1532 = -1;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "    gFogEnabledLocationQ1450 = glGetUniformLocation(gProgram, \"uQ1450FogEnabled\");"
    "    gFogEnabledLocationQ1450 = glGetUniformLocation(gProgram, \"uQ1450FogEnabled\");\n    gFogNearVertexLocationQ1532 = glGetUniformLocation(gProgram, \"uFogNearVertexQ1532\");\n    gFogFarVertexLocationQ1532 = glGetUniformLocation(gProgram, \"uFogFarVertexQ1532\");\n    gFogPowerVertexLocationQ1532 = glGetUniformLocation(gProgram, \"uFogPowerVertexQ1532\");"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1532_STATIC_SELECTOR_UPLOAD_OLD [=[
    if (gFogEnabledLocationQ1450 >= 0) {
        glUniform1f(gFogEnabledLocationQ1450, GetFo3FogEnabledQ1450() ? 1.0f : 0.0f);
    }
]=])
set(Q1532_STATIC_SELECTOR_UPLOAD_NEW [=[
    if (gFogEnabledLocationQ1450 >= 0) {
        glUniform1f(gFogEnabledLocationQ1450, GetFo3FogEnabledQ1450() ? 1.0f : 0.0f);
    }
    const float q1532StaticFogNear =
        (q1000Env.valid && q1000Env.fogFar > q1000Env.fogNear + 1.0f)
            ? std::max(0.0f, q1000Env.fogNear / FO3_UNITS_PER_METRE)
            : 10000.0f;
    const float q1532StaticFogFar =
        (q1000Env.valid && q1000Env.fogFar > q1000Env.fogNear + 1.0f)
            ? std::max(0.1f, q1000Env.fogFar / FO3_UNITS_PER_METRE)
            : 10001.0f;
    if (gFogNearVertexLocationQ1532 >= 0)
        glUniform1f(gFogNearVertexLocationQ1532, q1532StaticFogNear);
    if (gFogFarVertexLocationQ1532 >= 0)
        glUniform1f(gFogFarVertexLocationQ1532, q1532StaticFogFar);
    if (gFogPowerVertexLocationQ1532 >= 0)
        glUniform1f(gFogPowerVertexLocationQ1532, GetFo3FogPowerQ1410());
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1532_STATIC_SELECTOR_UPLOAD_OLD}" Q1532_STATIC_UPLOAD_POS)
if(Q1532_STATIC_UPLOAD_POS EQUAL -1)
    message(FATAL_ERROR "Q15.3b could not find static Q14.5 selector upload")
endif()
string(REPLACE "${Q1532_STATIC_SELECTOR_UPLOAD_OLD}" "${Q1532_STATIC_SELECTOR_UPLOAD_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# LAND vertex-only uniform locations, reset, query and uploads.
# -----------------------------------------------------------------------------
string(REPLACE
    "GLint q1450TerrainFogEnabledLocation = -1;"
    "GLint q1450TerrainFogEnabledLocation = -1;\nGLint q1532TerrainFogNearVertexLocation = -1;\nGLint q1532TerrainFogFarVertexLocation = -1;\nGLint q1532TerrainFogPowerVertexLocation = -1;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

string(REPLACE
    "    q1450TerrainFogEnabledLocation = -1;"
    "    q1450TerrainFogEnabledLocation = -1;\n    q1532TerrainFogNearVertexLocation = -1;\n    q1532TerrainFogFarVertexLocation = -1;\n    q1532TerrainFogPowerVertexLocation = -1;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

string(REPLACE
    "    q1450TerrainFogEnabledLocation = glGetUniformLocation(q76bProgram, \"uQ1450FogEnabled\");"
    "    q1450TerrainFogEnabledLocation = glGetUniformLocation(q76bProgram, \"uQ1450FogEnabled\");\n    q1532TerrainFogNearVertexLocation = glGetUniformLocation(q76bProgram, \"uFogNearVertexQ1532\");\n    q1532TerrainFogFarVertexLocation = glGetUniformLocation(q76bProgram, \"uFogFarVertexQ1532\");\n    q1532TerrainFogPowerVertexLocation = glGetUniformLocation(q76bProgram, \"uFogPowerVertexQ1532\");"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

set(Q1532_LAND_SELECTOR_UPLOAD_OLD [=[
    if (q1450TerrainFogEnabledLocation >= 0) {
        glUniform1f(q1450TerrainFogEnabledLocation,
                    GetFo3FogEnabledQ1450Bridge() ? 1.0f : 0.0f);
    }
]=])
set(Q1532_LAND_SELECTOR_UPLOAD_NEW [=[
    if (q1450TerrainFogEnabledLocation >= 0) {
        glUniform1f(q1450TerrainFogEnabledLocation,
                    GetFo3FogEnabledQ1450Bridge() ? 1.0f : 0.0f);
    }
    const float q1532TerrainFogNear =
        (q1000Env.valid && q1000Env.fogFar > q1000Env.fogNear + 1.0f)
            ? std::max(0.0f, q1000Env.fogNear / 70.0f)
            : 10000.0f;
    const float q1532TerrainFogFar =
        (q1000Env.valid && q1000Env.fogFar > q1000Env.fogNear + 1.0f)
            ? std::max(0.1f, q1000Env.fogFar / 70.0f)
            : 10001.0f;
    if (q1532TerrainFogNearVertexLocation >= 0)
        glUniform1f(q1532TerrainFogNearVertexLocation, q1532TerrainFogNear);
    if (q1532TerrainFogFarVertexLocation >= 0)
        glUniform1f(q1532TerrainFogFarVertexLocation, q1532TerrainFogFar);
    if (q1532TerrainFogPowerVertexLocation >= 0)
        glUniform1f(q1532TerrainFogPowerVertexLocation, GetFo3FogPowerQ1410());
]=])
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1532_LAND_SELECTOR_UPLOAD_OLD}" Q1532_LAND_UPLOAD_POS)
if(Q1532_LAND_UPLOAD_POS EQUAL -1)
    message(FATAL_ERROR "Q15.3b could not find LAND Q14.5 selector upload")
endif()
string(REPLACE "${Q1532_LAND_SELECTOR_UPLOAD_OLD}" "${Q1532_LAND_SELECTOR_UPLOAD_NEW}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# Diagnostics: make the existing X action/logs describe selector semantics rather
# than the obsolete Q14.5 fog on/off meaning.
string(REPLACE "Q14.5 FOG A/B READY:" "Q15.3b SP17 FOG A/B READY:"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE "onlyFogBlendChanges=1"
       "fogDataUnchanged=1 sp17Metric=MVP_PRE_DIVIDE_XYZ sp17Placement=VERTEX_INTERPOLATED"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1532_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1532_Q4_INPUT}")
    message(FATAL_ERROR "Q15.3b expected final Q15.2 OpenXR source at ${Q1532_Q4_INPUT}")
endif()
file(READ "${Q1532_Q4_INPUT}" Q1532_Q4_SOURCE)
string(REPLACE "Q14.5 FOG MODE:" "Q15.3b SP17 FOG MODE:"
       Q1532_Q4_SOURCE "${Q1532_Q4_SOURCE}")
string(REPLACE "onlyFogBlendChanges=1"
       "fogDataUnchanged=1 sp17Metric=MVP_PRE_DIVIDE_XYZ sp17Placement=VERTEX_INTERPOLATED"
       Q1532_Q4_SOURCE "${Q1532_Q4_SOURCE}")
file(WRITE "${Q1532_Q4_INPUT}" "${Q1532_Q4_SOURCE}")

# Hard guards. These deliberately check the exact corrected metric and the unique
# vertex-only uniform names, and reject the broken local/world subtraction from
# the first Q15.3 experiment.
string(FIND "${Q6H_NATIVE_SOURCE}" "length(q1532Clip.xyz)" Q1532_STATIC_METRIC_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "length(q1532Clip.xyz)" Q1532_LAND_METRIC_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "in highp float vFogFactorQ1532;" Q1532_STATIC_VARYING_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "in highp float vFogFactorQ1532;" Q1532_LAND_VARYING_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "uFogNearVertexQ1532" Q1532_STATIC_UNIFORM_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "uFogNearVertexQ1532" Q1532_LAND_UNIFORM_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "length(aPosition - uEyePosition)" Q1532_BROKEN_STATIC_POS)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "length(aPosition - uEyePosition)" Q1532_BROKEN_LAND_POS)
string(FIND "${Q1532_Q4_SOURCE}" "Q15.3b SP17 FOG MODE" Q1532_LOG_OK)
if(Q1532_STATIC_METRIC_OK EQUAL -1 OR Q1532_LAND_METRIC_OK EQUAL -1 OR
   Q1532_STATIC_VARYING_OK EQUAL -1 OR Q1532_LAND_VARYING_OK EQUAL -1 OR
   Q1532_STATIC_UNIFORM_OK EQUAL -1 OR Q1532_LAND_UNIFORM_OK EQUAL -1 OR
   NOT Q1532_BROKEN_STATIC_POS EQUAL -1 OR NOT Q1532_BROKEN_LAND_POS EQUAL -1 OR
   Q1532_LOG_OK EQUAL -1)
    message(FATAL_ERROR
        "Q15.3b verification failed: staticMetric=${Q1532_STATIC_METRIC_OK} landMetric=${Q1532_LAND_METRIC_OK} staticVary=${Q1532_STATIC_VARYING_OK} landVary=${Q1532_LAND_VARYING_OK} staticUniform=${Q1532_STATIC_UNIFORM_OK} landUniform=${Q1532_LAND_UNIFORM_OK} brokenStatic=${Q1532_BROKEN_STATIC_POS} brokenLand=${Q1532_BROKEN_LAND_POS} log=${Q1532_LOG_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")

message(STATUS "Q15.3b SP17 projected-XYZ vertex/interpolated fog A/B enabled on LEFT X; Q15.2 remains default")
