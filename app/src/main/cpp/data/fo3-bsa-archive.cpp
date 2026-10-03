#include "fo3-bsa-archive.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <zlib.h>

namespace fo3assets {
namespace {

constexpr size_t kMaxEntryBytes = 128u * 1024u * 1024u;
struct FileCloser { void operator()(FILE* file) const { std::fclose(file); } };
using File = std::unique_ptr<FILE, FileCloser>;

uint32_t ReadU32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

bool ReadExact(FILE* file, void* dst, size_t bytes) {
    return std::fread(dst, 1, bytes, file) == bytes;
}

bool ReadCString(FILE* file, std::string& out) {
    out.clear();
    for (size_t i = 0; i < 8192u; ++i) {
        const int c = std::fgetc(file);
        if (c == EOF) return false;
        if (c == 0) return true;
        out.push_back(static_cast<char>(c));
    }
    return false;
}

std::string TextureKey(std::string path) {
    path = NormalizeArchivePath(std::move(path));
    if (path.rfind("data\\", 0) == 0) path.erase(0, 5);
    if (path.rfind("textures\\", 0) == 0) path.erase(0, 9);
    return path;
}

} // namespace

std::string NormalizeArchivePath(std::string path) {
    for (char& c : path) {
        if (c == '/') c = '\\';
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    while (!path.empty() && path.front() == '\\') path.erase(path.begin());
    return path;
}

struct BsaArchive::Impl {
    struct Entry {
        uint32_t storedBytes = 0u;
        uint32_t offset = 0u;
        bool compressionToggle = false;
    };
    struct Pending { std::string folder; Entry entry; };

    explicit Impl(std::string path) : archivePath(std::move(path)) {}
    std::string archivePath;
    std::once_flag once;
    bool ready = false;
    uint32_t flags = 0u;
    std::unordered_map<std::string, Entry> files;
    // Preserve the texture reader's Data/Textures/bare-key aliases and
    // last-entry-wins policy, without rescanning a BSA for raw HUD assets.
    std::unordered_map<std::string, std::string> texturePaths;

    bool BuildIndex() {
        File file(std::fopen(archivePath.c_str(), "rb"));
        if (!file || fseeko(file.get(), 0, SEEK_END) != 0) return false;
        const off_t end = ftello(file.get());
        if (end < 36 || fseeko(file.get(), 0, SEEK_SET) != 0) return false;
        const uint64_t fileSize = static_cast<uint64_t>(end);
        uint8_t header[36]{};
        if (!ReadExact(file.get(), header, sizeof(header)) ||
            std::memcmp(header, "BSA\0", 4) != 0 || ReadU32(header + 4) != 104u)
            return false;
        const uint32_t foldersOffset = ReadU32(header + 8);
        const uint32_t archiveFlags = ReadU32(header + 12);
        const uint32_t folderCount = ReadU32(header + 16);
        const uint32_t fileCount = ReadU32(header + 20);
        if ((archiveFlags & 3u) != 3u || foldersOffset < 36u ||
            folderCount == 0u || folderCount > 1000000u ||
            fileCount == 0u || fileCount > 3000000u ||
            uint64_t(foldersOffset) + uint64_t(folderCount) * 16u > fileSize ||
            uint64_t(fileCount) * 17u > fileSize ||
            fseeko(file.get(), foldersOffset, SEEK_SET) != 0) return false;

        std::vector<uint32_t> counts;
        counts.reserve(folderCount);
        uint64_t totalCount = 0u;
        for (uint32_t i = 0; i < folderCount; ++i) {
            uint8_t record[16]{};
            if (!ReadExact(file.get(), record, sizeof(record))) return false;
            const uint32_t count = ReadU32(record + 8);
            totalCount += count;
            if (totalCount > fileCount) return false;
            counts.push_back(count);
        }
        if (totalCount != fileCount) return false;

        std::vector<Pending> pending;
        pending.reserve(fileCount);
        for (uint32_t count : counts) {
            uint8_t length = 0u;
            if (!ReadExact(file.get(), &length, 1) || length == 0u) return false;
            std::string folder(length, '\0');
            if (!ReadExact(file.get(), folder.data(), folder.size())) return false;
            if (folder.back() == '\0') folder.pop_back();
            folder = NormalizeArchivePath(std::move(folder));
            for (uint32_t i = 0; i < count; ++i) {
                uint8_t record[16]{};
                if (!ReadExact(file.get(), record, sizeof(record))) return false;
                const uint32_t rawSize = ReadU32(record + 8);
                Entry entry{rawSize & 0x3fffffffu, ReadU32(record + 12),
                            (rawSize & 0x40000000u) != 0u};
                if (uint64_t(entry.offset) + entry.storedBytes > fileSize) return false;
                pending.push_back({folder, entry});
            }
        }

        // Publish only a complete index; a failed parse cannot leak entries.
        std::unordered_map<std::string, Entry> indexed;
        std::unordered_map<std::string, std::string> aliases;
        indexed.reserve(fileCount);
        aliases.reserve(fileCount);
        for (const Pending& item : pending) {
            std::string name;
            if (!ReadCString(file.get(), name)) return false;
            std::string path = item.folder;
            if (!path.empty() && !name.empty()) path += "\\";
            path += NormalizeArchivePath(std::move(name));
            indexed[path] = item.entry;
            aliases[TextureKey(path)] = path;
        }
        files = std::move(indexed);
        texturePaths = std::move(aliases);
        flags = archiveFlags;
        return true;
    }

    bool EnsureIndex() {
        std::call_once(once, [this]() { ready = BuildIndex(); });
        return ready;
    }

    const Entry* Find(const std::string& request, BsaPathKind kind,
                      std::string& resolved) const {
        if (kind == BsaPathKind::Texture) {
            const auto alias = texturePaths.find(TextureKey(request));
            if (alias == texturePaths.end()) return nullptr;
            resolved = alias->second;
            return &files.at(resolved);
        }
        resolved = NormalizeArchivePath(request);
        auto found = files.find(resolved);
        if (found == files.end() && kind == BsaPathKind::Mesh) {
            resolved = resolved.rfind("meshes\\", 0) == 0
                ? resolved.substr(7) : "meshes\\" + resolved;
            found = files.find(resolved);
        }
        return found == files.end() ? nullptr : &found->second;
    }

    BsaFileInfo Info(const std::string& path, const Entry& entry) const {
        return {path, entry.storedBytes, ((flags & 4u) != 0u) != entry.compressionToggle};
    }
};

BsaArchive::BsaArchive(std::string path) : impl_(new Impl(std::move(path))) {}
BsaArchive::~BsaArchive() = default;

bool BsaArchive::Read(const std::string& request, std::vector<uint8_t>& out,
                      BsaFileInfo* info, BsaPathKind kind, size_t maxBytes) {
    out.clear();
    if (info) *info = {};
    if (request.empty() || !impl_->EnsureIndex()) return false;
    std::string resolved;
    const Impl::Entry* entry = impl_->Find(request, kind, resolved);
    const size_t limit = std::min(maxBytes, kMaxEntryBytes);
    if (!entry || entry->storedBytes == 0u || entry->storedBytes > limit) return false;

    File file(std::fopen(impl_->archivePath.c_str(), "rb"));
    if (!file || fseeko(file.get(), entry->offset, SEEK_SET) != 0) return false;
    size_t remaining = entry->storedBytes;
    if ((impl_->flags & 0x100u) != 0u) {
        uint8_t length = 0u;
        if (!ReadExact(file.get(), &length, 1) || remaining < size_t(length) + 1u ||
            fseeko(file.get(), length, SEEK_CUR) != 0) return false;
        remaining -= size_t(length) + 1u;
    }

    const BsaFileInfo metadata = impl_->Info(resolved, *entry);
    std::vector<uint8_t> bytes;
    if (!metadata.compressed) {
        if (remaining == 0u || remaining > limit) return false;
        bytes.resize(remaining);
        if (!ReadExact(file.get(), bytes.data(), bytes.size())) return false;
    } else {
        uint8_t sizeBytes[4]{};
        if (remaining < 4u || !ReadExact(file.get(), sizeBytes, sizeof(sizeBytes))) return false;
        const uint32_t originalSize = ReadU32(sizeBytes);
        remaining -= 4u;
        if (originalSize == 0u || originalSize > limit || remaining == 0u) return false;
        std::vector<uint8_t> packed(remaining);
        if (!ReadExact(file.get(), packed.data(), packed.size())) return false;
        bytes.resize(originalSize);
        uLongf length = static_cast<uLongf>(bytes.size());
        if (uncompress(bytes.data(), &length, packed.data(),
                       static_cast<uLong>(packed.size())) != Z_OK || length != originalSize)
            return false;
    }
    out = std::move(bytes);
    if (info) *info = metadata;
    return true;
}

bool BsaArchive::List(const std::string& prefix, std::vector<BsaFileInfo>& out,
                      BsaPathKind kind, size_t maxResults) {
    out.clear();
    if (!impl_->EnsureIndex()) return false;
    const std::string normalized = NormalizeArchivePath(prefix);
    const std::string alternate = normalized.rfind("meshes\\", 0) == 0
        ? normalized.substr(7) : "meshes\\" + normalized;
    for (const auto& pair : impl_->files) {
        bool match = normalized.empty() || pair.first.rfind(normalized, 0) == 0;
        if (!match && kind == BsaPathKind::Mesh)
            match = !alternate.empty() && pair.first.rfind(alternate, 0) == 0;
        if (!match && kind == BsaPathKind::Texture)
            match = TextureKey(pair.first).rfind(TextureKey(prefix), 0) == 0;
        if (match) out.push_back(impl_->Info(pair.first, pair.second));
    }
    std::sort(out.begin(), out.end(), [](const BsaFileInfo& a, const BsaFileInfo& b) {
        return a.path < b.path;
    });
    if (maxResults != 0u && out.size() > maxResults) out.resize(maxResults);
    return true;
}

} // namespace fo3assets
