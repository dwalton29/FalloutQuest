#include "fo3-chair-render-mesh.h"

#include <android/log.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
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
        if (remaining() < 1) return false;
        value = data_[pos_++];
        return true;
    }
    bool U16(uint16_t& value) {
        if (remaining() < 2) return false;
        value = ReadLe16(data_ + pos_);
        pos_ += 2;
        return true;
    }
    bool U32(uint32_t& value) {
        if (remaining() < 4) return false;
        value = ReadLe32(data_ + pos_);
        pos_ += 4;
        return true;
    }
    bool F32(float& value) {
        if (remaining() < 4) return false;
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
    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    size_t pos_ = 0;
};

struct NifHeader {
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
    uint32_t userVersion = 0;
    if (!c.U32(version) || !c.U8(endian) || !c.U32(userVersion) || !c.U32(out.numBlocks)) {
        return false;
    }
    if (version != NIF_VERSION_FO3 || endian != 1 ||
        out.numBlocks == 0 || out.numBlocks > MAX_BLOCKS) return false;

    uint32_t bsVersion = 0;
    std::string author, processScript, exportScript;
    if (!c.U32(bsVersion) || !c.ExportString(author) ||
        !c.ExportString(processScript) || !c.ExportString(exportScript)) return false;
    if (bsVersion >= 103u) {
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

bool ParseTriStripsData(const uint8_t* data, size_t size, Fo3StaticMesh& outMesh,
                        uint32_t& outDeclaredTriangles) {
    Cursor c(data, size);
    uint32_t groupId = 0;
    uint16_t numVertices = 0;
    uint8_t keepFlags = 0, compressFlags = 0, hasVertices = 0;
    if (!c.U32(groupId) || !c.U16(numVertices) || !c.U8(keepFlags) ||
        !c.U8(compressFlags) || !c.U8(hasVertices) || !hasVertices || numVertices == 0) {
        return false;
    }

    std::vector<float> positions;
    positions.reserve(static_cast<size_t>(numVertices) * 3u);
    for (uint16_t i = 0; i < numVertices; ++i) {
        float x = 0.0f, y = 0.0f, z = 0.0f;
        if (!c.F32(x) || !c.F32(y) || !c.F32(z)) return false;
        positions.push_back(x);
        positions.push_back(y);
        positions.push_back(z);
    }

    uint16_t bsDataFlags = 0;
    uint8_t hasNormals = 0;
    if (!c.U16(bsDataFlags) || !c.U8(hasNormals)) return false;
    if (hasNormals) {
        if (!c.Skip(static_cast<size_t>(numVertices) * 12u)) return false;
        if ((bsDataFlags & 0x1000u) != 0 &&
            !c.Skip(static_cast<size_t>(numVertices) * 24u)) return false;
    }

    if (!c.Skip(16u)) return false; // bounding sphere

    uint8_t hasVertexColors = 0;
    if (!c.U8(hasVertexColors)) return false;
    if (hasVertexColors && !c.Skip(static_cast<size_t>(numVertices) * 16u)) return false;
    if ((bsDataFlags & 0x0001u) != 0 &&
        !c.Skip(static_cast<size_t>(numVertices) * 8u)) return false;
    if (!c.Skip(2u) || !c.Skip(4u)) return false; // consistency + additional data

    uint16_t declaredTriangles = 0;
    uint16_t numStrips = 0;
    if (!c.U16(declaredTriangles) || !c.U16(numStrips) || numStrips == 0) return false;

    std::vector<uint16_t> stripLengths(numStrips);
    for (uint16_t i = 0; i < numStrips; ++i) {
        if (!c.U16(stripLengths[i])) return false;
    }

    uint8_t hasPoints = 0;
    if (!c.U8(hasPoints) || !hasPoints) return false;

    std::vector<uint32_t> indices;
    for (uint16_t strip = 0; strip < numStrips; ++strip) {
        const uint16_t length = stripLengths[strip];
        std::vector<uint16_t> points(length);
        for (uint16_t i = 0; i < length; ++i) {
            if (!c.U16(points[i]) || points[i] >= numVertices) return false;
        }
        for (uint16_t i = 2; i < length; ++i) {
            uint16_t a = points[i - 2];
            uint16_t b = points[i - 1];
            const uint16_t d = points[i];
            if ((i & 1u) != 0u) {
                const uint16_t tmp = a;
                a = b;
                b = tmp;
            }
            if (a == b || b == d || a == d) continue;
            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(d);
        }
    }

    if (indices.empty()) return false;
    outMesh.positions = std::move(positions);
    outMesh.indices = std::move(indices);
    outDeclaredTriangles = declaredTriangles;
    return true;
}

} // namespace

bool LoadMegatonChairStripMesh(Fo3StaticMesh& outMesh) {
    outMesh.positions.clear();
    outMesh.indices.clear();

    std::vector<uint8_t> nif;
    if (!LoadChairNif(nif)) {
        FQ_LOGE("Q5G chair NIF extraction failed");
        return false;
    }

    NifHeader header;
    if (!ParseHeader(nif, header)) {
        FQ_LOGE("Q5G chair NIF header parse failed");
        return false;
    }

    size_t offset = header.blockDataOffset;
    for (uint32_t block = 0; block < header.numBlocks; ++block) {
        const uint32_t blockSize = header.blockSizes[block];
        if (blockSize > nif.size() - offset) return false;
        const uint16_t typeIndex = header.blockTypeIndices[block] & 0x7fffu;
        if (header.blockTypes[typeIndex] == "NiTriStripsData") {
            uint32_t declaredTriangles = 0;
            if (!ParseTriStripsData(nif.data() + offset, blockSize,
                                    outMesh, declaredTriangles)) {
                FQ_LOGE("Q5G failed to decode NiTriStripsData block=%u", block);
                return false;
            }
            FQ_LOGI("Q5G CHAIR DECODED: block=%u vertices=%zu triangles=%zu declaredTriangles=%u",
                    block, outMesh.positions.size() / 3u,
                    outMesh.indices.size() / 3u, declaredTriangles);
            return true;
        }
        offset += blockSize;
    }

    FQ_LOGE("Q5G no NiTriStripsData block found");
    return false;
}
