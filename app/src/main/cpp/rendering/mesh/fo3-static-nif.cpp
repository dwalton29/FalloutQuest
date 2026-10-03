#include "fo3-static-nif.h"
#include "fo3-bsa-reader.h"

#include <android/log.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr const char* TAG = "FalloutQuest";
constexpr uint32_t NIF_VERSION_FO3 = 0x14020007u;
constexpr uint32_t MAX_NIF_BYTES = 128u * 1024u * 1024u;
constexpr uint32_t MAX_BLOCKS = 100000u;
constexpr uint32_t MAX_STRINGS = 100000u;
constexpr uint32_t INVALID_REF = 0xffffffffu;

#define Q6H_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q6H_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define Q6H_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

uint16_t ReadLe16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8);
}

uint32_t ReadLe32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

float ReadLeFloat(const uint8_t* p) {
    const uint32_t bits = ReadLe32(p);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

class Cursor {
public:
    Cursor(const uint8_t* data, size_t size) : data_(data), size_(size) {}
    explicit Cursor(const std::vector<uint8_t>& data) : Cursor(data.data(), data.size()) {}

    size_t remaining() const { return size_ - pos_; }
    size_t offset() const { return pos_; }

    bool Skip(size_t count) {
        if (count > remaining()) return false;
        pos_ += count;
        return true;
    }
    bool U8(uint8_t& value) {
        if (remaining() < 1u) return false;
        value = data_[pos_++];
        return true;
    }
    bool U16(uint16_t& value) {
        if (remaining() < 2u) return false;
        value = ReadLe16(data_ + pos_);
        pos_ += 2u;
        return true;
    }
    bool U32(uint32_t& value) {
        if (remaining() < 4u) return false;
        value = ReadLe32(data_ + pos_);
        pos_ += 4u;
        return true;
    }
    bool F32(float& value) {
        if (remaining() < 4u) return false;
        value = ReadLeFloat(data_ + pos_);
        pos_ += 4u;
        return true;
    }
    bool SizedString(std::string& out) {
        uint32_t length = 0;
        if (!U32(length) || length > remaining() || length > MAX_NIF_BYTES) return false;
        out.assign(reinterpret_cast<const char*>(data_ + pos_), length);
        pos_ += length;
        return true;
    }
    bool ExportString(std::string& out) {
        uint8_t length = 0;
        if (!U8(length) || length > remaining()) return false;
        out.assign(reinterpret_cast<const char*>(data_ + pos_), length);
        pos_ += length;
        while (!out.empty() && out.back() == '\0') out.pop_back();
        return true;
    }
    bool HeaderLine(std::string& out) {
        out.clear();
        while (remaining() && out.size() < 256u) {
            uint8_t ch = 0;
            if (!U8(ch)) return false;
            if (ch == '\n') return true;
            if (ch != '\r') out.push_back(static_cast<char>(ch));
        }
        return false;
    }

private:
    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    size_t pos_ = 0;
};

struct NifHeader {
    uint32_t userVersion = 0;
    uint32_t bsVersion = 0;
    uint32_t numBlocks = 0;
    std::vector<std::string> blockTypes;
    std::vector<uint16_t> blockTypeIndices;
    std::vector<uint32_t> blockSizes;
    std::vector<size_t> blockOffsets;
    std::vector<std::string> strings;
    size_t blockDataOffset = 0;
};

struct NifTransform {
    float translation[3]{0.0f, 0.0f, 0.0f};
    float rotation[9]{
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
    };
    float scale = 1.0f;
    bool valid = false;
};

struct ShapeObject {
    uint32_t block = INVALID_REF;
    uint32_t dataRef = INVALID_REF;
    uint32_t skinRef = INVALID_REF;
    NifTransform transform;
    std::vector<uint32_t> properties;
};

bool ParseHeader(const std::vector<uint8_t>& nif, NifHeader& out) {
    Cursor c(nif);
    std::string line;
    if (!c.HeaderLine(line) ||
        line.rfind("Gamebryo File Format, Version 20.2.0.7", 0) != 0) return false;

    uint32_t version = 0;
    uint8_t endian = 0;
    if (!c.U32(version) || !c.U8(endian) || !c.U32(out.userVersion) ||
        !c.U32(out.numBlocks)) return false;
    if (version != NIF_VERSION_FO3 || endian != 1u || out.numBlocks == 0u ||
        out.numBlocks > MAX_BLOCKS) return false;

    std::string author, processScript, exportScript;
    if (!c.U32(out.bsVersion) || !c.ExportString(author) ||
        !c.ExportString(processScript) || !c.ExportString(exportScript)) return false;
    if (out.bsVersion >= 103u) {
        std::string maxFilepath;
        if (!c.ExportString(maxFilepath)) return false;
    }

    uint16_t numBlockTypes = 0;
    if (!c.U16(numBlockTypes) || numBlockTypes == 0u) return false;
    out.blockTypes.reserve(numBlockTypes);
    for (uint16_t i = 0; i < numBlockTypes; ++i) {
        std::string type;
        if (!c.SizedString(type)) return false;
        out.blockTypes.push_back(std::move(type));
    }

    out.blockTypeIndices.resize(out.numBlocks);
    for (uint32_t i = 0; i < out.numBlocks; ++i) {
        if (!c.U16(out.blockTypeIndices[i])) return false;
        if ((out.blockTypeIndices[i] & 0x7fffu) >= out.blockTypes.size()) return false;
    }

    out.blockSizes.resize(out.numBlocks);
    uint64_t totalBytes = 0;
    for (uint32_t i = 0; i < out.numBlocks; ++i) {
        if (!c.U32(out.blockSizes[i])) return false;
        totalBytes += out.blockSizes[i];
        if (totalBytes > nif.size()) return false;
    }

    uint32_t numStrings = 0, maxStringLength = 0;
    if (!c.U32(numStrings) || !c.U32(maxStringLength) ||
        numStrings > MAX_STRINGS || maxStringLength > MAX_NIF_BYTES) return false;
    out.strings.clear();
    out.strings.reserve(numStrings);
    for (uint32_t i = 0; i < numStrings; ++i) {
        std::string value;
        if (!c.SizedString(value)) return false;
        out.strings.push_back(std::move(value));
    }

    uint32_t numGroups = 0;
    if (!c.U32(numGroups) || numGroups > out.numBlocks ||
        !c.Skip(static_cast<size_t>(numGroups) * 4u)) return false;

    out.blockDataOffset = c.offset();
    if (totalBytes > nif.size() - out.blockDataOffset) return false;

    out.blockOffsets.resize(out.numBlocks);
    size_t offset = out.blockDataOffset;
    for (uint32_t i = 0; i < out.numBlocks; ++i) {
        out.blockOffsets[i] = offset;
        offset += out.blockSizes[i];
    }
    return true;
}

const std::string& BlockType(const NifHeader& header, uint32_t block) {
    static const std::string empty;
    if (block >= header.numBlocks) return empty;
    const uint16_t typeIndex = header.blockTypeIndices[block] & 0x7fffu;
    if (typeIndex >= header.blockTypes.size()) return empty;
    return header.blockTypes[typeIndex];
}

const uint8_t* BlockData(const std::vector<uint8_t>& nif,
                         const NifHeader& header, uint32_t block) {
    if (block >= header.numBlocks) return nullptr;
    return nif.data() + header.blockOffsets[block];
}

bool ParseObjectNetPrefix(Cursor& c) {
    uint32_t nameIndex = 0;
    uint32_t numExtraData = 0;
    uint32_t controller = 0;
    return c.U32(nameIndex) && c.U32(numExtraData) && numExtraData <= MAX_BLOCKS &&
           c.Skip(static_cast<size_t>(numExtraData) * 4u) && c.U32(controller);
}

bool ParseAvObjectPrefix(Cursor& c, const NifHeader& header,
                         NifTransform& transform,
                         std::vector<uint32_t>* properties = nullptr) {
    if (!ParseObjectNetPrefix(c)) return false;

    uint16_t flags = 0;
    if (!c.U16(flags)) return false;
    if (header.userVersion >= 11u && header.bsVersion > 26u) {
        uint16_t unknown = 0;
        if (!c.U16(unknown)) return false;
    }

    for (float& value : transform.translation) if (!c.F32(value)) return false;
    for (float& value : transform.rotation) if (!c.F32(value)) return false;
    if (!c.F32(transform.scale)) return false;

    if (header.userVersion <= 11u) {
        uint32_t count = 0;
        if (!c.U32(count) || count > MAX_BLOCKS) return false;
        if (properties) properties->clear();
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t ref = INVALID_REF;
            if (!c.U32(ref)) return false;
            if (properties) properties->push_back(ref);
        }
    }

    uint32_t collisionRef = INVALID_REF;
    if (!c.U32(collisionRef)) return false;
    transform.valid = true;
    return true;
}

bool ParseNode(const uint8_t* data, size_t size, const NifHeader& header,
               NifTransform& transform, std::vector<uint32_t>& children) {
    Cursor c(data, size);
    if (!ParseAvObjectPrefix(c, header, transform)) return false;
    uint32_t childCount = 0;
    if (!c.U32(childCount) || childCount > MAX_BLOCKS) return false;
    children.resize(childCount);
    for (uint32_t& child : children) if (!c.U32(child)) return false;
    uint32_t effectCount = 0;
    if (!c.U32(effectCount) || effectCount > MAX_BLOCKS ||
        !c.Skip(static_cast<size_t>(effectCount) * 4u)) return false;
    return c.remaining() == 0u;
}

