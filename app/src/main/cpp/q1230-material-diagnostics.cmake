# Q12.3: diagnostic-only material audit for the two remaining Megaton repros.
#
# IMPORTANT: this milestone intentionally changes no render state, shader math,
# texture formats, material values, geometry or filtering. It only logs the CPU
# material/UV/vertex-colour inputs for MegatonBrassLanternSign and
# MegatonChurchofAtom, plus decoded RGBA statistics for the textures those assets
# are expected to use. This is evidence gathering before the next visual fix.

# -----------------------------------------------------------------------------
# Texture decode audit. UploadTexture already owns the decoded Fo3RgbaTexture,
# so inspect it immediately before the unchanged GLES upload.
# -----------------------------------------------------------------------------
set(Q1230_OLD_TEX_UPLOAD [==[
    glGenTextures(1, &textureId);
]==])
set(Q1230_NEW_TEX_UPLOAD [==[
    const bool q1230TextureTarget =
        cacheKey.find("megatonsignscrap01") != std::string::npos ||
        cacheKey.find("megatonsignscrap02") != std::string::npos ||
        cacheKey.find("megatonsignscrap03") != std::string::npos ||
        cacheKey.find("metalscrapshing") != std::string::npos ||
        cacheKey.find("metalscrapdoor03") != std::string::npos ||
        cacheKey.find("fxwhite.dds") != std::string::npos ||
        cacheKey.find("fxsoftglowspot01.dds") != std::string::npos;
    if (q1230TextureTarget && texture.rgba.size() >= 4u) {
        uint8_t q1230Min[4]{255u, 255u, 255u, 255u};
        uint8_t q1230Max[4]{0u, 0u, 0u, 0u};
        uint64_t q1230Sum[4]{0u, 0u, 0u, 0u};
        size_t q1230AlphaZero = 0u;
        size_t q1230AlphaMid = 0u;
        size_t q1230AlphaFull = 0u;
        const size_t q1230Pixels = texture.rgba.size() / 4u;
        for (size_t q1230I = 0u; q1230I < q1230Pixels; ++q1230I) {
            const size_t q1230Base = q1230I * 4u;
            for (size_t q1230C = 0u; q1230C < 4u; ++q1230C) {
                const uint8_t q1230V = texture.rgba[q1230Base + q1230C];
                if (q1230V < q1230Min[q1230C]) q1230Min[q1230C] = q1230V;
                if (q1230V > q1230Max[q1230C]) q1230Max[q1230C] = q1230V;
                q1230Sum[q1230C] += q1230V;
            }
            const uint8_t q1230A = texture.rgba[q1230Base + 3u];
            if (q1230A == 0u) ++q1230AlphaZero;
            else if (q1230A == 255u) ++q1230AlphaFull;
            else ++q1230AlphaMid;
        }
        Q6H_LOGI("Q12.3 TEX: key=%s real=%d source=%s size=%dx%d format=%s min=(%u %u %u %u) max=(%u %u %u %u) avg=(%u %u %u %u) alpha0=%zu alphaMid=%zu alpha255=%zu pixels=%zu",
                 cacheKey.c_str(), real ? 1 : 0, texture.sourcePath.c_str(),
                 texture.width, texture.height, texture.format.c_str(),
                 static_cast<unsigned>(q1230Min[0]), static_cast<unsigned>(q1230Min[1]),
                 static_cast<unsigned>(q1230Min[2]), static_cast<unsigned>(q1230Min[3]),
                 static_cast<unsigned>(q1230Max[0]), static_cast<unsigned>(q1230Max[1]),
                 static_cast<unsigned>(q1230Max[2]), static_cast<unsigned>(q1230Max[3]),
                 static_cast<unsigned>(q1230Sum[0] / q1230Pixels),
                 static_cast<unsigned>(q1230Sum[1] / q1230Pixels),
                 static_cast<unsigned>(q1230Sum[2] / q1230Pixels),
                 static_cast<unsigned>(q1230Sum[3] / q1230Pixels),
                 q1230AlphaZero, q1230AlphaMid, q1230AlphaFull, q1230Pixels);
    }

    glGenTextures(1, &textureId);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1230_OLD_TEX_UPLOAD}" Q1230_TEX_POS)
if(Q1230_TEX_POS EQUAL -1)
    message(FATAL_ERROR "Q12.3 could not find UploadTexture GLES upload point")
endif()
string(REPLACE "${Q1230_OLD_TEX_UPLOAD}" "${Q1230_NEW_TEX_UPLOAD}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Per-shape CPU material audit. Hook immediately before Q11.1's diffuse upload;
# all authored NIF fields are populated here but no GL material state has changed.
# -----------------------------------------------------------------------------
set(Q1230_OLD_MESH_HOOK [==[
    const bool q1110NoLightingVertexColorOnly =
]==])
set(Q1230_NEW_MESH_HOOK [==[
    std::string q1230ModelLower = cpu.placement.modelPath;
    for (char& q1230Ch : q1230ModelLower) {
        if (q1230Ch == '/') q1230Ch = '\\';
        q1230Ch = static_cast<char>(std::tolower(static_cast<unsigned char>(q1230Ch)));
    }
    const bool q1230MeshTarget =
        q1230ModelLower.find("megatonbrasslanternsign") != std::string::npos ||
        q1230ModelLower.find("megatonchurchofatom") != std::string::npos;
    if (q1230MeshTarget) {
        Q6H_LOGI("Q12.3 MESH: ref=%08X EDID=%s model=%s verts=%zu tris=%zu diffuse=%s normal=%s glow=%s noLighting=%d flags1=%08X flags2=%08X alphaBlend=%d src=%u dst=%u alphaTest=%d threshold=%.4f matAlpha=%.4f useVC=%d useVA=%d specEnabled=%d spec=(%.4f %.4f %.4f) emissive=(%.4f %.4f %.4f) emissiveMult=%.4f gloss=%.4f envScale=%.4f",
                 gpu.refFormId,
                 cpu.placement.editorId.empty() ? "<none>" : cpu.placement.editorId.c_str(),
                 cpu.placement.modelPath.c_str(), vertexCount, cpu.mesh.indices.size() / 3u,
                 cpu.mesh.diffuseTexturePath.empty() ? "<none>" : cpu.mesh.diffuseTexturePath.c_str(),
                 cpu.mesh.normalTexturePath.empty() ? "<none>" : cpu.mesh.normalTexturePath.c_str(),
                 cpu.mesh.glowTexturePath.empty() ? "<none>" : cpu.mesh.glowTexturePath.c_str(),
                 cpu.mesh.noLighting ? 1 : 0, cpu.mesh.shaderFlags1, cpu.mesh.shaderFlags2,
                 cpu.mesh.alphaBlend ? 1 : 0,
                 static_cast<unsigned>(cpu.mesh.alphaSourceBlend),
                 static_cast<unsigned>(cpu.mesh.alphaDestBlend),
                 cpu.mesh.alphaTest ? 1 : 0, cpu.mesh.alphaThreshold, cpu.mesh.alpha,
                 gpu.useVertexColor ? 1 : 0, gpu.useVertexAlpha ? 1 : 0,
                 gpu.specularEnabled ? 1 : 0,
                 cpu.mesh.specularColor[0], cpu.mesh.specularColor[1], cpu.mesh.specularColor[2],
                 cpu.mesh.emissiveColor[0], cpu.mesh.emissiveColor[1], cpu.mesh.emissiveColor[2],
                 cpu.mesh.emissiveMult, cpu.mesh.glossiness, cpu.mesh.environmentMapScale);

        if (cpu.mesh.texcoords.size() >= 2u) {
            float q1230MinU = cpu.mesh.texcoords[0], q1230MaxU = cpu.mesh.texcoords[0];
            float q1230MinV = cpu.mesh.texcoords[1], q1230MaxV = cpu.mesh.texcoords[1];
            double q1230SumU = 0.0, q1230SumV = 0.0;
            const size_t q1230UvCount = cpu.mesh.texcoords.size() / 2u;
            for (size_t q1230I = 0u; q1230I < q1230UvCount; ++q1230I) {
                const float q1230U = cpu.mesh.texcoords[q1230I * 2u];
                const float q1230V = cpu.mesh.texcoords[q1230I * 2u + 1u];
                if (q1230U < q1230MinU) q1230MinU = q1230U;
                if (q1230U > q1230MaxU) q1230MaxU = q1230U;
                if (q1230V < q1230MinV) q1230MinV = q1230V;
                if (q1230V > q1230MaxV) q1230MaxV = q1230V;
                q1230SumU += q1230U;
                q1230SumV += q1230V;
            }
            Q6H_LOGI("Q12.3 UV: ref=%08X model=%s tris=%zu count=%zu min=(%.5f %.5f) max=(%.5f %.5f) avg=(%.5f %.5f)",
                     gpu.refFormId, cpu.placement.modelPath.c_str(), cpu.mesh.indices.size() / 3u,
                     q1230UvCount, q1230MinU, q1230MinV, q1230MaxU, q1230MaxV,
                     static_cast<float>(q1230SumU / static_cast<double>(q1230UvCount)),
                     static_cast<float>(q1230SumV / static_cast<double>(q1230UvCount)));
        }

        if (cpu.mesh.vertexColors.size() >= 4u) {
            float q1230MinC[4]{cpu.mesh.vertexColors[0], cpu.mesh.vertexColors[1],
                               cpu.mesh.vertexColors[2], cpu.mesh.vertexColors[3]};
            float q1230MaxC[4]{q1230MinC[0], q1230MinC[1], q1230MinC[2], q1230MinC[3]};
            double q1230SumC[4]{0.0, 0.0, 0.0, 0.0};
            const size_t q1230ColorCount = cpu.mesh.vertexColors.size() / 4u;
            for (size_t q1230I = 0u; q1230I < q1230ColorCount; ++q1230I) {
                for (size_t q1230C = 0u; q1230C < 4u; ++q1230C) {
                    const float q1230V = cpu.mesh.vertexColors[q1230I * 4u + q1230C];
                    if (q1230V < q1230MinC[q1230C]) q1230MinC[q1230C] = q1230V;
                    if (q1230V > q1230MaxC[q1230C]) q1230MaxC[q1230C] = q1230V;
                    q1230SumC[q1230C] += q1230V;
                }
            }
            Q6H_LOGI("Q12.3 VCOLOR: ref=%08X model=%s tris=%zu count=%zu min=(%.4f %.4f %.4f %.4f) max=(%.4f %.4f %.4f %.4f) avg=(%.4f %.4f %.4f %.4f)",
                     gpu.refFormId, cpu.placement.modelPath.c_str(), cpu.mesh.indices.size() / 3u,
                     q1230ColorCount,
                     q1230MinC[0], q1230MinC[1], q1230MinC[2], q1230MinC[3],
                     q1230MaxC[0], q1230MaxC[1], q1230MaxC[2], q1230MaxC[3],
                     static_cast<float>(q1230SumC[0] / static_cast<double>(q1230ColorCount)),
                     static_cast<float>(q1230SumC[1] / static_cast<double>(q1230ColorCount)),
                     static_cast<float>(q1230SumC[2] / static_cast<double>(q1230ColorCount)),
                     static_cast<float>(q1230SumC[3] / static_cast<double>(q1230ColorCount)));
        } else {
            Q6H_LOGI("Q12.3 VCOLOR: ref=%08X model=%s tris=%zu count=0",
                     gpu.refFormId, cpu.placement.modelPath.c_str(), cpu.mesh.indices.size() / 3u);
        }
    }

    const bool q1110NoLightingVertexColorOnly =
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1230_OLD_MESH_HOOK}" Q1230_MESH_POS)
if(Q1230_MESH_POS EQUAL -1)
    message(FATAL_ERROR "Q12.3 could not find Q11.1 material upload hook")
endif()
string(REPLACE "${Q1230_OLD_MESH_HOOK}" "${Q1230_NEW_MESH_HOOK}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(FIND "${Q6H_NATIVE_SOURCE}" "Q12.3 TEX:" Q1230_TEX_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q12.3 MESH:" Q1230_MESH_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q12.3 VCOLOR:" Q1230_VCOLOR_OK)
if(Q1230_TEX_OK EQUAL -1 OR Q1230_MESH_OK EQUAL -1 OR Q1230_VCOLOR_OK EQUAL -1)
    message(FATAL_ERROR "Q12.3 diagnostic instrumentation verification failed")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q12.3 diagnostic-only Megaton material audit enabled")
