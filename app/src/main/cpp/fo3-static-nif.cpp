#include "fo3-static-nif.h"
#include "fo3-bsa-reader.h"

#include <android/log.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr const char* TAG = "FalloutQuest";
constexpr uint32_t NIF_VERSION_FO3 = 0x14020007u;
constexpr uint32_t MAX_NIF_BYTES = 128u * 1024u * 1024u;
constexpr uint32_t MAX_BLOCKS = 100000u;
constexpr uint32_t MAX_STRINGS = 100000u;
constexpr uint32_t INVALID_REF = 0xffffffffu;

#define Q6A_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q6A_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define Q6A_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

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
    for (uint32_t i = 0; i < numStrings; ++i) {
        std::string ignored;
        if (!c.SizedString(ignored)) return false;
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

    uint32_t skinRef = INVALID_REF;
    if (!c.U32(out.dataRef) || !c.U32(skinRef)) return false;

    uint32_t numMaterials = 0;
    if (!c.U32(numMaterials) || numMaterials > 4096u) return false;
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

bool ParseMaterialProperty(const uint8_t* data, size_t size,
                           float& glossiness, float& alpha) {
    Cursor c(data, size);
    if (!ParseObjectNetPrefix(c)) return false;
    float emit = 1.0f;
    if (!c.Skip(12u) || !c.Skip(12u) || !c.F32(glossiness) ||
        !c.F32(alpha) || !c.F32(emit)) return false;
    return c.remaining() == 0u;
}

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
    if (hasVertexColors && !c.Skip(static_cast<size_t>(numVertices) * 16u)) return false;

    const uint16_t uvSets = dataFlags & 0x003fu;
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
    Q6A_LOGI("Q6A NIF STRIPS: declared=%u raw=%u drawable=%u degenerates=%u",
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

    Q6A_LOGI("Q6A NIF TRISHAPE: declared=%u drawable=%zu",
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

        if (type == "BSShaderPPLightingProperty") {
            uint32_t refOut = INVALID_REF;
            if (ParseShaderTextureRef(prop, header.blockSizes[ref], header, refOut)) {
                textureSetRef = refOut;
            }
        } else if (type == "NiMaterialProperty") {
            ParseMaterialProperty(prop, header.blockSizes[ref],
                                  candidate.glossiness, candidate.alpha);
        } else if (type == "NiAlphaProperty") {
            ParseAlphaProperty(prop, header.blockSizes[ref],
                               candidate.alphaBlend, candidate.alphaTest,
                               candidate.alphaThreshold);
        }
    }

    if (textureSetRef >= header.numBlocks || BlockType(header, textureSetRef) != "BSShaderTextureSet") {
        uint32_t onlyTextureSet = INVALID_REF;
        for (uint32_t block = 0; block < header.numBlocks; ++block) {
            if (BlockType(header, block) == "BSShaderTextureSet") {
                if (onlyTextureSet != INVALID_REF) {
                    onlyTextureSet = INVALID_REF;
                    break;
                }
                onlyTextureSet = block;
            }
        }
        textureSetRef = onlyTextureSet;
    }

    if (textureSetRef < header.numBlocks) {
        ParseTextureSet(BlockData(nif, header, textureSetRef),
                        header.blockSizes[textureSetRef],
                        candidate.diffuseTexturePath, candidate.normalTexturePath);
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
    }

    std::vector<NifTransform> ancestors;
    CollectAncestorTransforms(nif, header, shape.block, ancestors);
    ApplyTransforms(candidate, shape.transform, ancestors, root);
    mesh = std::move(candidate);
    return true;
}

} // namespace

bool LoadFo3StaticNif(const std::string& modelPath, Fo3StaticNifMesh& outMesh) {
    outMesh = {};
    outMesh.modelPath = modelPath;

    std::vector<uint8_t> nif;
    std::string resolved;
    if (!LoadFalloutMeshFile(modelPath, nif, &resolved)) return false;
    if (nif.empty() || nif.size() > MAX_NIF_BYTES) return false;

    NifHeader header;
    if (!ParseHeader(nif, header)) {
        Q6A_LOGW("Q6A NIF UNSUPPORTED HEADER: %s", resolved.c_str());
        return false;
    }

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

    for (uint32_t block = 0; block < header.numBlocks; ++block) {
        const std::string& type = BlockType(header, block);
        if (type != "NiTriStrips" && type != "NiTriShape") continue;

        ShapeObject shape;
        shape.block = block;
        if (!ParseShapeObject(BlockData(nif, header, block), header.blockSizes[block],
                              header, shape)) continue;

        const bool directRootChild = haveRoot &&
            std::find(rootChildren.begin(), rootChildren.end(), block) != rootChildren.end();
        Fo3StaticNifMesh candidate;
        if (!TryLoadShape(nif, header, shape,
                          directRootChild ? &rootTransform : nullptr,
                          candidate)) continue;

        candidate.modelPath = resolved;
        outMesh = std::move(candidate);
        Q6A_LOGI("Q6A NIF READY: model=%s shape=%u type=%s data=%u vertices=%zu triangles=%zu diffuse=%s normal=%s rootTransform=%d",
                 resolved.c_str(), block, type.c_str(), shape.dataRef,
                 outMesh.positions.size() / 3u, outMesh.indices.size() / 3u,
                 outMesh.diffuseTexturePath.empty() ? "<none>" : outMesh.diffuseTexturePath.c_str(),
                 outMesh.normalTexturePath.empty() ? "<none>" : outMesh.normalTexturePath.c_str(),
                 directRootChild ? 1 : 0);
        return true;
    }

    Q6A_LOGW("Q6A NIF NO SUPPORTED STATIC SHAPE: %s blocks=%u", resolved.c_str(), header.numBlocks);
    return false;
}