bool ParseShapeObject(const uint8_t* data, size_t size, const NifHeader& header,
                      ShapeObject& out) {
    Cursor c(data, size);
    if (!ParseAvObjectPrefix(c, header, out.transform, &out.properties)) return false;

    if (!c.U32(out.dataRef) || !c.U32(out.skinRef)) return false;

    uint32_t numMaterials = 0;
    if (!c.U32(numMaterials) || numMaterials > 4096u) return false;
    if (!c.Skip(static_cast<size_t>(numMaterials) * 4u) ||
        !c.Skip(static_cast<size_t>(numMaterials) * 4u)) return false;

    uint32_t activeMaterial = 0;
    uint8_t dirtyFlag = 0;
    if (!c.U32(activeMaterial) || !c.U8(dirtyFlag)) return false;
    // Derived Bethesda geometry such as BSSegmentedTriShape appends bounded
    // segment metadata after the NiTriShape prefix. The geometry dataRef and
    // material properties above are already complete for rendering/probing.
    return true;
}

bool ParseTextureSet(const uint8_t* data, size_t size,
                     std::string& diffuse, std::string& normal,
                     std::string& glow,
                     std::string& environmentCube,
                     std::string& environmentMask) {
    Cursor c(data, size);
    uint32_t count = 0;
    if (!c.U32(count) || count == 0u || count > 32u) return false;
    for (uint32_t i = 0; i < count; ++i) {
        std::string value;
        if (!c.SizedString(value)) return false;
        if (i == 0u) diffuse = value;
        if (i == 1u) normal = value;
        if (i == 2u) glow = value;
        // Directly observed in supplied Fallout 3 Megaton meshes.
        if (i == 4u) environmentCube = value;
        if (i == 5u) environmentMask = value;
    }
    return !diffuse.empty();
}

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

bool ParseTileShaderProperty(const uint8_t* data, size_t size,
                             const NifHeader& header, std::string& fileName) {
    // TileShaderProperty inherits BSShaderLightingProperty, then stores its
    // texture as a SizedString (not a BSShaderTextureSet reference).
    Cursor c(data, size);
    uint16_t flags = 0;
    uint32_t shaderType = 0, shaderFlags1 = 0, shaderFlags2 = 0, clamp = 0;
    float environmentScale = 1;
    std::string path;
    if (!ParseObjectNetPrefix(c) || !c.U16(flags) || !c.U32(shaderType) ||
        !c.U32(shaderFlags1) || !c.U32(shaderFlags2)) return false;
    if (header.userVersion == 11u && !c.F32(environmentScale)) return false;
    if (header.userVersion <= 11u && !c.U32(clamp)) return false;
    if (!c.SizedString(path) || path.empty() || c.remaining() != 0u) return false;
    fileName = std::move(path);
    return true;
}

bool ParseNoLightingPropertyQ1020(const uint8_t* data, size_t size,
                                  const NifHeader& header,
                                  uint32_t& shaderFlags1, uint32_t& shaderFlags2,
                                  float& environmentMapScale, std::string& fileName,
                                  bool& hasFalloff, float falloffParams[4]) {
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
    if (!c.SizedString(fileName)) return false;
    hasFalloff = header.bsVersion >= 27u;
    if (hasFalloff) {
        for (int i = 0; i < 4; ++i) {
            if (!c.F32(falloffParams[i])) return false;
        }
    }
    return true;
}

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

bool ReadVec3Array(Cursor& c, uint16_t count, std::vector<float>& out) {
    out.clear();
    out.reserve(static_cast<size_t>(count) * 3u);
    for (uint16_t i = 0; i < count; ++i) {
        float x = 0.0f, y = 0.0f, z = 0.0f;
        if (!c.F32(x) || !c.F32(y) || !c.F32(z)) return false;
        out.push_back(x);
        out.push_back(y);
        out.push_back(z);
    }
    return true;
}

bool ParseGeometryPrefix(Cursor& c, Fo3StaticNifMesh& mesh,
                         uint16_t& numVertices, uint16_t& dataFlags) {
    uint32_t groupId = 0;
    uint8_t keepFlags = 0, compressFlags = 0, hasVertices = 0;
    if (!c.U32(groupId) || !c.U16(numVertices) || !c.U8(keepFlags) ||
        !c.U8(compressFlags) || !c.U8(hasVertices) || !hasVertices || numVertices == 0u) {
        return false;
    }
    if (!ReadVec3Array(c, numVertices, mesh.positions)) return false;

    uint8_t hasNormals = 0;
    if (!c.U16(dataFlags) || !c.U8(hasNormals)) return false;
    if (hasNormals) {
        if (!ReadVec3Array(c, numVertices, mesh.normals)) return false;
        if ((dataFlags & 0x1000u) != 0u) {
            if (!ReadVec3Array(c, numVertices, mesh.tangents) ||
                !ReadVec3Array(c, numVertices, mesh.bitangents)) return false;
        }
    }

    if (!c.Skip(16u)) return false;

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

    const uint16_t uvSets = (dataFlags & 0x0001u) != 0u ? 1u : 0u;
    mesh.texcoords.clear();
    if (uvSets > 0u) {
        mesh.texcoords.reserve(static_cast<size_t>(numVertices) * 2u);
        for (uint16_t set = 0; set < uvSets; ++set) {
            for (uint16_t i = 0; i < numVertices; ++i) {
                float u = 0.0f, v = 0.0f;
                if (!c.F32(u) || !c.F32(v)) return false;
                if (set == 0u) {
                    mesh.texcoords.push_back(u);
                    mesh.texcoords.push_back(v);
                }
            }
        }
    }

    return c.Skip(2u) && c.Skip(4u);
}

bool ParseTriStripsData(const uint8_t* data, size_t size, Fo3StaticNifMesh& mesh) {
    Cursor c(data, size);
    uint16_t numVertices = 0, dataFlags = 0;
    if (!ParseGeometryPrefix(c, mesh, numVertices, dataFlags)) return false;

    uint16_t declaredTriangles = 0, numStrips = 0;
    if (!c.U16(declaredTriangles) || !c.U16(numStrips) || numStrips == 0u) return false;

    std::vector<uint16_t> lengths(numStrips);
    for (uint16_t& length : lengths) if (!c.U16(length)) return false;

    uint8_t hasPoints = 0;
    if (!c.U8(hasPoints) || !hasPoints) return false;

    mesh.indices.clear();
    uint32_t rawTriangles = 0;
    for (uint16_t strip = 0; strip < numStrips; ++strip) {
        const uint16_t length = lengths[strip];
        if (length >= 2u) rawTriangles += length - 2u;
        std::vector<uint16_t> points(length);
        for (uint16_t& point : points) {
            if (!c.U16(point) || point >= numVertices) return false;
        }
        for (uint16_t i = 2; i < length; ++i) {
            uint16_t a = points[i - 2u];
            uint16_t b = points[i - 1u];
            const uint16_t d = points[i];
            if ((i & 1u) != 0u) std::swap(a, b);
            if (a == b || b == d || a == d) continue;
            mesh.indices.push_back(a);
            mesh.indices.push_back(b);
            mesh.indices.push_back(d);
        }
    }

    const uint32_t drawable = static_cast<uint32_t>(mesh.indices.size() / 3u);
    Q6H_LOGI("Q6H NIF STRIPS: declared=%u raw=%u drawable=%u degenerates=%u",
             declaredTriangles, rawTriangles, drawable,
             rawTriangles > drawable ? rawTriangles - drawable : 0u);
    return !mesh.indices.empty() && c.remaining() == 0u;
}

bool ParseTriShapeData(const uint8_t* data, size_t size, Fo3StaticNifMesh& mesh) {
    Cursor c(data, size);
    uint16_t numVertices = 0, dataFlags = 0;
    if (!ParseGeometryPrefix(c, mesh, numVertices, dataFlags)) return false;

    uint16_t numTriangles = 0;
    uint32_t numTrianglePoints = 0;
    uint8_t hasTriangles = 0;
    if (!c.U16(numTriangles) || !c.U32(numTrianglePoints) || !c.U8(hasTriangles) ||
        !hasTriangles || numTriangles == 0u) return false;
    if (numTrianglePoints < static_cast<uint32_t>(numTriangles) * 3u) return false;

    mesh.indices.clear();
    mesh.indices.reserve(static_cast<size_t>(numTriangles) * 3u);
    for (uint16_t i = 0; i < numTriangles; ++i) {
        uint16_t a = 0, b = 0, d = 0;
        if (!c.U16(a) || !c.U16(b) || !c.U16(d) ||
            a >= numVertices || b >= numVertices || d >= numVertices) return false;
        if (a == b || b == d || a == d) continue;
        mesh.indices.push_back(a);
        mesh.indices.push_back(b);
        mesh.indices.push_back(d);
    }

    const uint32_t consumedPoints = static_cast<uint32_t>(numTriangles) * 3u;
    if (numTrianglePoints > consumedPoints &&
        !c.Skip(static_cast<size_t>(numTrianglePoints - consumedPoints) * 2u)) return false;

    Q6H_LOGI("Q6H NIF TRISHAPE: declared=%u drawable=%zu",
             numTriangles, mesh.indices.size() / 3u);
    return !mesh.indices.empty();
}

void Normalize3(float& x, float& y, float& z) {
    const float len = std::sqrt(x * x + y * y + z * z);
    if (len < 1e-8f) {
        x = 0.0f; y = 0.0f; z = 1.0f;
        return;
    }
    x /= len; y /= len; z /= len;
}

