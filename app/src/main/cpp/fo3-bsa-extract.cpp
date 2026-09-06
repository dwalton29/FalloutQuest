#include <android/log.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <zlib.h>

namespace {

constexpr const char* TAG = "FalloutQuest";
constexpr const char* BSA_PATH = "/data/user/0/com.falloutquest.app/files/Fallout3/Data/Fallout - Meshes.bsa";
constexpr const char* TARGET_PATH = "meshes\\furniture\\chair03.nif";
constexpr uint32_t BSA_VERSION_FO3 = 104u;
constexpr uint32_t MAX_TARGET_BYTES = 64u * 1024u * 1024u;

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
    uint32_t totalFolderNameLength = 0;
    uint32_t totalFileNameLength = 0;
    uint32_t fileFlags = 0;
};

struct TargetEntry {
    bool found = false;
    std::string storedPath;
    uint32_t size = 0;
    uint32_t offset = 0;
    bool compressionToggle = false;
};

uint32_t ReadLe32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

bool ReadExact(FILE* file, void* dst, size_t size) {
    return std::fread(dst, 1, size, file) == size;
}

std::string NormalizePath(std::string value) {
    for (char& ch : value) {
        if (ch == '/') ch = '\\';
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    while (!value.empty() && (value.front() == '\\' || value.front() == '/')) {
        value.erase(value.begin());
    }
    return value;
}

bool ReadCString(FILE* file, std::string& out) {
    out.clear();
    for (size_t i = 0; i < 4096; ++i) {
        int ch = std::fgetc(file);
        if (ch == EOF) return false;
        if (ch == 0) return true;
        out.push_back(static_cast<char>(ch));
    }
    return false;
}

bool ReadHeader(FILE* file, BsaHeader& out) {
    uint8_t header[36]{};
    if (!ReadExact(file, header, sizeof(header))) return false;
    if (std::memcmp(header, "BSA\0", 4) != 0) return false;

    out.version = ReadLe32(header + 4);
    out.foldersOffset = ReadLe32(header + 8);
    out.archiveFlags = ReadLe32(header + 12);
    out.folderCount = ReadLe32(header + 16);
    out.fileCount = ReadLe32(header + 20);
    out.totalFolderNameLength = ReadLe32(header + 24);
    out.totalFileNameLength = ReadLe32(header + 28);
    out.fileFlags = ReadLe32(header + 32);
    return true;
}

bool FindTarget(FILE* file, const BsaHeader& header, TargetEntry& target) {
    if (header.version != BSA_VERSION_FO3) return false;
    if ((header.archiveFlags & 1u) == 0 || (header.archiveFlags & 2u) == 0) return false;
    if (header.folderCount == 0 || header.folderCount > 1000000u ||
        header.fileCount == 0 || header.fileCount > 2000000u) return false;
    if (header.foldersOffset < 36u) return false;
    if (fseeko(file, static_cast<off_t>(header.foldersOffset), SEEK_SET) != 0) return false;

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
        if (!ReadExact(file, &nameLen, 1)) return false;
        if (nameLen == 0) return false;
        std::vector<char> nameBytes(nameLen);
        if (!ReadExact(file, nameBytes.data(), nameBytes.size())) return false;
        if (!nameBytes.empty() && nameBytes.back() == '\0') nameBytes.pop_back();
        const std::string folderName = NormalizePath(std::string(nameBytes.begin(), nameBytes.end()));

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

    if (rawFiles.size() != header.fileCount) {
        __android_log_print(ANDROID_LOG_WARN, TAG,
                            "Q5D BSA index count mismatch: header=%u parsed=%zu",
                            header.fileCount, rawFiles.size());
    }

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

bool InflateZlib(const std::vector<uint8_t>& compressed, uint32_t originalSize,
                 std::vector<uint8_t>& output) {
    if (originalSize == 0 || originalSize > MAX_TARGET_BYTES) return false;
    output.resize(originalSize);
    uLongf outLen = static_cast<uLongf>(output.size());
    const int result = uncompress(reinterpret_cast<Bytef*>(output.data()), &outLen,
                                  reinterpret_cast<const Bytef*>(compressed.data()),
                                  static_cast<uLong>(compressed.size()));
    if (result != Z_OK) {
        output.clear();
        return false;
    }
    output.resize(static_cast<size_t>(outLen));
    return true;
}

bool ExtractTarget(FILE* file, const BsaHeader& header, const TargetEntry& entry,
                   std::vector<uint8_t>& output, bool& wasCompressed,
                   size_t& embeddedNameBytes) {
    if (entry.size == 0 || entry.size > MAX_TARGET_BYTES) return false;
    if (fseeko(file, static_cast<off_t>(entry.offset), SEEK_SET) != 0) return false;

    size_t remaining = entry.size;
    embeddedNameBytes = 0;
    const bool embedNames = header.version >= BSA_VERSION_FO3 &&
                            (header.archiveFlags & 0x100u) != 0;
    if (embedNames) {
        uint8_t nameLen = 0;
        if (!ReadExact(file, &nameLen, 1)) return false;
        if (remaining < static_cast<size_t>(nameLen) + 1u) return false;
        if (fseeko(file, static_cast<off_t>(nameLen), SEEK_CUR) != 0) return false;
        embeddedNameBytes = static_cast<size_t>(nameLen) + 1u;
        remaining -= embeddedNameBytes;
    }

    const bool compressedByDefault = (header.archiveFlags & 4u) != 0;
    wasCompressed = compressedByDefault != entry.compressionToggle;

    if (!wasCompressed) {
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

    std::vector<uint8_t> compressed(remaining);
    if (!ReadExact(file, compressed.data(), compressed.size())) return false;
    return InflateZlib(compressed, originalSize, output);
}

std::string NifHeaderLine(const std::vector<uint8_t>& data) {
    std::string line;
    const size_t limit = std::min<size_t>(data.size(), 128);
    for (size_t i = 0; i < limit; ++i) {
        const unsigned char ch = data[i];
        if (ch == '\n' || ch == '\r' || ch == 0) break;
        line.push_back(std::isprint(ch) ? static_cast<char>(ch) : '?');
    }
    return line;
}

void ProbeFirstMegatonNif() {
    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "Q5D BSA EXTRACT START: %s", TARGET_PATH);

    FILE* file = std::fopen(BSA_PATH, "rb");
    if (!file) {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "Q5D failed to open Fallout - Meshes.bsa");
        return;
    }

    BsaHeader header;
    if (!ReadHeader(file, header)) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Q5D invalid BSA header");
        std::fclose(file);
        return;
    }

    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "Q5D BSA OPEN: version=%u folders=%u files=%u flags=0x%08X compressedDefault=%d embedNames=%d",
                        header.version, header.folderCount, header.fileCount,
                        header.archiveFlags, (header.archiveFlags & 4u) ? 1 : 0,
                        (header.archiveFlags & 0x100u) ? 1 : 0);

    TargetEntry target;
    if (!FindTarget(file, header, target)) {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "Q5D BSA index parse failed");
        std::fclose(file);
        return;
    }
    if (!target.found) {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "Q5D target not found in mesh BSA: %s", TARGET_PATH);
        std::fclose(file);
        return;
    }

    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "Q5D TARGET FOUND: path=%s offset=%u storedBytes=%u compressionToggle=%d",
                        target.storedPath.c_str(), target.offset, target.size,
                        target.compressionToggle ? 1 : 0);

    std::vector<uint8_t> nif;
    bool compressed = false;
    size_t embeddedNameBytes = 0;
    if (!ExtractTarget(file, header, target, nif, compressed, embeddedNameBytes)) {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "Q5D target extraction/decompression failed");
        std::fclose(file);
        return;
    }
    std::fclose(file);

    const std::string headerLine = NifHeaderLine(nif);
    const bool isNif = headerLine.rfind("Gamebryo File Format", 0) == 0 ||
                       headerLine.rfind("NetImmerse File Format", 0) == 0;
    __android_log_print(isNif ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, TAG,
                        "Q5D NIF EXTRACTED: bytes=%zu compressed=%d embeddedNameBytes=%zu header=%s",
                        nif.size(), compressed ? 1 : 0, embeddedNameBytes,
                        headerLine.empty() ? "<empty>" : headerLine.c_str());

    if (isNif) {
        __android_log_print(ANDROID_LOG_INFO, TAG,
                            "Q5D SUCCESS: real Megaton Chair03 NIF extracted from Fallout - Meshes.bsa on Quest");
    } else {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "Q5D INVALID NIF: extracted bytes did not begin with a Gamebryo/NetImmerse header");
    }
}

__attribute__((constructor)) void Fallout3BsaExtractConstructor() {
    ProbeFirstMegatonNif();
}

} // namespace
