#pragma once

#include "fo3-bsa-reader.h"

#include <android/log.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <utility>
#include <vector>

enum class Fo3CollisionShapeKindQ6F : uint8_t {
    TriangleMesh = 0,
    ConvexHull,
    Box,
    Sphere,
    Capsule,
};

inline const char* Fo3CollisionShapeKindNameQ6F(Fo3CollisionShapeKindQ6F kind) {
    switch (kind) {
        case Fo3CollisionShapeKindQ6F::TriangleMesh: return "TriangleMesh";
        case Fo3CollisionShapeKindQ6F::ConvexHull: return "ConvexHull";
        case Fo3CollisionShapeKindQ6F::Box: return "Box";
        case Fo3CollisionShapeKindQ6F::Sphere: return "Sphere";
        case Fo3CollisionShapeKindQ6F::Capsule: return "Capsule";
    }
    return "Unknown";
}

// Q6F collision output. positions/indices are an exact surface for authored
// triangle/convex data and a debug tessellation for analytic primitives.
// Primitive parameters remain exact in local NIF units; localToModel maps them
// into the same model space used by visible NIF geometry.
struct Fo3NifCollisionShapeQ6F {
    Fo3CollisionShapeKindQ6F kind = Fo3CollisionShapeKindQ6F::TriangleMesh;
    std::vector<float> positions;
    std::vector<uint32_t> indices;
    std::string modelPath;
    std::string sourceShapeType;

    uint32_t collisionObjectBlock = 0xffffffffu;
    uint32_t targetBlock = 0xffffffffu;
    uint32_t bodyBlock = 0xffffffffu;
    uint32_t sourceShapeBlock = 0xffffffffu;
    uint32_t dataBlock = 0xffffffffu;

    bool compressedVertices = false;
    bool bodyTransformApplied = false;
    bool nestedTransformApplied = false;

    // Exact analytic primitive data in local NIF units.
    float radiusNif = 0.0f;
    float radius2Nif = 0.0f;
    float halfExtentsNif[3]{0.0f, 0.0f, 0.0f};
    float pointANif[3]{0.0f, 0.0f, 0.0f};
    float pointBNif[3]{0.0f, 0.0f, 0.0f};

    // Row-major affine matrix. Applies local primitive coordinates to model NIF
    // coordinates. For triangle/convex meshes, positions have already had this
    // matrix applied too.
    float localToModel[16]{
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1,
    };
};

namespace fo3_q6f_collision_detail {

constexpr const char* TAG = "FalloutQuest";
constexpr uint32_t NIF_VERSION_FO3 = 0x14020007u;
constexpr uint32_t MAX_NIF_BYTES = 128u * 1024u * 1024u;
constexpr uint32_t MAX_BLOCKS = 100000u;
constexpr uint32_t MAX_STRINGS = 100000u;
constexpr uint32_t MAX_COLLISION_VERTICES = 4000000u;
constexpr uint32_t MAX_COLLISION_TRIANGLES = 6000000u;
constexpr uint32_t INVALID_REF = 0xffffffffu;
constexpr float FO3_HAVOK_TO_NIF = 7.0f;
constexpr float PI = 3.14159265358979323846f;

#define Q6F_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q6F_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)

inline uint16_t ReadLe16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8);
}

inline uint32_t ReadLe32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

inline float ReadLeFloat(const uint8_t* p) {
    const uint32_t bits = ReadLe32(p);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline float HalfToFloat(uint16_t h) {
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
            bits = sign |
                   (static_cast<uint32_t>(127 - 15 - shift) << 23u) |
                   (mantissa << 13u);
        }
    } else if (exponent == 0x1fu) {
        bits = sign | 0x7f800000u | (mantissa << 13u);
    } else {
        bits = sign | ((exponent + (127u - 15u)) << 23u) | (mantissa << 13u);
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
    bool Skip(size_t n) { if (n > remaining()) return false; pos_ += n; return true; }
    bool U8(uint8_t& v) { if (remaining() < 1u) return false; v = data_[pos_++]; return true; }
    bool U16(uint16_t& v) { if (remaining() < 2u) return false; v = ReadLe16(data_ + pos_); pos_ += 2u; return true; }
    bool U32(uint32_t& v) { if (remaining() < 4u) return false; v = ReadLe32(data_ + pos_); pos_ += 4u; return true; }
    bool F32(float& v) { if (remaining() < 4u) return false; v = ReadLeFloat(data_ + pos_); pos_ += 4u; return true; }
    bool SizedString(std::string& out) {
        uint32_t n = 0;
        if (!U32(n) || n > remaining() || n > MAX_NIF_BYTES) return false;
        out.assign(reinterpret_cast<const char*>(data_ + pos_), n);
        pos_ += n;
        return true;
    }
    bool ExportString(std::string& out) {
        uint8_t n = 0;
        if (!U8(n) || n > remaining()) return false;
        out.assign(reinterpret_cast<const char*>(data_ + pos_), n);
        pos_ += n;
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

struct Mat4 {
    float m[16]{
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1,
    };
};

inline Mat4 Identity() { return Mat4{}; }

inline Mat4 Mul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            r.m[row * 4 + col] =
                a.m[row * 4 + 0] * b.m[0 * 4 + col] +
                a.m[row * 4 + 1] * b.m[1 * 4 + col] +
                a.m[row * 4 + 2] * b.m[2 * 4 + col] +
                a.m[row * 4 + 3] * b.m[3 * 4 + col];
        }
    }
    return r;
}