void EnsureNormals(Fo3StaticNifMesh& mesh) {
    const size_t vertexCount = mesh.positions.size() / 3u;
    if (mesh.normals.size() == vertexCount * 3u) return;
    mesh.normals.assign(vertexCount * 3u, 0.0f);

    for (size_t i = 0; i + 2u < mesh.indices.size(); i += 3u) {
        const uint32_t ia = mesh.indices[i], ib = mesh.indices[i + 1u], ic = mesh.indices[i + 2u];
        const float ax = mesh.positions[ia * 3u], ay = mesh.positions[ia * 3u + 1u], az = mesh.positions[ia * 3u + 2u];
        const float bx = mesh.positions[ib * 3u], by = mesh.positions[ib * 3u + 1u], bz = mesh.positions[ib * 3u + 2u];
        const float cx = mesh.positions[ic * 3u], cy = mesh.positions[ic * 3u + 1u], cz = mesh.positions[ic * 3u + 2u];
        const float ux = bx - ax, uy = by - ay, uz = bz - az;
        const float vx = cx - ax, vy = cy - ay, vz = cz - az;
        const float nx = uy * vz - uz * vy;
        const float ny = uz * vx - ux * vz;
        const float nz = ux * vy - uy * vx;
        for (uint32_t index : {ia, ib, ic}) {
            mesh.normals[index * 3u] += nx;
            mesh.normals[index * 3u + 1u] += ny;
            mesh.normals[index * 3u + 2u] += nz;
        }
    }
    for (size_t i = 0; i < vertexCount; ++i) {
        Normalize3(mesh.normals[i * 3u], mesh.normals[i * 3u + 1u], mesh.normals[i * 3u + 2u]);
    }
}

void EnsureTangentBasis(Fo3StaticNifMesh& mesh) {
    const size_t vertexCount = mesh.positions.size() / 3u;
    if (mesh.tangents.size() == vertexCount * 3u &&
        mesh.bitangents.size() == vertexCount * 3u) return;

    mesh.tangents.assign(vertexCount * 3u, 0.0f);
    mesh.bitangents.assign(vertexCount * 3u, 0.0f);

    if (mesh.texcoords.size() == vertexCount * 2u) {
        for (size_t i = 0; i + 2u < mesh.indices.size(); i += 3u) {
            const uint32_t i0 = mesh.indices[i], i1 = mesh.indices[i + 1u], i2 = mesh.indices[i + 2u];
            const float* p0 = &mesh.positions[i0 * 3u];
            const float* p1 = &mesh.positions[i1 * 3u];
            const float* p2 = &mesh.positions[i2 * 3u];
            const float* w0 = &mesh.texcoords[i0 * 2u];
            const float* w1 = &mesh.texcoords[i1 * 2u];
            const float* w2 = &mesh.texcoords[i2 * 2u];

            const float x1 = p1[0] - p0[0], x2 = p2[0] - p0[0];
            const float y1 = p1[1] - p0[1], y2 = p2[1] - p0[1];
            const float z1 = p1[2] - p0[2], z2 = p2[2] - p0[2];
            const float s1 = w1[0] - w0[0], s2 = w2[0] - w0[0];
            const float t1 = w1[1] - w0[1], t2 = w2[1] - w0[1];
            const float denom = s1 * t2 - s2 * t1;
            if (std::fabs(denom) < 1e-8f) continue;
            const float r = 1.0f / denom;
            const float tx = (t2 * x1 - t1 * x2) * r;
            const float ty = (t2 * y1 - t1 * y2) * r;
            const float tz = (t2 * z1 - t1 * z2) * r;
            const float bx = (s1 * x2 - s2 * x1) * r;
            const float by = (s1 * y2 - s2 * y1) * r;
            const float bz = (s1 * z2 - s2 * z1) * r;
            for (uint32_t index : {i0, i1, i2}) {
                mesh.tangents[index * 3u] += tx;
                mesh.tangents[index * 3u + 1u] += ty;
                mesh.tangents[index * 3u + 2u] += tz;
                mesh.bitangents[index * 3u] += bx;
                mesh.bitangents[index * 3u + 1u] += by;
                mesh.bitangents[index * 3u + 2u] += bz;
            }
        }
    }

    for (size_t i = 0; i < vertexCount; ++i) {
        float& tx = mesh.tangents[i * 3u];
        float& ty = mesh.tangents[i * 3u + 1u];
        float& tz = mesh.tangents[i * 3u + 2u];
        if (std::fabs(tx) + std::fabs(ty) + std::fabs(tz) < 1e-7f) {
            const float nx = mesh.normals[i * 3u];
            const float ny = mesh.normals[i * 3u + 1u];
            const float nz = mesh.normals[i * 3u + 2u];
            if (std::fabs(nz) < 0.9f) {
                tx = -ny; ty = nx; tz = 0.0f;
            } else {
                tx = 1.0f; ty = 0.0f; tz = 0.0f;
            }
        }
        Normalize3(tx, ty, tz);

        float& bx = mesh.bitangents[i * 3u];
        float& by = mesh.bitangents[i * 3u + 1u];
        float& bz = mesh.bitangents[i * 3u + 2u];
        if (std::fabs(bx) + std::fabs(by) + std::fabs(bz) < 1e-7f) {
            const float nx = mesh.normals[i * 3u];
            const float ny = mesh.normals[i * 3u + 1u];
            const float nz = mesh.normals[i * 3u + 2u];
            bx = ny * tz - nz * ty;
            by = nz * tx - nx * tz;
            bz = nx * ty - ny * tx;
        }
        Normalize3(bx, by, bz);
    }
}

void ApplyPoint(const NifTransform& t, float& x, float& y, float& z) {
    if (!t.valid) return;
    const float sx = x * t.scale, sy = y * t.scale, sz = z * t.scale;
    const float rx = t.rotation[0] * sx + t.rotation[1] * sy + t.rotation[2] * sz;
    const float ry = t.rotation[3] * sx + t.rotation[4] * sy + t.rotation[5] * sz;
    const float rz = t.rotation[6] * sx + t.rotation[7] * sy + t.rotation[8] * sz;
    x = rx + t.translation[0];
    y = ry + t.translation[1];
    z = rz + t.translation[2];
}

void ApplyVector(const NifTransform& t, float& x, float& y, float& z) {
    if (!t.valid) return;
    const float rx = t.rotation[0] * x + t.rotation[1] * y + t.rotation[2] * z;
    const float ry = t.rotation[3] * x + t.rotation[4] * y + t.rotation[5] * z;
    const float rz = t.rotation[6] * x + t.rotation[7] * y + t.rotation[8] * z;
    x = rx; y = ry; z = rz;
}

bool CollectAncestorTransforms(const std::vector<uint8_t>& nif,
                               const NifHeader& header,
                               uint32_t childBlock,
                               std::vector<NifTransform>& ancestors) {
    ancestors.clear();
    if (childBlock >= header.numBlocks) return false;

    std::vector<uint32_t> parent(header.numBlocks, INVALID_REF);
    std::vector<NifTransform> nodeTransforms(header.numBlocks);
    std::vector<uint8_t> parsedNode(header.numBlocks, 0u);

    for (uint32_t block = 0; block < header.numBlocks; ++block) {
        const std::string& type = BlockType(header, block);
        if (type != "NiNode" && type != "BSFadeNode") continue;
        std::vector<uint32_t> children;
        NifTransform transform;
        if (!ParseNode(BlockData(nif, header, block), header.blockSizes[block],
                       header, transform, children)) continue;
        nodeTransforms[block] = transform;
        parsedNode[block] = 1u;
        for (uint32_t child : children) {
            if (child < header.numBlocks && parent[child] == INVALID_REF) parent[child] = block;
        }
    }

    uint32_t current = parent[childBlock];
    for (uint32_t guard = 0; current < header.numBlocks && guard < header.numBlocks; ++guard) {
        if (!parsedNode[current]) break;
        ancestors.push_back(nodeTransforms[current]);
        const uint32_t next = parent[current];
        if (next == current) break;
        current = next;
    }
    return !ancestors.empty();
}

void ApplyTransforms(Fo3StaticNifMesh& mesh,
                     const NifTransform& shape,
                     const std::vector<NifTransform>& ancestors,
                     const NifTransform* rootFallback) {
    const bool haveFullChain = !ancestors.empty();
    const size_t vertexCount = mesh.positions.size() / 3u;
    for (size_t i = 0; i < vertexCount; ++i) {
        float& px = mesh.positions[i * 3u];
        float& py = mesh.positions[i * 3u + 1u];
        float& pz = mesh.positions[i * 3u + 2u];
        ApplyPoint(shape, px, py, pz);
        if (haveFullChain) {
            for (const NifTransform& parent : ancestors) ApplyPoint(parent, px, py, pz);
        } else if (rootFallback) {
            ApplyPoint(*rootFallback, px, py, pz);
        }

        std::vector<float>* arrays[] = {&mesh.normals, &mesh.tangents, &mesh.bitangents};
        for (std::vector<float>* values : arrays) {
            float& x = (*values)[i * 3u];
            float& y = (*values)[i * 3u + 1u];
            float& z = (*values)[i * 3u + 2u];
            ApplyVector(shape, x, y, z);
            if (haveFullChain) {
                for (const NifTransform& parent : ancestors) ApplyVector(parent, x, y, z);
            } else if (rootFallback) {
                ApplyVector(*rootFallback, x, y, z);
            }
            Normalize3(x, y, z);
        }
    }
}

struct Q970NodeInfo {
    bool parsed = false;
    bool isSwitch = false;
    bool isLod = false;
    uint16_t flagsLow = 0u;
    uint32_t switchIndex = 0u;
    NifTransform transform;
    std::vector<uint32_t> children;
};

struct Q970SceneStats {
    size_t parsedNodes = 0u;
    size_t lodNodes = 0u;
    size_t switchNodes = 0u;
    size_t hiddenNodes = 0u;
    size_t hiddenShapes = 0u;
    size_t suppressedLodChildren = 0u;
    size_t selectedShapes = 0u;
    size_t unparentedShapes = 0u;
    bool fallbackAll = false;
};

