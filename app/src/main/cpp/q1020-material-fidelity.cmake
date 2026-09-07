# Q10.2: Fallout 3 NIF material fidelity.
# Preserve authored vertex colours, NoLighting shader textures, PP shader flags,
# NiMaterial emissive/specular data and glow-map slot 3. The renderer then uses
# those fields directly; no new/invented light sources are introduced.

# -----------------------------------------------------------------------------
# NIF parser: preserve material data that Q6H previously discarded.
# -----------------------------------------------------------------------------
set(Q1020_OLD_TEXTURE_SET [=[
bool ParseTextureSet(const uint8_t* data, size_t size,
                     std::string& diffuse, std::string& normal) {
    Cursor c(data, size);
    uint32_t count = 0;
    if (!c.U32(count) || count == 0u || count > 32u) return false;
    for (uint32_t i = 0; i < count; ++i) {
        std::string value;
        if (!c.SizedString(value)) return false;
        if (i == 0u) diffuse = value;
        if (i == 1u) normal = value;
    }
    return !diffuse.empty();
}
]=])
set(Q1020_NEW_TEXTURE_SET [=[
bool ParseTextureSet(const uint8_t* data, size_t size,
                     std::string& diffuse, std::string& normal,
                     std::string& glow) {
    Cursor c(data, size);
    uint32_t count = 0;
    if (!c.U32(count) || count == 0u || count > 32u) return false;
    for (uint32_t i = 0; i < count; ++i) {
        std::string value;
        if (!c.SizedString(value)) return false;
        if (i == 0u) diffuse = value;
        if (i == 1u) normal = value;
        if (i == 2u) glow = value;
    }
    return !diffuse.empty();
}
]=])
string(REPLACE "${Q1020_OLD_TEXTURE_SET}" "${Q1020_NEW_TEXTURE_SET}"
       Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")

set(Q1020_OLD_MATERIAL [=[
bool ParseMaterialProperty(const uint8_t* data, size_t size,
                           float& glossiness, float& alpha) {
    Cursor c(data, size);
    if (!ParseObjectNetPrefix(c)) return false;
    float emit = 1.0f;
    if (!c.Skip(12u) || !c.Skip(12u) || !c.F32(glossiness) ||
        !c.F32(alpha) || !c.F32(emit)) return false;
    return c.remaining() == 0u;
}
]=])
set(Q1020_NEW_MATERIAL [=[
bool ParseMaterialProperty(const uint8_t* data, size_t size,
                           float specular[3], float emissive[3],
                           float& glossiness, float& alpha, float& emissiveMult) {
    Cursor c(data, size);
    if (!ParseObjectNetPrefix(c)) return false;
    for (int i = 0; i < 3; ++i) if (!c.F32(specular[i])) return false;
    for (int i = 0; i < 3; ++i) if (!c.F32(emissive[i])) return false;
    if (!c.F32(glossiness) || !c.F32(alpha) || !c.F32(emissiveMult)) return false;
    return c.remaining() == 0u;
}
]=])
string(REPLACE "${Q1020_OLD_MATERIAL}" "${Q1020_NEW_MATERIAL}"
       Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")

set(Q1020_OLD_SHADER_REF [=[
bool ParseShaderTextureRef(const uint8_t* data, size_t size,
                           const NifHeader& header, uint32_t& textureSetRef) {
    Cursor c(data, size);
    if (!ParseObjectNetPrefix(c)) return false;

    uint16_t flags = 0;
    uint32_t shaderType = 0, shaderFlags = 0, unknownInt2 = 0;
    if (!c.U16(flags) || !c.U32(shaderType) || !c.U32(shaderFlags) || !c.U32(unknownInt2)) {
        return false;
    }
    if (header.userVersion == 11u) {
        float envmapScale = 1.0f;
        if (!c.F32(envmapScale)) return false;
    }
    if (header.userVersion <= 11u) {
        uint32_t unknownInt3 = 0;
        if (!c.U32(unknownInt3)) return false;
    }
    return c.U32(textureSetRef);
}
]=])
set(Q1020_NEW_SHADER_REF [=[
bool ParseShaderTextureRef(const uint8_t* data, size_t size,
                           const NifHeader& header,
                           uint32_t& shaderFlags1, uint32_t& shaderFlags2,
                           float& environmentMapScale, uint32_t& textureSetRef) {
    Cursor c(data, size);
    if (!ParseObjectNetPrefix(c)) return false;

    uint16_t flags = 0;
    uint32_t shaderType = 0;
    if (!c.U16(flags) || !c.U32(shaderType) ||
        !c.U32(shaderFlags1) || !c.U32(shaderFlags2)) return false;
    if (header.userVersion == 11u) {
        if (!c.F32(environmentMapScale)) return false;
    }
    if (header.userVersion <= 11u) {
        uint32_t textureClampMode = 0;
        if (!c.U32(textureClampMode)) return false;
    }
    return c.U32(textureSetRef);
}

bool ParseNoLightingPropertyQ1020(const uint8_t* data, size_t size,
                                  const NifHeader& header,
                                  uint32_t& shaderFlags1, uint32_t& shaderFlags2,
                                  float& environmentMapScale, std::string& fileName) {
    Cursor c(data, size);
    if (!ParseObjectNetPrefix(c)) return false;
    uint16_t flags = 0;
    uint32_t shaderType = 0;
    if (!c.U16(flags) || !c.U32(shaderType) ||
        !c.U32(shaderFlags1) || !c.U32(shaderFlags2)) return false;
    if (header.userVersion == 11u && !c.F32(environmentMapScale)) return false;
    if (header.userVersion <= 11u) {
        uint32_t textureClampMode = 0;
        if (!c.U32(textureClampMode)) return false;
    }
    // BSShaderNoLightingProperty stores a SizedString texture immediately after
    // the shared BSShaderLightingProperty prefix. Remaining falloff fields are
    // intentionally left uninterpreted for this first faithful render path.
    return c.SizedString(fileName) && !fileName.empty();
}
]=])
string(REPLACE "${Q1020_OLD_SHADER_REF}" "${Q1020_NEW_SHADER_REF}"
       Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")

set(Q1020_OLD_VERTEX_COLORS [=[
    uint8_t hasVertexColors = 0;
    if (!c.U8(hasVertexColors)) return false;
    if (hasVertexColors && !c.Skip(static_cast<size_t>(numVertices) * 16u)) return false;
]=])
set(Q1020_NEW_VERTEX_COLORS [=[
    uint8_t hasVertexColors = 0;
    if (!c.U8(hasVertexColors)) return false;
    mesh.vertexColors.clear();
    if (hasVertexColors) {
        mesh.vertexColors.reserve(static_cast<size_t>(numVertices) * 4u);
        for (uint16_t i = 0; i < numVertices; ++i) {
            float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
            if (!c.F32(r) || !c.F32(g) || !c.F32(b) || !c.F32(a)) return false;
            mesh.vertexColors.insert(mesh.vertexColors.end(), {r, g, b, a});
        }
    }
]=])
string(REPLACE "${Q1020_OLD_VERTEX_COLORS}" "${Q1020_NEW_VERTEX_COLORS}"
       Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")

set(Q1020_OLD_PROPERTY_LOOP [=[
        if (type == "BSShaderPPLightingProperty") {
            uint32_t refOut = INVALID_REF;
            if (ParseShaderTextureRef(prop, header.blockSizes[ref], header, refOut)) {
                textureSetRef = refOut;
            }
        } else if (type == "NiMaterialProperty") {
            ParseMaterialProperty(prop, header.blockSizes[ref],
                                  candidate.glossiness, candidate.alpha);
        } else if (type == "NiAlphaProperty") {
]=])
set(Q1020_NEW_PROPERTY_LOOP [=[
        if (type == "BSShaderPPLightingProperty") {
            uint32_t refOut = INVALID_REF;
            if (ParseShaderTextureRef(prop, header.blockSizes[ref], header,
                                      candidate.shaderFlags1, candidate.shaderFlags2,
                                      candidate.environmentMapScale, refOut)) {
                textureSetRef = refOut;
            }
        } else if (type == "BSShaderNoLightingProperty") {
            std::string directTexture;
            if (ParseNoLightingPropertyQ1020(prop, header.blockSizes[ref], header,
                                             candidate.shaderFlags1, candidate.shaderFlags2,
                                             candidate.environmentMapScale, directTexture)) {
                candidate.noLighting = true;
                candidate.diffuseTexturePath = directTexture;
            }
        } else if (type == "NiMaterialProperty") {
            ParseMaterialProperty(prop, header.blockSizes[ref],
                                  candidate.specularColor, candidate.emissiveColor,
                                  candidate.glossiness, candidate.alpha,
                                  candidate.emissiveMult);
        } else if (type == "NiAlphaProperty") {
]=])
string(REPLACE "${Q1020_OLD_PROPERTY_LOOP}" "${Q1020_NEW_PROPERTY_LOOP}"
       Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")

string(REPLACE
    "if (textureSetRef >= header.numBlocks || BlockType(header, textureSetRef) != \"BSShaderTextureSet\") {"
    "if (!candidate.noLighting && (textureSetRef >= header.numBlocks || BlockType(header, textureSetRef) != \"BSShaderTextureSet\")) {"
    Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")
string(REPLACE
    "    if (textureSetRef < header.numBlocks) {\n        ParseTextureSet(BlockData(nif, header, textureSetRef),\n                        header.blockSizes[textureSetRef],\n                        candidate.diffuseTexturePath, candidate.normalTexturePath);\n    }"
    "    if (!candidate.noLighting && textureSetRef < header.numBlocks) {\n        ParseTextureSet(BlockData(nif, header, textureSetRef),\n                        header.blockSizes[textureSetRef],\n                        candidate.diffuseTexturePath, candidate.normalTexturePath,\n                        candidate.glowTexturePath);\n    }"
    Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")
string(REPLACE
    "        candidate.normalTexturePath.clear();"
    "        candidate.normalTexturePath.clear();\n        candidate.glowTexturePath.clear();"
    Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")

# Re-write Q6H's generated NIF loader after the material extensions.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-static-nif-q6h.cpp" "${Q6H_NIF_SOURCE}")

# -----------------------------------------------------------------------------
# Static renderer: consume the preserved NIF material data.
# -----------------------------------------------------------------------------
string(REPLACE
    "    GLuint normal = 0;"
    "    GLuint normal = 0;\n    GLuint glow = 0;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "    bool realNormal = false;"
    "    bool realNormal = false;\n    bool realGlow = false;\n    bool noLighting = false;\n    bool useVertexColor = false;\n    bool useVertexAlpha = false;\n    bool specularEnabled = false;\n    float specularColor[3]{1.0f, 1.0f, 1.0f};\n    float emissiveColor[3]{0.0f, 0.0f, 0.0f};\n    float emissiveMult = 1.0f;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "        layout(location = 4) in vec2 aUv;"
    "        layout(location = 4) in vec2 aUv;\n        layout(location = 5) in vec4 aColor;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "        out vec3 vPosition;"
    "        out vec3 vPosition;\n        out vec4 vColor;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "            vPosition = aPosition;"
    "            vPosition = aPosition;\n            vColor = aColor;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "        in vec3 vPosition;"
    "        in vec3 vPosition;\n        in vec4 vColor;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "        uniform sampler2D uNormalGloss;"
    "        uniform sampler2D uNormalGloss;\n        uniform sampler2D uGlow;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "        uniform float uAlphaThreshold;"
    "        uniform float uAlphaThreshold;\n        uniform float uNoLighting;\n        uniform float uUseVertexColor;\n        uniform float uUseVertexAlpha;\n        uniform float uSpecularEnabled;\n        uniform vec3 uSpecularColor;\n        uniform vec3 uEmissiveColor;\n        uniform float uEmissiveMult;\n        uniform float uGlowEnabled;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "            vec4 diffuseTexel = texture(uDiffuse, vUv);"
    "            vec4 diffuseTexel = texture(uDiffuse, vUv);\n            vec3 baseColor = diffuseTexel.rgb * mix(vec3(1.0), vColor.rgb, uUseVertexColor);"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "            float alpha = diffuseTexel.a * uMaterialAlpha;"
    "            float alpha = diffuseTexel.a * uMaterialAlpha * mix(1.0, vColor.a, uUseVertexAlpha);"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1020_OLD_SPECULAR [=[
            vec3 viewDirection = normalize(vec3(0.0, 0.15, 1.0));
            vec3 halfVector = normalize(lightDirection + viewDirection);
            float exponent = clamp(uGlossiness, 2.0, 96.0);
            float specular = pow(max(dot(mappedNormal, halfVector), 0.0), exponent)
                           * normalGloss.a * 0.32;
]=])
set(Q1020_NEW_SPECULAR [=[
            vec3 viewDirection = normalize(uEyePosition - vPosition);
            vec3 halfVector = normalize(lightDirection + viewDirection);
            float exponent = clamp(uGlossiness, 2.0, 96.0);
            float specularAmount = pow(max(dot(mappedNormal, halfVector), 0.0), exponent)
                                 * normalGloss.a * 0.32 * uSpecularEnabled;
            vec3 specularContribution = uSpecularColor * specularAmount;
]=])
string(REPLACE "${Q1020_OLD_SPECULAR}" "${Q1020_NEW_SPECULAR}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "            vec3 lit = diffuseTexel.rgb * (uAmbientColor + uSunlightColor * lambert) + uSunlightColor * specular;"
    "            vec3 lit = uNoLighting > 0.5\n                ? baseColor\n                : baseColor * (uAmbientColor + uSunlightColor * lambert) + uSunlightColor * specularContribution;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "                if (i >= uLocalLightCount) break;"
    "                if (uNoLighting > 0.5 || i >= uLocalLightCount) break;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "                lit += diffuseTexel.rgb * localColor * localLambert * attenuation;"
    "                lit += baseColor * localColor * localLambert * attenuation;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "            float fogDistance = length(vPosition - uEyePosition);"
    "            vec3 emissiveMask = uGlowEnabled > 0.5 ? texture(uGlow, vUv).rgb : vec3(1.0);\n            lit += emissiveMask * uEmissiveColor * uEmissiveMult;\n            float fogDistance = length(vPosition - uEyePosition);"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Uniform handles.
string(REPLACE
    "GLint gAlphaThresholdLocation = -1;"
    "GLint gAlphaThresholdLocation = -1;\nGLint gGlowLocationQ1020 = -1;\nGLint gNoLightingLocationQ1020 = -1;\nGLint gUseVertexColorLocationQ1020 = -1;\nGLint gUseVertexAlphaLocationQ1020 = -1;\nGLint gSpecularEnabledLocationQ1020 = -1;\nGLint gSpecularColorLocationQ1020 = -1;\nGLint gEmissiveColorLocationQ1020 = -1;\nGLint gEmissiveMultLocationQ1020 = -1;\nGLint gGlowEnabledLocationQ1020 = -1;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "    gAlphaThresholdLocation = glGetUniformLocation(gProgram, \"uAlphaThreshold\");"
    "    gAlphaThresholdLocation = glGetUniformLocation(gProgram, \"uAlphaThreshold\");\n    gGlowLocationQ1020 = glGetUniformLocation(gProgram, \"uGlow\");\n    gNoLightingLocationQ1020 = glGetUniformLocation(gProgram, \"uNoLighting\");\n    gUseVertexColorLocationQ1020 = glGetUniformLocation(gProgram, \"uUseVertexColor\");\n    gUseVertexAlphaLocationQ1020 = glGetUniformLocation(gProgram, \"uUseVertexAlpha\");\n    gSpecularEnabledLocationQ1020 = glGetUniformLocation(gProgram, \"uSpecularEnabled\");\n    gSpecularColorLocationQ1020 = glGetUniformLocation(gProgram, \"uSpecularColor\");\n    gEmissiveColorLocationQ1020 = glGetUniformLocation(gProgram, \"uEmissiveColor\");\n    gEmissiveMultLocationQ1020 = glGetUniformLocation(gProgram, \"uEmissiveMult\");\n    gGlowEnabledLocationQ1020 = glGetUniformLocation(gProgram, \"uGlowEnabled\");"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# GPU vertex/material upload.
string(REPLACE
    "    constexpr size_t FLOATS_PER_VERTEX = 14u;"
    "    constexpr size_t FLOATS_PER_VERTEX = 18u;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
set(Q1020_OLD_UV [=[
        const float u = cpu.mesh.texcoords[static_cast<size_t>(index) * 2u];
        const float v = 1.0f - cpu.mesh.texcoords[static_cast<size_t>(index) * 2u + 1u];

        expanded.insert(expanded.end(), {
            p.x, p.y, p.z,
            n.x, n.y, n.z,
            t.x, t.y, t.z,
            b.x, b.y, b.z,
            u, v,
        });
]=])
set(Q1020_NEW_UV [=[
        const float u = cpu.mesh.texcoords[static_cast<size_t>(index) * 2u];
        const float v = 1.0f - cpu.mesh.texcoords[static_cast<size_t>(index) * 2u + 1u];
        float cr = 1.0f, cg = 1.0f, cb = 1.0f, ca = 1.0f;
        if (cpu.mesh.vertexColors.size() == vertexCount * 4u) {
            const size_t ci = static_cast<size_t>(index) * 4u;
            cr = cpu.mesh.vertexColors[ci + 0u];
            cg = cpu.mesh.vertexColors[ci + 1u];
            cb = cpu.mesh.vertexColors[ci + 2u];
            ca = cpu.mesh.vertexColors[ci + 3u];
        }

        expanded.insert(expanded.end(), {
            p.x, p.y, p.z,
            n.x, n.y, n.z,
            t.x, t.y, t.z,
            b.x, b.y, b.z,
            u, v,
            cr, cg, cb, ca,
        });
]=])
string(REPLACE "${Q1020_OLD_UV}" "${Q1020_NEW_UV}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "    gpu.alphaTest = cpu.mesh.alphaTest;"
    "    gpu.alphaTest = cpu.mesh.alphaTest;\n    gpu.noLighting = cpu.mesh.noLighting;\n    gpu.useVertexColor = cpu.mesh.vertexColors.size() == vertexCount * 4u;\n    gpu.useVertexAlpha = gpu.useVertexColor && (cpu.mesh.shaderFlags1 & 0x00000008u) != 0u;\n    gpu.specularEnabled = !gpu.noLighting && (cpu.mesh.shaderFlags1 & 0x00000001u) != 0u;\n    for (int i = 0; i < 3; ++i) { gpu.specularColor[i] = cpu.mesh.specularColor[i]; gpu.emissiveColor[i] = cpu.mesh.emissiveColor[i]; }\n    gpu.emissiveMult = std::max(0.0f, cpu.mesh.emissiveMult);"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "    if (!UploadTexture(cpu.mesh.normalTexturePath, {128u, 128u, 255u, 0u},\n                       gpu.normal, gpu.realNormal, \"NORMAL\", gpu.refFormId)) return false;"
    "    if (!UploadTexture(cpu.mesh.normalTexturePath, {128u, 128u, 255u, 0u},\n                       gpu.normal, gpu.realNormal, \"NORMAL\", gpu.refFormId)) return false;\n    if (!UploadTexture(cpu.mesh.glowTexturePath, {0u, 0u, 0u, 255u},\n                       gpu.glow, gpu.realGlow, \"GLOW\", gpu.refFormId)) return false;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, stride,\n                          reinterpret_cast<const void*>(12 * sizeof(float)));\n    glEnableVertexAttribArray(4);"
    "    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, stride,\n                          reinterpret_cast<const void*>(12 * sizeof(float)));\n    glEnableVertexAttribArray(4);\n    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride,\n                          reinterpret_cast<const void*>(14 * sizeof(float)));\n    glEnableVertexAttribArray(5);"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Per-draw material state and glow sampler.
set(Q1020_OLD_DRAW_PREFIX [=[
void DrawSceneObject(const GpuObject& object) {
    glUniform1f(gGlossinessLocation, object.glossiness);
]=])
set(Q1020_NEW_DRAW_PREFIX [=[
void DrawSceneObject(const GpuObject& object) {
    glUniform1f(gGlossinessLocation, object.glossiness);
    glUniform1f(gNoLightingLocationQ1020, object.noLighting ? 1.0f : 0.0f);
    glUniform1f(gUseVertexColorLocationQ1020, object.useVertexColor ? 1.0f : 0.0f);
    glUniform1f(gUseVertexAlphaLocationQ1020, object.useVertexAlpha ? 1.0f : 0.0f);
    glUniform1f(gSpecularEnabledLocationQ1020, object.specularEnabled ? 1.0f : 0.0f);
    glUniform3fv(gSpecularColorLocationQ1020, 1, object.specularColor);
    glUniform3fv(gEmissiveColorLocationQ1020, 1, object.emissiveColor);
    glUniform1f(gEmissiveMultLocationQ1020, object.emissiveMult);
    glUniform1f(gGlowEnabledLocationQ1020, object.realGlow ? 1.0f : 0.0f);
]=])
string(REPLACE "${Q1020_OLD_DRAW_PREFIX}" "${Q1020_NEW_DRAW_PREFIX}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "    glActiveTexture(GL_TEXTURE1);\n    glBindTexture(GL_TEXTURE_2D, object.normal);"
    "    glActiveTexture(GL_TEXTURE1);\n    glBindTexture(GL_TEXTURE_2D, object.normal);\n    glActiveTexture(GL_TEXTURE2);\n    glBindTexture(GL_TEXTURE_2D, object.glow);"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "    glUniform1i(gNormalLocation, 1);"
    "    glUniform1i(gNormalLocation, 1);\n    glUniform1i(gGlowLocationQ1020, 2);"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Preserve caller texture unit 2 as well.
string(REPLACE
    "    GLint previousTexture0 = 0, previousTexture1 = 0;"
    "    GLint previousTexture0 = 0, previousTexture1 = 0, previousTexture2 = 0;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "    glActiveTexture(GL_TEXTURE1);\n    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture1);"
    "    glActiveTexture(GL_TEXTURE1);\n    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture1);\n    glActiveTexture(GL_TEXTURE2);\n    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture2);"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "    glActiveTexture(GL_TEXTURE1);\n    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture1));"
    "    glActiveTexture(GL_TEXTURE2);\n    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture2));\n    glActiveTexture(GL_TEXTURE1);\n    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture1));"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# One concise device-side proof line instead of relying on visual judgement.
set(Q1020_OLD_READY_COUNTERS [=[
        size_t realDiffuse = 0, realNormal = 0;
        size_t triangles = 0, alphaBlend = 0, alphaTest = 0;
]=])
set(Q1020_NEW_READY_COUNTERS [=[
        size_t realDiffuse = 0, realNormal = 0;
        size_t triangles = 0, alphaBlend = 0, alphaTest = 0;
        size_t q1020NoLighting = 0, q1020VertexColor = 0, q1020Glow = 0,
               q1020Specular = 0, q1020Emissive = 0;
]=])
string(REPLACE "${Q1020_OLD_READY_COUNTERS}" "${Q1020_NEW_READY_COUNTERS}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "            if (object.alphaTest) ++alphaTest;"
    "            if (object.alphaTest) ++alphaTest;\n            if (object.noLighting) ++q1020NoLighting;\n            if (object.useVertexColor) ++q1020VertexColor;\n            if (object.realGlow) ++q1020Glow;\n            if (object.specularEnabled) ++q1020Specular;\n            if (object.emissiveMult > 0.0f && (object.emissiveColor[0] != 0.0f || object.emissiveColor[1] != 0.0f || object.emissiveColor[2] != 0.0f)) ++q1020Emissive;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "        Q6H_LOGI(\"Q6H READY: ESM->REFR->BASE->MODL->BSA->NIF objects=%zu triangles=%zu realDiffuse=%zu realNormal=%zu uniqueTextures=%zu alphaBlend=%zu alphaTest=%zu cell=MegatonPlayerHouse\","
    "        Q6H_LOGI(\"Q10.2 MATERIAL READY: drawShapes=%zu noLighting=%zu vertexColor=%zu glowMaps=%zu specular=%zu emissive=%zu\", gObjects.size(), q1020NoLighting, q1020VertexColor, q1020Glow, q1020Specular, q1020Emissive);\n        Q6H_LOGI(\"Q6H READY: ESM->REFR->BASE->MODL->BSA->NIF objects=%zu triangles=%zu realDiffuse=%zu realNormal=%zu uniqueTextures=%zu alphaBlend=%zu alphaTest=%zu cell=MegatonPlayerHouse\","
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Q10.2 is the last renderer transform; overwrite the generated renderer once.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
