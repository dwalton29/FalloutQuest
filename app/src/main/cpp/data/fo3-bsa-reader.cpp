#include "fo3-bsa-reader.h"
#include "fo3-asset-store.h"

#include <android/log.h>
#include <utility>

namespace {
constexpr const char* TAG = "FalloutQuest";
std::shared_ptr<fo3assets::BsaArchive> MeshArchive() {
    return fo3assets::GetBsaArchive(fo3assets::FalloutDataPath("Fallout - Meshes.bsa"));
}
} // namespace

bool ListFalloutMeshFilesByPrefix(const std::string& prefix,
                                  std::vector<FalloutMeshIndexEntry>& outEntries,
                                  size_t maxResults) {
    outEntries.clear();
    std::vector<fo3assets::BsaFileInfo> entries;
    if (!MeshArchive()->List(prefix, entries, fo3assets::BsaPathKind::Mesh,
                             maxResults)) return false;
    outEntries.reserve(entries.size());
    for (auto& entry : entries) {
        outEntries.push_back({std::move(entry.path), entry.storedBytes, entry.compressed});
    }
    return true;
}

bool LoadFalloutMeshFile(const std::string& modelPath,
                         std::vector<uint8_t>& outBytes,
                         std::string* outResolvedPath) {
    fo3assets::BsaFileInfo info;
    if (!MeshArchive()->Read(modelPath, outBytes, &info,
                             fo3assets::BsaPathKind::Mesh)) {
        __android_log_print(ANDROID_LOG_WARN, TAG,
                            "Q6A MESH MISS OR EXTRACT FAILED: %s", modelPath.c_str());
        return false;
    }
    if (outResolvedPath) *outResolvedPath = info.path;
    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "Q6A MESH EXTRACTED: path=%s bytes=%zu compressed=%d",
                        info.path.c_str(), outBytes.size(), info.compressed ? 1 : 0);
    return true;
}