bool Q970IsShapeType(const std::string& type) {
    return type == "NiTriStrips" || type == "NiTriShape" ||
           type == "BSSegmentedTriShape";
}

bool Q970LooksLikeNode(const std::string& type) {
    if (type == "NiNode" || type == "BSFadeNode" ||
        type == "NiSwitchNode" || type == "NiLODNode") return true;
    return type.size() >= 4u && type.compare(type.size() - 4u, 4u, "Node") == 0;
}

bool Q970ParseAvObjectBase(Cursor& c, const NifHeader& header,
                           NifTransform& transform, uint16_t& flagsLow) {
    if (!ParseObjectNetPrefix(c)) return false;

    if (!c.U16(flagsLow)) return false;
    if (header.userVersion >= 11u && header.bsVersion > 26u) {
        uint16_t flagsHigh = 0u;
        if (!c.U16(flagsHigh)) return false;
    }

    for (float& value : transform.translation) if (!c.F32(value)) return false;
    for (float& value : transform.rotation) if (!c.F32(value)) return false;
    if (!c.F32(transform.scale)) return false;

    if (header.userVersion <= 11u) {
        uint32_t propertyCount = 0u;
        if (!c.U32(propertyCount) || propertyCount > MAX_BLOCKS) return false;
        if (!c.Skip(static_cast<size_t>(propertyCount) * 4u)) return false;
    }

    uint32_t collisionRef = INVALID_REF;
    if (!c.U32(collisionRef)) return false;
    transform.valid = true;
    return true;
}

bool Q970ReadAvFlags(const std::vector<uint8_t>& nif,
                     const NifHeader& header,
                     uint32_t block,
                     uint16_t& flagsLow) {
    if (block >= header.numBlocks) return false;
    Cursor c(BlockData(nif, header, block), header.blockSizes[block]);
    if (!ParseObjectNetPrefix(c)) return false;
    if (!c.U16(flagsLow)) return false;
    return true;
}

bool Q970ParseNodeInfo(const std::vector<uint8_t>& nif,
                       const NifHeader& header,
                       uint32_t block,
                       Q970NodeInfo& out) {
    out = {};
    if (block >= header.numBlocks) return false;
    const std::string& type = BlockType(header, block);
    if (!Q970LooksLikeNode(type)) return false;

    Cursor c(BlockData(nif, header, block), header.blockSizes[block]);
    if (!Q970ParseAvObjectBase(c, header, out.transform, out.flagsLow)) return false;

    uint32_t childCount = 0u;
    if (!c.U32(childCount) || childCount > MAX_BLOCKS) return false;
    out.children.resize(childCount);
    for (uint32_t& child : out.children) if (!c.U32(child)) return false;

    uint32_t effectCount = 0u;
    if (!c.U32(effectCount) || effectCount > MAX_BLOCKS ||
        !c.Skip(static_cast<size_t>(effectCount) * 4u)) return false;

    out.isLod = type == "NiLODNode";
    out.isSwitch = out.isLod || type == "NiSwitchNode";
    if (out.isSwitch) {
        uint16_t switchFlags = 0u;
        if (!c.U16(switchFlags) || !c.U32(out.switchIndex)) return false;
        // NiLODNode appends a Ref<NiLODData> in Fallout 3. We do not need its
        // ranges because VR correctness currently forces the nearest child.
        if (out.isLod && c.remaining() >= 4u) {
            uint32_t lodDataRef = INVALID_REF;
            if (!c.U32(lodDataRef)) return false;
        }
    }

    // Derived NiNode classes may append fields after the base node payload.
    // They do not change the child list needed for reachability here.
    out.parsed = true;
    return true;
}

bool Q970CollectAncestorTransforms(const std::vector<uint8_t>& nif,
                                    const NifHeader& header,
                                    uint32_t childBlock,
                                    std::vector<NifTransform>& ancestors) {
    ancestors.clear();
    if (childBlock >= header.numBlocks) return false;

    std::vector<uint32_t> parent(header.numBlocks, INVALID_REF);
    std::vector<Q970NodeInfo> nodes(header.numBlocks);
    for (uint32_t block = 0u; block < header.numBlocks; ++block) {
        Q970NodeInfo info;
        if (!Q970ParseNodeInfo(nif, header, block, info)) continue;
        nodes[block] = std::move(info);
        for (uint32_t child : nodes[block].children) {
            if (child < header.numBlocks && parent[child] == INVALID_REF) {
                parent[child] = block;
            }
        }
    }

    uint32_t current = parent[childBlock];
    for (uint32_t guard = 0u;
         current < header.numBlocks && guard < header.numBlocks;
         ++guard) {
        if (!nodes[current].parsed) break;
        ancestors.push_back(nodes[current].transform);
        const uint32_t next = parent[current];
        if (next == current) break;
        current = next;
    }
    return !ancestors.empty();
}

bool Q970BuildRenderableSet(const std::vector<uint8_t>& nif,
                            const NifHeader& header,
                            std::vector<uint8_t>& renderable,
                            Q970SceneStats& stats) {
    renderable.assign(header.numBlocks, 0u);
    stats = {};

    std::vector<Q970NodeInfo> nodes(header.numBlocks);
    std::vector<uint32_t> parent(header.numBlocks, INVALID_REF);
    size_t shapeCount = 0u;

    for (uint32_t block = 0u; block < header.numBlocks; ++block) {
        const std::string& type = BlockType(header, block);
        if (Q970IsShapeType(type)) ++shapeCount;

        Q970NodeInfo info;
        if (!Q970ParseNodeInfo(nif, header, block, info)) continue;
        nodes[block] = std::move(info);
        ++stats.parsedNodes;
        if (nodes[block].isLod) ++stats.lodNodes;
        else if (nodes[block].isSwitch) ++stats.switchNodes;
    }

    for (uint32_t block = 0u; block < header.numBlocks; ++block) {
        if (!nodes[block].parsed) continue;
        for (uint32_t child : nodes[block].children) {
            if (child < header.numBlocks && parent[child] == INVALID_REF) {
                parent[child] = block;
            }
        }
    }

    std::vector<uint32_t> stack;
    stack.reserve(header.numBlocks);
    for (uint32_t block = 0u; block < header.numBlocks; ++block) {
        if (nodes[block].parsed && parent[block] == INVALID_REF) stack.push_back(block);
    }

    std::vector<uint8_t> visited(header.numBlocks, 0u);
    while (!stack.empty()) {
        const uint32_t block = stack.back();
        stack.pop_back();
        if (block >= header.numBlocks || visited[block]) continue;
        visited[block] = 1u;

        const std::string& type = BlockType(header, block);
        if (nodes[block].parsed) {
            if ((nodes[block].flagsLow & 0x0001u) != 0u) {
                ++stats.hiddenNodes;
                continue;
            }

            const std::vector<uint32_t>& children = nodes[block].children;
            if (nodes[block].isLod) {
                uint32_t chosen = INVALID_REF;
                for (uint32_t child : children) {
                    if (child < header.numBlocks) {
                        chosen = child;
                        break;
                    }
                }
                if (chosen != INVALID_REF) stack.push_back(chosen);
                if (children.size() > 1u) {
                    stats.suppressedLodChildren += children.size() - 1u;
                }
            } else if (nodes[block].isSwitch) {
                uint32_t chosen = INVALID_REF;
                if (nodes[block].switchIndex < children.size()) {
                    chosen = children[nodes[block].switchIndex];
                } else {
                    for (uint32_t child : children) {
                        if (child < header.numBlocks) {
                            chosen = child;
                            break;
                        }
                    }
                }
                if (chosen < header.numBlocks) stack.push_back(chosen);
            } else {
                for (uint32_t child : children) {
                    if (child < header.numBlocks) stack.push_back(child);
                }
            }
            continue;
        }

        if (Q970IsShapeType(type)) {
            uint16_t flagsLow = 0u;
            if (Q970ReadAvFlags(nif, header, block, flagsLow) &&
                (flagsLow & 0x0001u) != 0u) {
                ++stats.hiddenShapes;
                continue;
            }
            renderable[block] = 1u;
            ++stats.selectedShapes;
        }
    }

    // Preserve valid top-level geometry that is not parented by any node.
    for (uint32_t block = 0u; block < header.numBlocks; ++block) {
        if (!Q970IsShapeType(BlockType(header, block)) ||
            parent[block] != INVALID_REF || renderable[block] != 0u) continue;
        uint16_t flagsLow = 0u;
        if (Q970ReadAvFlags(nif, header, block, flagsLow) &&
            (flagsLow & 0x0001u) != 0u) {
            ++stats.hiddenShapes;
            continue;
        }
        renderable[block] = 1u;
        ++stats.selectedShapes;
        ++stats.unparentedShapes;
    }

    // If an unfamiliar node subclass prevented traversal, fail open rather
    // than making the entire model disappear. Hidden shapes remain excluded.
    if (shapeCount > 0u && stats.selectedShapes == 0u) {
        stats.fallbackAll = true;
        for (uint32_t block = 0u; block < header.numBlocks; ++block) {
            if (!Q970IsShapeType(BlockType(header, block))) continue;
            uint16_t flagsLow = 0u;
            if (Q970ReadAvFlags(nif, header, block, flagsLow) &&
                (flagsLow & 0x0001u) != 0u) continue;
            renderable[block] = 1u;
            ++stats.selectedShapes;
        }
    }

    return shapeCount > 0u;
}


struct Q211SkinInstance {
    uint32_t dataRef = INVALID_REF;
    uint32_t partitionRef = INVALID_REF;
    uint32_t skeletonRoot = INVALID_REF;
    std::vector<uint32_t> bones;
    bool dismember = false;
    std::vector<uint16_t> partitionFlags;
    std::vector<uint16_t> bodyParts;
};

