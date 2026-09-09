# Q15.3: isolate the remaining proven Shader Package 17 fog-placement mismatch.
#
# EXE + shader-package trace established that Fallout 3 stages:
#   FogParam = { fogFar, fogFar - fogNear, fogPower, 0 }
# and the shipped SLS fogged vertex shaders evaluate:
#   t = clamp((distance - near) / (far - near), 0, 1)
#   fog = pow(t, power)
# before rasterization. The pixel shader then uses the INTERPOLATED fog value:
#   rgb = mix(lit, FogColor, fog)
#
# FalloutQuest Q14.1 currently evaluates the same nonlinear equation per fragment.
# For Megaton FogPower=0.5 this is not equivalent: sqrt() is concave, so evaluating
# after interpolation tends to produce a stronger interior-triangle fog veil.
#
# Q15.3 changes no WTHR data and deliberately keeps the current world/eye distance
# metric for this first test. LEFT X now selects only WHERE the exact same fog
# equation is evaluated:
#   X default / true  = QUEST_FRAGMENT_FOG (Q14.1 behaviour)
#   X toggled / false = VANILLA_VERTEX_FOG (evaluate per vertex, interpolate)
#
# A later test can isolate Package 17's pre-divide ModelViewProj.xyz distance
# metric. Mixing that here would confound fog-placement with PC-vs-VR projection.

set(Q1530_VERTEX_UNIFORM_OLD [=[
        uniform mat4 uMvp;
        out vec3 vNormal;
]=])
set(Q1530_VERTEX_UNIFORM_NEW [=[
        uniform mat4 uMvp;
        uniform vec3 uEyePosition;
        uniform float uFogNear;
        uniform float uFogFar;
        uniform float uFogPower;
        out vec3 vNormal;
        out float vFogFactorQ1530;
]=])

# Both final world shaders have the same vertex-stage anchor after Q10.1.
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1530_VERTEX_UNIFORM_OLD}" Q1530_STATIC_VERTEX_UNIFORM_POS)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1530_VERTEX_UNIFORM_OLD}" Q1530_LAND_VERTEX_UNIFORM_POS)
if(Q1530_STATIC_VERTEX_UNIFORM_POS EQUAL -1 OR Q1530_LAND_VERTEX_UNIFORM_POS EQUAL -1)
    message(FATAL_ERROR
        "Q15.3 could not find vertex uniform anchors: static=${Q1530_STATIC_VERTEX_UNIFORM_POS} land=${Q1530_LAND_VERTEX_UNIFORM_POS}")
