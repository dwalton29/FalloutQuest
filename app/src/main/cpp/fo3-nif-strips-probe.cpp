#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
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

#define FQ_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define FQ_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

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
    if (!file) {
        FQ_LOGE("Q5F failed to open Fallout - Meshes.bsa");
        return false;
    }

    if (fseeko(file, static_cast<off_t>(CHAIR_OFFSET), SEEK_SET) != 0) {
        FQ_LOGE("Q5F failed to seek chair03.nif BSA entry");
        std::fclose(file);
        return false;
    }

    uint8_t originalSizeBytes[4]{};
    if (!ReadExact(file, originalSizeBytes, sizeof(originalSizeBytes))) {
        std::fclose(file);
        return false;
    }

    const uint32_t originalSize = ReadLe32(originalSizeBytes);
    if (originalSize == 0 || originalSize > MAX_NIF_BYTES || CHAIR_STORED_BYTES <= 4u) {
        std::fclose(file);
        return false;
    }

    const uint32_t compressedSize = CHAIR_STORED_BYTES - 4u;
    std::vector<uint8_t> compressed(compressedSize);
    if (!ReadExact(file, compressed.data(), compressed.size())) {
        std::fclose(file);
        return false;
    }
    std::fclose(file);

    nif.resize(originalSize);
    uLongf outLen = static_cast<uLongf>(nif.size());
    const int result = uncompress(reinterpret_cast<Bytef*>(nif.data()),
                                  &outLen,
                                  reinterpret_cast<const Bytef*>(compressed.data()),
                                  static_cast<uLong>(compressed.size()));
    if (result != Z_OK || outLen != originalSize) {
        FQ_LOGE("Q5F zlib inflate failed result=%d expected=%u actual=%lu",
                result, originalSize, static_cast<unsigned long>(outLen));
        nif.clear();
        return false;
    }

    FQ_LOGI("Q5F NIF LOADED: compressed=%u bytes inflated=%u bytes",
            compressedSize, originalSize);
    return true;
}

class Cursor {
public:
    Cursor(const uint8_t* data, size_t size) : data_(data), size_(size) {}
    explicit Cursor(const std::vector<uint8_t>& data) : Cursor(data.data(), data.size()) {}

    size_t offset() const { return pos_; }
    size_t remaining() const { return size_ - pos_; }

    bool Skip(size_t count) {
        if (!Require(count)) return false;
        pos_ += count;
        return true;
    }

    bool U8(uint8_t& value) {
        if (!Require(1)) return false;
        value = data_[pos_++];
        return true;
    }

    bool U16(uint16_t& value) {
        if (!Require(2)) return false;
        value = ReadLe16(data_ + pos_);
        pos_ += 2;
        return true;
    }

    bool U32(uint32_t& value) {
        if (!Require(4)) return false;
        value = ReadLe32(data_ + pos_);
        pos_ += 4;
        return true;
    }

    bool F32(float& value) {
        if (!Require(4)) return false;
        value = ReadLeFloat(data_ + pos_);
        pos_ += 4;
        return true;
    }

    bool SizedString(std::string& out) {
        uint32_t length = 0;
        if (!U32(length) || length > remaining()) return false;
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
    bool Require(size_t count) const {
        return count <= size_ - pos_;
    }

    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    size_t pos_ = 0;
};

struct NifHeader {
    uint32_t version = 0;
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
        line.rfind("Gamebryo File Format, Version 20.2.0.7", 0) != 0) {
        return false;
    }

    uint8_t endian = 0;
    if (!c.U32(out.version) || !c.U8(endian) ||
        !c.U32(out.userVersion) || !c.U32(out.numBlocks)) {
        return false;
    }
    if (out.version != NIF_VERSION_FO3 || endian != 1 ||
        out.numBlocks == 0 || out.numBlocks > MAX_BLOCKS) {
        return false;
    }