bool Q211ReadNiTransform(Cursor& c, NifTransform& out) {
    out = {};
    for (float& value : out.rotation) if (!c.F32(value)) return false;
    for (float& value : out.translation) if (!c.F32(value)) return false;
    if (!c.F32(out.scale)) return false;
    out.valid = true;
    return true;
}

std::string Q211BlockName(const std::vector<uint8_t>& nif,
                          const NifHeader& header,
                          uint32_t block) {
    if (block >= header.numBlocks || header.blockSizes[block] < 4u)
        return {};
    const uint8_t* data = BlockData(nif, header, block);
    if (!data) return {};
    const uint32_t nameIndex = ReadLe32(data);
    if (nameIndex == INVALID_REF || nameIndex >= header.strings.size())
        return {};
    return header.strings[nameIndex];
}

bool Q211ParseSkinInstance(const std::vector<uint8_t>& nif,
                           const NifHeader& header,
                           uint32_t skinRef,
                           Q211SkinInstance& out) {
    out = {};
    if (skinRef >= header.numBlocks) return false;
    const std::string& type = BlockType(header, skinRef);
    if (type != "NiSkinInstance" &&
        type != "BSDismemberSkinInstance") return false;

    Cursor c(BlockData(nif, header, skinRef), header.blockSizes[skinRef]);
    uint32_t count = 0u;
    if (!c.U32(out.dataRef) ||
        !c.U32(out.partitionRef) ||
        !c.U32(out.skeletonRoot) ||
        !c.U32(count) ||
        count == 0u || count > 256u) {
        return false;
    }
    out.bones.resize(count);
    for (uint32_t& bone : out.bones) {
        if (!c.U32(bone)) return false;
    }

    if (type == "BSDismemberSkinInstance") {
        uint32_t numPartitions = 0u;
        if (!c.U32(numPartitions) || numPartitions > 128u) return false;
        out.dismember = true;
        out.partitionFlags.resize(numPartitions);
        out.bodyParts.resize(numPartitions);
        for (uint32_t i = 0u; i < numPartitions; ++i) {
            if (!c.U16(out.partitionFlags[i]) ||
                !c.U16(out.bodyParts[i])) {
                return false;
            }
        }
    }
    return true;
}

void Q211AddInfluence(
        std::vector<std::vector<std::pair<uint16_t, float>>>& influences,
        uint32_t vertex, uint32_t bone, float weight) {
    if (vertex >= influences.size() || bone > 0xfffeu ||
        !(weight > 0.000001f) || !std::isfinite(weight)) {
        return;
    }
    influences[vertex].push_back(
        {static_cast<uint16_t>(bone), weight});
}

bool Q211ParseSkinDataWeights(
        const std::vector<uint8_t>& nif,
        const NifHeader& header,
        const Q211SkinInstance& skin,
        std::vector<std::vector<std::pair<uint16_t, float>>>& influences,
        bool& hasSoftwareWeights,
        NifTransform& skinTransformOut,
        std::vector<NifTransform>& boneOffsetsOut) {
    hasSoftwareWeights = false;
    if (skin.dataRef >= header.numBlocks ||
        BlockType(header, skin.dataRef) != "NiSkinData") return false;

    Cursor c(BlockData(nif, header, skin.dataRef),
             header.blockSizes[skin.dataRef]);
    NifTransform skinTransform;
    uint32_t numBones = 0u;
    uint8_t hasWeights = 0u;
    if (!Q211ReadNiTransform(c, skinTransform) ||
        !c.U32(numBones) || numBones > 256u ||
        !c.U8(hasWeights)) {
        return false;
    }

    skinTransformOut = skinTransform;
    boneOffsetsOut.clear();
    boneOffsetsOut.reserve(numBones);

    hasSoftwareWeights = hasWeights != 0u;
    for (uint32_t bone = 0u; bone < numBones; ++bone) {
        NifTransform boneOffset;
        float center[3]{};
        float radius = 0.0f;
        uint16_t numVertices = 0u;
        if (!Q211ReadNiTransform(c, boneOffset)) return false;
        boneOffsetsOut.push_back(boneOffset);
        for (float& value : center) if (!c.F32(value)) return false;
        if (!c.F32(radius) || !c.U16(numVertices)) return false;

        if (hasSoftwareWeights) {
            for (uint16_t i = 0u; i < numVertices; ++i) {
                uint16_t vertex = 0u;
                float weight = 0.0f;
                if (!c.U16(vertex) || !c.F32(weight)) return false;
                Q211AddInfluence(influences, vertex, bone, weight);
            }
        }
    }
    return true;
}

bool Q211ParseSkinPartitionWeights(
        const std::vector<uint8_t>& nif,
        const NifHeader& header,
        const Q211SkinInstance& skin,
        std::vector<std::vector<std::pair<uint16_t, float>>>& influences) {
    if (skin.partitionRef >= header.numBlocks ||
        BlockType(header, skin.partitionRef) != "NiSkinPartition") {
        return false;
    }

    Cursor c(BlockData(nif, header, skin.partitionRef),
             header.blockSizes[skin.partitionRef]);
    uint32_t numPartitions = 0u;
    if (!c.U32(numPartitions) || numPartitions == 0u ||
        numPartitions > 128u) return false;

    bool any = false;
    for (uint32_t part = 0u; part < numPartitions; ++part) {
        uint16_t numVertices = 0u, numTriangles = 0u, numBones = 0u;
        uint16_t numStrips = 0u, weightsPerVertex = 0u;
        if (!c.U16(numVertices) || !c.U16(numTriangles) ||
            !c.U16(numBones) || !c.U16(numStrips) ||
            !c.U16(weightsPerVertex) ||
            numVertices > 65530u || numBones > 256u ||
            weightsPerVertex == 0u || weightsPerVertex > 8u) {
            return false;
        }

        std::vector<uint16_t> bones(numBones);
        for (uint16_t& bone : bones) if (!c.U16(bone)) return false;

        uint8_t hasVertexMap = 0u;
        if (!c.U8(hasVertexMap)) return false;
        std::vector<uint16_t> vertexMap(numVertices);
        if (hasVertexMap) {
            for (uint16_t& vertex : vertexMap) {
                if (!c.U16(vertex)) return false;
            }
        } else {
            for (uint32_t i = 0u; i < numVertices; ++i)
                vertexMap[i] = static_cast<uint16_t>(i);
        }

        uint8_t hasVertexWeights = 0u;
        if (!c.U8(hasVertexWeights)) return false;
        std::vector<float> weights(
            static_cast<size_t>(numVertices) * weightsPerVertex, 0.0f);
        if (hasVertexWeights) {
            for (float& weight : weights) if (!c.F32(weight)) return false;
        }

        std::vector<uint16_t> stripLengths(numStrips);
        size_t stripPointCount = 0u;
        for (uint16_t& length : stripLengths) {
            if (!c.U16(length)) return false;
            stripPointCount += length;
        }

        uint8_t hasFaces = 0u;
        if (!c.U8(hasFaces)) return false;
        if (hasFaces) {
            if (numStrips != 0u) {
                if (!c.Skip(stripPointCount * sizeof(uint16_t))) return false;
            } else {
                if (!c.Skip(static_cast<size_t>(numTriangles) * 3u *
                            sizeof(uint16_t))) return false;
            }
        }

        uint8_t hasBoneIndices = 0u;
        if (!c.U8(hasBoneIndices)) return false;
        std::vector<uint8_t> boneIndices(
            static_cast<size_t>(numVertices) * weightsPerVertex, 0u);
        if (hasBoneIndices) {
            for (uint8_t& index : boneIndices) {
                if (!c.U8(index)) return false;
            }
        }

        if (!hasVertexWeights || !hasBoneIndices) continue;
        for (uint32_t v = 0u; v < numVertices; ++v) {
            const uint32_t sourceVertex = vertexMap[v];
            for (uint32_t slot = 0u; slot < weightsPerVertex; ++slot) {
                const size_t at =
                    static_cast<size_t>(v) * weightsPerVertex + slot;
                const uint32_t palette = boneIndices[at];
                if (palette >= bones.size()) continue;
                const uint32_t skinBone = bones[palette];
                Q211AddInfluence(
                    influences, sourceVertex, skinBone, weights[at]);
                any = any || weights[at] > 0.000001f;
            }
        }
    }
    return any;
}

uint64_t Q215TriangleKey(uint32_t a, uint32_t b, uint32_t c) {
    if (a > b) std::swap(a, b);
    if (b > c) std::swap(b, c);
    if (a > b) std::swap(a, b);
    return static_cast<uint64_t>(a) |
           (static_cast<uint64_t>(b) << 16u) |
           (static_cast<uint64_t>(c) << 32u);
}

