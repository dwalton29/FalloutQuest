# Q11.5: preserve the blend functions authored in Fallout 3 NiAlphaProperty.
#
# NiAlphaProperty flags are a packed Gamebryo render state:
#   bit 0     blend enable
#   bits 1-4  source blend function
#   bits 5-8  destination blend function
#   bit 9     alpha test enable
# Q6H previously kept only the enable/test bits and then rendered every blended
# shape as GL_SRC_ALPHA / GL_ONE_MINUS_SRC_ALPHA. That destroys additive glow,
# soft-light and other effect materials even when their DDS and NoLighting shader
# are resolved correctly. Q11.5 carries the authored source/destination factors
# from NIF -> CPU mesh -> GPU object and applies them per draw.

set(Q1150_NIF_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/fo3-static-nif-q6h.cpp")
if(NOT EXISTS "${Q1150_NIF_GENERATED}")
    message(FATAL_ERROR "Q11.5 expected generated NIF source at ${Q1150_NIF_GENERATED}")
endif()
file(READ "${Q1150_NIF_GENERATED}" Q1150_NIF_SOURCE)

set(Q1150_OLD_ALPHA_PARSE [==[
bool ParseAlphaProperty(const uint8_t* data, size_t size,
                        bool& alphaBlend, bool& alphaTest, float& alphaThreshold) {
    Cursor c(data, size);
    if (!ParseObjectNetPrefix(c)) return false;
    uint16_t flags = 0;
    uint8_t threshold = 0;
    if (!c.U16(flags) || !c.U8(threshold)) return false;
    alphaBlend = (flags & 0x0001u) != 0u;
    alphaTest = (flags & 0x0200u) != 0u;
    alphaThreshold = static_cast<float>(threshold) / 255.0f;
    return c.remaining() == 0u;
}
]==])
set(Q1150_NEW_ALPHA_PARSE [==[
bool ParseAlphaProperty(const uint8_t* data, size_t size,
                        bool& alphaBlend, bool& alphaTest, float& alphaThreshold,
                        uint8_t& alphaSourceBlend, uint8_t& alphaDestBlend) {
    Cursor c(data, size);
    if (!ParseObjectNetPrefix(c)) return false;
    uint16_t flags = 0;
    uint8_t threshold = 0;
    if (!c.U16(flags) || !c.U8(threshold)) return false;
    alphaBlend = (flags & 0x0001u) != 0u;
    alphaSourceBlend = static_cast<uint8_t>((flags >> 1u) & 0x0fu);
    alphaDestBlend = static_cast<uint8_t>((flags >> 5u) & 0x0fu);
    alphaTest = (flags & 0x0200u) != 0u;
    alphaThreshold = static_cast<float>(threshold) / 255.0f;
    return c.remaining() == 0u;
}
]==])
string(FIND "${Q1150_NIF_SOURCE}" "${Q1150_OLD_ALPHA_PARSE}" Q1150_PARSE_POS)
if(Q1150_PARSE_POS EQUAL -1)
    message(FATAL_ERROR "Q11.5 could not find NiAlphaProperty parser")
endif()
string(REPLACE "${Q1150_OLD_ALPHA_PARSE}" "${Q1150_NEW_ALPHA_PARSE}"
       Q1150_NIF_SOURCE "${Q1150_NIF_SOURCE}")

set(Q1150_OLD_ALPHA_CALL [==[
            ParseAlphaProperty(prop, header.blockSizes[ref],
                               candidate.alphaBlend, candidate.alphaTest,
                               candidate.alphaThreshold);
]==])
set(Q1150_NEW_ALPHA_CALL [==[
            ParseAlphaProperty(prop, header.blockSizes[ref],
                               candidate.alphaBlend, candidate.alphaTest,
                               candidate.alphaThreshold,
                               candidate.alphaSourceBlend, candidate.alphaDestBlend);
]==])
string(FIND "${Q1150_NIF_SOURCE}" "${Q1150_OLD_ALPHA_CALL}" Q1150_CALL_POS)
if(Q1150_CALL_POS EQUAL -1)
    message(FATAL_ERROR "Q11.5 could not find NiAlphaProperty material call")
endif()
string(REPLACE "${Q1150_OLD_ALPHA_CALL}" "${Q1150_NEW_ALPHA_CALL}"
       Q1150_NIF_SOURCE "${Q1150_NIF_SOURCE}")
file(WRITE "${Q1150_NIF_GENERATED}" "${Q1150_NIF_SOURCE}")

# GPU object keeps the decoded Gamebryo blend modes. Defaults match the renderer's
# historical conventional-alpha path for meshes that do not carry NiAlphaProperty.
string(REPLACE
    "    bool alphaTest = false;"
    "    bool alphaTest = false;\n    uint8_t alphaSourceBlend = 6u;\n    uint8_t alphaDestBlend = 7u;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "    gpu.alphaTest = cpu.mesh.alphaTest;"
    "    gpu.alphaTest = cpu.mesh.alphaTest;\n    gpu.alphaSourceBlend = cpu.mesh.alphaSourceBlend;\n    gpu.alphaDestBlend = cpu.mesh.alphaDestBlend;\n    if (gpu.alphaBlend) {\n        Q6H_LOGI(\"Q11.5 BLEND STATE: ref=%08X model=%s src=%u dst=%u noLighting=%d\",\n                 gpu.refFormId, cpu.placement.modelPath.c_str(),\n                 static_cast<unsigned>(gpu.alphaSourceBlend),\n                 static_cast<unsigned>(gpu.alphaDestBlend),\n                 cpu.mesh.noLighting ? 1 : 0);\n    }"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1150_DRAW_MARKER [==[
void DrawSceneObject(const GpuObject& object) {
]==])
set(Q1150_DRAW_REPLACEMENT [==[
GLenum Q1150BlendFactor(uint8_t mode, bool source) {
    switch (mode) {
        case 0u: return GL_ONE;
        case 1u: return GL_ZERO;
        case 2u: return GL_SRC_COLOR;
        case 3u: return GL_ONE_MINUS_SRC_COLOR;
        case 4u: return GL_DST_COLOR;
        case 5u: return GL_ONE_MINUS_DST_COLOR;
        case 6u: return GL_SRC_ALPHA;
        case 7u: return GL_ONE_MINUS_SRC_ALPHA;
        case 8u: return GL_DST_ALPHA;
        case 9u: return GL_ONE_MINUS_DST_ALPHA;
        case 10u: return GL_SRC_ALPHA_SATURATE;
        default: return source ? GL_SRC_ALPHA : GL_ONE_MINUS_SRC_ALPHA;
    }
}

void DrawSceneObject(const GpuObject& object) {
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1150_DRAW_MARKER}" Q1150_DRAW_POS)
if(Q1150_DRAW_POS EQUAL -1)
    message(FATAL_ERROR "Q11.5 could not find DrawSceneObject hook")
endif()
string(REPLACE "${Q1150_DRAW_MARKER}" "${Q1150_DRAW_REPLACEMENT}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1150_OLD_ALPHA_PASS [==[
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    for (const GpuObject& object : gObjects) {
        if (object.alphaBlend) DrawSceneObject(object);
    }
]==])
set(Q1150_NEW_ALPHA_PASS [==[
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    for (const GpuObject& object : gObjects) {
        if (!object.alphaBlend) continue;
        glBlendFunc(Q1150BlendFactor(object.alphaSourceBlend, true),
                    Q1150BlendFactor(object.alphaDestBlend, false));
        DrawSceneObject(object);
    }
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1150_OLD_ALPHA_PASS}" Q1150_PASS_POS)
if(Q1150_PASS_POS EQUAL -1)
    message(FATAL_ERROR "Q11.5 could not find hardcoded alpha-blend pass")
endif()
string(REPLACE "${Q1150_OLD_ALPHA_PASS}" "${Q1150_NEW_ALPHA_PASS}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(FIND "${Q6H_NATIVE_SOURCE}" "Q11.5 BLEND STATE" Q1150_LOG_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q1150BlendFactor(object.alphaSourceBlend" Q1150_DRAW_OK)
if(Q1150_LOG_OK EQUAL -1 OR Q1150_DRAW_OK EQUAL -1)
    message(FATAL_ERROR "Q11.5 authored alpha blend verification failed")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q11.5 authored NiAlphaProperty blend functions enabled")