inline void TransformPoint(const Mat4& m, float& x, float& y, float& z) {
    const float ox = x, oy = y, oz = z;
    x = m.m[0] * ox + m.m[1] * oy + m.m[2] * oz + m.m[3];
    y = m.m[4] * ox + m.m[5] * oy + m.m[6] * oz + m.m[7];
    z = m.m[8] * ox + m.m[9] * oy + m.m[10] * oz + m.m[11];
}

inline void ApplyMatrix(std::vector<float>& positions, const Mat4& m) {
    for (size_t i = 0; i + 2u < positions.size(); i += 3u) {
        TransformPoint(m, positions[i], positions[i + 1u], positions[i + 2u]);
    }
}

inline void StoreMatrix(const Mat4& m, float out[16]) {
    std::memcpy(out, m.m, sizeof(m.m));
}

inline bool ParseHeader(const std::vector<uint8_t>& nif, NifHeader& out) {
    Cursor c(nif);
    std::string line;
    if (!c.HeaderLine(line) ||
        line.rfind("Gamebryo File Format, Version 20.2.0.7", 0) != 0) return false;
    uint32_t version = 0;
    uint8_t endian = 0;
    if (!c.U32(version) || !c.U8(endian) || !c.U32(out.userVersion) || !c.U32(out.numBlocks)) return false;
    if (version != NIF_VERSION_FO3 || endian != 1u || out.numBlocks == 0u || out.numBlocks > MAX_BLOCKS) return false;

    std::string author, processScript, exportScript;
    if (!c.U32(out.bsVersion) || !c.ExportString(author) || !c.ExportString(processScript) || !c.ExportString(exportScript)) return false;
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
    if (!c.U32(numStrings) || !c.U32(maxStringLength) || numStrings > MAX_STRINGS || maxStringLength > MAX_NIF_BYTES) return false;
    for (uint32_t i = 0; i < numStrings; ++i) {
        std::string ignored;
        if (!c.SizedString(ignored)) return false;
    }

    uint32_t numGroups = 0;
    if (!c.U32(numGroups) || numGroups > out.numBlocks || !c.Skip(static_cast<size_t>(numGroups) * 4u)) return false;
    if (totalBytes > nif.size() - c.offset()) return false;

    out.blockOffsets.resize(out.numBlocks);
    size_t offset = c.offset();
    for (uint32_t i = 0; i < out.numBlocks; ++i) {
        out.blockOffsets[i] = offset;
        offset += out.blockSizes[i];
    }
    return true;
}

inline const std::string& BlockType(const NifHeader& h, uint32_t block) {
    static const std::string empty;
    if (block >= h.numBlocks) return empty;
    const uint16_t index = h.blockTypeIndices[block] & 0x7fffu;
    return index < h.blockTypes.size() ? h.blockTypes[index] : empty;
}

inline const uint8_t* BlockData(const std::vector<uint8_t>& nif, const NifHeader& h, uint32_t block) {
    return block < h.numBlocks ? nif.data() + h.blockOffsets[block] : nullptr;
}

inline bool ParseObjectNetPrefix(Cursor& c) {
    uint32_t nameIndex = 0, extraCount = 0, controller = 0;
    return c.U32(nameIndex) && c.U32(extraCount) && extraCount <= MAX_BLOCKS &&
           c.Skip(static_cast<size_t>(extraCount) * 4u) && c.U32(controller);
}

inline bool ParseTargetMatrix(const uint8_t* data, size_t size, const NifHeader& h, Mat4& out) {
    Cursor c(data, size);
    if (!ParseObjectNetPrefix(c)) return false;
    uint16_t flags = 0;
    if (!c.U16(flags)) return false;
    if (h.userVersion >= 11u && h.bsVersion > 26u) {
        uint16_t unknown = 0;
        if (!c.U16(unknown)) return false;
    }
    float t[3]{}, r[9]{}, s = 1.0f;
    for (float& v : t) if (!c.F32(v)) return false;
    for (float& v : r) if (!c.F32(v)) return false;
    if (!c.F32(s) || !std::isfinite(s) || std::fabs(s) > 10000.0f) return false;
    out = Identity();
    out.m[0] = r[0] * s; out.m[1] = r[1] * s; out.m[2] = r[2] * s; out.m[3] = t[0];
    out.m[4] = r[3] * s; out.m[5] = r[4] * s; out.m[6] = r[5] * s; out.m[7] = t[1];
    out.m[8] = r[6] * s; out.m[9] = r[7] * s; out.m[10] = r[8] * s; out.m[11] = t[2];
    return true;
}

