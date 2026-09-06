#include "fo3-chair-material-mesh.h"

#include <android/log.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include <zlib.h>

namespace {

constexpr const char* TAG = "FalloutQuest";
constexpr const char* BSA_PATH =
        "/data/user/0/com.falloutquest.app/files/Fallout3/Data/Fallout - Meshes.bsa";
constexpr uint32_t CHAIR_OFFSET = 181486972u;
constexpr uint32_t CHAIR_STORED_BYTES = 24172u;
constexpr uint32_t NIF_VERSION_FO3 = 0x14020007u;
constexpr uint32_t MAX_NIF_BYTES = 64u * 1024u * 1024u;
constexpr uint32_t MAX_BLOCKS = 100000u;
constexpr uint32_t MAX_STRINGS = 100000u;

#define Q5I_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q5I_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define Q5I_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

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

bool ReadExact(FILE* file, void* dst, size_t size) {
    return std::fread(dst, 1, size, file) == size;
}

bool LoadChairNif(std::vector<uint8_t>& nif) {
    FILE* file = std::fopen(BSA_PATH, "rb");
    if (!file) return false;
    if (fseeko(file, static_cast<off_t>(CHAIR_OFFSET), SEEK_SET) != 0) {
        std::fclose(file);
        return false;
    }

    uint8_t sizeBytes[4]{};
    if (!ReadExact(file, sizeBytes, sizeof(sizeBytes))) {
        std::fclose(file);
        return false;
    }
    const uint32_t originalSize = ReadLe32(sizeBytes);
    if (originalSize == 0 || originalSize > MAX_NIF_BYTES || CHAIR_STORED_BYTES <= 4u) {
        std::fclose(file);
        return false;
    }

    std::vector<uint8_t> compressed(CHAIR_STORED_BYTES - 4u);
    if (!ReadExact(file, compressed.data(), compressed.size())) {
        std::fclose(file);
        return false;
    }
    std::fclose(file);

    nif.resize(originalSize);
    uLongf outLen = static_cast<uLongf>(nif.size());
    const int result = uncompress(reinterpret_cast<Bytef*>(nif.data()), &outLen,
                                  reinterpret_cast<const Bytef*>(compressed.data()),
                                  static_cast<uLong>(compressed.size()));
    return result == Z_OK && outLen == originalSize;
}

class Cursor {
public:
    Cursor(const uint8_t* data, size_t size) : data_(data), size_(size) {}
    explicit Cursor(const std::vector<uint8_t>& data) : Cursor(data.data(), data.size()) {}

    size_t offset() const { return pos_; }
    size_t remaining() const { return size_ - pos_; }

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
    size_t blockDataOffset = 0;
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
    if (version != NIF_VERSION_FO3 || endian != 1 || out.numBlocks == 0 ||
        out.numBlocks > MAX_BLOCKS) return false;

    std::string author, processScript, exportScript;
    if (!c.U32(out.bsVersion) || !c.ExportString(author) ||
        !c.ExportString(processScript) || !c.ExportString(exportScript)) return false;
    if (out.bsVersion >= 103u) {
        std::string maxFilepath;
        if (!c.ExportString(maxFilepath)) return false;
    }

    uint16_t numBlockTypes = 0;
    if (!c.U16(numBlockTypes) || numBlockTypes == 0) return false;
    out.blockTypes.reserve(numBlockTypes);
    for (uint16_t i = 0; i < numBlockTypes; ++i) {
        std::string type;
        if (!c.SizedString(type)) return false;
        out.blockTypes.push_back(type);
    }

    out.blockTypeIndices.resize(out.numBlocks);
    for (uint32_t i = 0; i < out.numBlocks; ++i) {
        if (!c.U16(out.blockTypeIndices[i])) return false;
        if ((out.blockTypeIndices[i] & 0x7fffu) >= out.blockTypes.size()) return false;
    }

    out.blockSizes.resize(out.numBlocks);
    uint64_t totalBlockBytes = 0;
    for (uint32_t i = 0; i < out.numBlocks; ++i) {
        if (!c.U32(out.blockSizes[i])) return false;
        totalBlockBytes += out.blockSizes[i];
        if (totalBlockBytes > nif.size()) return false;
    }

    uint32_t numStrings = 0, maxStringLength = 0;
    if (!c.U32(numStrings) || !c.U32(maxStringLength) ||
        numStrings > MAX_STRINGS || maxStringLength > MAX_NIF_BYTES) return false;
    for (uint32_t i = 0; i < numStrings; ++i) {
        std::string value;
        if (!c.SizedString(value)) return false;
    }