    std::string author;
    std::string processScript;
    std::string exportScript;
    if (!c.U32(out.bsVersion) ||
        !c.ExportString(author) ||
        !c.ExportString(processScript) ||
        !c.ExportString(exportScript)) {
        return false;
    }
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
        const uint16_t typeIndex = out.blockTypeIndices[i] & 0x7fffu;
        if (typeIndex >= out.blockTypes.size()) return false;
    }

    out.blockSizes.resize(out.numBlocks);
    uint64_t totalBlockBytes = 0;
    for (uint32_t i = 0; i < out.numBlocks; ++i) {
        if (!c.U32(out.blockSizes[i])) return false;
        totalBlockBytes += out.blockSizes[i];
        if (totalBlockBytes > nif.size()) return false;
    }

    uint32_t numStrings = 0;
    uint32_t maxStringLength = 0;
    if (!c.U32(numStrings) || !c.U32(maxStringLength) ||
        numStrings > MAX_STRINGS || maxStringLength > MAX_NIF_BYTES) {
        return false;
    }
    for (uint32_t i = 0; i < numStrings; ++i) {
        std::string value;
        if (!c.SizedString(value)) return false;
    }

    uint32_t numGroups = 0;
    if (!c.U32(numGroups) || numGroups > out.numBlocks ||
        !c.Skip(static_cast<size_t>(numGroups) * 4u)) {
        return false;
    }

    out.blockDataOffset = c.offset();
    if (totalBlockBytes > nif.size() - out.blockDataOffset) return false;

    FQ_LOGI("Q5F HEADER: user=%u bs=%u blocks=%u blockTypes=%zu dataOffset=%zu",
            out.userVersion, out.bsVersion, out.numBlocks,
            out.blockTypes.size(), out.blockDataOffset);
    return true;
}

struct GeometryPrefix {
    uint16_t numVertices = 0;
    uint16_t bsDataFlags = 0;
    float minX = 0.0f;
    float minY = 0.0f;
    float minZ = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    float maxZ = 0.0f;
};

bool ParseGeometryPrefix(Cursor& c, GeometryPrefix& out) {
    uint32_t groupId = 0;
    uint8_t keepFlags = 0;
    uint8_t compressFlags = 0;
    uint8_t hasVertices = 0;
    if (!c.U32(groupId) || !c.U16(out.numVertices) ||
        !c.U8(keepFlags) || !c.U8(compressFlags) || !c.U8(hasVertices)) {
        return false;
    }
    if (!hasVertices || out.numVertices == 0) return false;

    out.minX = out.minY = out.minZ = std::numeric_limits<float>::infinity();
    out.maxX = out.maxY = out.maxZ = -std::numeric_limits<float>::infinity();
    for (uint16_t i = 0; i < out.numVertices; ++i) {
        float x = 0.0f, y = 0.0f, z = 0.0f;
        if (!c.F32(x) || !c.F32(y) || !c.F32(z) ||
            !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
            return false;
        }
        out.minX = std::min(out.minX, x);
        out.minY = std::min(out.minY, y);
        out.minZ = std::min(out.minZ, z);
        out.maxX = std::max(out.maxX, x);
        out.maxY = std::max(out.maxY, y);
        out.maxZ = std::max(out.maxZ, z);
    }

    uint8_t hasNormals = 0;
    if (!c.U16(out.bsDataFlags) || !c.U8(hasNormals)) return false;
    if (hasNormals) {
        if (!c.Skip(static_cast<size_t>(out.numVertices) * 12u)) return false;
        if ((out.bsDataFlags & 0x1000u) != 0) {
            if (!c.Skip(static_cast<size_t>(out.numVertices) * 24u)) return false;
        }
    }

    if (!c.Skip(16u)) return false; // bounding sphere

    uint8_t hasVertexColors = 0;
    if (!c.U8(hasVertexColors)) return false;
    if (hasVertexColors && !c.Skip(static_cast<size_t>(out.numVertices) * 16u)) {
        return false;
    }

    if ((out.bsDataFlags & 0x0001u) != 0 &&
        !c.Skip(static_cast<size_t>(out.numVertices) * 8u)) {
        return false;
    }

    // ConsistencyType (ushort) + Additional Data Ref (int32).
    return c.Skip(2u) && c.Skip(4u);
}

