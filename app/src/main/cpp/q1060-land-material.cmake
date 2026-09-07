# Q10.6 LAND TX01 normal/specular material state.

string(REPLACE
    "struct Q711TerrainBatch {\n    std::string texturePath;"
    "struct Q711TerrainBatch {\n    std::string texturePath;\n    std::string normalPath;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    GLuint texture = 0;\n    GLsizei vertexCount = 0;"
    "    GLuint texture = 0;\n    GLuint normalTexture = 0;\n    GLsizei vertexCount = 0;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    bool realTexture = false;\n    bool ownsTexture = false;"
    "    bool realTexture = false;\n    bool realNormal = false;\n    bool specularEnabled = false;\n    bool ownsTexture = false;\n    bool ownsNormal = false;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "struct Q711CpuBatch {\n    std::string texturePath;"
    "struct Q711CpuBatch {\n    std::string texturePath;\n    std::string normalPath;\n    bool specularEnabled = false;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "        batch.texturePath = texturePath;\n        batch.alphaLayer = alphaLayer;"
    "        batch.texturePath = texturePath;\n        batch.normalPath = ResolveFo3TerrainNormalTextureQ1060(texturePath);\n        batch.specularEnabled = IsFo3TerrainSpecularEnabledQ1060(texturePath);\n        batch.alphaLayer = alphaLayer;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# Vertex colour attribute + normal-map shader inputs.
string(REPLACE
    "        layout(location = 3) in float aAlpha;"
    "        layout(location = 3) in float aAlpha;\n        layout(location = 4) in vec3 aVertexColor;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "        out float vAlpha;"
    "        out float vAlpha;\n        out vec3 vVertexColor;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "            vAlpha = aAlpha;"
    "            vAlpha = aAlpha;\n            vVertexColor = aVertexColor;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "        in float vAlpha;"
    "        in float vAlpha;\n        in vec3 vVertexColor;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "        uniform sampler2D uDiffuse;"
    "        uniform sampler2D uDiffuse;\n        uniform sampler2D uNormalGloss;\n        uniform int uUseNormal;\n        uniform float uTerrainSpecularEnabled;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

set(Q1060_TERRAIN_NORMAL_CODE [=[
            vec3 N = normalize(vNormal);
            vec4 normalGloss = vec4(0.5, 0.5, 1.0, 0.0);
            if (uUseNormal != 0) {
                normalGloss = texture(uNormalGloss, vTexCoord);
                vec3 tangentNormal = normalGloss.xyz * 2.0 - 1.0;
                vec3 dp1 = dFdx(vPosition);
                vec3 dp2 = dFdy(vPosition);
                vec2 duv1 = dFdx(vTexCoord);
                vec2 duv2 = dFdy(vTexCoord);
                float determinant = duv1.x * duv2.y - duv1.y * duv2.x;
                if (abs(determinant) > 0.000001) {
                    vec3 T = normalize(dp1 * duv2.y - dp2 * duv1.y);
                    T = normalize(T - N * dot(N, T));
                    vec3 B = normalize(cross(N, T));
                    if (determinant < 0.0) B = -B;
                    N = normalize(mat3(T, B, N) * tangentNormal);
                }
            }
]=])
string(REPLACE
    "            vec3 N = normalize(vNormal);"
    "${Q1060_TERRAIN_NORMAL_CODE}"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "            vec3 albedo = uUseTexture != 0 ? texture(uDiffuse, vTexCoord).rgb : earthFallback;"
    "            vec3 albedo = (uUseTexture != 0 ? texture(uDiffuse, vTexCoord).rgb : earthFallback) * vVertexColor;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

set(Q1060_OLD_TERRAIN_LIT [=[
            float q1050SunVisibility = Q1050ShadowVisibility(vShadowCoord, N, lightDirection);
            vec3 lit = albedo * (uAmbientColor + uSunlightColor * lambert * q1050SunVisibility);
]=])
set(Q1060_NEW_TERRAIN_LIT [=[
            float q1050SunVisibility = Q1050ShadowVisibility(vShadowCoord, N, lightDirection);
            vec3 lit = albedo * (uAmbientColor + uSunlightColor * lambert * q1050SunVisibility);
            if (uUseNormal != 0 && uTerrainSpecularEnabled > 0.5) {
                vec3 V = normalize(uEyePosition - vPosition);
                vec3 H = normalize(lightDirection + V);
                float specular = pow(max(dot(N, H), 0.0), 18.0) * normalGloss.a * 0.24;
                lit += uSunlightColor * specular * q1050SunVisibility;
            }
]=])
string(REPLACE "${Q1060_OLD_TERRAIN_LIT}" "${Q1060_NEW_TERRAIN_LIT}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

string(REPLACE
    "GLint q76bUseTextureLocation = -1;"
    "GLint q76bUseTextureLocation = -1;\nGLint q1060TerrainNormalSamplerLocation = -1;\nGLint q1060TerrainUseNormalLocation = -1;\nGLint q1060TerrainSpecularLocation = -1;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    q76bUseTextureLocation = -1;"
    "    q76bUseTextureLocation = -1;\n    q1060TerrainNormalSamplerLocation = -1;\n    q1060TerrainUseNormalLocation = -1;\n    q1060TerrainSpecularLocation = -1;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    q76bUseTextureLocation = glGetUniformLocation(q76bProgram, \"uUseTexture\");"
    "    q76bUseTextureLocation = glGetUniformLocation(q76bProgram, \"uUseTexture\");\n    q1060TerrainNormalSamplerLocation = glGetUniformLocation(q76bProgram, \"uNormalGloss\");\n    q1060TerrainUseNormalLocation = glGetUniformLocation(q76bProgram, \"uUseNormal\");\n    q1060TerrainSpecularLocation = glGetUniformLocation(q76bProgram, \"uTerrainSpecularEnabled\");"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

string(REPLACE
    "        gpu.texturePath = cpu.texturePath;\n        gpu.alphaLayer = cpu.alphaLayer;"
    "        gpu.texturePath = cpu.texturePath;\n        gpu.normalPath = cpu.normalPath;\n        gpu.specularEnabled = cpu.specularEnabled;\n        gpu.alphaLayer = cpu.alphaLayer;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    std::unordered_map<std::string, GLuint> uploadedByPath;"
    "    std::unordered_map<std::string, GLuint> uploadedByPath;\n    std::unordered_map<std::string, GLuint> uploadedNormalsQ1060;\n    size_t q1060NormalUploads = 0u;\n    size_t q1060NormalMisses = 0u;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
set(Q1060_NORMAL_UPLOAD [=[
        if (!cpu.normalPath.empty()) {
            const auto cachedNormal = uploadedNormalsQ1060.find(cpu.normalPath);
            if (cachedNormal != uploadedNormalsQ1060.end()) {
                gpu.normalTexture = cachedNormal->second;
                gpu.realNormal = gpu.normalTexture != 0u;
            } else if (q1060NormalUploads < Q713_MAX_REAL_TEXTURES) {
                gpu.normalTexture = Q711UploadTexture(cpu.normalPath);
                gpu.realNormal = gpu.normalTexture != 0u;
                if (gpu.realNormal) {
                    gpu.ownsNormal = true;
                    uploadedNormalsQ1060[cpu.normalPath] = gpu.normalTexture;
                    ++q1060NormalUploads;
                } else {
                    ++q1060NormalMisses;
                }
            }
        }

]=])
string(REPLACE
    "        // Never draw an unresolved alpha layer as the brown fallback. Base LAND"
    "${Q1060_NORMAL_UPLOAD}        // Never draw an unresolved alpha layer as the brown fallback. Base LAND"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "        constexpr GLsizei stride = static_cast<GLsizei>(9 * sizeof(float));"
    "        constexpr GLsizei stride = static_cast<GLsizei>(12 * sizeof(float));"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride,\n                              reinterpret_cast<const void*>(8 * sizeof(float)));\n        glEnableVertexAttribArray(3);"
    "        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride,\n                              reinterpret_cast<const void*>(8 * sizeof(float)));\n        glEnableVertexAttribArray(3);\n        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride,\n                              reinterpret_cast<const void*>(9 * sizeof(float)));\n        glEnableVertexAttribArray(4);"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "        gpu.vertexCount = static_cast<GLsizei>(cpu.vertices.size() / 9u);"
    "        gpu.vertexCount = static_cast<GLsizei>(cpu.vertices.size() / 12u);"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "        if (batch.texture && batch.ownsTexture) glDeleteTextures(1, &batch.texture);"
    "        if (batch.texture && batch.ownsTexture) glDeleteTextures(1, &batch.texture);\n        if (batch.normalTexture && batch.ownsNormal) glDeleteTextures(1, &batch.normalTexture);"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

string(REPLACE
    "    GLint previousTexture0 = 0;\n    GLint previousTexture3 = 0;"
    "    GLint previousTexture0 = 0;\n    GLint previousTexture1 = 0;\n    GLint previousTexture3 = 0;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    glActiveTexture(GL_TEXTURE0);\n    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture0);\n    glActiveTexture(GL_TEXTURE3);"
    "    glActiveTexture(GL_TEXTURE0);\n    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture0);\n    glActiveTexture(GL_TEXTURE1);\n    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture1);\n    glActiveTexture(GL_TEXTURE3);"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    glUniform1i(q76bSamplerLocation, 0);"
    "    glUniform1i(q76bSamplerLocation, 0);\n    if (q1060TerrainNormalSamplerLocation >= 0) glUniform1i(q1060TerrainNormalSamplerLocation, 1);"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

set(Q1060_BIND_MATERIAL [=[
        if (q1060TerrainUseNormalLocation >= 0) glUniform1i(q1060TerrainUseNormalLocation, batch.realNormal ? 1 : 0);
        if (q1060TerrainSpecularLocation >= 0) glUniform1f(q1060TerrainSpecularLocation, batch.specularEnabled ? 1.0f : 0.0f);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, batch.realNormal ? batch.normalTexture : 0u);
        glActiveTexture(GL_TEXTURE0);
]=])
string(REPLACE
    "        glUniform1i(q76bUseTextureLocation, batch.realTexture ? 1 : 0);\n        glBindTexture(GL_TEXTURE_2D, batch.realTexture ? batch.texture : 0u);"
    "        glUniform1i(q76bUseTextureLocation, batch.realTexture ? 1 : 0);\n${Q1060_BIND_MATERIAL}        glBindTexture(GL_TEXTURE_2D, batch.realTexture ? batch.texture : 0u);"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "        glUniform1i(q76bUseTextureLocation, 1);\n        glBindTexture(GL_TEXTURE_2D, batch.texture);"
    "        glUniform1i(q76bUseTextureLocation, 1);\n${Q1060_BIND_MATERIAL}        glBindTexture(GL_TEXTURE_2D, batch.texture);"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    glActiveTexture(GL_TEXTURE3);\n    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture3));\n    glActiveTexture(static_cast<GLenum>(previousActiveTexture));"
    "    glActiveTexture(GL_TEXTURE3);\n    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture3));\n    glActiveTexture(GL_TEXTURE1);\n    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture1));\n    glActiveTexture(static_cast<GLenum>(previousActiveTexture));"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

string(REPLACE
    "    q76bReady = true;\n    Q76B_LOGI(\"Q7.20 TERRAIN GPU READY:"
    "    q76bReady = true;\n    size_t q1060VclrCells = 0u;\n    for (const Fo3TerrainCellQ76& c : terrain) if (GetFo3TerrainVertexColorsQ1060(c.landFormId).size() == Q76B_HEIGHT_COUNT * 3u) ++q1060VclrCells;\n    Q76B_LOGI(\"Q10.6 TERRAIN FIDELITY READY: cells=%zu VCLRcells=%zu normalUploads=%zu normalMisses=%zu authoredTX01=1 specularMaskAlpha=1\", acceptedCells, q1060VclrCells, q1060NormalUploads, q1060NormalMisses);\n    Q76B_LOGI(\"Q7.20 TERRAIN GPU READY:"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
