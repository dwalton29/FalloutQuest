#include "fo3-nif-collision.h"
#include "fo3-bsa-reader.h"

#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr const char* TAG = "FalloutQuest";
constexpr uint32_t NIF_VERSION_FO3 = 0x14020007u;
constexpr uint32_t MAX_NIF_BYTES = 128u * 1024u * 1024u;
constexpr uint32_t MAX_BLOCKS = 100000u;
constexpr uint32_t MAX_STRINGS = 100000u;
constexpr uint32_t MAX_COLLISION_VERTICES = 4000000u;
constexpr uint32_t MAX_COLLISION_TRIANGLES = 6000000u;
constexpr uint32_t INVALID_REF = 0xffffffffu;
constexpr float FO3_HAVOK_TO_NIF = 7.0f;

#define Q6E_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q6E_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define Q6E_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

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

float HalfToFloat(uint16_t h) {
    const uint32_t sign = static_cast<uint32_t>(h & 0x8000u) << 16u;
    uint32_t exponent = (h >> 10u) & 0x1fu;
    uint32_t mantissa = h & 0x03ffu;
    uint32_t bits = 0;

    if (exponent == 0u) {
        if (mantissa == 0u) {
            bits = sign;
        } else {
            int shift = 0;
            while ((mantissa & 0x0400u) == 0u) {
                mantissa <<= 1u;
                ++shift;
            }
            mantissa &= 0x03ffu;
            const uint32_t exponent32 = static_cast<uint32_t>(127 - 15 - shift);
            bits = sign | (exponent32 << 23u) | (mantissa << 13u);
        }
    } else if (exponent == 0x1fu) {
        bits = sign | 0x7f800000u | (mantissa << 13u);
    } else {
        exponent = exponent + (127u - 15u);
        bits = sign | (exponent << 23u) | (mantissa << 13u);
    }

    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

class Cursor {
public:
    Cursor(const uint8_t* data, size_t size) : data_(data), size_(size) {}
    explicit Cursor(const std::vector<uint8_t>& data) : Cursor(data.data(), data.size()) {}

    size_t remaining() const { return pos_ <= size_ ? size_ - pos_ : 0u; }
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

    if (totalBytes > nif.size() - c.offset()) return false;
    out.blockOffsets.resize(out.numBlocks);
    size_t offset = c.offset();
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
    uint32_t extraCount = 0;
    uint32_t controller = 0;
    return c.U32(nameIndex) && c.U32(extraCount) && extraCount <= MAX_BLOCKS &&
           c.Skip(static_cast<size_t>(extraCount) * 4u) && c.U32(controller);
}

bool ParseAvTransform(const uint8_t* data, size_t size, const NifHeader& header,
                      NifTransform& out) {
    Cursor c(data, size);
    if (!ParseObjectNetPrefix(c)) return false;

    uint16_t flags = 0;
    if (!c.U16(flags)) return false;
    if (header.userVersion >= 11u && header.bsVersion > 26u) {
        uint16_t unknown = 0;
        if (!c.U16(unknown)) return false;
    }
    for (float& v : out.translation) if (!c.F32(v)) return false;
    for (float& v : out.rotation) if (!c.F32(v)) return false;
    if (!c.F32(out.scale)) return false;
    out.valid = std::isfinite(out.scale) && std::fabs(out.scale) < 10000.0f;
    return out.valid;
}

void ApplyNifPoint(const NifTransform& t, float& x, float& y, float& z) {
    if (!t.valid) return;
    const float sx = x * t.scale, sy = y * t.scale, sz = z * t.scale;
    const float rx = t.rotation[0] * sx + t.rotation[1] * sy + t.rotation[2] * sz;
    const float ry = t.rotation[3] * sx + t.rotation[4] * sy + t.rotation[5] * sz;
    const float rz = t.rotation[6] * sx + t.rotation[7] * sy + t.rotation[8] * sz;
    x = rx + t.translation[0];
    y = ry + t.translation[1];
    z = rz + t.translation[2];
}

bool ApplyRigidBodyT(const uint8_t* data, size_t size,
                     std::vector<float>& positions) {
    // FO3 bhkRigidBodyT stores the rigid-body CInfo translation/rotation here.
    // Translation is in Havok units; packed vertices are also Havok units.
    if (!data || size < 84u) return false;
    float tx = ReadLeFloat(data + 52u);
    float ty = ReadLeFloat(data + 56u);
    float tz = ReadLeFloat(data + 60u);
    float qx = ReadLeFloat(data + 68u);
    float qy = ReadLeFloat(data + 72u);
    float qz = ReadLeFloat(data + 76u);
    float qw = ReadLeFloat(data + 80u);
    if (!std::isfinite(tx) || !std::isfinite(ty) || !std::isfinite(tz) ||
        !std::isfinite(qx) || !std::isfinite(qy) || !std::isfinite(qz) ||
        !std::isfinite(qw)) return false;

    const float qLen = std::sqrt(qx*qx + qy*qy + qz*qz + qw*qw);
    if (qLen < 0.5f || qLen > 1.5f ||
        std::fabs(tx) > 100000.0f || std::fabs(ty) > 100000.0f || std::fabs(tz) > 100000.0f) {
        return false;
    }
    qx /= qLen; qy /= qLen; qz /= qLen; qw /= qLen;

    const float tnx = tx * FO3_HAVOK_TO_NIF;
    const float tny = ty * FO3_HAVOK_TO_NIF;
    const float tnz = tz * FO3_HAVOK_TO_NIF;
    for (size_t i = 0; i + 2u < positions.size(); i += 3u) {
        const float x = positions[i], y = positions[i + 1u], z = positions[i + 2u];
        // Quaternion-vector rotation: v' = v + 2w(q x v) + 2(q x (q x v)).
        const float cx = qy*z - qz*y;
        const float cy = qz*x - qx*z;
        const float cz = qx*y - qy*x;
        const float ccx = qy*cz - qz*cy;
        const float ccy = qz*cx - qx*cz;
        const float ccz = qx*cy - qy*cx;
        positions[i]      = x + 2.0f * (qw*cx + ccx) + tnx;
        positions[i + 1u] = y + 2.0f * (qw*cy + ccy) + tny;
        positions[i + 2u] = z + 2.0f * (qw*cz + ccz) + tnz;
    }
    return true;
}

bool ParsePackedData(const uint8_t* data, size_t size,
                     Fo3NifCollisionMesh& out) {
    Cursor c(data, size);
    uint32_t numTriangles = 0;
    if (!c.U32(numTriangles) || numTriangles == 0u ||
        numTriangles > MAX_COLLISION_TRIANGLES) return false;

    struct RawTriangle { uint16_t a, b, d; };
    std::vector<RawTriangle> triangles;
    triangles.reserve(numTriangles);
    for (uint32_t i = 0; i < numTriangles; ++i) {
        uint16_t a = 0, b = 0, d = 0, welding = 0;
        if (!c.U16(a) || !c.U16(b) || !c.U16(d) || !c.U16(welding)) return false;
        triangles.push_back({a, b, d});
    }

    uint32_t numVertices = 0;
    uint8_t compressed = 0;
    if (!c.U32(numVertices) || numVertices == 0u ||
        numVertices > MAX_COLLISION_VERTICES || !c.U8(compressed)) return false;
    out.compressedVertices = compressed != 0u;
    out.positions.clear();
    out.positions.reserve(static_cast<size_t>(numVertices) * 3u);

    for (uint32_t i = 0; i < numVertices; ++i) {
        float x = 0.0f, y = 0.0f, z = 0.0f;
        if (!compressed) {
            if (!c.F32(x) || !c.F32(y) || !c.F32(z)) return false;
        } else {
            uint16_t hx = 0, hy = 0, hz = 0;
            if (!c.U16(hx) || !c.U16(hy) || !c.U16(hz)) return false;
            x = HalfToFloat(hx);
            y = HalfToFloat(hy);
            z = HalfToFloat(hz);
        }
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
            std::fabs(x) > 100000.0f || std::fabs(y) > 100000.0f || std::fabs(z) > 100000.0f) {
            return false;
        }
        out.positions.push_back(x * FO3_HAVOK_TO_NIF);
        out.positions.push_back(y * FO3_HAVOK_TO_NIF);
        out.positions.push_back(z * FO3_HAVOK_TO_NIF);
    }

    uint16_t subShapes = 0;
    if (!c.U16(subShapes)) return false;
    out.subShapeCount = subShapes;
    const size_t subShapeBytes = static_cast<size_t>(subShapes) * 12u;
    if (!c.Skip(subShapeBytes)) return false;
    if (c.remaining() != 0u) {
        Q6E_LOGW("Q6E COLLISION DATA TRAILING: bytes=%zu", c.remaining());
        return false;
    }

    out.indices.clear();
    out.indices.reserve(static_cast<size_t>(numTriangles) * 3u);
    for (const RawTriangle& tri : triangles) {
        if (tri.a >= numVertices || tri.b >= numVertices || tri.d >= numVertices) return false;
        if (tri.a == tri.b || tri.b == tri.d || tri.a == tri.d) continue;
        out.indices.push_back(tri.a);
        out.indices.push_back(tri.b);
        out.indices.push_back(tri.d);
    }
    return !out.indices.empty();
}