endif()
string(REPLACE "${Q1530_VERTEX_UNIFORM_OLD}" "${Q1530_VERTEX_UNIFORM_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE "${Q1530_VERTEX_UNIFORM_OLD}" "${Q1530_VERTEX_UNIFORM_NEW}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

set(Q1530_VERTEX_POSITION_OLD [=[
            vPosition = aPosition;
]=])
set(Q1530_VERTEX_POSITION_NEW [=[
            vPosition = aPosition;
            // Q15.3: Package 17 placement semantics. Keep Q14.1's distance
            // metric for this isolated A/B; only move nonlinear evaluation to
            // the vertex stage, where vanilla evaluates it before interpolation.
            float q1530VertexFogDistance = length(aPosition - uEyePosition);
            float q1530VertexFogT = clamp(
                (q1530VertexFogDistance - uFogNear) /
                max(uFogFar - uFogNear, 0.01), 0.0, 1.0);
            vFogFactorQ1530 = pow(q1530VertexFogT, max(uFogPower, 0.01));
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1530_VERTEX_POSITION_OLD}" Q1530_STATIC_VERTEX_BODY_POS)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1530_VERTEX_POSITION_OLD}" Q1530_LAND_VERTEX_BODY_POS)
if(Q1530_STATIC_VERTEX_BODY_POS EQUAL -1 OR Q1530_LAND_VERTEX_BODY_POS EQUAL -1)
    message(FATAL_ERROR
        "Q15.3 could not find vertex position anchors: static=${Q1530_STATIC_VERTEX_BODY_POS} land=${Q1530_LAND_VERTEX_BODY_POS}")
endif()
string(REPLACE "${Q1530_VERTEX_POSITION_OLD}" "${Q1530_VERTEX_POSITION_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE "${Q1530_VERTEX_POSITION_OLD}" "${Q1530_VERTEX_POSITION_NEW}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# Receive the rasterizer-interpolated vertex fog factor in both pixel shaders.
string(REPLACE
    "        in vec3 vPosition;"
    "        in vec3 vPosition;\n        in float vFogFactorQ1530;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "        in vec3 vPosition;"
    "        in vec3 vPosition;\n        in float vFogFactorQ1530;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

set(Q1530_FRAGMENT_FOG_OLD [=[
            float fogDistance = length(vPosition - uEyePosition);
            float fogT = clamp((fogDistance - uFogNear) /
                               max(uFogFar - uFogNear, 0.01), 0.0, 1.0);
            float fogFactor = uQ1450FogEnabled > 0.5
                ? pow(fogT, max(uFogPower, 0.01))
                : 0.0;
            lit = mix(lit, uFogColor, fogFactor);
]=])
set(Q1530_FRAGMENT_FOG_NEW [=[
            float fogDistance = length(vPosition - uEyePosition);
            float fogT = clamp((fogDistance - uFogNear) /
                               max(uFogFar - uFogNear, 0.01), 0.0, 1.0);
            float q1530FragmentFog = pow(fogT, max(uFogPower, 0.01));
            // Legacy Q14.5 boolean is intentionally reused as the A/B selector:
            // true=current per-fragment; false=Package-17 vertex/interpolated.
            float fogFactor = uQ1450FogEnabled > 0.5
                ? q1530FragmentFog
                : vFogFactorQ1530;
            lit = mix(lit, uFogColor, fogFactor);
]=])

string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1530_FRAGMENT_FOG_OLD}" Q1530_STATIC_FRAGMENT_POS)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1530_FRAGMENT_FOG_OLD}" Q1530_LAND_FRAGMENT_POS)
if(Q1530_STATIC_FRAGMENT_POS EQUAL -1 OR Q1530_LAND_FRAGMENT_POS EQUAL -1)
    message(FATAL_ERROR
        "Q15.3 could not find Q14.5 fog blocks: static=${Q1530_STATIC_FRAGMENT_POS} land=${Q1530_LAND_FRAGMENT_POS}")
endif()
string(REPLACE "${Q1530_FRAGMENT_FOG_OLD}" "${Q1530_FRAGMENT_FOG_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE "${Q1530_FRAGMENT_FOG_OLD}" "${Q1530_FRAGMENT_FOG_NEW}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# Rewrite diagnostics to state the new meaning of the existing LEFT X selector.
string(REPLACE "Q14.5 FOG A/B READY:" "Q15.3 FOG PLACEMENT A/B READY:"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE "onlyFogBlendChanges=1" "fogDataUnchanged=1 equationUnchanged=1"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1530_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1530_Q4_INPUT}")
    message(FATAL_ERROR "Q15.3 expected final OpenXR source at ${Q1530_Q4_INPUT}")
endif()
file(READ "${Q1530_Q4_INPUT}" Q1530_Q4_SOURCE)
string(REPLACE "Q14.5 FOG MODE:" "Q15.3 FOG PLACEMENT MODE:"
       Q1530_Q4_SOURCE "${Q1530_Q4_SOURCE}")
string(REPLACE "onlyFogBlendChanges=1" "fogDataUnchanged=1 equationUnchanged=1"
       Q1530_Q4_SOURCE "${Q1530_Q4_SOURCE}")
file(WRITE "${Q1530_Q4_INPUT}" "${Q1530_Q4_SOURCE}")

# Hard guards: both world renderers must have a vertex-computed varying and the
# fragment-stage selector. No colour, distance, power, lighting or post value is
# allowed to change in this patch.
string(FIND "${Q6H_NATIVE_SOURCE}" "vFogFactorQ1530 = pow(q1530VertexFogT" Q1530_STATIC_VERTEX_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "vFogFactorQ1530 = pow(q1530VertexFogT" Q1530_LAND_VERTEX_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" ": vFogFactorQ1530;" Q1530_STATIC_SELECT_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" ": vFogFactorQ1530;" Q1530_LAND_SELECT_OK)
string(FIND "${Q1530_Q4_SOURCE}" "Q15.3 FOG PLACEMENT MODE" Q1530_LOG_OK)
if(Q1530_STATIC_VERTEX_OK EQUAL -1 OR Q1530_LAND_VERTEX_OK EQUAL -1 OR
   Q1530_STATIC_SELECT_OK EQUAL -1 OR Q1530_LAND_SELECT_OK EQUAL -1 OR
   Q1530_LOG_OK EQUAL -1)
    message(FATAL_ERROR
        "Q15.3 verification failed: staticV=${Q1530_STATIC_VERTEX_OK} landV=${Q1530_LAND_VERTEX_OK} staticSel=${Q1530_STATIC_SELECT_OK} landSel=${Q1530_LAND_SELECT_OK} log=${Q1530_LOG_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")

message(STATUS "Q15.3 Package-17 vertex/interpolated fog A/B enabled on LEFT X; WTHR fog data unchanged")