inline bool ParseRigidBodyTMatrix(const uint8_t* body, size_t size, Mat4& out) {
    if (!body || size < 84u) return false;
    float tx = ReadLeFloat(body + 52u), ty = ReadLeFloat(body + 56u), tz = ReadLeFloat(body + 60u);
    float qx = ReadLeFloat(body + 68u), qy = ReadLeFloat(body + 72u), qz = ReadLeFloat(body + 76u), qw = ReadLeFloat(body + 80u);
    if (!std::isfinite(tx) || !std::isfinite(ty) || !std::isfinite(tz) ||
        !std::isfinite(qx) || !std::isfinite(qy) || !std::isfinite(qz) || !std::isfinite(qw)) return false;
    float qlen = std::sqrt(qx*qx + qy*qy + qz*qz + qw*qw);
    if (qlen < 0.5f || qlen > 1.5f) return false;
    qx /= qlen; qy /= qlen; qz /= qlen; qw /= qlen;
    out = Identity();
    out.m[0] = 1.0f - 2.0f * (qy*qy + qz*qz);
    out.m[1] = 2.0f * (qx*qy - qz*qw);
    out.m[2] = 2.0f * (qx*qz + qy*qw);
    out.m[4] = 2.0f * (qx*qy + qz*qw);
    out.m[5] = 1.0f - 2.0f * (qx*qx + qz*qz);
    out.m[6] = 2.0f * (qy*qz - qx*qw);
    out.m[8] = 2.0f * (qx*qz - qy*qw);
    out.m[9] = 2.0f * (qy*qz + qx*qw);
    out.m[10] = 1.0f - 2.0f * (qx*qx + qy*qy);
    out.m[3] = tx * FO3_HAVOK_TO_NIF;
    out.m[7] = ty * FO3_HAVOK_TO_NIF;
    out.m[11] = tz * FO3_HAVOK_TO_NIF;
    return true;
}

inline bool ParseTransformShape(const uint8_t* data, size_t size, uint32_t& child, Mat4& transform) {
    // FO3 bhkTransformShape/bhkConvexTransformShape:
    // child ref, material, radius, 8 unused bytes, Matrix44.
    if (!data || size < 84u) return false;
    child = ReadLe32(data + 0u);
    transform = Identity();
    for (int i = 0; i < 16; ++i) {
        transform.m[i] = ReadLeFloat(data + 20u + static_cast<size_t>(i) * 4u);
        if (!std::isfinite(transform.m[i])) return false;
    }
    // Matrix44 translation is stored in Havok units.
    transform.m[3] *= FO3_HAVOK_TO_NIF;
    transform.m[7] *= FO3_HAVOK_TO_NIF;
    transform.m[11] *= FO3_HAVOK_TO_NIF;
    return true;
}

inline bool ParsePackedData(const uint8_t* data, size_t size, Fo3NifCollisionShapeQ6F& out) {
    Cursor c(data, size);
    uint32_t triangleCount = 0;
    if (!c.U32(triangleCount) || triangleCount == 0u || triangleCount > MAX_COLLISION_TRIANGLES) return false;
    struct Tri { uint16_t a,b,c; };
    std::vector<Tri> tris;
    tris.reserve(triangleCount);
    for (uint32_t i = 0; i < triangleCount; ++i) {
        uint16_t a=0,b=0,d=0,weld=0;
        if (!c.U16(a) || !c.U16(b) || !c.U16(d) || !c.U16(weld)) return false;
        tris.push_back({a,b,d});
    }
    uint32_t vertexCount = 0;
    uint8_t compressed = 0;
    if (!c.U32(vertexCount) || vertexCount == 0u || vertexCount > MAX_COLLISION_VERTICES || !c.U8(compressed)) return false;
    out.compressedVertices = compressed != 0u;
    out.positions.clear();
    out.positions.reserve(static_cast<size_t>(vertexCount) * 3u);
    for (uint32_t i = 0; i < vertexCount; ++i) {
        float x=0,y=0,z=0;
        if (!compressed) {
            if (!c.F32(x) || !c.F32(y) || !c.F32(z)) return false;
        } else {
            uint16_t hx=0,hy=0,hz=0;
            if (!c.U16(hx) || !c.U16(hy) || !c.U16(hz)) return false;
            x = HalfToFloat(hx); y = HalfToFloat(hy); z = HalfToFloat(hz);
        }
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return false;
        out.positions.insert(out.positions.end(), {x * FO3_HAVOK_TO_NIF, y * FO3_HAVOK_TO_NIF, z * FO3_HAVOK_TO_NIF});
    }
    uint16_t subShapes = 0;
    if (!c.U16(subShapes) || !c.Skip(static_cast<size_t>(subShapes) * 12u)) return false;
    if (c.remaining() != 0u) return false;
    out.indices.clear();
    out.indices.reserve(static_cast<size_t>(triangleCount) * 3u);
    for (const Tri& t : tris) {
        if (t.a >= vertexCount || t.b >= vertexCount || t.c >= vertexCount) return false;
        if (t.a == t.b || t.b == t.c || t.a == t.c) continue;
        out.indices.insert(out.indices.end(), {t.a,t.b,t.c});
    }
    return !out.indices.empty();
}

