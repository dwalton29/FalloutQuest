#include "fo3-nif-mesh.h"

#include <android/log.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <zlib.h>

namespace {

constexpr const char* TAG = "FalloutQuest";
constexpr const char* BSA_PATH =
        "/data/user/0/com.falloutquest.app/files/Fallout3/Data/Fallout - Meshes.bsa";
constexpr const char* TARGET_PATH = "meshes\\furniture\\chair03.nif";
constexpr uint32_t BSA_VERSION_FO3 = 104u;
constexpr uint32_t NIF_VERSION_FO3 = 0x14020007u; // 20.2.0.7
constexpr uint32_t MAX_TARGET_BYTES = 64u * 1024u * 1024u;
constexpr uint32_t MAX_NIF_BLOCKS = 100000u;
constexpr uint32_t MAX_NIF_STRINGS = 100000u;

#define FQ_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define FQ_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define FQ_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)

struct FolderRecord {
    uint32_t count = 0;
};

struct RawFileRecord {
    std::string folder;
    uint32_t size = 0;
    uint32_t offset = 0;
    bool compressionToggle = false;
};

struct BsaHeader {
    uint32_t version = 0;
    uint32_t foldersOffset = 0;
    uint32_t archiveFlags = 0;
    uint32_t folderCount = 0;
    uint32_t fileCount = 0;
};

struct TargetEntry {
    bool found = false;
    std::string storedPath;
    uint32_t size = 0;
    uint32_t offset = 0;
    bool compressionToggle = false;
};

uint16_t ReadLe16(const uint8_t* p) {
    return static_cast<uint16_t>(
            static_cast<uint16_t>(p[0]) |
            (static_cast<uint16_t>(p[1]) << 8));
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

std::string NormalizePath(std::string value) {
    for (char& ch : value) {
        if (ch == '/') ch = '\\';
        ch = static_cast<char>(
                std::tolower(static_cast<unsigned char>(ch)));
    }
    while (!value.empty() &&
           (value.front() == '\\' || value.front() == '/')) {
        value.erase(value.begin());
    }
    return value;
}

bool ReadCString(FILE* file, std::string& out) {
    out.clear();
    for (size_t i = 0; i < 4096; ++i) {
        const int ch = std::fgetc(file);
        if (ch == EOF) return false;
        if (ch == 0) return true;
        out.push_back(static_cast<char>(ch));
    }
    return false;
}

bool ReadBsaHeader(FILE* file, BsaHeader& out) {
    uint8_t header[36]{};
    if (!ReadExact(file, header, sizeof(header))) return false;
    if (std::memcmp(header, "BSA\0", 4) != 0) return false;

    out.version = ReadLe32(header + 4);
    out.foldersOffset = ReadLe32(header + 8);
    out.archiveFlags = ReadLe32(header + 12);
    out.folderCount = ReadLe32(header + 16);
    out.fileCount = ReadLe32(header + 20);
    return true;
}

bool FindBsaTarget(FILE* file, const BsaHeader& header, TargetEntry& target) {
    if (header.version != BSA_VERSION_FO3) return false;
    if ((header.archiveFlags & 1u) == 0 ||
        (header.archiveFlags & 2u) == 0) {
        return false;
    }
    if (header.folderCount == 0 || header.folderCount > 1000000u ||
        header.fileCount == 0 || header.fileCount > 2000000u ||
        header.foldersOffset < 36u) {
        return false;
    }

    if (fseeko(file, static_cast<off_t>(header.foldersOffset), SEEK_SET) != 0) {
        return false;
    }

    std::vector<FolderRecord> folders;
    folders.reserve(header.folderCount);
    for (uint32_t i = 0; i < header.folderCount; ++i) {
        uint8_t record[16]{};
        if (!ReadExact(file, record, sizeof(record))) return false;
        const uint32_t count = ReadLe32(record + 8);
        if (count > header.fileCount) return false;
        folders.push_back(FolderRecord{count});
    }

    std::vector<RawFileRecord> rawFiles;
    rawFiles.reserve(header.fileCount);

    for (const FolderRecord& folder : folders) {
        uint8_t nameLen = 0;
        if (!ReadExact(file, &nameLen, 1) || nameLen == 0) return false;

        std::vector<char> nameBytes(nameLen);
        if (!ReadExact(file, nameBytes.data(), nameBytes.size())) return false;
        if (!nameBytes.empty() && nameBytes.back() == '\0') {
            nameBytes.pop_back();
        }

        const std::string folderName =
                NormalizePath(std::string(nameBytes.begin(), nameBytes.end()));

        for (uint32_t j = 0; j < folder.count; ++j) {
            uint8_t fileRecord[16]{};
            if (!ReadExact(file, fileRecord, sizeof(fileRecord))) return false;

            const uint32_t sizeRaw = ReadLe32(fileRecord + 8);
            RawFileRecord raw;
            raw.folder = folderName;
            raw.size = sizeRaw & 0x3fffffffu;
            raw.offset = ReadLe32(fileRecord + 12);
            raw.compressionToggle = (sizeRaw & 0x40000000u) != 0;
            rawFiles.push_back(std::move(raw));
        }
    }

    if (rawFiles.size() != header.fileCount) return false;

    const std::string wanted = NormalizePath(TARGET_PATH);
    for (RawFileRecord& raw : rawFiles) {
        std::string fileName;
        if (!ReadCString(file, fileName)) return false;

        fileName = NormalizePath(fileName);
        std::string fullPath = raw.folder;
        if (!fullPath.empty() && !fileName.empty()) fullPath += "\\";
        fullPath += fileName;
        fullPath = NormalizePath(fullPath);

        if (fullPath == wanted || fullPath == "furniture\\chair03.nif") {
            target.found = true;
            target.storedPath = fullPath;
            target.size = raw.size;
            target.offset = raw.offset;
            target.compressionToggle = raw.compressionToggle;
            return true;
        }
    }

    return true;
}

bool InflateZlib(const std::vector<uint8_t>& compressed,
                 uint32_t originalSize,
                 std::vector<uint8_t>& output) {
    if (originalSize == 0 || originalSize > MAX_TARGET_BYTES) return false;

    output.resize(originalSize);
    uLongf outLen = static_cast<uLongf>(output.size());
    const int result = uncompress(
            reinterpret_cast<Bytef*>(output.data()),
            &outLen,
            reinterpret_cast<const Bytef*>(compressed.data()),
            static_cast<uLong>(compressed.size()));

    if (result != Z_OK || outLen != originalSize) {
        output.clear();
        return false;
    }

    return true;
}

bool ExtractBsaTarget(FILE* file,
                      const BsaHeader& header,
                      const TargetEntry& entry,
                      std::vector<uint8_t>& output) {
    if (entry.size == 0 || entry.size > MAX_TARGET_BYTES) return false;
    if (fseeko(file, static_cast<off_t>(entry.offset), SEEK_SET) != 0) {
        return false;
    }

    size_t remaining = entry.size;

    const bool embedNames = (header.archiveFlags & 0x100u) != 0;
    if (embedNames) {
        uint8_t nameLen = 0;
        if (!ReadExact(file, &nameLen, 1)) return false;
        if (remaining < static_cast<size_t>(nameLen) + 1u) return false;
        if (fseeko(file, static_cast<off_t>(nameLen), SEEK_CUR) != 0) {
            return false;
        }
        remaining -= static_cast<size_t>(nameLen) + 1u;
    }

    const bool compressedByDefault = (header.archiveFlags & 4u) != 0;
    const bool compressed =
            compressedByDefault != entry.compressionToggle;

    if (!compressed) {
        if (remaining == 0 || remaining > MAX_TARGET_BYTES) return false;
        output.resize(remaining);
        return ReadExact(file, output.data(), output.size());
    }

    if (remaining < 4) return false;

    uint8_t sizeBytes[4]{};
    if (!ReadExact(file, sizeBytes, sizeof(sizeBytes))) return false;
    const uint32_t originalSize = ReadLe32(sizeBytes);
    remaining -= 4;

    if (remaining == 0 || remaining > MAX_TARGET_BYTES) return false;

    std::vector<uint8_t> compressedBytes(remaining);
    if (!ReadExact(file, compressedBytes.data(), compressedBytes.size())) {
        return false;
    }

    return InflateZlib(compressedBytes, originalSize, output);
}

bool LoadChairNifBytes(std::vector<uint8_t>& nif) {
    FILE* file = std::fopen(BSA_PATH, "rb");
    if (!file) {
        FQ_LOGE("Q5E failed to open Fallout - Meshes.bsa");
        return false;
    }

    BsaHeader header;
    if (!ReadBsaHeader(file, header)) {
        FQ_LOGE("Q5E invalid BSA header");
        std::fclose(file);
        return false;
    }

    TargetEntry target;
    if (!FindBsaTarget(file, header, target) || !target.found) {
        FQ_LOGE("Q5E chair03.nif not found in mesh BSA");
        std::fclose(file);
        return false;
    }

    const bool ok = ExtractBsaTarget(file, header, target, nif);
    std::fclose(file);

    if (!ok) {
        FQ_LOGE("Q5E chair03.nif extraction failed");
        return false;
    }

    return true;
}

class Cursor {
public:
    Cursor(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    explicit Cursor(const std::vector<uint8_t>& data)
            : Cursor(data.data(), data.size()) {}

    size_t offset() const { return pos_; }
    size_t remaining() const { return size_ - pos_; }
    bool ok() const { return ok_; }

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

    bool Bytes(size_t count, const uint8_t*& ptr) {
        if (!Require(count)) return false;
        ptr = data_ + pos_;
        pos_ += count;
        return true;
    }

    bool SizedString(std::string& out) {
        uint32_t length = 0;
        if (!U32(length)) return false;
        if (length > remaining()) return Fail();
        const uint8_t* bytes = nullptr;
        if (!Bytes(length, bytes)) return false;
        out.assign(reinterpret_cast<const char*>(bytes), length);
        return true;
    }

    bool ExportString(std::string& out) {
        uint8_t length = 0;
        if (!U8(length)) return false;
        if (length > remaining()) return Fail();
        const uint8_t* bytes = nullptr;
        if (!Bytes(length, bytes)) return false;
        out.assign(reinterpret_cast<const char*>(bytes), length);
        while (!out.empty() && out.back() == '\0') out.pop_back();
        return true;
    }

    bool HeaderLine(std::string& out) {
        out.clear();
        while (remaining() > 0 && out.size() < 256) {
            uint8_t ch = 0;
            if (!U8(ch)) return false;
            if (ch == '\n') return true;
            if (ch != '\r') out.push_back(static_cast<char>(ch));
        }
        return false;
    }

private:
    bool Require(size_t count) {
        if (!ok_ || count > size_ - pos_) return Fail();
        return true;
    }

    bool Fail() {
        ok_ = false;
        return false;
    }

    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    size_t pos_ = 0;
    bool ok_ = true;
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

bool ParseNifHeader(const std::vector<uint8_t>& nif, NifHeader& out) {
    Cursor c(nif);

    std::string headerLine;
    if (!c.HeaderLine(headerLine)) return false;
    if (headerLine.rfind("Gamebryo File Format, Version 20.2.0.7", 0) != 0) {
        FQ_LOGE("Q5E unexpected NIF header: %s", headerLine.c_str());
        return false;
    }

    uint8_t endian = 0;
    if (!c.U32(out.version) ||
        !c.U8(endian) ||
        !c.U32(out.userVersion) ||
        !c.U32(out.numBlocks)) {
        return false;
    }

    if (out.version != NIF_VERSION_FO3 || endian != 1 ||
        out.numBlocks == 0 || out.numBlocks > MAX_NIF_BLOCKS) {
        return false;
    }

    // Bethesda stream header. FO3's version triple is 20.2.0.7 / 12 / 34.
    std::string author;
    std::string processScript;
    std::string exportScript;

    if (!c.U32(out.bsVersion) ||
        !c.ExportString(author) ||
        !c.ExportString(processScript) ||
        !c.ExportString(exportScript)) {
        return false;
    }

    // Max Filepath exists only for Bethesda stream >= 103, not FO3 stream 34.
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
        out.blockTypes.push_back(std::move(type));
    }

    out.blockTypeIndices.resize(out.numBlocks);
    for (uint32_t i = 0; i < out.numBlocks; ++i) {
        if (!c.U16(out.blockTypeIndices[i])) return false;
        const uint16_t typeIndex =
                static_cast<uint16_t>(out.blockTypeIndices[i] & 0x7fffu);
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
    if (!c.U32(numStrings) ||
        !c.U32(maxStringLength) ||
        numStrings > MAX_NIF_STRINGS ||
        maxStringLength > MAX_TARGET_BYTES) {
        return false;
    }

    for (uint32_t i = 0; i < numStrings; ++i) {
        std::string value;
        if (!c.SizedString(value)) return false;
        if (value.size() > maxStringLength && maxStringLength != 0) {
            return false;
        }
    }

    uint32_t numGroups = 0;
    if (!c.U32(numGroups)) return false;
    if (numGroups > out.numBlocks ||
        !c.Skip(static_cast<size_t>(numGroups) * sizeof(uint32_t))) {
        return false;
    }

    out.blockDataOffset = c.offset();
    if (totalBlockBytes > nif.size() - out.blockDataOffset) {
        return false;
    }

    FQ_LOGI("Q5E NIF HEADER: version=20.2.0.7 user=%u bs=%u blocks=%u blockTypes=%u dataOffset=%zu",
            out.userVersion, out.bsVersion, out.numBlocks,
            static_cast<unsigned>(out.blockTypes.size()),
            out.blockDataOffset);

    return true;
}

bool ParseNiTriShapeData(const uint8_t* data,
                         size_t size,
                         uint32_t blockIndex,
                         Fo3StaticMesh& mesh,
                         uint32_t& outVertices,
                         uint32_t& outTriangles) {
    Cursor c(data, size);

    uint32_t groupId = 0;
    uint16_t numVertices = 0;
    uint8_t keepFlags = 0;
    uint8_t compressFlags = 0;
    uint8_t hasVertices = 0;

    if (!c.U32(groupId) ||
        !c.U16(numVertices) ||
        !c.U8(keepFlags) ||
        !c.U8(compressFlags) ||
        !c.U8(hasVertices)) {
        return false;
    }

    if (numVertices == 0 || !hasVertices) return false;

    const uint32_t baseVertex =
            static_cast<uint32_t>(mesh.positions.size() / 3u);
    mesh.positions.reserve(mesh.positions.size() +
                           static_cast<size_t>(numVertices) * 3u);

    float minX = std::numeric_limits<float>::infinity();
    float minY = std::numeric_limits<float>::infinity();
    float minZ = std::numeric_limits<float>::infinity();
    float maxX = -std::numeric_limits<float>::infinity();
    float maxY = -std::numeric_limits<float>::infinity();
    float maxZ = -std::numeric_limits<float>::infinity();

    for (uint16_t i = 0; i < numVertices; ++i) {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        if (!c.F32(x) || !c.F32(y) || !c.F32(z)) return false;
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
            return false;
        }

        mesh.positions.push_back(x);
        mesh.positions.push_back(y);
        mesh.positions.push_back(z);

        minX = std::min(minX, x);
        minY = std::min(minY, y);
        minZ = std::min(minZ, z);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
        maxZ = std::max(maxZ, z);
    }

    uint16_t bsDataFlags = 0;
    uint8_t hasNormals = 0;
    if (!c.U16(bsDataFlags) || !c.U8(hasNormals)) return false;

    if (hasNormals) {
        if (!c.Skip(static_cast<size_t>(numVertices) * 12u)) return false;

        if ((bsDataFlags & 0x1000u) != 0) {
            // Tangents then bitangents.
            if (!c.Skip(static_cast<size_t>(numVertices) * 12u) ||
                !c.Skip(static_cast<size_t>(numVertices) * 12u)) {
                return false;
            }
        }
    }

    // Bounding sphere: Vector3 center + float radius.
    if (!c.Skip(16)) return false;

    uint8_t hasVertexColors = 0;
    if (!c.U8(hasVertexColors)) return false;
    if (hasVertexColors &&
        !c.Skip(static_cast<size_t>(numVertices) * 16u)) {
        return false;
    }

    const uint32_t numUvSets = (bsDataFlags & 0x0001u) ? 1u : 0u;
    if (numUvSets != 0 &&
        !c.Skip(static_cast<size_t>(numVertices) * 8u)) {
        return false;
    }

    // Consistency flags (ushort) + Additional Data reference (int32).
    if (!c.Skip(2) || !c.Skip(4)) return false;

    uint16_t numTriangles = 0;
    uint32_t numTrianglePoints = 0;
    uint8_t hasTriangles = 0;

    if (!c.U16(numTriangles) ||
        !c.U32(numTrianglePoints) ||
        !c.U8(hasTriangles)) {
        return false;
    }

    if (!hasTriangles || numTriangles == 0 ||
        numTrianglePoints != static_cast<uint32_t>(numTriangles) * 3u) {
        return false;
    }

    mesh.indices.reserve(mesh.indices.size() +
                         static_cast<size_t>(numTriangles) * 3u);

    for (uint16_t triangle = 0; triangle < numTriangles; ++triangle) {
        uint16_t a = 0;
        uint16_t b = 0;
        uint16_t d = 0;
        if (!c.U16(a) || !c.U16(b) || !c.U16(d)) return false;

        if (a >= numVertices || b >= numVertices || d >= numVertices) {
            FQ_LOGE("Q5E block=%u triangle=%u has out-of-range index (%u,%u,%u) vertices=%u",
                    blockIndex, triangle, a, b, d, numVertices);
            return false;
        }

        mesh.indices.push_back(baseVertex + a);
        mesh.indices.push_back(baseVertex + b);
        mesh.indices.push_back(baseVertex + d);
    }

    outVertices = numVertices;
    outTriangles = numTriangles;

    FQ_LOGI("Q5E MESH BLOCK: block=%u vertices=%u triangles=%u uv=%u normals=%u tangents=%u bounds=[%.2f %.2f %.2f]-[%.2f %.2f %.2f]",
            blockIndex,
            numVertices,
            numTriangles,
            numUvSets,
            hasNormals ? 1u : 0u,
            (hasNormals && (bsDataFlags & 0x1000u)) ? 1u : 0u,
            minX, minY, minZ, maxX, maxY, maxZ);

    return true;
}

bool DecodeChairNif(const std::vector<uint8_t>& nif, Fo3StaticMesh& mesh) {
    NifHeader header;
    if (!ParseNifHeader(nif, header)) {
        FQ_LOGE("Q5E NIF header parse failed");
        return false;
    }

    size_t blockOffset = header.blockDataOffset;
    uint32_t meshBlocks = 0;
    uint32_t totalSourceVertices = 0;
    uint32_t totalSourceTriangles = 0;

    for (uint32_t block = 0; block < header.numBlocks; ++block) {
        const uint32_t blockSize = header.blockSizes[block];
        if (blockSize > nif.size() - blockOffset) {
            FQ_LOGE("Q5E block %u exceeds NIF bounds", block);
            return false;
        }

        const uint16_t rawTypeIndex = header.blockTypeIndices[block];
        const uint16_t typeIndex =
                static_cast<uint16_t>(rawTypeIndex & 0x7fffu);
        const std::string& type = header.blockTypes[typeIndex];

        if (type == "NiTriShapeData") {
            uint32_t vertices = 0;
            uint32_t triangles = 0;
            if (!ParseNiTriShapeData(
                        nif.data() + blockOffset,
                        blockSize,
                        block,
                        mesh,
                        vertices,
                        triangles)) {
                FQ_LOGE("Q5E failed to decode NiTriShapeData block=%u bytes=%u",
                        block, blockSize);
                return false;
            }

            ++meshBlocks;
            totalSourceVertices += vertices;
            totalSourceTriangles += triangles;
        }

        blockOffset += blockSize;
    }

    if (meshBlocks == 0 || mesh.positions.empty() || mesh.indices.empty()) {
        FQ_LOGE("Q5E no NiTriShapeData geometry found");
        return false;
    }

    FQ_LOGI("Q5E NIF DECODED: meshBlocks=%u vertices=%u triangles=%u cpuPositions=%zu cpuIndices=%zu",
            meshBlocks,
            totalSourceVertices,
            totalSourceTriangles,
            mesh.positions.size() / 3u,
            mesh.indices.size());

    return true;
}

void RunNifMeshProbe() {
    FQ_LOGI("Q5E NIF MESH START: %s", TARGET_PATH);

    Fo3StaticMesh mesh;
    if (!LoadMegatonChairMesh(mesh)) {
        FQ_LOGE("Q5E FAILED: chair03.nif did not decode to triangle geometry");
        return;
    }

    FQ_LOGI("Q5E SUCCESS: real chair03.nif decoded into %zu vertices / %zu triangles ready for GLES upload",
            mesh.positions.size() / 3u,
            mesh.indices.size() / 3u);
}

__attribute__((constructor)) void Fallout3NifMeshConstructor() {
    RunNifMeshProbe();
}

} // namespace

bool LoadMegatonChairMesh(Fo3StaticMesh& outMesh) {
    outMesh.positions.clear();
    outMesh.indices.clear();

    std::vector<uint8_t> nif;
    if (!LoadChairNifBytes(nif)) return false;

    return DecodeChairNif(nif, outMesh);
}