    uint32_t numGroups = 0;
    if (!c.U32(numGroups) || numGroups > out.numBlocks ||
        !c.Skip(static_cast<size_t>(numGroups) * 4u)) return false;

    out.blockDataOffset = c.offset();
    return totalBlockBytes <= nif.size() - out.blockDataOffset;
}

bool ParseObjectNetPrefix(Cursor& c) {
    uint32_t nameIndex = 0;
    uint32_t numExtraData = 0;
    uint32_t controllerRef = 0;
    if (!c.U32(nameIndex) || !c.U32(numExtraData) || numExtraData > MAX_BLOCKS ||
        !c.Skip(static_cast<size_t>(numExtraData) * 4u) || !c.U32(controllerRef)) {
        return false;
    }
    return true;
}

bool ParseAvObjectPrefix(Cursor& c, const NifHeader& header,
                         Fo3NifTransform& transform,
                         std::vector<uint32_t>* propertyRefs = nullptr) {
    if (!ParseObjectNetPrefix(c)) return false;

    uint16_t flags = 0;
    if (!c.U16(flags)) return false;
    if (header.userVersion >= 11u && header.bsVersion > 26u) {
        uint16_t unknownShort = 0;
        if (!c.U16(unknownShort)) return false;
    }

    for (float& v : transform.translation) {
        if (!c.F32(v)) return false;
    }
    for (float& v : transform.rotation) {
        if (!c.F32(v)) return false;
    }
    if (!c.F32(transform.scale)) return false;

    if (header.userVersion <= 11u) {
        uint32_t numProperties = 0;
        if (!c.U32(numProperties) || numProperties > MAX_BLOCKS) return false;
        if (propertyRefs) propertyRefs->clear();
        for (uint32_t i = 0; i < numProperties; ++i) {
            uint32_t ref = 0;
            if (!c.U32(ref)) return false;
            if (propertyRefs) propertyRefs->push_back(ref);
        }
    }

    uint32_t collisionRef = 0;
    if (!c.U32(collisionRef)) return false;
    transform.valid = true;
    return true;
}

bool ParseRootNode(const uint8_t* data, size_t size, const NifHeader& header,
                   Fo3NifTransform& transform, std::vector<uint32_t>& children) {
    Cursor c(data, size);
    if (!ParseAvObjectPrefix(c, header, transform)) return false;

    uint32_t numChildren = 0;
    if (!c.U32(numChildren) || numChildren > MAX_BLOCKS) return false;
    children.resize(numChildren);
    for (uint32_t& child : children) {
        if (!c.U32(child)) return false;
    }

    uint32_t numEffects = 0;
    if (!c.U32(numEffects) || numEffects > MAX_BLOCKS ||
        !c.Skip(static_cast<size_t>(numEffects) * 4u)) return false;

    return c.remaining() == 0u;
}

bool ParseTriStripsObject(const uint8_t* data, size_t size, const NifHeader& header,
                          Fo3NifTransform& transform, uint32_t& dataRef,
                          std::vector<uint32_t>& propertyRefs) {
    Cursor c(data, size);
    if (!ParseAvObjectPrefix(c, header, transform, &propertyRefs)) return false;

    uint32_t skinRef = 0;
    if (!c.U32(dataRef) || !c.U32(skinRef)) return false;

    uint32_t numMaterials = 0;
    if (!c.U32(numMaterials) || numMaterials > 1024u) return false;
    if (!c.Skip(static_cast<size_t>(numMaterials) * 4u) ||
        !c.Skip(static_cast<size_t>(numMaterials) * 4u)) return false;

    uint32_t activeMaterial = 0;
    uint8_t dirtyFlag = 0;
    if (!c.U32(activeMaterial) || !c.U8(dirtyFlag)) return false;

    return c.remaining() == 0u;
}

bool ParseTextureSet(const uint8_t* data, size_t size,
                     std::string& diffuse, std::string& normal) {
    Cursor c(data, size);
    uint32_t numTextures = 0;
    if (!c.U32(numTextures) || numTextures == 0 || numTextures > 32u) return false;

    for (uint32_t i = 0; i < numTextures; ++i) {
        std::string value;
        if (!c.SizedString(value)) return false;
        if (i == 0u) diffuse = value;
        if (i == 1u) normal = value;
        Q5I_LOGI("Q5I TEXTURE SLOT: index=%u path=%s", i,
                  value.empty() ? "<empty>" : value.c_str());
    }
    return !diffuse.empty();
}

bool ParseMaterialProperty(const uint8_t* data, size_t size,
                           float& glossiness, float& alpha, float& emitMultiplier) {
    Cursor c(data, size);
    if (!ParseObjectNetPrefix(c)) return false;

    // Fallout 3 (20.2.0.7 / user 11 / BS 34) omits ambient + diffuse here.
    // Remaining layout is specular RGB, emissive RGB, glossiness, alpha, emit multiplier.
    if (!c.Skip(12u) || !c.Skip(12u) || !c.F32(glossiness) ||
        !c.F32(alpha) || !c.F32(emitMultiplier)) return false;
    return c.remaining() == 0u;
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

bool ParseTriStripsData(const uint8_t* data, size_t size,
                        Fo3MaterialChairMesh& outMesh,
                        uint32_t& outDeclaredTriangles,
                        uint32_t& outRawStripTriangles) {
    Cursor c(data, size);
    uint32_t groupId = 0;
    uint16_t numVertices = 0;
    uint8_t keepFlags = 0, compressFlags = 0, hasVertices = 0;
    if (!c.U32(groupId) || !c.U16(numVertices) || !c.U8(keepFlags) ||
        !c.U8(compressFlags) || !c.U8(hasVertices) || !hasVertices || numVertices == 0) {
        return false;
    }

    if (!ReadVec3Array(c, numVertices, outMesh.positions)) return false;

    uint16_t bsDataFlags = 0;
    uint8_t hasNormals = 0;
    if (!c.U16(bsDataFlags) || !c.U8(hasNormals) || !hasNormals) return false;
    if (!ReadVec3Array(c, numVertices, outMesh.normals)) return false;

    const bool hasTangents = (bsDataFlags & 0x1000u) != 0u;
    if (hasTangents) {
        if (!ReadVec3Array(c, numVertices, outMesh.tangents) ||
            !ReadVec3Array(c, numVertices, outMesh.bitangents)) return false;
    } else {
        Q5I_LOGE("Q5I chair geometry unexpectedly has no tangent basis");
        return false;
    }

    if (!c.Skip(16u)) return false; // bounding sphere

    uint8_t hasVertexColors = 0;
    if (!c.U8(hasVertexColors)) return false;
    if (hasVertexColors && !c.Skip(static_cast<size_t>(numVertices) * 16u)) return false;

    if ((bsDataFlags & 0x0001u) == 0u) return false;
    outMesh.texcoords.clear();
    outMesh.texcoords.reserve(static_cast<size_t>(numVertices) * 2u);
    for (uint16_t i = 0; i < numVertices; ++i) {
        float u = 0.0f, v = 0.0f;
        if (!c.F32(u) || !c.F32(v)) return false;
        outMesh.texcoords.push_back(u);
        outMesh.texcoords.push_back(v);
    }

    if (!c.Skip(2u) || !c.Skip(4u)) return false; // consistency + additional data ref

    uint16_t declaredTriangles = 0;
    uint16_t numStrips = 0;
    if (!c.U16(declaredTriangles) || !c.U16(numStrips) || numStrips == 0) return false;

    std::vector<uint16_t> stripLengths(numStrips);
    uint32_t rawStripTriangles = 0;
    for (uint16_t i = 0; i < numStrips; ++i) {
        if (!c.U16(stripLengths[i])) return false;
        if (stripLengths[i] >= 2u) rawStripTriangles += stripLengths[i] - 2u;
    }

    uint8_t hasPoints = 0;
    if (!c.U8(hasPoints) || !hasPoints) return false;

    outMesh.indices.clear();
    for (uint16_t strip = 0; strip < numStrips; ++strip) {
        const uint16_t length = stripLengths[strip];
        std::vector<uint16_t> points(length);
        for (uint16_t i = 0; i < length; ++i) {
            if (!c.U16(points[i]) || points[i] >= numVertices) return false;
        }
        for (uint16_t i = 2; i < length; ++i) {
            uint16_t a = points[i - 2u];
            uint16_t b = points[i - 1u];
            const uint16_t d = points[i];
            if ((i & 1u) != 0u) std::swap(a, b);
            if (a == b || b == d || a == d) continue;
            outMesh.indices.push_back(a);
            outMesh.indices.push_back(b);
            outMesh.indices.push_back(d);
        }
    }

    outDeclaredTriangles = declaredTriangles;
    outRawStripTriangles = rawStripTriangles;
    return !outMesh.indices.empty() && c.remaining() == 0u;
}

void ApplyPoint(const Fo3NifTransform& t, float& x, float& y, float& z) {
    if (!t.valid) return;
    const float sx = x * t.scale;
    const float sy = y * t.scale;
    const float sz = z * t.scale;
    const float rx = t.rotation[0] * sx + t.rotation[1] * sy + t.rotation[2] * sz;
    const float ry = t.rotation[3] * sx + t.rotation[4] * sy + t.rotation[5] * sz;
    const float rz = t.rotation[6] * sx + t.rotation[7] * sy + t.rotation[8] * sz;
    x = rx + t.translation[0];
    y = ry + t.translation[1];
    z = rz + t.translation[2];
}

void ApplyVector(const Fo3NifTransform& t, float& x, float& y, float& z) {
    if (!t.valid) return;
    const float rx = t.rotation[0] * x + t.rotation[1] * y + t.rotation[2] * z;
    const float ry = t.rotation[3] * x + t.rotation[4] * y + t.rotation[5] * z;
    const float rz = t.rotation[6] * x + t.rotation[7] * y + t.rotation[8] * z;
    x = rx;
    y = ry;
    z = rz;
}

void ApplyHierarchyTransforms(Fo3MaterialChairMesh& mesh, bool useRoot) {
    const size_t vertexCount = mesh.positions.size() / 3u;
    for (size_t i = 0; i < vertexCount; ++i) {
        float& px = mesh.positions[i * 3u + 0u];
        float& py = mesh.positions[i * 3u + 1u];
        float& pz = mesh.positions[i * 3u + 2u];
        ApplyPoint(mesh.shapeTransform, px, py, pz);
        if (useRoot) ApplyPoint(mesh.rootTransform, px, py, pz);

        std::vector<float>* vectors[] = {&mesh.normals, &mesh.tangents, &mesh.bitangents};
        for (std::vector<float>* values : vectors) {
            float& x = (*values)[i * 3u + 0u];
            float& y = (*values)[i * 3u + 1u];
            float& z = (*values)[i * 3u + 2u];
            ApplyVector(mesh.shapeTransform, x, y, z);
            if (useRoot) ApplyVector(mesh.rootTransform, x, y, z);
        }
    }
}

void LogTransform(const char* label, const Fo3NifTransform& t) {
    Q5I_LOGI("Q5I %s XFORM: valid=%d T=(%.4f %.4f %.4f) scale=%.4f R=[%.3f %.3f %.3f | %.3f %.3f %.3f | %.3f %.3f %.3f]",
              label, t.valid ? 1 : 0,
              t.translation[0], t.translation[1], t.translation[2], t.scale,
              t.rotation[0], t.rotation[1], t.rotation[2],
              t.rotation[3], t.rotation[4], t.rotation[5],
              t.rotation[6], t.rotation[7], t.rotation[8]);
}

} // namespace

bool LoadMegatonChairMaterialMesh(Fo3MaterialChairMesh& outMesh) {
    outMesh = {};

    std::vector<uint8_t> nif;
    if (!LoadChairNif(nif)) {
        Q5I_LOGE("Q5I chair NIF extraction failed");
        return false;
    }

    NifHeader header;
    if (!ParseHeader(nif, header)) {
        Q5I_LOGE("Q5I chair NIF header parse failed");
        return false;
    }

    bool haveGeometry = false;
    bool haveTextureSet = false;
    bool haveShape = false;
    bool haveRoot = false;
    bool haveMaterial = false;
    uint32_t declaredTriangles = 0;
    uint32_t rawStripTriangles = 0;
    uint32_t shapeDataRef = 0xffffffffu;
    std::vector<uint32_t> shapeProperties;
    std::vector<uint32_t> rootChildren;

    size_t offset = header.blockDataOffset;
    for (uint32_t block = 0; block < header.numBlocks; ++block) {
        const uint32_t blockSize = header.blockSizes[block];
        if (blockSize > nif.size() - offset) return false;
        const uint16_t typeIndex = header.blockTypeIndices[block] & 0x7fffu;
        const std::string& type = header.blockTypes[typeIndex];
        const uint8_t* blockData = nif.data() + offset;

        if (type == "BSFadeNode") {
            if (ParseRootNode(blockData, blockSize, header,
                              outMesh.rootTransform, rootChildren)) {
                haveRoot = true;
                Q5I_LOGI("Q5I ROOT NODE: block=%u children=%zu", block, rootChildren.size());
            }
        } else if (type == "NiTriStrips") {
            if (!ParseTriStripsObject(blockData, blockSize, header,
                                      outMesh.shapeTransform, shapeDataRef,
                                      shapeProperties)) {
                Q5I_LOGE("Q5I failed to parse NiTriStrips object block=%u", block);
                return false;
            }
            haveShape = true;
            Q5I_LOGI("Q5I SHAPE LINKS: block=%u data=%u properties=%zu first=%u second=%u",
                      block, shapeDataRef, shapeProperties.size(),
                      shapeProperties.size() > 0 ? shapeProperties[0] : 0xffffffffu,
                      shapeProperties.size() > 1 ? shapeProperties[1] : 0xffffffffu);
        } else if (type == "BSShaderTextureSet") {
            std::string diffuse, normal;
            if (ParseTextureSet(blockData, blockSize, diffuse, normal)) {
                outMesh.diffuseTexturePath = diffuse;
                outMesh.normalTexturePath = normal;
                haveTextureSet = true;
            }
        } else if (type == "NiMaterialProperty") {
            if (ParseMaterialProperty(blockData, blockSize,
                                      outMesh.glossiness, outMesh.alpha,
                                      outMesh.emitMultiplier)) {
                haveMaterial = true;
                Q5I_LOGI("Q5I MATERIAL: block=%u gloss=%.3f alpha=%.3f emit=%.3f",
                          block, outMesh.glossiness, outMesh.alpha,
                          outMesh.emitMultiplier);
            }
        } else if (type == "NiTriStripsData") {
            if (!ParseTriStripsData(blockData, blockSize, outMesh,
                                    declaredTriangles, rawStripTriangles)) {
                Q5I_LOGE("Q5I failed to decode NiTriStripsData block=%u", block);
                return false;
            }
            haveGeometry = true;
            if (shapeDataRef != 0xffffffffu && shapeDataRef != block) {
                Q5I_LOGW("Q5I shape data ref=%u but decoded geometry block=%u", shapeDataRef, block);
            }
        }

        offset += blockSize;
    }

    if (!haveGeometry || !haveTextureSet || !haveShape ||
        outMesh.normalTexturePath.empty()) {
        Q5I_LOGE("Q5I incomplete chair material mesh geometry=%d textures=%d shape=%d normalPath=%d",
                  haveGeometry ? 1 : 0, haveTextureSet ? 1 : 0,
                  haveShape ? 1 : 0, outMesh.normalTexturePath.empty() ? 0 : 1);
        return false;
    }

    const size_t vertexCount = outMesh.positions.size() / 3u;
    if (outMesh.normals.size() / 3u != vertexCount ||
        outMesh.tangents.size() / 3u != vertexCount ||
        outMesh.bitangents.size() / 3u != vertexCount ||
        outMesh.texcoords.size() / 2u != vertexCount) {
        Q5I_LOGE("Q5I vertex attribute count mismatch");
        return false;
    }

    bool shapeIsRootChild = false;
    if (haveRoot) {
        // chair03.nif's visible NiTriStrips is block 9. Only apply the root transform
        // when that block is actually attached to the root node.
        shapeIsRootChild = std::find(rootChildren.begin(), rootChildren.end(), 9u) != rootChildren.end();
    }

    LogTransform("ROOT", outMesh.rootTransform);
    LogTransform("SHAPE", outMesh.shapeTransform);
    Q5I_LOGI("Q5I HIERARCHY: rootParsed=%d shapeIsRootChild=%d",
              haveRoot ? 1 : 0, shapeIsRootChild ? 1 : 0);

    ApplyHierarchyTransforms(outMesh, shapeIsRootChild);

    const uint32_t generatedTriangles = static_cast<uint32_t>(outMesh.indices.size() / 3u);
    const uint32_t degenerateTriangles = rawStripTriangles > generatedTriangles
            ? rawStripTriangles - generatedTriangles : 0u;

    Q5I_LOGI("Q5I MESH READY: vertices=%zu normals=%zu tangents=%zu uvs=%zu declared=%u rawStrip=%u drawable=%u degeneratesSkipped=%u diffuse=%s normal=%s material=%s",
              vertexCount,
              outMesh.normals.size() / 3u,
              outMesh.tangents.size() / 3u,
              outMesh.texcoords.size() / 2u,
              declaredTriangles, rawStripTriangles, generatedTriangles,
              degenerateTriangles,
              outMesh.diffuseTexturePath.c_str(),
              outMesh.normalTexturePath.c_str(),
              haveMaterial ? "REAL" : "DEFAULT");
    return true;
}
