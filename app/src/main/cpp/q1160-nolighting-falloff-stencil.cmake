# Q11.6: honor Fallout 3's authored NoLighting falloff and NiStencil draw mode.
#
# V11.5 proved that preserving NiAlphaProperty blend factors alone is not enough
# for Megaton's white footprint overlays or neon/effect meshes. Fallout 3's
# BSShaderNoLightingProperty (Bethesda version >= 27) stores four floats directly
# after File Name. The original renderer uses them as view-angle alpha falloff.
# We previously stopped parsing at File Name, so every such surface rendered at
# full authored alpha regardless of view angle.
#
# The same effect meshes commonly carry NiStencilProperty. Its packed Draw Mode
# controls front-face winding and whether both sides are drawn, even when stencil
# testing itself is not needed. Preserve that draw mode without inventing state.

set(Q1160_NIF_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/fo3-static-nif-q6h.cpp")
if(NOT EXISTS "${Q1160_NIF_GENERATED}")
    message(FATAL_ERROR "Q11.6 expected generated NIF source at ${Q1160_NIF_GENERATED}")
endif()
file(READ "${Q1160_NIF_GENERATED}" Q1160_NIF_SOURCE)

# Extend the already-correct FO3 NoLighting parser. Q10.9 deliberately accepts
# an empty File Name; Q11.6 continues parsing the four falloff floats after it.
set(Q1160_OLD_NOLIGHT_SIGNATURE [==[
                                  float& environmentMapScale, std::string& fileName) {
]==])
set(Q1160_NEW_NOLIGHT_SIGNATURE [==[
                                  float& environmentMapScale, std::string& fileName,
                                  bool& hasFalloff, float falloffParams[4]) {
]==])
string(FIND "${Q1160_NIF_SOURCE}" "${Q1160_OLD_NOLIGHT_SIGNATURE}" Q1160_NOLIGHT_SIG_POS)
if(Q1160_NOLIGHT_SIG_POS EQUAL -1)
    message(FATAL_ERROR "Q11.6 could not find NoLighting parser signature")
endif()
string(REPLACE "${Q1160_OLD_NOLIGHT_SIGNATURE}" "${Q1160_NEW_NOLIGHT_SIGNATURE}"
       Q1160_NIF_SOURCE "${Q1160_NIF_SOURCE}")

set(Q1160_OLD_NOLIGHT_TAIL [==[
    if (!c.SizedString(fileName)) return false;
    return true;
}
]==])
set(Q1160_NEW_NOLIGHT_TAIL [==[
    if (!c.SizedString(fileName)) return false;
    hasFalloff = header.bsVersion >= 27u;
    if (hasFalloff) {
        for (int i = 0; i < 4; ++i) {
            if (!c.F32(falloffParams[i])) return false;
        }
    }
    return true;
}
]==])
string(FIND "${Q1160_NIF_SOURCE}" "${Q1160_OLD_NOLIGHT_TAIL}" Q1160_NOLIGHT_TAIL_POS)
if(Q1160_NOLIGHT_TAIL_POS EQUAL -1)
    message(FATAL_ERROR "Q11.6 could not find Q10.9 NoLighting parser tail")
endif()
string(REPLACE "${Q1160_OLD_NOLIGHT_TAIL}" "${Q1160_NEW_NOLIGHT_TAIL}"
       Q1160_NIF_SOURCE "${Q1160_NIF_SOURCE}")

