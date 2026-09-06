#include "fo3-bsa-reader.h"

#include <android/log.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <zlib.h>

namespace {

constexpr const char* TAG = "FalloutQuest";
constexpr const char* MESH_BSA =
        "/data/user/0/com.falloutquest.app/files/Fallout3/Data/Fallout - Meshes.bsa";
constexpr uint32_t BSA_VERSION = 104u;
constexpr uint32_t MAX_FILE_BYTES = 128u * 1024u * 1024u;

#define Q6A_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q6A_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define Q6A_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

uint32_t ReadLe32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

bool ReadExact(FILE* file, void* dst, size_t size) {
    return std::fread(dst, 1, size, file) == size;
}

std::string Normalize(std::string value) {
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
    for (size_t i = 0; i < 8192u; ++i) {
        const int ch = std::fgetc(file);
        if (ch == EOF) return false;
        if (ch == 0) return true;
        out.push_back(static_cast<char>(ch));
    }
    return false;
}

struct Entry {
    uint32_t storedBytes = 0;
    uint32_t offset = 0;
    bool compressionToggle = false;
};

struct PendingEntry {
    std::string folder;
    Entry entry;
};

struct ArchiveIndex {
    bool attempted = false;
    bool ready = false;
    uint32_t archiveFlags = 0;
    std::unordered_map<std::string, Entry> files;
};

ArchiveIndex gIndex;

bool BuildIndex() {
    if (gIndex.attempted) return gIndex.ready;
    gIndex.attempted = true;

    FILE* file = std::fopen(MESH_BSA, "rb");
    if (!file) {
        Q6A_LOGE("Q6A BSA INDEX: cannot open Fallout - Meshes.bsa");
        return false;
    }

    uint8_t header[36]{};
    if (!ReadExact(file, header, sizeof(header)) || std::memcmp(header, "BSA\0", 4) != 0) {
        std::fclose(file);
        Q6A_LOGE("Q6A BSA INDEX: invalid header");
        return false;
    }

    const uint32_t version = ReadLe32(header + 4);
    const uint32_t foldersOffset = ReadLe32(header + 8);
    const uint32_t archiveFlags = ReadLe32(header + 12);
    const uint32_t folderCount = ReadLe32(header + 16);
    const uint32_t fileCount = ReadLe32(header + 20);

    if (version != BSA_VERSION || folderCount == 0 || fileCount == 0 ||
        folderCount > 1000000u || fileCount > 2000000u || foldersOffset < 36u ||
        (archiveFlags & 1u) == 0u || (archiveFlags & 2u) == 0u) {
        std::fclose(file);
        Q6A_LOGE("Q6A BSA INDEX: unsupported archive version=%u folders=%u files=%u flags=%08X",
                 version, folderCount, fileCount, archiveFlags);
        return false;
    }

    if (fseeko(file, static_cast<off_t>(foldersOffset), SEEK_SET) != 0) {
        std::fclose(file);
        return false;
    }

    std::vector<uint32_t> folderFileCounts;
    folderFileCounts.reserve(folderCount);
    for (uint32_t i = 0; i < folderCount; ++i) {
        uint8_t record[16]{};
        if (!ReadExact(file, record, sizeof(record))) {
            std::fclose(file);
            return false;
        }
        const uint32_t count = ReadLe32(record + 8);
        if (count > fileCount) {
            std::fclose(file);
            return false;
        }
        folderFileCounts.push_back(count);
    }

    std::vector<PendingEntry> pending;
    pending.reserve(fileCount);
    for (uint32_t count : folderFileCounts) {
        uint8_t nameLen = 0;
        if (!ReadExact(file, &nameLen, 1) || nameLen == 0u) {
            std::fclose(file);
            return false;
        }
        std::vector<char> folderBytes(nameLen);
        if (!ReadExact(file, folderBytes.data(), folderBytes.size())) {
            std::fclose(file);
            return false;
        }
        if (!folderBytes.empty() && folderBytes.back() == '\0') folderBytes.pop_back();
        const std::string folder = Normalize(std::string(folderBytes.begin(), folderBytes.end()));

        for (uint32_t j = 0; j < count; ++j) {
            uint8_t record[16]{};
            if (!ReadExact(file, record, sizeof(record))) {
                std::fclose(file);
                return false;
            }
            const uint32_t rawSize = ReadLe32(record + 8);
            PendingEntry p;
            p.folder = folder;
            p.entry.storedBytes = rawSize & 0x3fffffffu;
            p.entry.compressionToggle = (rawSize & 0x40000000u) != 0u;
            p.entry.offset = ReadLe32(record + 12);
            pending.push_back(std::move(p));
        }
    }

    if (pending.size() != fileCount) {
        Q6A_LOGW("Q6A BSA INDEX: header file count=%u parsed=%zu", fileCount, pending.size());
    }

    gIndex.files.reserve(pending.size() * 2u);
    for (PendingEntry& p : pending) {
        std::string fileName;
        if (!ReadCString(file, fileName)) {
            std::fclose(file);
            return false;
        }
        std::string path = p.folder;
        if (!path.empty() && !fileName.empty()) path += "\\";
        path += Normalize(fileName);
        gIndex.files[Normalize(path)] = p.entry;
    }

    std::fclose(file);
    gIndex.archiveFlags = archiveFlags;
    gIndex.ready = true;
    Q6A_LOGI("Q6A BSA INDEX READY: meshes=%zu flags=0x%08X", gIndex.files.size(), archiveFlags);
    return true;
}

const Entry* FindEntry(const std::string& requested, std::string& resolved) {
    if (!BuildIndex()) return nullptr;

    const std::string normalized = Normalize(requested);
    const std::string candidates[] = {
        normalized,
        normalized.rfind("meshes\\", 0) == 0 ? normalized.substr(7) : "meshes\\" + normalized,
    };

    for (const std::string& candidate : candidates) {
        auto it = gIndex.files.find(candidate);
        if (it != gIndex.files.end()) {
            resolved = candidate;
            return &it->second;
        }
    }
    return nullptr;
}

bool Inflate(const std::vector<uint8_t>& compressed, uint32_t originalSize,
             std::vector<uint8_t>& out) {
    if (originalSize == 0 || originalSize > MAX_FILE_BYTES) return false;
    out.resize(originalSize);
    uLongf outLen = static_cast<uLongf>(out.size());
    const int result = uncompress(reinterpret_cast<Bytef*>(out.data()), &outLen,
                                  reinterpret_cast<const Bytef*>(compressed.data()),
                                  static_cast<uLong>(compressed.size()));
    if (result != Z_OK || outLen != originalSize) {
        out.clear();
        return false;
    }
    return true;
}

} // namespace

