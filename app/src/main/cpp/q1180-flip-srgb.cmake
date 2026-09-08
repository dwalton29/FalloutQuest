# Q11.8: controller-driven legacy textures + correct sRGB albedo sampling.
#
# V11.7 produced no visual change for FOOD or the washed-out Atom material.
# The remaining concrete gaps are:
#   1) NiTexturingProperty may deliberately have no static base texture because
#      NiFlipController supplies that texture at runtime. Q6H discards all
#      property controller refs, leaving those surfaces on the white fallback.
#   2) OpenXR already prefers a GL_SRGB8_ALPHA8 swapchain, but static diffuse and
#      LAND albedo textures are uploaded as linear GL_RGBA8. That makes authored
#      Fallout 3 colour textures too bright/washed out when lit.
#
# Q11.8 resolves the first authored NiFlipController source for a texture slot
# (static first-frame support; animation can follow later) and uploads only colour
# albedo/diffuse textures as sRGB. Normal/gloss maps remain linear.

set(Q1180_NIF_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/fo3-static-nif-q6h.cpp")
if(NOT EXISTS "${Q1180_NIF_GENERATED}")
    message(FATAL_ERROR "Q11.8 expected generated NIF source at ${Q1180_NIF_GENERATED}")
endif()
file(READ "${Q1180_NIF_GENERATED}" Q1180_NIF_SOURCE)

# Insert after Q11.7's NiSourceTexture resolver, before the legacy texturing parser.
set(Q1180_CONTROLLER_HELPERS [==[
bool PropertyControllerRefQ1180(const uint8_t* data, size_t size,
                                uint32_t& controllerRef) {
    Cursor c(data, size);
    uint32_t nameIndex = INVALID_REF;
    uint32_t numExtraData = 0u;
    controllerRef = INVALID_REF;
    if (!c.U32(nameIndex) || !c.U32(numExtraData) || numExtraData > MAX_BLOCKS) return false;
    if (!c.Skip(static_cast<size_t>(numExtraData) * 4u)) return false;
    return c.U32(controllerRef);
}

bool ResolveFlipControllerTextureQ1180(const std::vector<uint8_t>& nif,
                                       const NifHeader& header,
                                       uint32_t controllerRef,
                                       uint32_t wantedSlot,
                                       std::string& outPath,
                                       uint32_t& outControllerBlock,
                                       uint32_t& outSourceBlock) {
    outPath.clear();
    outControllerBlock = INVALID_REF;
    outSourceBlock = INVALID_REF;

    uint32_t current = controllerRef;
    for (uint32_t hop = 0u; hop < 16u && current != INVALID_REF && current < header.numBlocks; ++hop) {
        const std::string type = BlockType(header, current);
        const uint8_t* data = BlockData(nif, header, current);
        if (!data) return false;
        Cursor c(data, header.blockSizes[current]);

        // NiTimeController prefix in Fallout 3 / NIF 20.2.0.7.
        uint32_t next = INVALID_REF;
        uint16_t flags = 0u;
        float frequency = 1.0f, phase = 0.0f, start = 0.0f, stop = 0.0f;
        uint32_t target = INVALID_REF;
        if (!c.U32(next) || !c.U16(flags) ||
            !c.F32(frequency) || !c.F32(phase) ||
            !c.F32(start) || !c.F32(stop) || !c.U32(target)) {
            return false;
        }

        if (type == "NiFlipController") {
            // NiFlipController -> NiFloatInterpController -> NiSingleInterpController.
            // 20.2.0.7 stores one interpolator ref, then texture slot, then a
            // record list of NiSourceTexture refs. (Delta only existed <=10.1.0.103.)
            uint32_t interpolator = INVALID_REF;
            uint32_t textureSlot = INVALID_REF;
            uint32_t sourceCount = 0u;
            if (!c.U32(interpolator) || !c.U32(textureSlot) ||
                !c.U32(sourceCount) || sourceCount > MAX_BLOCKS) return false;

            for (uint32_t i = 0u; i < sourceCount; ++i) {
                uint32_t sourceRef = INVALID_REF;
                if (!c.U32(sourceRef)) return false;
                if (textureSlot != wantedSlot) continue;
                std::string sourcePath;
                if (ParseSourceTexturePathQ1170(nif, header, sourceRef, sourcePath)) {
                    outPath = std::move(sourcePath);
                    outControllerBlock = current;
                    outSourceBlock = sourceRef;
                    return true;
                }
            }
        }

        current = next;
    }
    return false;
}

]==])
set(Q1180_INSERT_MARKER [==[
bool ParseLegacyTexturingPropertyQ1170(const uint8_t* data, size_t size,
]==])
string(FIND "${Q1180_NIF_SOURCE}" "${Q1180_INSERT_MARKER}" Q1180_INSERT_POS)
if(Q1180_INSERT_POS EQUAL -1)
    message(FATAL_ERROR "Q11.8 could not find Q11.7 legacy texture parser insertion point")
endif()
string(REPLACE "${Q1180_INSERT_MARKER}"
       "${Q1180_CONTROLLER_HELPERS}${Q1180_INSERT_MARKER}"
       Q1180_NIF_SOURCE "${Q1180_NIF_SOURCE}")

# After Q11.7 resolves static Base/Glow slots, let a NiFlipController override
# the matching slot with its first authored source. This mirrors the NIF's
# controller ownership instead of inventing a fallback texture.
set(Q1180_OLD_LEGACY_TAIL [==[
                if (haveBaseQ1170 || haveGlowQ1170) {
                    Q6H_LOGI("Q11.7 LEGACY TEX: shape=%u baseRef=%u glowRef=%u diffuse=%s glow=%s",
                             shape.block, baseSourceRefQ1170, glowSourceRefQ1170,
                             haveBaseQ1170 ? basePathQ1170.c_str() : "<none>",
                             haveGlowQ1170 ? glowPathQ1170.c_str() : "<none>");
                }
]==])
set(Q1180_NEW_LEGACY_TAIL [==[
                if (haveBaseQ1170 || haveGlowQ1170) {
                    Q6H_LOGI("Q11.7 LEGACY TEX: shape=%u baseRef=%u glowRef=%u diffuse=%s glow=%s",
                             shape.block, baseSourceRefQ1170, glowSourceRefQ1170,
                             haveBaseQ1170 ? basePathQ1170.c_str() : "<none>",
                             haveGlowQ1170 ? glowPathQ1170.c_str() : "<none>");
                }

                uint32_t controllerRefQ1180 = INVALID_REF;
                if (PropertyControllerRefQ1180(prop, header.blockSizes[ref], controllerRefQ1180) &&
                    controllerRefQ1180 != INVALID_REF) {
                    std::string flipBaseQ1180, flipGlowQ1180;
                    uint32_t flipControllerBaseQ1180 = INVALID_REF, flipSourceBaseQ1180 = INVALID_REF;
                    uint32_t flipControllerGlowQ1180 = INVALID_REF, flipSourceGlowQ1180 = INVALID_REF;
                    const bool haveFlipBaseQ1180 = ResolveFlipControllerTextureQ1180(
                        nif, header, controllerRefQ1180, 0u, flipBaseQ1180,
                        flipControllerBaseQ1180, flipSourceBaseQ1180);
                    const bool haveFlipGlowQ1180 = ResolveFlipControllerTextureQ1180(
                        nif, header, controllerRefQ1180, 4u, flipGlowQ1180,
                        flipControllerGlowQ1180, flipSourceGlowQ1180);
                    if (haveFlipBaseQ1180) candidate.diffuseTexturePath = flipBaseQ1180;
                    if (haveFlipGlowQ1180) candidate.glowTexturePath = flipGlowQ1180;
                    if (haveFlipBaseQ1180 || haveFlipGlowQ1180) {
                        Q6H_LOGI("Q11.8 FLIP TEX: shape=%u property=%u controller=%u baseSource=%u glowSource=%u diffuse=%s glow=%s",
                                 shape.block, ref,
                                 haveFlipBaseQ1180 ? flipControllerBaseQ1180 : flipControllerGlowQ1180,
                                 flipSourceBaseQ1180, flipSourceGlowQ1180,
                                 haveFlipBaseQ1180 ? flipBaseQ1180.c_str() : "<none>",
                                 haveFlipGlowQ1180 ? flipGlowQ1180.c_str() : "<none>");
                    }
                }
]==])
string(FIND "${Q1180_NIF_SOURCE}" "${Q1180_OLD_LEGACY_TAIL}" Q1180_LEGACY_TAIL_POS)
if(Q1180_LEGACY_TAIL_POS EQUAL -1)
    message(FATAL_ERROR "Q11.8 could not find Q11.7 legacy texture tail")
endif()
string(REPLACE "${Q1180_OLD_LEGACY_TAIL}" "${Q1180_NEW_LEGACY_TAIL}"
       Q1180_NIF_SOURCE "${Q1180_NIF_SOURCE}")

string(FIND "${Q1180_NIF_SOURCE}" "Q11.8 FLIP TEX" Q1180_FLIP_LOG_OK)
string(FIND "${Q1180_NIF_SOURCE}" "ResolveFlipControllerTextureQ1180" Q1180_FLIP_FN_OK)
if(Q1180_FLIP_LOG_OK EQUAL -1 OR Q1180_FLIP_FN_OK EQUAL -1)
    message(FATAL_ERROR "Q11.8 NiFlipController verification failed")
endif()
file(WRITE "${Q1180_NIF_GENERATED}" "${Q1180_NIF_SOURCE}")

# -----------------------------------------------------------------------------
# Static NIF diffuse colour textures: sample as sRGB. Normal/gloss + glow-mask
# uploads stay linear because their channels are shader data rather than albedo.
# The OpenXR runtime already selects GL_SRGB8_ALPHA8 for the eye swapchains.
# -----------------------------------------------------------------------------
set(Q1180_OLD_STATIC_UPLOAD [==[
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture.width, texture.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, texture.rgba.data());
]==])
set(Q1180_NEW_STATIC_UPLOAD [==[
    const GLenum q1180InternalFormat =
        std::string(label) == "DIFFUSE" ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    glTexImage2D(GL_TEXTURE_2D, 0, q1180InternalFormat, texture.width, texture.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, texture.rgba.data());
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1180_OLD_STATIC_UPLOAD}" Q1180_STATIC_UPLOAD_POS)
if(Q1180_STATIC_UPLOAD_POS EQUAL -1)
    message(FATAL_ERROR "Q11.8 could not find static texture upload")
endif()
string(REPLACE "${Q1180_OLD_STATIC_UPLOAD}" "${Q1180_NEW_STATIC_UPLOAD}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(FIND "${Q6H_NATIVE_SOURCE}" "q1180InternalFormat" Q1180_STATIC_SRGB_OK)
if(Q1180_STATIC_SRGB_OK EQUAL -1)
    message(FATAL_ERROR "Q11.8 static sRGB verification failed")
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")

# LAND albedo textures are colour data too. Keep the entire terrain shader in
# linear space by letting GLES decode its authored sRGB DDS samples.
set(Q1180_OLD_TERRAIN_UPLOAD [==[
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 image.width, image.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
]==])
set(Q1180_NEW_TERRAIN_UPLOAD [==[
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8,
                 image.width, image.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
]==])
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "${Q1180_OLD_TERRAIN_UPLOAD}" Q1180_TERRAIN_UPLOAD_POS)
if(Q1180_TERRAIN_UPLOAD_POS EQUAL -1)
    message(FATAL_ERROR "Q11.8 could not find LAND colour texture upload")
endif()
string(REPLACE "${Q1180_OLD_TERRAIN_UPLOAD}" "${Q1180_NEW_TERRAIN_UPLOAD}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")

message(STATUS "Q11.8 NiFlipController first-frame textures + sRGB albedo enabled")
