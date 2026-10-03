#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fo3assets {

enum class BsaPathKind { Exact, Mesh, Texture };

struct BsaFileInfo {
    std::string path;
    uint32_t storedBytes = 0u;
    bool compressed = false;
};

std::string NormalizeArchivePath(std::string path);

// Fallout 3 BSA v104. Index publication is once-only and thread-safe; reads
// use independent file handles. Archives are immutable for a running session.
// No Android or graphics dependency: the same reader can be tested on a host.
class BsaArchive {
public:
    explicit BsaArchive(std::string archivePath);
    ~BsaArchive();
    BsaArchive(const BsaArchive&) = delete;
    BsaArchive& operator=(const BsaArchive&) = delete;

    bool Read(const std::string& requestedPath, std::vector<uint8_t>& bytes,
              BsaFileInfo* info = nullptr,
              BsaPathKind kind = BsaPathKind::Exact,
              size_t maxBytes = 128u * 1024u * 1024u);
    bool List(const std::string& prefix, std::vector<BsaFileInfo>& entries,
              BsaPathKind kind = BsaPathKind::Exact, size_t maxResults = 0u);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fo3assets
