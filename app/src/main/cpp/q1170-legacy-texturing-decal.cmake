# Q11.7: restore Fallout 3 legacy NiTexturingProperty textures and authored decal depth bias.
#
# Device V11.6 makes the remaining failures much more specific:
#   * FOOD neon is a white fallback plane even though the NIF carries legacy
#     NiTexturingProperty state.
#   * Church/Atom effect geometry is textured but can be washed out by an
#     additional unresolved legacy texture layer.
#   * thin/flat signs shimmer because FO3 decal surfaces are coplanar and need
#     the BSShaderProperty Decal depth bias used by the original renderer.
#
# FO3 NIF 20.2.0.7 stores NiSourceTexture file names as indices into the header
# string table. Q6H previously discarded that table and ignored NiTexturingProperty
# entirely. Preserve the table, resolve Base/Glow texture slots through
# NiSourceTexture, and use them only when a shape has no stronger direct shader
# texture. Also honor BSSFlag1_Decal (0x04000000) with polygon offset.

set(Q1170_NIF_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/fo3-static-nif-q6h.cpp")
if(NOT EXISTS "${Q1170_NIF_GENERATED}")
    message(FATAL_ERROR "Q11.7 expected generated NIF source at ${Q1170_NIF_GENERATED}")
endif()
file(READ "${Q1170_NIF_GENERATED}" Q1170_NIF_SOURCE)

# Keep the NIF header string table so NiSourceTexture string indices can resolve.
set(Q1170_OLD_HEADER_FIELDS [==[
    std::vector<uint32_t> blockSizes;
    std::vector<size_t> blockOffsets;
]==])
set(Q1170_NEW_HEADER_FIELDS [==[
    std::vector<uint32_t> blockSizes;
    std::vector<size_t> blockOffsets;
    std::vector<std::string> strings;
]==])
string(FIND "${Q1170_NIF_SOURCE}" "${Q1170_OLD_HEADER_FIELDS}" Q1170_HEADER_FIELDS_POS)
if(Q1170_HEADER_FIELDS_POS EQUAL -1)
    message(FATAL_ERROR "Q11.7 could not find NifHeader fields")
endif()
string(REPLACE "${Q1170_OLD_HEADER_FIELDS}" "${Q1170_NEW_HEADER_FIELDS}"
       Q1170_NIF_SOURCE "${Q1170_NIF_SOURCE}")

set(Q1170_OLD_STRING_TABLE [==[
    for (uint32_t i = 0; i < numStrings; ++i) {
        std::string ignored;
        if (!c.SizedString(ignored)) return false;
    }
]==])
set(Q1170_NEW_STRING_TABLE [==[
    out.strings.clear();
    out.strings.reserve(numStrings);
    for (uint32_t i = 0; i < numStrings; ++i) {
        std::string value;
        if (!c.SizedString(value)) return false;
        out.strings.push_back(std::move(value));
    }
]==])
string(FIND "${Q1170_NIF_SOURCE}" "${Q1170_OLD_STRING_TABLE}" Q1170_STRING_TABLE_POS)
if(Q1170_STRING_TABLE_POS EQUAL -1)
    message(FATAL_ERROR "Q11.7 could not find discarded NIF string table")
endif()
string(REPLACE "${Q1170_OLD_STRING_TABLE}" "${Q1170_NEW_STRING_TABLE}"
       Q1170_NIF_SOURCE "${Q1170_NIF_SOURCE}")

# FO3 20.2.0.7 NiTexturingProperty texture slot layout. We only need the
# authored Base (0) and Glow (4) NiSourceTexture references, but still advance
# correctly across enabled slots/optional transforms to reach them safely.
set(Q1170_LEGACY_TEXTURE_PARSERS [==[
bool ParseSourceTexturePathQ1170(const std::vector<uint8_t>& nif,
                                 const NifHeader& header,
                                 uint32_t sourceRef,
                                 std::string& outPath) {
    outPath.clear();
    if (sourceRef >= header.numBlocks ||
        BlockType(header, sourceRef) != "NiSourceTexture") return false;
    Cursor c(BlockData(nif, header, sourceRef), header.blockSizes[sourceRef]);
    if (!ParseObjectNetPrefix(c)) return false;
    uint8_t external = 0u;
    uint32_t fileStringIndex = INVALID_REF;
    if (!c.U8(external) || !c.U32(fileStringIndex)) return false;
    if (fileStringIndex == INVALID_REF || fileStringIndex >= header.strings.size()) return false;
    outPath = header.strings[fileStringIndex];
    return !outPath.empty();
}

bool ParseLegacyTexturingPropertyQ1170(const uint8_t* data, size_t size,
                                       uint32_t& baseSourceRef,
                                       uint32_t& glowSourceRef) {
    Cursor c(data, size);
    if (!ParseObjectNetPrefix(c)) return false;

    // NIF 20.2.0.7: NiTexturingProperty has a 16-bit flags field and no
    // standalone Apply Mode field (that field ended at 20.1.0.1).
    uint16_t propertyFlags = 0u;
    uint32_t textureCount = 0u;
    if (!c.U16(propertyFlags) || !c.U32(textureCount) || textureCount > 32u) return false;

    baseSourceRef = INVALID_REF;
    glowSourceRef = INVALID_REF;
    for (uint32_t slot = 0u; slot < textureCount; ++slot) {
        uint8_t enabled = 0u;
        if (!c.U8(enabled)) return false;
        if (!enabled) continue;

        uint32_t sourceRef = INVALID_REF;
        uint16_t textureFlags = 0u;
        uint8_t hasTransform = 0u;
        if (!c.U32(sourceRef) || !c.U16(textureFlags) || !c.U8(hasTransform)) return false;
        if (hasTransform && !c.Skip(32u)) return false; // offset, scale, rot, method, origin

        if (slot == 0u) baseSourceRef = sourceRef;
        if (slot == 4u) glowSourceRef = sourceRef;

        // Bump slot carries luma bias (vec2) + bump matrix (vec4).
        if (slot == 5u && !c.Skip(24u)) return false;
        // Parallax slot in 20.2.0.7 carries one extra float.
        if (slot == 7u && !c.Skip(4u)) return false;
    }
    return true;
}

]==])
set(Q1170_PARSER_INSERT [==[
bool ParseStencilDrawModeQ1160(const uint8_t* data, size_t size,
]==])
string(FIND "${Q1170_NIF_SOURCE}" "${Q1170_PARSER_INSERT}" Q1170_PARSER_INSERT_POS)
if(Q1170_PARSER_INSERT_POS EQUAL -1)
    message(FATAL_ERROR "Q11.7 could not find post-Q11.6 parser insertion point")
endif()
string(REPLACE "${Q1170_PARSER_INSERT}"
       "${Q1170_LEGACY_TEXTURE_PARSERS}${Q1170_PARSER_INSERT}"
       Q1170_NIF_SOURCE "${Q1170_NIF_SOURCE}")

# Empty NoLighting File Name must not erase a texture already resolved from
# NiTexturingProperty if the property order is legacy-texture -> NoLighting.
set(Q1170_OLD_DIRECT_ASSIGN [==[
                candidate.noLighting = true;
                candidate.diffuseTexturePath = directTexture;
]==])
set(Q1170_NEW_DIRECT_ASSIGN [==[
                candidate.noLighting = true;
                if (!directTexture.empty() || candidate.diffuseTexturePath.empty())
                    candidate.diffuseTexturePath = directTexture;
]==])
string(FIND "${Q1170_NIF_SOURCE}" "${Q1170_OLD_DIRECT_ASSIGN}" Q1170_DIRECT_ASSIGN_POS)
if(Q1170_DIRECT_ASSIGN_POS EQUAL -1)
    message(FATAL_ERROR "Q11.7 could not find NoLighting direct texture assignment")
endif()
string(REPLACE "${Q1170_OLD_DIRECT_ASSIGN}" "${Q1170_NEW_DIRECT_ASSIGN}"
       Q1170_NIF_SOURCE "${Q1170_NIF_SOURCE}")

# Resolve legacy Base/Glow slots on each shape. Direct BSShader textures remain
# authoritative; legacy paths only fill currently-empty material slots.
set(Q1170_OLD_STENCIL_BRANCH [==[
        } else if (type == "NiStencilProperty") {
]==])
set(Q1170_NEW_STENCIL_BRANCH [==[
        } else if (type == "NiTexturingProperty") {
            uint32_t baseSourceRefQ1170 = INVALID_REF;
            uint32_t glowSourceRefQ1170 = INVALID_REF;
            if (ParseLegacyTexturingPropertyQ1170(prop, header.blockSizes[ref],
                                                  baseSourceRefQ1170, glowSourceRefQ1170)) {
                std::string basePathQ1170, glowPathQ1170;
                const bool haveBaseQ1170 = ParseSourceTexturePathQ1170(
                    nif, header, baseSourceRefQ1170, basePathQ1170);
                const bool haveGlowQ1170 = ParseSourceTexturePathQ1170(
                    nif, header, glowSourceRefQ1170, glowPathQ1170);
                if (candidate.diffuseTexturePath.empty() && haveBaseQ1170)
                    candidate.diffuseTexturePath = basePathQ1170;
                if (candidate.glowTexturePath.empty() && haveGlowQ1170)
                    candidate.glowTexturePath = glowPathQ1170;
                if (haveBaseQ1170 || haveGlowQ1170) {
                    Q6H_LOGI("Q11.7 LEGACY TEX: shape=%u baseRef=%u glowRef=%u diffuse=%s glow=%s",
                             shape.block, baseSourceRefQ1170, glowSourceRefQ1170,
                             haveBaseQ1170 ? basePathQ1170.c_str() : "<none>",
                             haveGlowQ1170 ? glowPathQ1170.c_str() : "<none>");
                }
            }
        } else if (type == "NiStencilProperty") {
]==])
string(FIND "${Q1170_NIF_SOURCE}" "${Q1170_OLD_STENCIL_BRANCH}" Q1170_STENCIL_BRANCH_POS)
if(Q1170_STENCIL_BRANCH_POS EQUAL -1)
    message(FATAL_ERROR "Q11.7 could not find NiStencil branch for legacy texture insertion")
endif()
string(REPLACE "${Q1170_OLD_STENCIL_BRANCH}" "${Q1170_NEW_STENCIL_BRANCH}"
       Q1170_NIF_SOURCE "${Q1170_NIF_SOURCE}")

string(FIND "${Q1170_NIF_SOURCE}" "Q11.7 LEGACY TEX" Q1170_LEGACY_LOG_OK)
string(FIND "${Q1170_NIF_SOURCE}" "std::vector<std::string> strings" Q1170_STRINGS_OK)
if(Q1170_LEGACY_LOG_OK EQUAL -1 OR Q1170_STRINGS_OK EQUAL -1)
    message(FATAL_ERROR "Q11.7 legacy texturing verification failed")
endif()
file(WRITE "${Q1170_NIF_GENERATED}" "${Q1170_NIF_SOURCE}")

# -----------------------------------------------------------------------------
# Renderer: FO3 BSShaderProperty BSSFlag1_Decal = 0x04000000. Coplanar decals
# need a depth bias; without it, flat signs/labels shimmer as the depth winner
# changes between eyes and frames.
# -----------------------------------------------------------------------------
string(REPLACE
    "    uint8_t stencilDrawMode = 0u;"
    "    uint8_t stencilDrawMode = 0u;\n    bool decalQ1170 = false;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "    gpu.stencilDrawMode = cpu.mesh.stencilDrawMode;"
    "    gpu.stencilDrawMode = cpu.mesh.stencilDrawMode;\n    gpu.decalQ1170 = (cpu.mesh.shaderFlags1 & 0x04000000u) != 0u;\n    if (gpu.decalQ1170) {\n        Q6H_LOGI(\"Q11.7 DECAL BIAS: ref=%08X model=%s shaderFlags1=%08X\",\n                 gpu.refFormId, cpu.placement.modelPath.c_str(), cpu.mesh.shaderFlags1);\n    }"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1170_OLD_DRAW [==[
    glDrawArrays(GL_TRIANGLES, 0, object.vertexCount);

    if (object.stencilDrawModePresent) {
]==])
set(Q1170_NEW_DRAW [==[
    const GLboolean q1170OffsetWasEnabled = glIsEnabled(GL_POLYGON_OFFSET_FILL);
    GLfloat q1170OldFactor = 0.0f, q1170OldUnits = 0.0f;
    if (object.decalQ1170) {
        glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &q1170OldFactor);
        glGetFloatv(GL_POLYGON_OFFSET_UNITS, &q1170OldUnits);
        glEnable(GL_POLYGON_OFFSET_FILL);
        // Standard (non-reversed) depth: negative offset pulls the decal toward
        // the viewer, matching the intent of Gamebryo's decal render state.
        glPolygonOffset(-0.65f, -1.0f);
    }

    glDrawArrays(GL_TRIANGLES, 0, object.vertexCount);

    if (object.decalQ1170) {
        glPolygonOffset(q1170OldFactor, q1170OldUnits);
        if (q1170OffsetWasEnabled) glEnable(GL_POLYGON_OFFSET_FILL);
        else glDisable(GL_POLYGON_OFFSET_FILL);
    }

    if (object.stencilDrawModePresent) {
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1170_OLD_DRAW}" Q1170_DRAW_POS)
if(Q1170_DRAW_POS EQUAL -1)
    message(FATAL_ERROR "Q11.7 could not find Q11.6 static draw point")
endif()
string(REPLACE "${Q1170_OLD_DRAW}" "${Q1170_NEW_DRAW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(FIND "${Q6H_NATIVE_SOURCE}" "Q11.7 DECAL BIAS" Q1170_DECAL_LOG_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "glPolygonOffset(-0.65f, -1.0f)" Q1170_OFFSET_OK)
if(Q1170_DECAL_LOG_OK EQUAL -1 OR Q1170_OFFSET_OK EQUAL -1)
    message(FATAL_ERROR "Q11.7 decal verification failed")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q11.7 FO3 legacy NiTexturingProperty + decal depth bias enabled")
