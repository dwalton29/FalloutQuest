# Q11.4: Fallout 3 uses textureless BSShaderNoLightingProperty geometry with
# vertex colours + NiAlphaProperty as soft overlay/light/shading geometry on
# many compound Megaton statics. Q11.1 correctly made the diffuse base neutral
# white and Q11.2 preserved the RGB stream, but vertex alpha was still gated
# only by BSShaderFlags1::Vertex_Alpha (0x8).
#
# Device logs prove these textureless shapes are explicitly alpha blended even
# when that BS shader bit is absent. Ignoring their VCOL alpha turns a soft
# overlay into an opaque white polygon. For exactly this authored case, consume
# the vertex alpha stream. Textured PP-lit geometry is unchanged.

set(Q1140_OLD_ALPHA [==[
    gpu.useVertexAlpha =
        gpu.useVertexColor && (cpu.mesh.shaderFlags1 & 0x00000008u) != 0u;
]==])
set(Q1140_NEW_ALPHA [==[
    const bool q1140TexturelessNoLightingOverlay =
        q1120HasVertexColorStream && cpu.mesh.noLighting &&
        cpu.mesh.diffuseTexturePath.empty() && cpu.mesh.alphaBlend;
    gpu.useVertexAlpha =
        gpu.useVertexColor &&
        (((cpu.mesh.shaderFlags1 & 0x00000008u) != 0u) ||
         q1140TexturelessNoLightingOverlay);
    if (q1140TexturelessNoLightingOverlay) {
        float q1140MinAlpha = 1.0f;
        float q1140MaxAlpha = 0.0f;
        for (size_t q1140i = 3u; q1140i < cpu.mesh.vertexColors.size(); q1140i += 4u) {
            q1140MinAlpha = std::min(q1140MinAlpha, cpu.mesh.vertexColors[q1140i]);
            q1140MaxAlpha = std::max(q1140MaxAlpha, cpu.mesh.vertexColors[q1140i]);
        }
        Q6H_LOGI("Q11.4 OVERLAY ALPHA: ref=%08X model=%s min=%.3f max=%.3f applied=1",
                 gpu.refFormId, cpu.placement.modelPath.c_str(),
                 q1140MinAlpha, q1140MaxAlpha);
    }
]==])

string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1140_OLD_ALPHA}" Q1140_ALPHA_POS)
if(Q1140_ALPHA_POS EQUAL -1)
    message(FATAL_ERROR "Q11.4 could not find Q11.2 vertex-alpha hook")
endif()
string(REPLACE "${Q1140_OLD_ALPHA}" "${Q1140_NEW_ALPHA}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(FIND "${Q6H_NATIVE_SOURCE}" "Q11.4 OVERLAY ALPHA" Q1140_LOG_OK)
if(Q1140_LOG_OK EQUAL -1)
    message(FATAL_ERROR "Q11.4 textureless overlay alpha verification failed")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q11.4 textureless NoLighting overlay alpha enabled")
