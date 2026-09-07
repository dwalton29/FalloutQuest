# Q10.6 LAND VCLR fidelity repair. Visual-only; no collision/movement changes.

set(Q1060_VCLR_HELPERS [=[
std::unordered_map<uint32_t, std::vector<float>> gLandVertexColorsQ1060;

bool DecodeVclrQ1060(const uint8_t* bytes, uint32_t size, std::vector<float>& colors) {
    constexpr size_t vertexCount = 33u * 33u;
    constexpr uint32_t expected = static_cast<uint32_t>(vertexCount * 3u);
    if (!bytes || size < expected) return false;
    colors.resize(vertexCount * 3u);
    for (size_t i = 0u; i < vertexCount; ++i) {
        colors[i * 3u + 0u] = static_cast<float>(bytes[i * 3u + 0u]) / 255.0f;
        colors[i * 3u + 1u] = static_cast<float>(bytes[i * 3u + 1u]) / 255.0f;
        colors[i * 3u + 2u] = static_cast<float>(bytes[i * 3u + 2u]) / 255.0f;
    }
    return true;
}

const std::vector<float>& GetFo3TerrainVertexColorsQ1060(uint32_t landFormId) {
    static const std::vector<float> empty;
    const auto found = gLandVertexColorsQ1060.find(landFormId);
    return found == gLandVertexColorsQ1060.end() ? empty : found->second;
}

]=])
string(REPLACE
    "bool DecodeVnmlQ1010(const uint8_t* bytes, uint32_t size, std::vector<float>& normals) {"
    "${Q1060_VCLR_HELPERS}bool DecodeVnmlQ1010(const uint8_t* bytes, uint32_t size, std::vector<float>& normals) {"
    Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
string(REPLACE
    "        bool haveVhgt = false;\n        bool haveVnml = false;"
    "        bool haveVhgt = false;\n        bool haveVnml = false;\n        bool haveVclr = false;\n        std::vector<float> vertexColorsQ1060;"
    Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
set(Q1060_OLD_LAND_SUBRECORDS [=[
            if (std::memcmp(type, "VHGT", 4u) == 0 && !haveVhgt) {
                haveVhgt = DecodeVhgtQ76(bytes, subSize, terrain.heights);
            } else if (std::memcmp(type, "VNML", 4u) == 0 && !haveVnml) {
                haveVnml = DecodeVnmlQ1010(bytes, subSize, terrain.normals);
            }
]=])
set(Q1060_NEW_LAND_SUBRECORDS [=[
            if (std::memcmp(type, "VHGT", 4u) == 0 && !haveVhgt) {
                haveVhgt = DecodeVhgtQ76(bytes, subSize, terrain.heights);
            } else if (std::memcmp(type, "VNML", 4u) == 0 && !haveVnml) {
                haveVnml = DecodeVnmlQ1010(bytes, subSize, terrain.normals);
            } else if (std::memcmp(type, "VCLR", 4u) == 0 && !haveVclr) {
                haveVclr = DecodeVclrQ1060(bytes, subSize, vertexColorsQ1060);
            }
]=])
string(REPLACE "${Q1060_OLD_LAND_SUBRECORDS}" "${Q1060_NEW_LAND_SUBRECORDS}"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
string(REPLACE
    "        if (!haveVhgt) {"
    "        if (haveVclr) gLandVertexColorsQ1060[formId] = std::move(vertexColorsQ1060);\n        if (!haveVhgt) {"
    Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
string(REPLACE
    "    gTerrainDataQ76.clear();\n    gTerrainForceHideQ720.clear();"
    "    gTerrainDataQ76.clear();\n    gTerrainForceHideQ720.clear();\n    gLandVertexColorsQ1060.clear();"
    Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-data-q720.cpp"
     "${Q720_TERRAIN_DATA_SOURCE}")

# Put the authored TX01 resolver after the established Q7.18 diffuse resolver.
string(REPLACE
    "#include \"fo3-terrain-texture-q711.cpp\""
    "#include \"fo3-terrain-texture-q711.cpp\"\n#include \"fo3-terrain-material-q1060.cpp\""
    Q720_CELL_SOURCE_TEXT "${Q720_CELL_SOURCE_TEXT}")

# Extend Q10.1's VNML vertex path with the LAND VCLR RGB triplet.
string(REPLACE
    "                      Q76BV3 authoredC = {}) {"
    "                      Q76BV3 authoredC = {},\n                      Q76BV3 colorA = {1.0f, 1.0f, 1.0f},\n                      Q76BV3 colorB = {1.0f, 1.0f, 1.0f},\n                      Q76BV3 colorC = {1.0f, 1.0f, 1.0f}) {"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    const Q76BV3 authoredNormals[3]{authoredA, authoredB, authoredC};"
    "    const Q76BV3 authoredNormals[3]{authoredA, authoredB, authoredC};\n    const Q76BV3 authoredColors[3]{colorA, colorB, colorC};"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "            uv[i * 2], uv[i * 2 + 1],\n            std::clamp(alpha[i], 0.0f, 1.0f)"
    "            uv[i * 2], uv[i * 2 + 1],\n            std::clamp(alpha[i], 0.0f, 1.0f),\n            std::clamp(authoredColors[i].x, 0.0f, 1.0f),\n            std::clamp(authoredColors[i].y, 0.0f, 1.0f),\n            std::clamp(authoredColors[i].z, 0.0f, 1.0f)"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

set(Q1060_COLOR_LAMBDA [=[
        const std::vector<float>& authoredColorsQ1060 = GetFo3TerrainVertexColorsQ1060(cell.landFormId);
        const bool hasAuthoredColorsQ1060 = authoredColorsQ1060.size() == Q76B_HEIGHT_COUNT * 3u;
        auto authoredColorQ1060 = [&](int x, int y) -> Q76BV3 {
            if (!hasAuthoredColorsQ1060) return {1.0f, 1.0f, 1.0f};
            const size_t index = static_cast<size_t>(y * Q76B_LAND_SIDE + x) * 3u;
            return {authoredColorsQ1060[index + 0u],
                    authoredColorsQ1060[index + 1u],
                    authoredColorsQ1060[index + 2u]};
        };
]=])
string(REPLACE
    "        std::string quadrantTextures[4];"
    "${Q1060_COLOR_LAMBDA}\n        std::string quadrantTextures[4];"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

string(REPLACE
    "Q718PushTriangle(batch.vertices, p00, u0, v0, p10, u1, v0, p11, u1, v1, 1.0f, 1.0f, 1.0f, authoredNormal(x, y), authoredNormal(x + 1, y), authoredNormal(x + 1, y + 1));"
    "Q718PushTriangle(batch.vertices, p00, u0, v0, p10, u1, v0, p11, u1, v1, 1.0f, 1.0f, 1.0f, authoredNormal(x, y), authoredNormal(x + 1, y), authoredNormal(x + 1, y + 1), authoredColorQ1060(x, y), authoredColorQ1060(x + 1, y), authoredColorQ1060(x + 1, y + 1));"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "Q718PushTriangle(batch.vertices, p00, u0, v0, p11, u1, v1, p01, u0, v1, 1.0f, 1.0f, 1.0f, authoredNormal(x, y), authoredNormal(x + 1, y + 1), authoredNormal(x, y + 1));"
    "Q718PushTriangle(batch.vertices, p00, u0, v0, p11, u1, v1, p01, u0, v1, 1.0f, 1.0f, 1.0f, authoredNormal(x, y), authoredNormal(x + 1, y + 1), authoredNormal(x, y + 1), authoredColorQ1060(x, y), authoredColorQ1060(x + 1, y + 1), authoredColorQ1060(x, y + 1));"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
set(Q1060_OLD_ALPHA_A [=[
                    Q718PushTriangle(batch.vertices,
                                     p00, u0, v0, p10, u1, v0, p11, u1, v1,
                                     a00, a10, a11,
                                     authoredNormal(x, y), authoredNormal(x + 1, y),
                                     authoredNormal(x + 1, y + 1));
]=])
set(Q1060_NEW_ALPHA_A [=[
                    Q718PushTriangle(batch.vertices,
                                     p00, u0, v0, p10, u1, v0, p11, u1, v1,
                                     a00, a10, a11,
                                     authoredNormal(x, y), authoredNormal(x + 1, y),
                                     authoredNormal(x + 1, y + 1),
                                     authoredColorQ1060(x, y), authoredColorQ1060(x + 1, y),
                                     authoredColorQ1060(x + 1, y + 1));
]=])
string(REPLACE "${Q1060_OLD_ALPHA_A}" "${Q1060_NEW_ALPHA_A}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
set(Q1060_OLD_ALPHA_B [=[
                    Q718PushTriangle(batch.vertices,
                                     p00, u0, v0, p11, u1, v1, p01, u0, v1,
                                     a00, a11, a01,
                                     authoredNormal(x, y), authoredNormal(x + 1, y + 1),
                                     authoredNormal(x, y + 1));
]=])
set(Q1060_NEW_ALPHA_B [=[
                    Q718PushTriangle(batch.vertices,
                                     p00, u0, v0, p11, u1, v1, p01, u0, v1,
                                     a00, a11, a01,
                                     authoredNormal(x, y), authoredNormal(x + 1, y + 1),
                                     authoredNormal(x, y + 1),
                                     authoredColorQ1060(x, y), authoredColorQ1060(x + 1, y + 1),
                                     authoredColorQ1060(x, y + 1));
]=])
string(REPLACE "${Q1060_OLD_ALPHA_B}" "${Q1060_NEW_ALPHA_B}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
