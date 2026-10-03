#pragma once

#include "fo3-bsa-archive.h"
#include <array>

namespace fo3assets {

// Registry entries are shared across mesh, DDS and raw UI consumers. Only
// acquisition locks the registry; indexing and file reads belong to each BSA.
std::shared_ptr<BsaArchive> GetBsaArchive(const std::string& archivePath);
std::string FalloutDataPath(const std::string& filename);
const std::array<std::string, 2>& TextureArchivePaths();

// Preserve the existing texture archive fallback order. No loose-file or
// plugin precedence is introduced by this consolidation.
bool LoadTextureFile(const std::string& request, std::vector<uint8_t>& bytes,
                     BsaFileInfo* info = nullptr,
                     size_t maxBytes = 128u * 1024u * 1024u);

} // namespace fo3assets