void Bounds(const std::vector<float>& positions, float minimum[3], float maximum[3]) {
    minimum[0] = minimum[1] = minimum[2] = 1e30f;
    maximum[0] = maximum[1] = maximum[2] = -1e30f;
    for (size_t i = 0; i + 2u < positions.size(); i += 3u) {
        minimum[0] = std::min(minimum[0], positions[i]);
        minimum[1] = std::min(minimum[1], positions[i + 1u]);
        minimum[2] = std::min(minimum[2], positions[i + 2u]);
        maximum[0] = std::max(maximum[0], positions[i]);
        maximum[1] = std::max(maximum[1], positions[i + 1u]);
        maximum[2] = std::max(maximum[2], positions[i + 2u]);
    }
}

} // namespace

bool LoadFo3NifCollisionMeshes(const std::string& modelPath,
                               std::vector<Fo3NifCollisionMesh>& outMeshes) {
    outMeshes.clear();

    std::vector<uint8_t> nif;
    std::string resolved;
    if (!LoadFalloutMeshFile(modelPath, nif, &resolved) || nif.empty() ||
        nif.size() > MAX_NIF_BYTES) return false;

    NifHeader header;
    if (!ParseHeader(nif, header)) {
        Q6E_LOGW("Q6E COLLISION HEADER UNSUPPORTED: model=%s", resolved.c_str());
        return false;
    }

    size_t totalTriangles = 0;
    for (uint32_t coBlock = 0; coBlock < header.numBlocks; ++coBlock) {
        const std::string& coType = BlockType(header, coBlock);
        if (coType != "bhkCollisionObject" && coType != "bhkBlendCollisionObject" &&
            coType != "bhkPCollisionObject" && coType != "bhkSPCollisionObject") continue;

        const uint8_t* co = BlockData(nif, header, coBlock);
        const size_t coSize = header.blockSizes[coBlock];
        if (!co || coSize < 10u) continue;
        const uint32_t targetRef = ReadLe32(co + 0u);
        const uint32_t bodyRef = ReadLe32(co + 6u);
        if (bodyRef >= header.numBlocks) continue;

        const std::string& bodyType = BlockType(header, bodyRef);
        if (bodyType != "bhkRigidBody" && bodyType != "bhkRigidBodyT") continue;
        const uint8_t* body = BlockData(nif, header, bodyRef);
        const size_t bodySize = header.blockSizes[bodyRef];
        if (!body || bodySize < 4u) continue;

        uint32_t shapeRef = ReadLe32(body + 0u);
        if (shapeRef >= header.numBlocks) continue;

        uint32_t moppRef = INVALID_REF;
        uint32_t packedRef = INVALID_REF;
        const std::string& shapeType = BlockType(header, shapeRef);
        if (shapeType == "bhkMoppBvTreeShape") {
            moppRef = shapeRef;
            const uint8_t* mopp = BlockData(nif, header, moppRef);
            if (!mopp || header.blockSizes[moppRef] < 4u) continue;
            packedRef = ReadLe32(mopp + 0u);
        } else if (shapeType == "bhkPackedNiTriStripsShape") {
            packedRef = shapeRef;
        } else {
            Q6E_LOGW("Q6E COLLISION SHAPE UNSUPPORTED: model=%s co=%u body=%u shape=%u type=%s",
                     resolved.c_str(), coBlock, bodyRef, shapeRef, shapeType.c_str());
            continue;
        }

        if (packedRef >= header.numBlocks ||
            BlockType(header, packedRef) != "bhkPackedNiTriStripsShape") continue;
        const uint8_t* packed = BlockData(nif, header, packedRef);
        if (!packed || header.blockSizes[packedRef] < 56u) continue;
        const uint32_t dataRef = ReadLe32(packed + 52u);
        if (dataRef >= header.numBlocks ||
            BlockType(header, dataRef) != "hkPackedNiTriStripsData") continue;

        Q6E_LOGI("Q6E COLLISION CHAIN: model=%s co=%u target=%u body=%u bodyType=%s mopp=%u packed=%u data=%u",
                 resolved.c_str(), coBlock, targetRef, bodyRef, bodyType.c_str(),
                 moppRef, packedRef, dataRef);

        Fo3NifCollisionMesh mesh;
        mesh.modelPath = resolved;
        mesh.collisionObjectBlock = coBlock;
        mesh.bodyBlock = bodyRef;
        mesh.moppBlock = moppRef;
        mesh.packedShapeBlock = packedRef;
        mesh.dataBlock = dataRef;
        const uint8_t* data = BlockData(nif, header, dataRef);
        if (!data || !ParsePackedData(data, header.blockSizes[dataRef], mesh)) {
            Q6E_LOGW("Q6E COLLISION DATA FAILED: model=%s data=%u bytes=%u",
                     resolved.c_str(), dataRef, header.blockSizes[dataRef]);
            continue;
        }

        if (bodyType == "bhkRigidBodyT") {
            mesh.bodyTransformApplied = ApplyRigidBodyT(body, bodySize, mesh.positions);
            if (!mesh.bodyTransformApplied) {
                Q6E_LOGW("Q6E COLLISION BODY T INVALID: model=%s body=%u bytes=%zu; using packed coordinates",
                         resolved.c_str(), bodyRef, bodySize);
            }
        }

        if (targetRef < header.numBlocks) {
            NifTransform targetTransform;
            const uint8_t* target = BlockData(nif, header, targetRef);
            if (target && ParseAvTransform(target, header.blockSizes[targetRef], header, targetTransform)) {
                for (size_t i = 0; i + 2u < mesh.positions.size(); i += 3u) {
                    ApplyNifPoint(targetTransform,
                                  mesh.positions[i], mesh.positions[i + 1u], mesh.positions[i + 2u]);
                }
            }
        }

        float minimum[3], maximum[3];
        Bounds(mesh.positions, minimum, maximum);
        const size_t triangles = mesh.indices.size() / 3u;
        totalTriangles += triangles;
        Q6E_LOGI("Q6E COLLISION DATA: model=%s vertices=%zu triangles=%zu subShapes=%u compressed=%d bodyTransform=%d bounds=[%.2f %.2f %.2f]-[%.2f %.2f %.2f]",
                 resolved.c_str(), mesh.positions.size() / 3u, triangles,
                 static_cast<unsigned>(mesh.subShapeCount),
                 mesh.compressedVertices ? 1 : 0, mesh.bodyTransformApplied ? 1 : 0,
                 minimum[0], minimum[1], minimum[2],
                 maximum[0], maximum[1], maximum[2]);
        outMeshes.push_back(std::move(mesh));
    }

    if (outMeshes.empty()) {
        Q6E_LOGW("Q6E COLLISION MODEL MISS: model=%s blocks=%u", resolved.c_str(), header.numBlocks);
        return false;
    }

    Q6E_LOGI("Q6E COLLISION MODEL READY: model=%s meshes=%zu triangles=%zu",
             resolved.c_str(), outMeshes.size(), totalTriangles);
    return true;
}