bool Q215PopulateDismemberTriangleParts(
        const std::vector<uint8_t>& nif,
        const NifHeader& header,
        const Q211SkinInstance& skin,
        Fo3StaticNifMesh& mesh) {
    mesh.dismemberSkin = skin.dismember;
    mesh.skinTriangleBodyParts.assign(
        mesh.indices.size() / 3u, static_cast<uint16_t>(0xffffu));

    if (!skin.dismember ||
        skin.partitionRef >= header.numBlocks ||
        BlockType(header, skin.partitionRef) != "NiSkinPartition" ||
        skin.bodyParts.empty()) {
        return false;
    }

    Cursor c(BlockData(nif, header, skin.partitionRef),
             header.blockSizes[skin.partitionRef]);
    uint32_t numPartitions = 0u;
    if (!c.U32(numPartitions) || numPartitions == 0u ||
        numPartitions > 128u) {
        return false;
    }

    std::unordered_map<uint64_t, uint16_t> triangleParts;
    triangleParts.reserve(mesh.indices.size() / 3u);

    for (uint32_t part = 0u; part < numPartitions; ++part) {
        uint16_t numVertices = 0u, numTriangles = 0u, numBones = 0u;
        uint16_t numStrips = 0u, weightsPerVertex = 0u;
        if (!c.U16(numVertices) || !c.U16(numTriangles) ||
            !c.U16(numBones) || !c.U16(numStrips) ||
            !c.U16(weightsPerVertex) ||
            numVertices > 65530u || numBones > 256u ||
            weightsPerVertex == 0u || weightsPerVertex > 8u) {
            return false;
        }

        if (!c.Skip(static_cast<size_t>(numBones) * sizeof(uint16_t)))
            return false;

        uint8_t hasVertexMap = 0u;
        if (!c.U8(hasVertexMap)) return false;
        std::vector<uint16_t> vertexMap(numVertices);
        if (hasVertexMap) {
            for (uint16_t& vertex : vertexMap) {
                if (!c.U16(vertex)) return false;
            }
        } else {
            for (uint32_t i = 0u; i < numVertices; ++i)
                vertexMap[i] = static_cast<uint16_t>(i);
        }

        uint8_t hasVertexWeights = 0u;
        if (!c.U8(hasVertexWeights)) return false;
        if (hasVertexWeights &&
            !c.Skip(static_cast<size_t>(numVertices) *
                    weightsPerVertex * sizeof(float))) {
            return false;
        }

        std::vector<uint16_t> stripLengths(numStrips);
        size_t stripPointCount = 0u;
        for (uint16_t& length : stripLengths) {
            if (!c.U16(length)) return false;
            stripPointCount += length;
        }

        const uint16_t bodyPart =
            part < skin.bodyParts.size()
                ? skin.bodyParts[part]
                : static_cast<uint16_t>(0xffffu);

        uint8_t hasFaces = 0u;
        if (!c.U8(hasFaces)) return false;
        if (hasFaces) {
            if (numStrips != 0u) {
                std::vector<uint16_t> stripPoints(stripPointCount);
                for (uint16_t& point : stripPoints) {
                    if (!c.U16(point)) return false;
                }

                size_t stripCursor = 0u;
                for (uint16_t stripLength : stripLengths) {
                    if (stripCursor + stripLength > stripPoints.size())
                        return false;
                    for (uint32_t i = 2u; i < stripLength; ++i) {
                        uint16_t a = stripPoints[stripCursor + i - 2u];
                        uint16_t b = stripPoints[stripCursor + i - 1u];
                        uint16_t d = stripPoints[stripCursor + i];
                        if ((i & 1u) != 0u) std::swap(a, b);
                        if (a == b || b == d || a == d) continue;
                        if (a >= vertexMap.size() ||
                            b >= vertexMap.size() ||
                            d >= vertexMap.size()) {
                            continue;
                        }
                        triangleParts[Q215TriangleKey(
                            vertexMap[a], vertexMap[b], vertexMap[d])] =
                            bodyPart;
                    }
                    stripCursor += stripLength;
                }
            } else {
                for (uint32_t tri = 0u; tri < numTriangles; ++tri) {
                    uint16_t a = 0u, b = 0u, d = 0u;
                    if (!c.U16(a) || !c.U16(b) || !c.U16(d))
                        return false;
                    if (a >= vertexMap.size() ||
                        b >= vertexMap.size() ||
                        d >= vertexMap.size()) {
                        continue;
                    }
                    triangleParts[Q215TriangleKey(
                        vertexMap[a], vertexMap[b], vertexMap[d])] =
                        bodyPart;
                }
            }
        }

        uint8_t hasBoneIndices = 0u;
        if (!c.U8(hasBoneIndices)) return false;
        if (hasBoneIndices &&
            !c.Skip(static_cast<size_t>(numVertices) *
                    weightsPerVertex * sizeof(uint8_t))) {
            return false;
        }
    }

    size_t mapped = 0u;
    size_t capTriangles = 0u;
    for (size_t tri = 0u; tri * 3u + 2u < mesh.indices.size(); ++tri) {
        const uint64_t key = Q215TriangleKey(
            mesh.indices[tri * 3u + 0u],
            mesh.indices[tri * 3u + 1u],
            mesh.indices[tri * 3u + 2u]);
        const auto found = triangleParts.find(key);
        if (found == triangleParts.end()) continue;
        mesh.skinTriangleBodyParts[tri] = found->second;
        ++mapped;
        const uint16_t bodyPart = found->second;
        if ((bodyPart >= 101u && bodyPart <= 113u) ||
            (bodyPart >= 201u && bodyPart <= 213u)) {
            ++capTriangles;
        }
    }

    Q6H_LOGI("Q21.15 DISMEMBER MAP: partitions=%u metadata=%zu triangles=%zu mapped=%zu capTriangles=%zu",
             numPartitions, skin.bodyParts.size(),
             mesh.indices.size() / 3u, mapped, capTriangles);
    return mapped > 0u;
}

bool Q211InverseBindOrigin(const NifTransform& skinToBone,
                           float out[3]) {
    if (!skinToBone.valid ||
        !std::isfinite(skinToBone.scale) ||
        std::fabs(skinToBone.scale) < 1.0e-8f) {
        return false;
    }

    // skinToBone: p_bone = R * (s * p_skin) + t
    // Bone origin in skin space is inverse(skinToBone) * (0,0,0).
    const float tx = -skinToBone.translation[0];
    const float ty = -skinToBone.translation[1];
    const float tz = -skinToBone.translation[2];
    const float invScale = 1.0f / skinToBone.scale;

    out[0] = (skinToBone.rotation[0] * tx +
              skinToBone.rotation[3] * ty +
              skinToBone.rotation[6] * tz) * invScale;
    out[1] = (skinToBone.rotation[1] * tx +
              skinToBone.rotation[4] * ty +
              skinToBone.rotation[7] * tz) * invScale;
    out[2] = (skinToBone.rotation[2] * tx +
              skinToBone.rotation[5] * ty +
              skinToBone.rotation[8] * tz) * invScale;
    return std::isfinite(out[0]) &&
           std::isfinite(out[1]) &&
           std::isfinite(out[2]);
}

bool Q211BindPositionFromSkinData(
        const std::vector<uint8_t>& nif,
        const NifHeader& header,
        const ShapeObject& shape,
        const NifTransform& boneOffset,
        float out[3]) {
    if (!Q211InverseBindOrigin(boneOffset, out)) return false;

    // The mesh vertices are subsequently pushed through the geometry's
    // NiAVObject transform chain by ApplyTransforms(). Put the inverse-bind
    // pivot through the exact same chain so both live in one coordinate space.
    ApplyPoint(shape.transform, out[0], out[1], out[2]);
    std::vector<NifTransform> ancestors;
    Q970CollectAncestorTransforms(nif, header, shape.block, ancestors);
    for (const NifTransform& parent : ancestors)
        ApplyPoint(parent, out[0], out[1], out[2]);
    return true;
}

bool Q211PopulateSkinInfo(const std::vector<uint8_t>& nif,
                          const NifHeader& header,
                          const ShapeObject& shape,
                          Fo3StaticNifMesh& mesh) {
    mesh.skinned = false;
    mesh.skinBones.clear();
    mesh.skinBoneIndices.clear();
    mesh.skinBoneWeights.clear();
    mesh.dismemberSkin = false;
    mesh.skinTriangleBodyParts.clear();

    const size_t vertexCount = mesh.positions.size() / 3u;
    if (shape.skinRef == INVALID_REF || vertexCount == 0u) return false;

    Q211SkinInstance skin;
    if (!Q211ParseSkinInstance(nif, header, shape.skinRef, skin)) return false;

    std::vector<std::vector<std::pair<uint16_t, float>>> influences(vertexCount);
    bool softwareWeights = false;
    NifTransform skinTransform;
    std::vector<NifTransform> boneOffsets;
    const bool dataParsed =
        Q211ParseSkinDataWeights(
            nif, header, skin, influences, softwareWeights,
            skinTransform, boneOffsets);
    bool partitionWeights = false;
    if (!softwareWeights) {
        partitionWeights =
            Q211ParseSkinPartitionWeights(
                nif, header, skin, influences);
    }

    mesh.skinBones.resize(skin.bones.size());
    size_t inverseBindPivots = 0u;
    size_t nodeFallbackPivots = 0u;
    for (size_t i = 0u; i < skin.bones.size(); ++i) {
        Fo3NifSkinBone& bone = mesh.skinBones[i];
        bone.name = Q211BlockName(nif, header, skin.bones[i]);

        bool pivotReady = false;
        if (i < boneOffsets.size()) {
            pivotReady = Q211BindPositionFromSkinData(
                nif, header, shape, boneOffsets[i], bone.bindPosition);
            if (pivotReady) ++inverseBindPivots;
        }

        // Diagnostic fallback only. A valid NiSkinData bone transform is the
        // authoritative bind offset for a skinned mesh.
        if (!pivotReady) {
            Q970NodeInfo node;
            if (Q970ParseNodeInfo(nif, header, skin.bones[i], node)) {
                float x = 0.0f, y = 0.0f, z = 0.0f;
                ApplyPoint(node.transform, x, y, z);
                std::vector<NifTransform> ancestors;
                Q970CollectAncestorTransforms(
                    nif, header, skin.bones[i], ancestors);
                for (const NifTransform& parent : ancestors)
                    ApplyPoint(parent, x, y, z);
                bone.bindPosition[0] = x;
                bone.bindPosition[1] = y;
                bone.bindPosition[2] = z;
                ++nodeFallbackPivots;
            }
        }
    }

    mesh.skinBoneIndices.assign(vertexCount * 4u, 0xffffu);
    mesh.skinBoneWeights.assign(vertexCount * 4u, 0.0f);

    size_t weightedVertices = 0u;
    for (size_t vertex = 0u; vertex < vertexCount; ++vertex) {
        auto& list = influences[vertex];
        if (list.empty()) continue;

        std::sort(list.begin(), list.end(),
                  [](const auto& a, const auto& b) {
                      if (a.first != b.first) return a.first < b.first;
                      return a.second > b.second;
                  });
        std::vector<std::pair<uint16_t, float>> merged;
        for (const auto& influence : list) {
            if (!merged.empty() &&
                merged.back().first == influence.first) {
                merged.back().second =
                    std::max(merged.back().second, influence.second);
            } else {
                merged.push_back(influence);
            }
        }
        std::sort(merged.begin(), merged.end(),
                  [](const auto& a, const auto& b) {
                      return a.second > b.second;
                  });

        const size_t count = std::min<size_t>(4u, merged.size());
        float sum = 0.0f;
        for (size_t slot = 0u; slot < count; ++slot)
            sum += std::max(0.0f, merged[slot].second);
        if (sum <= 0.000001f) continue;

        ++weightedVertices;
        for (size_t slot = 0u; slot < count; ++slot) {
            mesh.skinBoneIndices[vertex * 4u + slot] =
                merged[slot].first;
            mesh.skinBoneWeights[vertex * 4u + slot] =
                std::max(0.0f, merged[slot].second) / sum;
        }
    }

    mesh.skinned = weightedVertices > 0u && !mesh.skinBones.empty();
    Q215PopulateDismemberTriangleParts(nif, header, skin, mesh);
    Q6H_LOGI("Q21.1C SKIN READY: shape=%u skinRef=%u bones=%zu vertices=%zu weightedVertices=%zu dataParsed=%d softwareWeights=%d partitionWeights=%d inverseBindPivots=%zu nodeFallbackPivots=%zu skinTransformT=(%.2f %.2f %.2f)",
             shape.block, shape.skinRef, mesh.skinBones.size(),
             vertexCount, weightedVertices,
             dataParsed ? 1 : 0,
             softwareWeights ? 1 : 0,
             partitionWeights ? 1 : 0,
             inverseBindPivots, nodeFallbackPivots,
             skinTransform.translation[0],
             skinTransform.translation[1],
             skinTransform.translation[2]);
    return mesh.skinned;
}