struct V3 { float x,y,z; };
inline V3 Sub(V3 a, V3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline V3 Cross(V3 a, V3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline float Dot(V3 a, V3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline float Len(V3 a) { return std::sqrt(Dot(a,a)); }
inline V3 Normalize(V3 a) { float l=Len(a); return l>1e-8f ? V3{a.x/l,a.y/l,a.z/l} : V3{0,0,1}; }

inline bool BuildConvexFaces(const std::vector<V3>& rawHavok,
                             const std::vector<std::array<float,4>>& planes,
                             std::vector<uint32_t>& indices) {
    indices.clear();
    if (rawHavok.size() < 4u) return false;
    float span = 1.0f;
    for (const V3& p : rawHavok) span = std::max(span, std::max({std::fabs(p.x),std::fabs(p.y),std::fabs(p.z)}));
    const float eps = 0.0025f + span * 0.00025f;
    std::set<std::array<uint32_t,3>> seen;

    auto addFace = [&](std::vector<uint32_t> face, V3 normal) {
        if (face.size() < 3u) return;
        V3 n = Normalize(normal);
        V3 helper = std::fabs(n.z) < 0.85f ? V3{0,0,1} : V3{0,1,0};
        V3 u = Normalize(Cross(helper, n));
        V3 v = Cross(n, u);
        V3 center{0,0,0};
        for (uint32_t i : face) { center.x += rawHavok[i].x; center.y += rawHavok[i].y; center.z += rawHavok[i].z; }
        const float inv = 1.0f / static_cast<float>(face.size());
        center.x*=inv; center.y*=inv; center.z*=inv;
        std::sort(face.begin(), face.end(), [&](uint32_t ia, uint32_t ib) {
            V3 a = Sub(rawHavok[ia], center), b = Sub(rawHavok[ib], center);
            return std::atan2(Dot(a,v), Dot(a,u)) < std::atan2(Dot(b,v), Dot(b,u));
        });
        for (size_t k = 1; k + 1u < face.size(); ++k) {
            uint32_t a = face[0], b = face[k], c = face[k+1u];
            V3 cross = Cross(Sub(rawHavok[b], rawHavok[a]), Sub(rawHavok[c], rawHavok[a]));
            if (Dot(cross, n) < 0.0f) std::swap(b,c);
            std::array<uint32_t,3> key{a,b,c};
            std::sort(key.begin(), key.end());
            if (seen.insert(key).second) indices.insert(indices.end(), {a,b,c});
        }
    };

    for (const auto& p : planes) {
        V3 n{p[0],p[1],p[2]};
        const float nlen = Len(n);
        if (nlen < 1e-6f) continue;
        std::vector<uint32_t> face;
        for (uint32_t i = 0; i < rawHavok.size(); ++i) {
            const float d = std::fabs(Dot(n, rawHavok[i]) + p[3]) / nlen;
            if (d <= eps) face.push_back(i);
        }
        addFace(std::move(face), n);
    }

    // Defensive fallback for unusual files whose plane list is absent or has a
    // different tolerance. This uses the actual convex vertices, never render data.
    if (indices.empty()) {
        for (uint32_t a = 0; a + 2u < rawHavok.size(); ++a) {
            for (uint32_t b = a + 1u; b + 1u < rawHavok.size(); ++b) {
                for (uint32_t c = b + 1u; c < rawHavok.size(); ++c) {
                    V3 n = Cross(Sub(rawHavok[b],rawHavok[a]), Sub(rawHavok[c],rawHavok[a]));
                    if (Len(n) < 1e-6f) continue;
                    bool pos=false, neg=false;
                    for (uint32_t q=0; q<rawHavok.size(); ++q) {
                        if (q==a || q==b || q==c) continue;
                        float d=Dot(n,Sub(rawHavok[q],rawHavok[a]));
                        pos |= d > eps; neg |= d < -eps;
                        if (pos && neg) break;
                    }
                    if (pos && neg) continue;
                    uint32_t aa=a,bb=b,cc=c;
                    if (pos) std::swap(bb,cc);
                    std::array<uint32_t,3> key{aa,bb,cc};
                    std::sort(key.begin(), key.end());
                    if (seen.insert(key).second) indices.insert(indices.end(), {aa,bb,cc});
                }
            }
        }
    }
    return !indices.empty();
}

inline bool ParseConvexVertices(const uint8_t* data, size_t size, Fo3NifCollisionShapeQ6F& out) {
    Cursor c(data,size);
    uint32_t material=0, vertexCount=0, normalCount=0;
    float shellRadius=0;
    if (!c.U32(material) || !c.F32(shellRadius) || !c.Skip(24u) || !c.U32(vertexCount) ||
        vertexCount < 4u || vertexCount > 65535u) return false;
    std::vector<V3> raw;
    raw.reserve(vertexCount);
    out.positions.clear();
    out.positions.reserve(static_cast<size_t>(vertexCount)*3u);
    for (uint32_t i=0;i<vertexCount;++i) {
        float x=0,y=0,z=0,w=0;
        if (!c.F32(x)||!c.F32(y)||!c.F32(z)||!c.F32(w) || !std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)) return false;
        raw.push_back({x,y,z});
        out.positions.insert(out.positions.end(), {x*FO3_HAVOK_TO_NIF,y*FO3_HAVOK_TO_NIF,z*FO3_HAVOK_TO_NIF});
    }
    if (!c.U32(normalCount) || normalCount > 65535u) return false;
    std::vector<std::array<float,4>> planes;
    planes.reserve(normalCount);
    for (uint32_t i=0;i<normalCount;++i) {
        std::array<float,4> p{};
        if (!c.F32(p[0])||!c.F32(p[1])||!c.F32(p[2])||!c.F32(p[3])) return false;
        planes.push_back(p);
    }
    if (c.remaining()!=0u || !BuildConvexFaces(raw,planes,out.indices)) return false;
    out.radiusNif = shellRadius * FO3_HAVOK_TO_NIF;
    return true;
}

inline void BuildBox(float hx,float hy,float hz,Fo3NifCollisionShapeQ6F& out) {
    out.positions = {
        -hx,-hy,-hz,  hx,-hy,-hz,  hx, hy,-hz, -hx, hy,-hz,
        -hx,-hy, hz,  hx,-hy, hz,  hx, hy, hz, -hx, hy, hz,
    };
    static const uint32_t tris[] = {
        0,2,1,0,3,2, 4,5,6,4,6,7,
        0,1,5,0,5,4, 3,7,6,3,6,2,
        0,4,7,0,7,3, 1,2,6,1,6,5,
    };
    out.indices.assign(std::begin(tris),std::end(tris));
}

inline void BuildSphere(float radius,Fo3NifCollisionShapeQ6F& out,int stacks=8,int slices=12) {
    out.positions.clear(); out.indices.clear();
    out.positions.insert(out.positions.end(), {0,0,-radius});
    for(int s=1;s<stacks;++s){
        float phi=-PI*0.5f+PI*static_cast<float>(s)/static_cast<float>(stacks);
        float zr=std::sin(phi)*radius, rr=std::cos(phi)*radius;
        for(int j=0;j<slices;++j){
            float a=2.0f*PI*static_cast<float>(j)/static_cast<float>(slices);
            out.positions.insert(out.positions.end(), {rr*std::cos(a),rr*std::sin(a),zr});
        }
    }
    uint32_t top=static_cast<uint32_t>(out.positions.size()/3u);
    out.positions.insert(out.positions.end(), {0,0,radius});
    for(int j=0;j<slices;++j){
        uint32_t a=1u+static_cast<uint32_t>(j), b=1u+static_cast<uint32_t>((j+1)%slices);
        out.indices.insert(out.indices.end(), {0u,b,a});
    }
    for(int ring=0;ring<stacks-2;++ring){
        uint32_t r0=1u+static_cast<uint32_t>(ring*slices), r1=r0+static_cast<uint32_t>(slices);
        for(int j=0;j<slices;++j){
            uint32_t n=static_cast<uint32_t>((j+1)%slices);
            uint32_t a=r0+static_cast<uint32_t>(j),b=r0+n,c=r1+static_cast<uint32_t>(j),d=r1+n;
            out.indices.insert(out.indices.end(), {a,b,d,a,d,c});
        }
    }
    uint32_t last=1u+static_cast<uint32_t>((stacks-2)*slices);
    for(int j=0;j<slices;++j){
        uint32_t a=last+static_cast<uint32_t>(j), b=last+static_cast<uint32_t>((j+1)%slices);
        out.indices.insert(out.indices.end(), {a,b,top});
    }
}

inline void AppendCapsuleRing(std::vector<float>& pos,V3 center,V3 u,V3 v,float radius,int slices){
    for(int j=0;j<slices;++j){
        float a=2.0f*PI*static_cast<float>(j)/static_cast<float>(slices);
        float ca=std::cos(a)*radius, sa=std::sin(a)*radius;
        pos.insert(pos.end(), {center.x+u.x*ca+v.x*sa,center.y+u.y*ca+v.y*sa,center.z+u.z*ca+v.z*sa});
    }
}

inline void BuildCapsule(V3 a,V3 b,float ra,float rb,Fo3NifCollisionShapeQ6F& out){
    constexpr int slices=12, hemi=3;
    V3 axis=Normalize(Sub(b,a));
    V3 helper=std::fabs(axis.z)<0.85f?V3{0,0,1}:V3{0,1,0};
    V3 u=Normalize(Cross(helper,axis)), v=Cross(axis,u);
    out.positions.clear(); out.indices.clear();
    // Pole A.
    V3 poleA{a.x-axis.x*ra,a.y-axis.y*ra,a.z-axis.z*ra};
    out.positions.insert(out.positions.end(), {poleA.x,poleA.y,poleA.z});
    std::vector<uint32_t> rings;
    for(int k=1;k<=hemi;++k){
        float phi=-PI*0.5f+(PI*0.5f)*static_cast<float>(k)/static_cast<float>(hemi);
        V3 c{a.x+axis.x*std::sin(phi)*ra,a.y+axis.y*std::sin(phi)*ra,a.z+axis.z*std::sin(phi)*ra};
        rings.push_back(static_cast<uint32_t>(out.positions.size()/3u));
        AppendCapsuleRing(out.positions,c,u,v,std::cos(phi)*ra,slices);
    }
    rings.push_back(static_cast<uint32_t>(out.positions.size()/3u));
    AppendCapsuleRing(out.positions,b,u,v,rb,slices);
    for(int k=1;k<hemi;++k){
        float phi=(PI*0.5f)*static_cast<float>(k)/static_cast<float>(hemi);
        V3 c{b.x+axis.x*std::sin(phi)*rb,b.y+axis.y*std::sin(phi)*rb,b.z+axis.z*std::sin(phi)*rb};
        rings.push_back(static_cast<uint32_t>(out.positions.size()/3u));
        AppendCapsuleRing(out.positions,c,u,v,std::cos(phi)*rb,slices);
    }
    uint32_t poleB=static_cast<uint32_t>(out.positions.size()/3u);
    V3 pB{b.x+axis.x*rb,b.y+axis.y*rb,b.z+axis.z*rb};
    out.positions.insert(out.positions.end(), {pB.x,pB.y,pB.z});
    for(int j=0;j<slices;++j){
        uint32_t x=rings.front()+static_cast<uint32_t>(j),y=rings.front()+static_cast<uint32_t>((j+1)%slices);
        out.indices.insert(out.indices.end(), {0u,x,y});
    }
    for(size_t r=0;r+1u<rings.size();++r){
        for(int j=0;j<slices;++j){
            uint32_t n=static_cast<uint32_t>((j+1)%slices);
            uint32_t x=rings[r]+static_cast<uint32_t>(j),y=rings[r]+n,z=rings[r+1]+static_cast<uint32_t>(j),w=rings[r+1]+n;
            out.indices.insert(out.indices.end(), {x,z,w,x,w,y});
        }
    }
    for(int j=0;j<slices;++j){
        uint32_t x=rings.back()+static_cast<uint32_t>(j),y=rings.back()+static_cast<uint32_t>((j+1)%slices);
        out.indices.insert(out.indices.end(), {x,poleB,y});
    }
}

inline bool ParseBox(const uint8_t* data,size_t size,Fo3NifCollisionShapeQ6F& out){
    Cursor c(data,size); uint32_t material=0; float shell=0,hx=0,hy=0,hz=0,minSize=0;
    if(!c.U32(material)||!c.F32(shell)||!c.Skip(8u)||!c.F32(hx)||!c.F32(hy)||!c.F32(hz)||!c.F32(minSize)||c.remaining()!=0u) return false;
    hx*=FO3_HAVOK_TO_NIF; hy*=FO3_HAVOK_TO_NIF; hz*=FO3_HAVOK_TO_NIF;
    if(hx<=0||hy<=0||hz<=0||!std::isfinite(hx)||!std::isfinite(hy)||!std::isfinite(hz)) return false;
    out.radiusNif=shell*FO3_HAVOK_TO_NIF;
    out.halfExtentsNif[0]=hx;out.halfExtentsNif[1]=hy;out.halfExtentsNif[2]=hz;
    BuildBox(hx,hy,hz,out); return true;
}

inline bool ParseSphere(const uint8_t* data,size_t size,Fo3NifCollisionShapeQ6F& out){
    if(!data||size!=8u) return false; float r=ReadLeFloat(data+4u)*FO3_HAVOK_TO_NIF;
    if(!(r>0.0f)||!std::isfinite(r)) return false; out.radiusNif=r; BuildSphere(r,out); return true;
}

inline bool ParseCapsule(const uint8_t* data,size_t size,Fo3NifCollisionShapeQ6F& out){
    Cursor c(data,size); uint32_t material=0; float shell=0; V3 a{},b{}; float ra=0,rb=0;
    if(!c.U32(material)||!c.F32(shell)||!c.Skip(8u)||!c.F32(a.x)||!c.F32(a.y)||!c.F32(a.z)||!c.F32(ra)||!c.F32(b.x)||!c.F32(b.y)||!c.F32(b.z)||!c.F32(rb)||c.remaining()!=0u) return false;
    a.x*=FO3_HAVOK_TO_NIF;a.y*=FO3_HAVOK_TO_NIF;a.z*=FO3_HAVOK_TO_NIF;
    b.x*=FO3_HAVOK_TO_NIF;b.y*=FO3_HAVOK_TO_NIF;b.z*=FO3_HAVOK_TO_NIF;
    ra*=FO3_HAVOK_TO_NIF;rb*=FO3_HAVOK_TO_NIF;
    if(!(ra>0&&rb>0)||!std::isfinite(ra)||!std::isfinite(rb)) return false;
    out.radiusNif=ra; out.radius2Nif=rb;
    out.pointANif[0]=a.x;out.pointANif[1]=a.y;out.pointANif[2]=a.z;
    out.pointBNif[0]=b.x;out.pointBNif[1]=b.y;out.pointBNif[2]=b.z;
    BuildCapsule(a,b,ra,rb,out); return true;
}

inline bool ParseListRefs(const uint8_t* data,size_t size,std::vector<uint32_t>& refs,bool convexList){
    Cursor c(data,size); uint32_t count=0;
    if(!c.U32(count)||count==0u||count>256u) return false;
    refs.resize(count);
    for(uint32_t& r:refs) if(!c.U32(r)) return false;
    // The rest is material/filter/cache metadata. We deliberately validate the
    // child array only; child refs are the semantic shape graph we need.
    (void)convexList;
    return true;
}

inline bool DecodeShape(const std::vector<uint8_t>& nif,const NifHeader& h,const std::string& resolved,
                        uint32_t shapeRef,const Mat4& accumulated,uint32_t coBlock,uint32_t targetBlock,uint32_t bodyBlock,
                        int depth,std::vector<uint32_t>& stack,std::vector<Fo3NifCollisionShapeQ6F>& out){
    if(shapeRef>=h.numBlocks||depth>32) return false;
    if(std::find(stack.begin(),stack.end(),shapeRef)!=stack.end()) return false;
    stack.push_back(shapeRef);
    const std::string type=BlockType(h,shapeRef);
    const uint8_t* data=BlockData(nif,h,shapeRef);
    const size_t size=h.blockSizes[shapeRef];
    bool any=false;

    if(type=="bhkMoppBvTreeShape"){
        if(data&&size>=4u){ uint32_t child=ReadLe32(data); any=DecodeShape(nif,h,resolved,child,accumulated,coBlock,targetBlock,bodyBlock,depth+1,stack,out); }
    } else if(type=="bhkListShape"||type=="bhkConvexListShape"){
        std::vector<uint32_t> refs;
        if(ParseListRefs(data,size,refs,type=="bhkConvexListShape")){
            Q6F_LOGI("Q6F COLLISION LIST: model=%s shape=%u type=%s children=%zu",resolved.c_str(),shapeRef,type.c_str(),refs.size());
            for(uint32_t child:refs) any=DecodeShape(nif,h,resolved,child,accumulated,coBlock,targetBlock,bodyBlock,depth+1,stack,out)||any;
        }
    } else if(type=="bhkTransformShape"||type=="bhkConvexTransformShape"){
        uint32_t child=INVALID_REF; Mat4 t;
        if(ParseTransformShape(data,size,child,t)){
            Mat4 next=Mul(accumulated,t);
            Q6F_LOGI("Q6F COLLISION TRANSFORM: model=%s shape=%u type=%s child=%u",resolved.c_str(),shapeRef,type.c_str(),child);
            any=DecodeShape(nif,h,resolved,child,next,coBlock,targetBlock,bodyBlock,depth+1,stack,out);
            if(any){ for(auto& s:out) if(s.collisionObjectBlock==coBlock) s.nestedTransformApplied=true; }
        }
    } else {
        Fo3NifCollisionShapeQ6F s;
        s.modelPath=resolved; s.sourceShapeType=type; s.collisionObjectBlock=coBlock; s.targetBlock=targetBlock; s.bodyBlock=bodyBlock; s.sourceShapeBlock=shapeRef;
        bool ok=false;
        if(type=="bhkPackedNiTriStripsShape"){
            if(data&&size>=56u){ uint32_t dr=ReadLe32(data+52u); if(dr<h.numBlocks&&BlockType(h,dr)=="hkPackedNiTriStripsData"){
                s.kind=Fo3CollisionShapeKindQ6F::TriangleMesh; s.dataBlock=dr; ok=ParsePackedData(BlockData(nif,h,dr),h.blockSizes[dr],s);
            }}
        } else if(type=="bhkConvexVerticesShape"){
            s.kind=Fo3CollisionShapeKindQ6F::ConvexHull; ok=ParseConvexVertices(data,size,s);
        } else if(type=="bhkBoxShape"){
            s.kind=Fo3CollisionShapeKindQ6F::Box; ok=ParseBox(data,size,s);
        } else if(type=="bhkSphereShape"){
            s.kind=Fo3CollisionShapeKindQ6F::Sphere; ok=ParseSphere(data,size,s);
        } else if(type=="bhkCapsuleShape"){
            s.kind=Fo3CollisionShapeKindQ6F::Capsule; ok=ParseCapsule(data,size,s);
        } else {
            Q6F_LOGW("Q6F COLLISION SHAPE UNSUPPORTED: model=%s co=%u body=%u shape=%u type=%s",resolved.c_str(),coBlock,bodyBlock,shapeRef,type.c_str());
        }
        if(ok){
            ApplyMatrix(s.positions,accumulated); StoreMatrix(accumulated,s.localToModel);
            Q6F_LOGI("Q6F COLLISION LEAF: model=%s shape=%u type=%s kind=%s vertices=%zu triangles=%zu depth=%d",
                     resolved.c_str(),shapeRef,type.c_str(),Fo3CollisionShapeKindNameQ6F(s.kind),s.positions.size()/3u,s.indices.size()/3u,depth);
            out.push_back(std::move(s)); any=true;
        } else if(type=="bhkPackedNiTriStripsShape"||type=="bhkConvexVerticesShape"||type=="bhkBoxShape"||type=="bhkSphereShape"||type=="bhkCapsuleShape"){
            Q6F_LOGW("Q6F COLLISION SHAPE PARSE FAILED: model=%s shape=%u type=%s bytes=%zu",resolved.c_str(),shapeRef,type.c_str(),size);
        }
    }
    stack.pop_back();
    return any;
}

inline void Bounds(const std::vector<float>& positions,float mn[3],float mx[3]){
    mn[0]=mn[1]=mn[2]=1e30f; mx[0]=mx[1]=mx[2]=-1e30f;
    for(size_t i=0;i+2u<positions.size();i+=3u){
        mn[0]=std::min(mn[0],positions[i]);mn[1]=std::min(mn[1],positions[i+1]);mn[2]=std::min(mn[2],positions[i+2]);
        mx[0]=std::max(mx[0],positions[i]);mx[1]=std::max(mx[1],positions[i+1]);mx[2]=std::max(mx[2],positions[i+2]);
    }
}

} // namespace fo3_q6f_collision_detail

inline bool LoadFo3NifCollisionShapesQ6F(const std::string& modelPath,
                                         std::vector<Fo3NifCollisionShapeQ6F>& outShapes) {
    using namespace fo3_q6f_collision_detail;
    outShapes.clear();
    std::vector<uint8_t> nif; std::string resolved;
    if(!LoadFalloutMeshFile(modelPath,nif,&resolved)||nif.empty()||nif.size()>MAX_NIF_BYTES) return false;
    NifHeader h;
    if(!ParseHeader(nif,h)){ Q6F_LOGW("Q6F COLLISION HEADER UNSUPPORTED: model=%s",resolved.c_str()); return false; }

    size_t collisionObjects=0, totalTriangles=0;
    std::array<size_t,5> kinds{};
    for(uint32_t co=0;co<h.numBlocks;++co){
        const std::string& cot=BlockType(h,co);
        if(cot!="bhkCollisionObject"&&cot!="bhkBlendCollisionObject"&&cot!="bhkPCollisionObject"&&cot!="bhkSPCollisionObject") continue;
        const uint8_t* cod=BlockData(nif,h,co); size_t cos=h.blockSizes[co];
        if(!cod||cos<10u) continue;
        uint32_t target=ReadLe32(cod+0u), bodyRef=ReadLe32(cod+6u);
        if(bodyRef>=h.numBlocks) continue;
        const std::string& bodyType=BlockType(h,bodyRef);
        if(bodyType!="bhkRigidBody"&&bodyType!="bhkRigidBodyT") continue;
        const uint8_t* body=BlockData(nif,h,bodyRef); size_t bodySize=h.blockSizes[bodyRef];
        if(!body||bodySize<4u) continue;
        uint32_t rootShape=ReadLe32(body+0u);
        if(rootShape>=h.numBlocks) continue;

        const size_t before=outShapes.size();
        std::vector<uint32_t> stack;
        DecodeShape(nif,h,resolved,rootShape,Identity(),co,target,bodyRef,0,stack,outShapes);
        if(outShapes.size()==before) continue;
        ++collisionObjects;

        Mat4 bodyM=Identity(); bool bodyApplied=false;
        if(bodyType=="bhkRigidBodyT") bodyApplied=ParseRigidBodyTMatrix(body,bodySize,bodyM);
        Mat4 targetM=Identity(); bool targetApplied=false;
        if(target<h.numBlocks) targetApplied=ParseTargetMatrix(BlockData(nif,h,target),h.blockSizes[target],h,targetM);
        const Mat4 outer=Mul(targetApplied?targetM:Identity(),bodyApplied?bodyM:Identity());

        for(size_t i=before;i<outShapes.size();++i){
            Fo3NifCollisionShapeQ6F& s=outShapes[i];
            ApplyMatrix(s.positions,outer);
            Mat4 local{}; std::memcpy(local.m,s.localToModel,sizeof(local.m));
            StoreMatrix(Mul(outer,local),s.localToModel);
            s.bodyTransformApplied=bodyApplied;
            totalTriangles+=s.indices.size()/3u;
            kinds[static_cast<size_t>(s.kind)]++;
            float mn[3],mx[3]; Bounds(s.positions,mn,mx);
            Q6F_LOGI("Q6F COLLISION DATA: model=%s co=%u body=%u root=%u leaf=%u kind=%s vertices=%zu triangles=%zu bodyT=%d targetT=%d bounds=[%.2f %.2f %.2f]-[%.2f %.2f %.2f]",
                     resolved.c_str(),co,bodyRef,rootShape,s.sourceShapeBlock,Fo3CollisionShapeKindNameQ6F(s.kind),s.positions.size()/3u,s.indices.size()/3u,bodyApplied?1:0,targetApplied?1:0,
                     mn[0],mn[1],mn[2],mx[0],mx[1],mx[2]);
        }
    }
    if(outShapes.empty()){ Q6F_LOGW("Q6F COLLISION MODEL MISS: model=%s blocks=%u",resolved.c_str(),h.numBlocks); return false; }
    Q6F_LOGI("Q6F COLLISION MODEL READY: model=%s collisionObjects=%zu shapes=%zu triangles=%zu packed=%zu convex=%zu box=%zu sphere=%zu capsule=%zu",
             resolved.c_str(),collisionObjects,outShapes.size(),totalTriangles,kinds[0],kinds[1],kinds[2],kinds[3],kinds[4]);
    return true;
}
