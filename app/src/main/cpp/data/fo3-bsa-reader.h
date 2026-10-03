#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct FalloutMeshIndexEntry {
    std::string path;
    uint32_t storedBytes = 0u;
    bool compressed = false;
};

// Read-only BSA index query. Paths are normalized to lower-case backslash form.
// Does not extract file payloads and is safe to use for archive-layout probes.
bool ListFalloutMeshFilesByPrefix(
    const std::string& prefix,
    std::vector<FalloutMeshIndexEntry>& outEntries,
    size_t maxResults = 0u);

// Loads one path from Fallout - Meshes.bsa. The BSA index is cached after the
// first call so Q6 can resolve many ESM-driven model paths without rescanning
// all ~14k archive entries for every object.
bool LoadFalloutMeshFile(const std::string& modelPath,
                         std::vector<uint8_t>& outBytes,
                         std::string* outResolvedPath = nullptr);