bool TryLoadShape(const std::vector<uint8_t>& nif, const NifHeader& header,
                  const ShapeObject& shape,
                  const NifTransform* root,
                  Fo3StaticNifMesh& mesh) {
    if (shape.dataRef >= header.numBlocks) return false;
    const std::string& dataType = BlockType(header, shape.dataRef);
    const uint8_t* data = BlockData(nif, header, shape.dataRef);
    if (!data) return false;

    Fo3StaticNifMesh candidate;
    bool geometryOk = false;
    if (dataType == "NiTriStripsData") {
        geometryOk = ParseTriStripsData(data, header.blockSizes[shape.dataRef], candidate);
    } else if (dataType == "NiTriShapeData") {
        geometryOk = ParseTriShapeData(data, header.blockSizes[shape.dataRef], candidate);
    }
    if (!geometryOk || candidate.positions.empty() || candidate.indices.empty()) return false;

    uint32_t textureSetRef = INVALID_REF;
    for (uint32_t ref : shape.properties) {
        if (ref >= header.numBlocks) continue;
        const std::string& type = BlockType(header, ref);
        const uint8_t* prop = BlockData(nif, header, ref);
        if (!prop) continue;

        if (type == "BSShaderPPLightingProperty" || type == "Lighting30ShaderProperty") {
            uint32_t refOut = INVALID_REF;
            if (ParseShaderTextureRef(prop, header.blockSizes[ref], header,
                                      candidate.shaderFlags1, candidate.shaderFlags2,
                                      candidate.environmentMapScale, refOut)) {
                textureSetRef = refOut;
            }
        } else if (type == "TileShaderProperty") {
            if (!ParseTileShaderProperty(prop, header.blockSizes[ref], header,
                                         candidate.diffuseTexturePath)) return false;
            candidate.noLighting = true;
        } else if (type == "BSShaderNoLightingProperty") {
            std::string directTexture;
            if (ParseNoLightingPropertyQ1020(prop, header.blockSizes[ref], header,
                                             candidate.shaderFlags1, candidate.shaderFlags2,
                                             candidate.environmentMapScale, directTexture,
                                             candidate.noLightingFalloff,
                                             candidate.noLightingFalloffParams)) {
                candidate.noLighting = true;
                if (!directTexture.empty() || candidate.diffuseTexturePath.empty())
                    candidate.diffuseTexturePath = directTexture;
            }
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
            uint8_t drawMode = 0u;
            if (ParseStencilDrawModeQ1160(prop, header.blockSizes[ref], drawMode)) {
                candidate.stencilDrawModePresent = true;
                candidate.stencilDrawMode = drawMode;
            }
        } else if (type == "NiMaterialProperty") {
            ParseMaterialProperty(prop, header.blockSizes[ref],
                                  candidate.specularColor, candidate.emissiveColor,
                                  candidate.glossiness, candidate.alpha,
                                  candidate.emissiveMult);
        } else if (type == "NiAlphaProperty") {
            ParseAlphaProperty(prop, header.blockSizes[ref],
                               candidate.alphaBlend, candidate.alphaTest,
                               candidate.alphaThreshold,
                               candidate.alphaSourceBlend, candidate.alphaDestBlend);
        }
    }


    if (!candidate.noLighting && textureSetRef < header.numBlocks) {
        ParseTextureSet(BlockData(nif, header, textureSetRef),
                        header.blockSizes[textureSetRef],
                        candidate.diffuseTexturePath, candidate.normalTexturePath,
                        candidate.glowTexturePath,
                        candidate.environmentCubeTexturePath,
                        candidate.environmentMaskTexturePath);
    }

    const size_t vertexCount = candidate.positions.size() / 3u;
    EnsureNormals(candidate);
    EnsureTangentBasis(candidate);
    if (candidate.normals.size() != vertexCount * 3u ||
        candidate.tangents.size() != vertexCount * 3u ||
        candidate.bitangents.size() != vertexCount * 3u) return false;

    if (candidate.texcoords.size() != vertexCount * 2u) {
        candidate.texcoords.assign(vertexCount * 2u, 0.0f);
        candidate.diffuseTexturePath.clear();
        candidate.normalTexturePath.clear();
        candidate.glowTexturePath.clear();
        candidate.environmentCubeTexturePath.clear();
        candidate.environmentMaskTexturePath.clear();
    }

    std::vector<NifTransform> ancestors;
    Q970CollectAncestorTransforms(nif, header, shape.block, ancestors);
    ApplyTransforms(candidate, shape.transform, ancestors, root);
    Q211PopulateSkinInfo(nif, header, shape, candidate);
    mesh = std::move(candidate);
    return true;
}

} // namespace