set(Q1160_OLD_NOLIGHT_CALL [==[
            if (ParseNoLightingPropertyQ1020(prop, header.blockSizes[ref], header,
                                             candidate.shaderFlags1, candidate.shaderFlags2,
                                             candidate.environmentMapScale, directTexture)) {
]==])
set(Q1160_NEW_NOLIGHT_CALL [==[
            if (ParseNoLightingPropertyQ1020(prop, header.blockSizes[ref], header,
                                             candidate.shaderFlags1, candidate.shaderFlags2,
                                             candidate.environmentMapScale, directTexture,
                                             candidate.noLightingFalloff,
                                             candidate.noLightingFalloffParams)) {
]==])
string(FIND "${Q1160_NIF_SOURCE}" "${Q1160_OLD_NOLIGHT_CALL}" Q1160_NOLIGHT_CALL_POS)
if(Q1160_NOLIGHT_CALL_POS EQUAL -1)
    message(FATAL_ERROR "Q11.6 could not find NoLighting property call")
endif()
string(REPLACE "${Q1160_OLD_NOLIGHT_CALL}" "${Q1160_NEW_NOLIGHT_CALL}"
       Q1160_NIF_SOURCE "${Q1160_NIF_SOURCE}")

# FO3 (20.2.0.7, newer than Oblivion) packs NiStencilProperty into a 16-bit
# flags field followed by stencil ref/mask. Bits 10-11 are Draw Mode:
# 0 default, 1 counter-clockwise, 2 clockwise, 3 both/two-sided.
set(Q1160_STENCIL_PARSER [==[
bool ParseStencilDrawModeQ1160(const uint8_t* data, size_t size,
                               uint8_t& drawMode) {
    Cursor c(data, size);
    if (!ParseObjectNetPrefix(c)) return false;
    uint16_t flags = 0;
    uint32_t stencilRef = 0, stencilMask = 0;
    if (!c.U16(flags) || !c.U32(stencilRef) || !c.U32(stencilMask)) return false;
    drawMode = static_cast<uint8_t>((flags >> 10u) & 0x03u);
    return true;
}

]==])
set(Q1160_STENCIL_INSERT_MARKER [==[
bool ReadVec3Array(Cursor& c, uint16_t count, std::vector<float>& out) {
]==])
string(FIND "${Q1160_NIF_SOURCE}" "${Q1160_STENCIL_INSERT_MARKER}" Q1160_STENCIL_INSERT_POS)
if(Q1160_STENCIL_INSERT_POS EQUAL -1)
    message(FATAL_ERROR "Q11.6 could not find stencil parser insertion point")
endif()
string(REPLACE "${Q1160_STENCIL_INSERT_MARKER}"
       "${Q1160_STENCIL_PARSER}${Q1160_STENCIL_INSERT_MARKER}"
       Q1160_NIF_SOURCE "${Q1160_NIF_SOURCE}")

set(Q1160_OLD_MATERIAL_BRANCH [==[
        } else if (type == "NiMaterialProperty") {
]==])
set(Q1160_NEW_MATERIAL_BRANCH [==[
        } else if (type == "NiStencilProperty") {
            uint8_t drawMode = 0u;
            if (ParseStencilDrawModeQ1160(prop, header.blockSizes[ref], drawMode)) {
                candidate.stencilDrawModePresent = true;
                candidate.stencilDrawMode = drawMode;
            }
        } else if (type == "NiMaterialProperty") {
]==])
string(FIND "${Q1160_NIF_SOURCE}" "${Q1160_OLD_MATERIAL_BRANCH}" Q1160_STENCIL_BRANCH_POS)
if(Q1160_STENCIL_BRANCH_POS EQUAL -1)
    message(FATAL_ERROR "Q11.6 could not find material property branch for stencil insertion")
endif()
string(REPLACE "${Q1160_OLD_MATERIAL_BRANCH}" "${Q1160_NEW_MATERIAL_BRANCH}"
       Q1160_NIF_SOURCE "${Q1160_NIF_SOURCE}")

string(FIND "${Q1160_NIF_SOURCE}" "candidate.noLightingFalloffParams" Q1160_FALLOFF_NIF_OK)
string(FIND "${Q1160_NIF_SOURCE}" "ParseStencilDrawModeQ1160" Q1160_STENCIL_NIF_OK)
if(Q1160_FALLOFF_NIF_OK EQUAL -1 OR Q1160_STENCIL_NIF_OK EQUAL -1)
    message(FATAL_ERROR "Q11.6 NIF-state verification failed")
endif()
file(WRITE "${Q1160_NIF_GENERATED}" "${Q1160_NIF_SOURCE}")

# -----------------------------------------------------------------------------
# Static renderer: carry falloff and stencil draw mode to GLES.
# -----------------------------------------------------------------------------
string(REPLACE
    "    bool noLighting = false;"
    "    bool noLighting = false;\n    bool noLightingFalloff = false;\n    float noLightingFalloffParams[4]{0.0f, 1.0f, 1.0f, 1.0f};\n    bool stencilDrawModePresent = false;\n    uint8_t stencilDrawMode = 0u;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "        uniform float uNoLighting;"
    "        uniform float uNoLighting;\n        uniform float uNoLightingFalloff;\n        uniform vec4 uNoLightingFalloffParams;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1160_OLD_ALPHA_SHADER [==[
            float alpha = diffuseTexel.a * uMaterialAlpha * mix(1.0, vColor.a, uUseVertexAlpha);
]==])
set(Q1160_NEW_ALPHA_SHADER [==[
            float alpha = diffuseTexel.a * uMaterialAlpha * mix(1.0, vColor.a, uUseVertexAlpha);
            if (uNoLightingFalloff > 0.5) {
                vec3 viewDirectionQ1160 = normalize(vPosition - uEyePosition);
                float viewAngleQ1160 = abs(dot(normalize(vNormal), viewDirectionQ1160));
                float falloffTQ1160 = smoothstep(uNoLightingFalloffParams.x,
                                                 uNoLightingFalloffParams.y,
                                                 viewAngleQ1160);
                float startOpacityQ1160 = min(uNoLightingFalloffParams.z, 1.0);
                float stopOpacityQ1160 = max(uNoLightingFalloffParams.w, 0.0);
                alpha *= mix(startOpacityQ1160, stopOpacityQ1160, falloffTQ1160);
            }
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1160_OLD_ALPHA_SHADER}" Q1160_ALPHA_SHADER_POS)
if(Q1160_ALPHA_SHADER_POS EQUAL -1)
    message(FATAL_ERROR "Q11.6 could not find fragment alpha hook")
endif()
string(REPLACE "${Q1160_OLD_ALPHA_SHADER}" "${Q1160_NEW_ALPHA_SHADER}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "GLint gNoLightingLocationQ1020 = -1;"
    "GLint gNoLightingLocationQ1020 = -1;\nGLint gNoLightingFalloffLocationQ1160 = -1;\nGLint gNoLightingFalloffParamsLocationQ1160 = -1;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "    gNoLightingLocationQ1020 = glGetUniformLocation(gProgram, \"uNoLighting\");"
    "    gNoLightingLocationQ1020 = glGetUniformLocation(gProgram, \"uNoLighting\");\n    gNoLightingFalloffLocationQ1160 = glGetUniformLocation(gProgram, \"uNoLightingFalloff\");\n    gNoLightingFalloffParamsLocationQ1160 = glGetUniformLocation(gProgram, \"uNoLightingFalloffParams\");"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "    gpu.noLighting = cpu.mesh.noLighting;"
    "    gpu.noLighting = cpu.mesh.noLighting;\n    gpu.noLightingFalloff = cpu.mesh.noLightingFalloff;\n    for (int q1160i = 0; q1160i < 4; ++q1160i) gpu.noLightingFalloffParams[q1160i] = cpu.mesh.noLightingFalloffParams[q1160i];\n    gpu.stencilDrawModePresent = cpu.mesh.stencilDrawModePresent;\n    gpu.stencilDrawMode = cpu.mesh.stencilDrawMode;\n    if (gpu.noLightingFalloff) {\n        Q6H_LOGI(\"Q11.6 NOLIGHT FALLOFF: ref=%08X model=%s angle=(%.4f %.4f) opacity=(%.4f %.4f)\",\n                 gpu.refFormId, cpu.placement.modelPath.c_str(),\n                 gpu.noLightingFalloffParams[0], gpu.noLightingFalloffParams[1],\n                 gpu.noLightingFalloffParams[2], gpu.noLightingFalloffParams[3]);\n    }\n    if (gpu.stencilDrawModePresent) {\n        Q6H_LOGI(\"Q11.6 STENCIL DRAW: ref=%08X model=%s mode=%u\",\n                 gpu.refFormId, cpu.placement.modelPath.c_str(),\n                 static_cast<unsigned>(gpu.stencilDrawMode));\n    }"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "    glUniform1f(gNoLightingLocationQ1020, object.noLighting ? 1.0f : 0.0f);"
    "    glUniform1f(gNoLightingLocationQ1020, object.noLighting ? 1.0f : 0.0f);\n    glUniform1f(gNoLightingFalloffLocationQ1160, object.noLightingFalloff ? 1.0f : 0.0f);\n    glUniform4fv(gNoLightingFalloffParamsLocationQ1160, 1, object.noLightingFalloffParams);"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1160_OLD_DRAW_TAIL [==[
    glBindVertexArray(object.vao);
    glDrawArrays(GL_TRIANGLES, 0, object.vertexCount);
}
]==])
set(Q1160_NEW_DRAW_TAIL [==[
    glBindVertexArray(object.vao);

    GLboolean q1160CullWasEnabled = GL_FALSE;
    GLint q1160PreviousFrontFace = GL_CCW;
    GLint q1160PreviousCullMode = GL_BACK;
    if (object.stencilDrawModePresent) {
        q1160CullWasEnabled = glIsEnabled(GL_CULL_FACE);
        glGetIntegerv(GL_FRONT_FACE, &q1160PreviousFrontFace);
        glGetIntegerv(GL_CULL_FACE_MODE, &q1160PreviousCullMode);
        if (object.stencilDrawMode == 3u) {
            glDisable(GL_CULL_FACE);
        } else {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glFrontFace(object.stencilDrawMode == 2u ? GL_CW : GL_CCW);
        }
    }

    glDrawArrays(GL_TRIANGLES, 0, object.vertexCount);

    if (object.stencilDrawModePresent) {
        glFrontFace(static_cast<GLenum>(q1160PreviousFrontFace));
        glCullFace(static_cast<GLenum>(q1160PreviousCullMode));
        if (q1160CullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    }
}
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1160_OLD_DRAW_TAIL}" Q1160_DRAW_TAIL_POS)
if(Q1160_DRAW_TAIL_POS EQUAL -1)
    message(FATAL_ERROR "Q11.6 could not find draw tail for stencil state")
endif()
string(REPLACE "${Q1160_OLD_DRAW_TAIL}" "${Q1160_NEW_DRAW_TAIL}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(FIND "${Q6H_NATIVE_SOURCE}" "Q11.6 NOLIGHT FALLOFF" Q1160_FALLOFF_LOG_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q11.6 STENCIL DRAW" Q1160_STENCIL_LOG_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "smoothstep(uNoLightingFalloffParams.x" Q1160_SHADER_OK)
if(Q1160_FALLOFF_LOG_OK EQUAL -1 OR Q1160_STENCIL_LOG_OK EQUAL -1 OR Q1160_SHADER_OK EQUAL -1)
    message(FATAL_ERROR "Q11.6 renderer verification failed")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q11.6 FO3 NoLighting falloff + NiStencil draw mode enabled")