bool ParseTriStrips(const uint8_t* data,
                    size_t size,
                    uint32_t blockIndex,
                    uint32_t& outVertices,
                    uint32_t& outTriangles) {
    Cursor c(data, size);
    GeometryPrefix geom;
    if (!ParseGeometryPrefix(c, geom)) return false;

    uint16_t declaredTriangles = 0;
    uint16_t numStrips = 0;
    if (!c.U16(declaredTriangles) || !c.U16(numStrips) || numStrips == 0) {
        return false;
    }

    std::vector<uint16_t> stripLengths(numStrips);
    uint64_t totalPoints = 0;
    for (uint16_t i = 0; i < numStrips; ++i) {
        if (!c.U16(stripLengths[i])) return false;
        totalPoints += stripLengths[i];
        if (totalPoints > 10000000u) return false;
    }

    uint8_t hasPoints = 0;
    if (!c.U8(hasPoints) || !hasPoints) return false;

    uint32_t generatedTriangles = 0;
    for (uint16_t stripIndex = 0; stripIndex < numStrips; ++stripIndex) {
        const uint16_t length = stripLengths[stripIndex];
        std::vector<uint16_t> points(length);
        for (uint16_t i = 0; i < length; ++i) {
            if (!c.U16(points[i]) || points[i] >= geom.numVertices) {
                FQ_LOGE("Q5F strip block=%u strip=%u invalid vertex index",
                        blockIndex, stripIndex);
                return false;
            }
        }

        for (uint16_t i = 2; i < length; ++i) {
            uint16_t a = points[i - 2];
            uint16_t b = points[i - 1];
            uint16_t d = points[i];
            if ((i & 1u) != 0u) std::swap(a, b);
            if (a == b || b == d || a == d) continue;
            ++generatedTriangles;
        }
    }

    outVertices = geom.numVertices;
    outTriangles = generatedTriangles;
    FQ_LOGI("Q5F TRISTRIPS: block=%u vertices=%u strips=%u declaredTriangles=%u generatedTriangles=%u uv=%u tangents=%u bounds=[%.2f %.2f %.2f]-[%.2f %.2f %.2f]",
            blockIndex,
            geom.numVertices,
            numStrips,
            declaredTriangles,
            generatedTriangles,
            (geom.bsDataFlags & 0x0001u) ? 1u : 0u,
            (geom.bsDataFlags & 0x1000u) ? 1u : 0u,
            geom.minX, geom.minY, geom.minZ,
            geom.maxX, geom.maxY, geom.maxZ);
    return generatedTriangles > 0;
}

void RunProbe() {
    FQ_LOGI("Q5F START: inspect chair03.nif block types + NiTriStripsData");

    std::vector<uint8_t> nif;
    if (!LoadChairNif(nif)) {
        FQ_LOGE("Q5F FAILED: chair NIF load failed");
        return;
    }

    NifHeader header;
    if (!ParseHeader(nif, header)) {
        FQ_LOGE("Q5F FAILED: NIF header parse failed");
        return;
    }

    size_t blockOffset = header.blockDataOffset;
    uint32_t stripBlocks = 0;
    uint32_t totalVertices = 0;
    uint32_t totalTriangles = 0;

    for (uint32_t block = 0; block < header.numBlocks; ++block) {
        const uint32_t blockSize = header.blockSizes[block];
        if (blockSize > nif.size() - blockOffset) {
            FQ_LOGE("Q5F FAILED: block=%u exceeds NIF bounds", block);
            return;
        }

        const uint16_t typeIndex = header.blockTypeIndices[block] & 0x7fffu;
        const std::string& type = header.blockTypes[typeIndex];
        FQ_LOGI("Q5F BLOCK: block=%u type=%s bytes=%u",
                block, type.c_str(), blockSize);

        if (type == "NiTriStripsData") {
            uint32_t vertices = 0;
            uint32_t triangles = 0;
            if (!ParseTriStrips(nif.data() + blockOffset,
                                blockSize,
                                block,
                                vertices,
                                triangles)) {
                FQ_LOGE("Q5F FAILED: could not decode NiTriStripsData block=%u", block);
                return;
            }
            ++stripBlocks;
            totalVertices += vertices;
            totalTriangles += triangles;
        }

        blockOffset += blockSize;
    }

    if (stripBlocks == 0) {
        FQ_LOGE("Q5F NO TRISTRIPS: block dump above shows the chair's actual geometry classes");
        return;
    }

    FQ_LOGI("Q5F SUCCESS: NiTriStripsData decoded blocks=%u vertices=%u triangles=%u",
            stripBlocks, totalVertices, totalTriangles);
}

__attribute__((constructor)) void Fallout3NifStripsProbeConstructor() {
    RunProbe();
}

} // namespace
