# Q11.2: honor Fallout 3 BSShaderFlags2::Vertex_Colors on lit meshes.
#
# FO3 nif.xml defines Shader Flags 2 bit 5 (0x20) as Vertex_Colors. Q10.2
# incorrectly enabled vertex-colour multiplication whenever a VCOL stream was
# physically present, even when the authored shader explicitly left that bit off.
# This can recolour otherwise-correct diffuse DDS textures across compound Megaton
# statics. Preserve Q11.1's textureless NoLighting behaviour, but gate PP-lit
# geometry by Bethesda's actual shader flag.

set(Q1120_OLD_VCOLOR [==[
    gpu.useVertexColor = cpu.mesh.vertexColors.size() == vertexCount * 4u;
    gpu.useVertexAlpha = gpu.useVertexColor && (cpu.mesh.shaderFlags1 & 0x00000008u) != 0u;
]==])

set(Q1120_NEW_VCOLOR [==[
    const bool q1120HasVertexColorStream =
        cpu.mesh.vertexColors.size() == vertexCount * 4u;
    gpu.useVertexColor =
        q1120HasVertexColorStream &&
        (cpu.mesh.noLighting || (cpu.mesh.shaderFlags2 & 0x00000020u) != 0u);
    gpu.useVertexAlpha =
        gpu.useVertexColor && (cpu.mesh.shaderFlags1 & 0x00000008u) != 0u;
    if (q1120HasVertexColorStream && !cpu.mesh.noLighting &&
        (cpu.mesh.shaderFlags2 & 0x00000020u) == 0u) {
        Q6H_LOGI("Q11.2 VCOLOR GATE: ref=%08X model=%s shaderFlags2=%08X stream=1 applied=0",
                 gpu.refFormId, cpu.placement.modelPath.c_str(),
                 cpu.mesh.shaderFlags2);
    }
]==])

string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1120_OLD_VCOLOR}" Q1120_VCOLOR_POS)
if(Q1120_VCOLOR_POS EQUAL -1)
    message(FATAL_ERROR "Q11.2 could not find Q10.2 vertex-colour upload hook")
endif()
string(REPLACE "${Q1120_OLD_VCOLOR}" "${Q1120_NEW_VCOLOR}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(FIND "${Q6H_NATIVE_SOURCE}" "Q11.2 VCOLOR GATE" Q1120_LOG_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "shaderFlags2 & 0x00000020u" Q1120_FLAG_OK)
if(Q1120_LOG_OK EQUAL -1 OR Q1120_FLAG_OK EQUAL -1)
    message(FATAL_ERROR "Q11.2 shader vertex-colour gate verification failed")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q11.2 FO3 shader vertex-colour gating enabled")

# Q11.3 corrects the D3D DDS / Gamebryo V-axis handling after Q11.2.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1130-static-uv-orientation.cmake")
