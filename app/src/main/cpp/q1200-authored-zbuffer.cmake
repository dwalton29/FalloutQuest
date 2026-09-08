# Q12.0: honor Fallout 3 BSShaderProperty Z-buffer state per material draw.
#
# The remaining concrete repros all expose the same renderer bug:
#   * MegatonBrassLanternSign (FOOD) PPLighting shapes carry shaderFlags2 bit 0.
#   * MegatonChurchofAtom alpha-bearing PPLighting shapes carry shaderFlags2 bit 0.
#   * SignStop02 carries shaderFlags1=0x82000000 / shaderFlags2=0x00000001.
#
# Bethesda BSShaderFlags1::ZBuffer_Test is 0x80000000 and
# BSShaderFlags2::ZBuffer_Write is 0x00000001.  Q6A historically forced every
# alpha-blended shape into a second pass with glDepthMask(GL_FALSE), ignoring the
# authored Z-write bit.  That lets layered sign/alpha geometry draw through one
# another and makes thin coplanar faces unstable in stereo.
#
# Preserve the existing opaque/alpha pass split and blend functions, but apply
# authored depth-test/depth-write state immediately before every object draw.

string(REPLACE
    "    uint8_t alphaDestBlend = 7u;"
    "    uint8_t alphaDestBlend = 7u;\n    bool zBufferTestQ1200 = true;\n    bool zBufferWriteQ1200 = true;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1200_OLD_ASSIGN [==[
    gpu.alphaDestBlend = cpu.mesh.alphaDestBlend;
]==])
set(Q1200_NEW_ASSIGN [==[
    gpu.alphaDestBlend = cpu.mesh.alphaDestBlend;
    gpu.zBufferTestQ1200 = (cpu.mesh.shaderFlags1 & 0x80000000u) != 0u;
    gpu.zBufferWriteQ1200 = (cpu.mesh.shaderFlags2 & 0x00000001u) != 0u;
    Q6H_LOGI("Q12.0 Z STATE: ref=%08X model=%s test=%d write=%d alphaBlend=%d flags1=%08X flags2=%08X",
             gpu.refFormId, cpu.placement.modelPath.c_str(),
             gpu.zBufferTestQ1200 ? 1 : 0, gpu.zBufferWriteQ1200 ? 1 : 0,
             gpu.alphaBlend ? 1 : 0, cpu.mesh.shaderFlags1, cpu.mesh.shaderFlags2);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1200_OLD_ASSIGN}" Q1200_ASSIGN_POS)
if(Q1200_ASSIGN_POS EQUAL -1)
    message(FATAL_ERROR "Q12.0 could not find Q11.5 alpha state assignment")
endif()
string(REPLACE "${Q1200_OLD_ASSIGN}" "${Q1200_NEW_ASSIGN}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1200_OLD_DEPTH_SAVE [==[
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLboolean previousDepthMask = GL_TRUE;
]==])
set(Q1200_NEW_DEPTH_SAVE [==[
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    const GLboolean depthTestWasEnabledQ1200 = glIsEnabled(GL_DEPTH_TEST);
    GLboolean previousDepthMask = GL_TRUE;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1200_OLD_DEPTH_SAVE}" Q1200_DEPTH_SAVE_POS)
if(Q1200_DEPTH_SAVE_POS EQUAL -1)
    message(FATAL_ERROR "Q12.0 could not find RenderScene depth-state save")
endif()
string(REPLACE "${Q1200_OLD_DEPTH_SAVE}" "${Q1200_NEW_DEPTH_SAVE}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1200_OLD_PASSES [==[
    // Opaque and alpha-tested cutouts first. Alpha testing still writes depth,
    // which is what FO3 fences/grates/wires expect.
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    for (const GpuObject& object : gObjects) {
        if (!object.alphaBlend) DrawSceneObject(object);
    }

    // True alpha-blended shapes are a second pass and do not write depth.
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    for (const GpuObject& object : gObjects) {
        if (!object.alphaBlend) continue;
        glBlendFunc(Q1150BlendFactor(object.alphaSourceBlend, true),
                    Q1150BlendFactor(object.alphaDestBlend, false));
        DrawSceneObject(object);
    }
]==])
set(Q1200_NEW_PASSES [==[
    glDisable(GL_BLEND);
    for (const GpuObject& object : gObjects) {
        if (object.alphaBlend) continue;
        if (object.zBufferTestQ1200) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        glDepthMask(object.zBufferWriteQ1200 ? GL_TRUE : GL_FALSE);
        DrawSceneObject(object);
    }

    glEnable(GL_BLEND);
    for (const GpuObject& object : gObjects) {
        if (!object.alphaBlend) continue;
        if (object.zBufferTestQ1200) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        glDepthMask(object.zBufferWriteQ1200 ? GL_TRUE : GL_FALSE);
        glBlendFunc(Q1150BlendFactor(object.alphaSourceBlend, true),
                    Q1150BlendFactor(object.alphaDestBlend, false));
        DrawSceneObject(object);
    }
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1200_OLD_PASSES}" Q1200_PASSES_POS)
if(Q1200_PASSES_POS EQUAL -1)
    message(FATAL_ERROR "Q12.0 could not find final Q11.5 scene passes")
endif()
string(REPLACE "${Q1200_OLD_PASSES}" "${Q1200_NEW_PASSES}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1200_OLD_RESTORE [==[
    glDepthMask(previousDepthMask);
    glBlendFuncSeparate(static_cast<GLenum>(previousBlendSrcRgb),
]==])
set(Q1200_NEW_RESTORE [==[
    glDepthMask(previousDepthMask);
    if (depthTestWasEnabledQ1200) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    glBlendFuncSeparate(static_cast<GLenum>(previousBlendSrcRgb),
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1200_OLD_RESTORE}" Q1200_RESTORE_POS)
if(Q1200_RESTORE_POS EQUAL -1)
    message(FATAL_ERROR "Q12.0 could not find RenderScene state restore")
endif()
string(REPLACE "${Q1200_OLD_RESTORE}" "${Q1200_NEW_RESTORE}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(FIND "${Q6H_NATIVE_SOURCE}" "Q12.0 Z STATE" Q1200_LOG_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "glDepthMask(object.zBufferWriteQ1200 ? GL_TRUE : GL_FALSE)" Q1200_WRITE_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "depthTestWasEnabledQ1200" Q1200_RESTORE_OK)
if(Q1200_LOG_OK EQUAL -1 OR Q1200_WRITE_OK EQUAL -1 OR Q1200_RESTORE_OK EQUAL -1)
    message(FATAL_ERROR "Q12.0 authored Z-buffer verification failed")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q12.0 authored Fallout 3 Z-buffer test/write state enabled")

# Q12.1 restores Gamebryo's default single-sided static rendering. NiStencil
# remains the explicit Fallout 3 override for two-sided/reversed face drawing.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1210-default-backface-culling.cmake")