bool LoadFo3StaticNifMeshes(const std::string& modelPath,
                            std::vector<Fo3StaticNifMesh>& outMeshes) {
    outMeshes.clear();

    std::vector<uint8_t> nif;
    std::string resolved;
    if (!LoadFalloutMeshFile(modelPath, nif, &resolved)) return false;
    if (nif.empty() || nif.size() > MAX_NIF_BYTES) return false;

    NifHeader header;
    if (!ParseHeader(nif, header)) {
        Q6H_LOGW("Q6H NIF UNSUPPORTED HEADER: %s", resolved.c_str());
        return false;
    }

    Q970SceneStats q970Scene;
    std::vector<uint8_t> q970Renderable;
    const bool q970SelectionReady =
        Q970BuildRenderableSet(nif, header, q970Renderable, q970Scene);
    Q6H_LOGI("Q9.70 NIF SCENEGRAPH: model=%s nodes=%zu lodNodes=%zu switchNodes=%zu selectedShapes=%zu suppressedLodChildren=%zu hiddenNodes=%zu hiddenShapes=%zu unparentedShapes=%zu fallbackAll=%d forceLOD0=1",
             resolved.c_str(), q970Scene.parsedNodes, q970Scene.lodNodes,
             q970Scene.switchNodes, q970Scene.selectedShapes,
             q970Scene.suppressedLodChildren, q970Scene.hiddenNodes,
             q970Scene.hiddenShapes, q970Scene.unparentedShapes,
             q970Scene.fallbackAll ? 1 : 0);

    NifTransform rootTransform;
    std::vector<uint32_t> rootChildren;
    bool haveRoot = false;
    for (uint32_t block = 0; block < header.numBlocks; ++block) {
        const std::string& type = BlockType(header, block);
        if (type != "NiNode" && type != "BSFadeNode") continue;
        std::vector<uint32_t> children;
        NifTransform transform;
        if (ParseNode(BlockData(nif, header, block), header.blockSizes[block],
                      header, transform, children)) {
            rootTransform = transform;
            rootChildren = std::move(children);
            haveRoot = true;
            break;
        }
    }

    size_t q1060ShapeBlocks = 0u, q1060ShapeObjectFailures = 0u, q1060ShapeLoadFailures = 0u, q1060FallbackDiffuse = 0u;
    for (uint32_t block = 0; block < header.numBlocks; ++block) {
        const std::string& type = BlockType(header, block);
        if (type != "NiTriStrips" && type != "NiTriShape" &&
            type != "BSSegmentedTriShape") continue;
        ++q1060ShapeBlocks;
        if (q970SelectionReady &&
            (block >= q970Renderable.size() || q970Renderable[block] == 0u)) {
            continue;
        }

        ShapeObject shape;
        shape.block = block;
        if (!ParseShapeObject(BlockData(nif, header, block), header.blockSizes[block],
                              header, shape)) {
            ++q1060ShapeObjectFailures;
            Q6H_LOGW("Q10.6 NIF SHAPE REJECT: model=%s block=%u type=%s stage=shape-object-parse",
                     resolved.c_str(), block, type.c_str());
            continue;
        }

        const bool directRootChild = haveRoot &&
            std::find(rootChildren.begin(), rootChildren.end(), block) != rootChildren.end();
        Fo3StaticNifMesh candidate;
        if (!TryLoadShape(nif, header, shape,
                          directRootChild ? &rootTransform : nullptr,
                          candidate)) {
            ++q1060ShapeLoadFailures;
            const std::string dataType = shape.dataRef < header.numBlocks
                ? BlockType(header, shape.dataRef) : std::string("<invalid>");
            Q6H_LOGW("Q10.6 NIF SHAPE REJECT: model=%s block=%u type=%s data=%u dataType=%s properties=%zu stage=geometry-material-load",
                     resolved.c_str(), block, type.c_str(), shape.dataRef,
                     dataType.c_str(), shape.properties.size());
            continue;
        }
        if (candidate.diffuseTexturePath.empty()) {
            ++q1060FallbackDiffuse;
            std::string q1080Properties;
            for (uint32_t propertyRef : shape.properties) {
                if (!q1080Properties.empty()) q1080Properties += ",";
                if (propertyRef < header.numBlocks) q1080Properties += BlockType(header, propertyRef);
                else q1080Properties += "<invalid>";
            }
            if (q1080Properties.empty()) q1080Properties = "<none>";
            const std::string q1080DataType = shape.dataRef < header.numBlocks
                ? BlockType(header, shape.dataRef) : std::string("<invalid>");
            Q6H_LOGW("Q10.9 MATERIAL STATE: model=%s shapeBlock=%u shapeType=%s data=%u dataType=%s properties=%s vertices=%zu triangles=%zu vertexColors=%zu noLighting=%d textureEmpty=%d shaderFlags1=%08X shaderFlags2=%08X",
                     resolved.c_str(), block, type.c_str(), shape.dataRef,
                     q1080DataType.c_str(), q1080Properties.c_str(),
                     candidate.positions.size() / 3u, candidate.indices.size() / 3u,
                     candidate.vertexColors.size() / 4u,
                     candidate.noLighting ? 1 : 0,
                     candidate.diffuseTexturePath.empty() ? 1 : 0,
                     candidate.shaderFlags1, candidate.shaderFlags2);
        }
        candidate.shapeBlock = block;
        candidate.modelPath = resolved;
        std::string q1100Properties;
        for (uint32_t q1100Ref : shape.properties) {
            if (!q1100Properties.empty()) q1100Properties += ",";
            q1100Properties += std::to_string(q1100Ref);
            q1100Properties += ":";
            if (q1100Ref < header.numBlocks) q1100Properties += BlockType(header, q1100Ref);
            else q1100Properties += "<invalid>";
        }
        if (q1100Properties.empty()) q1100Properties = "<none>";
        Q6H_LOGI("Q20.5 MATERIAL BIND: model=%s shapeBlock=%u shapeType=%s properties=%s diffuse=%s normal=%s glow=%s envCube=%s envMask=%s envScale=%.4f noLighting=%d shaderFlags1=%08X shaderFlags2=%08X",
                 resolved.c_str(), block, type.c_str(), q1100Properties.c_str(),
                 candidate.diffuseTexturePath.empty() ? "<none>" : candidate.diffuseTexturePath.c_str(),
                 candidate.normalTexturePath.empty() ? "<none>" : candidate.normalTexturePath.c_str(),
                 candidate.glowTexturePath.empty() ? "<none>" : candidate.glowTexturePath.c_str(),
                 candidate.environmentCubeTexturePath.empty() ? "<none>" : candidate.environmentCubeTexturePath.c_str(),
                 candidate.environmentMaskTexturePath.empty() ? "<none>" : candidate.environmentMaskTexturePath.c_str(),
                 candidate.environmentMapScale,
                 candidate.noLighting ? 1 : 0,
                 candidate.shaderFlags1, candidate.shaderFlags2);
        outMeshes.push_back(std::move(candidate));
    }

    Q6H_LOGI("Q10.9 NIF COVERAGE: model=%s shapeBlocks=%zu rendered=%zu shapeObjectReject=%zu shapeLoadReject=%zu fallbackDiffuse=%zu", resolved.c_str(), q1060ShapeBlocks, outMeshes.size(), q1060ShapeObjectFailures, q1060ShapeLoadFailures, q1060FallbackDiffuse);
    if (outMeshes.empty()) {
        Q6H_LOGW("Q6H NIF NO SUPPORTED STATIC SHAPE: %s blocks=%u", resolved.c_str(), header.numBlocks);
        return false;
    }
    Q6H_LOGI("Q6H NIF MODEL READY: model=%s visibleShapes=%zu", resolved.c_str(), outMeshes.size());
    return true;
}

bool LoadFo3StaticNif(const std::string& modelPath, Fo3StaticNifMesh& outMesh) {
    std::vector<Fo3StaticNifMesh> meshes;
    if (!LoadFo3StaticNifMeshes(modelPath, meshes) || meshes.empty()) return false;
    outMesh = std::move(meshes.front());
    return true;
}


namespace {

std::string Q210BlockName(const std::vector<uint8_t>& nif,
                          const NifHeader& header,
                          uint32_t block) {
    if (block >= header.numBlocks || header.blockSizes[block] < 4u)
        return {};
    const uint8_t* data = BlockData(nif, header, block);
    if (!data) return {};
    const uint32_t nameIndex = ReadLe32(data);
    if (nameIndex == INVALID_REF || nameIndex >= header.strings.size())
        return {};
    return header.strings[nameIndex];
}

bool Q210ParseSkinInstance(const std::vector<uint8_t>& nif,
                           const NifHeader& header,
                           uint32_t block,
                           std::vector<uint32_t>& bones) {
    bones.clear();
    if (block >= header.numBlocks) return false;
    const std::string& type = BlockType(header, block);
    if (type != "NiSkinInstance" &&
        type != "BSDismemberSkinInstance") return false;

    Cursor c(BlockData(nif, header, block), header.blockSizes[block]);
    uint32_t dataRef = INVALID_REF;
    uint32_t partitionRef = INVALID_REF;
    uint32_t skeletonRoot = INVALID_REF;
    uint32_t count = 0u;
    if (!c.U32(dataRef) || !c.U32(partitionRef) ||
        !c.U32(skeletonRoot) || !c.U32(count) ||
        count > MAX_BLOCKS) return false;
    bones.resize(count);
    for (uint32_t& bone : bones) {
        if (!c.U32(bone)) {
            bones.clear();
            return false;
        }
    }
    return true;
}

} // namespace

bool ProbeFo3NifSkin(const std::string& modelPath, Fo3NifSkinProbe& outProbe) {
    outProbe = {};
    std::vector<uint8_t> nif;
    std::string resolved;
    if (!LoadFalloutMeshFile(modelPath, nif, &resolved) ||
        nif.empty() || nif.size() > MAX_NIF_BYTES) {
        Q6H_LOGW("Q21.0 SKIN PROBE MISS: model=%s stage=bsa-load",
                 modelPath.c_str());
        return false;
    }

    NifHeader header;
    if (!ParseHeader(nif, header)) {
        Q6H_LOGW("Q21.0 SKIN PROBE MISS: model=%s stage=nif-header",
                 resolved.c_str());
        return false;
    }

    outProbe.loaded = true;
    outProbe.blocks = header.numBlocks;
    std::vector<uint32_t> referencedBoneBlocks;
    for (uint32_t block = 0u; block < header.numBlocks; ++block) {
        const std::string& type = BlockType(header, block);
        if (type == "NiNode" || type == "BSFadeNode") {
            ++outProbe.nodes;
            const std::string name = Q210BlockName(nif, header, block);
            if (!name.empty()) outProbe.nodeNames.push_back(name);
        } else if (type == "NiSkinData") {
            ++outProbe.skinDataBlocks;
        } else if (type == "NiSkinPartition") {
            ++outProbe.skinPartitions;
        } else if (type == "NiSkinInstance" ||
                   type == "BSDismemberSkinInstance") {
            ++outProbe.skinInstances;
            std::vector<uint32_t> bones;
            if (Q210ParseSkinInstance(nif, header, block, bones)) {
                for (uint32_t bone : bones) {
                    if (std::find(referencedBoneBlocks.begin(),
                                  referencedBoneBlocks.end(), bone) ==
                        referencedBoneBlocks.end()) {
                        referencedBoneBlocks.push_back(bone);
                    }
                }
            }
        }
    }

    outProbe.referencedBones =
        static_cast<uint32_t>(referencedBoneBlocks.size());
    for (uint32_t bone : referencedBoneBlocks) {
        const std::string name = Q210BlockName(nif, header, bone);
        if (!name.empty()) outProbe.boneNames.push_back(name);
    }

    std::string summary;
    const size_t shown = std::min<size_t>(outProbe.boneNames.size(), 24u);
    for (size_t i = 0u; i < shown; ++i) {
        if (!summary.empty()) summary += ",";
        summary += outProbe.boneNames[i];
    }
    if (summary.empty()) summary = "<none>";

    Q6H_LOGI("Q21.0 SKIN PROBE: model=%s blocks=%u nodes=%u skinInstances=%u skinData=%u skinPartitions=%u referencedBones=%u namedBones=%zu bones=%s",
             resolved.c_str(), outProbe.blocks, outProbe.nodes,
             outProbe.skinInstances, outProbe.skinDataBlocks,
             outProbe.skinPartitions, outProbe.referencedBones,
             outProbe.boneNames.size(), summary.c_str());
    return true;
}

#include "fo3-actor-animation-decode.inc"