bool LoadFalloutMeshFile(const std::string& modelPath,
                         std::vector<uint8_t>& outBytes,
                         std::string* outResolvedPath) {
    outBytes.clear();
    std::string resolved;
    const Entry* entry = FindEntry(modelPath, resolved);
    if (!entry) {
        Q6A_LOGW("Q6A MESH MISS: %s", modelPath.c_str());
        return false;
    }

    if (entry->storedBytes == 0u || entry->storedBytes > MAX_FILE_BYTES) return false;

    FILE* file = std::fopen(MESH_BSA, "rb");
    if (!file) return false;
    if (fseeko(file, static_cast<off_t>(entry->offset), SEEK_SET) != 0) {
        std::fclose(file);
        return false;
    }

    size_t remaining = entry->storedBytes;
    const bool embeddedNames = (gIndex.archiveFlags & 0x100u) != 0u;
    if (embeddedNames) {
        uint8_t nameLen = 0;
        if (!ReadExact(file, &nameLen, 1) || remaining < static_cast<size_t>(nameLen) + 1u) {
            std::fclose(file);
            return false;
        }
        if (fseeko(file, static_cast<off_t>(nameLen), SEEK_CUR) != 0) {
            std::fclose(file);
            return false;
        }
        remaining -= static_cast<size_t>(nameLen) + 1u;
    }

    const bool compressedDefault = (gIndex.archiveFlags & 4u) != 0u;
    const bool compressed = compressedDefault != entry->compressionToggle;

    bool ok = false;
    if (!compressed) {
        if (remaining > 0u && remaining <= MAX_FILE_BYTES) {
            outBytes.resize(remaining);
            ok = ReadExact(file, outBytes.data(), outBytes.size());
        }
    } else if (remaining >= 4u) {
        uint8_t sizeBytes[4]{};
        if (ReadExact(file, sizeBytes, sizeof(sizeBytes))) {
            const uint32_t originalSize = ReadLe32(sizeBytes);
            remaining -= 4u;
            if (remaining > 0u && remaining <= MAX_FILE_BYTES) {
                std::vector<uint8_t> compressedBytes(remaining);
                if (ReadExact(file, compressedBytes.data(), compressedBytes.size())) {
                    ok = Inflate(compressedBytes, originalSize, outBytes);
                }
            }
        }
    }

    std::fclose(file);
    if (!ok) {
        outBytes.clear();
        Q6A_LOGW("Q6A MESH EXTRACT FAILED: %s", resolved.c_str());
        return false;
    }

    if (outResolvedPath) *outResolvedPath = resolved;
    Q6A_LOGI("Q6A MESH EXTRACTED: path=%s bytes=%zu compressed=%d",
             resolved.c_str(), outBytes.size(), compressed ? 1 : 0);
    return true;
}
