#include "fo3-asset-store.h"
#include "fo3-install-paths.h"

#include <mutex>
#include <unordered_map>

namespace fo3assets {

std::shared_ptr<BsaArchive> GetBsaArchive(const std::string& path) {
    static std::mutex mutex;
    static std::unordered_map<std::string, std::shared_ptr<BsaArchive>> archives;
    std::lock_guard<std::mutex> lock(mutex);
    auto& archive = archives[path];
    if (!archive) archive = std::make_shared<BsaArchive>(path);
    return archive;
}

std::string FalloutDataPath(const std::string& filename) {
    return InstallFilePath(filename);
}

const std::array<std::string, 2>& TextureArchivePaths() {
    static const std::array<std::string, 2> paths{
        FalloutDataPath("Fallout - Textures.bsa"), FalloutDataPath("textures.bsa")};
    return paths;
}

bool LoadTextureFile(const std::string& request, std::vector<uint8_t>& bytes,
                     BsaFileInfo* info, size_t maxBytes) {
    bytes.clear();
    if (info) *info = {};
    for (const std::string& path : TextureArchivePaths()) {
        if (GetBsaArchive(path)->Read(
                request, bytes, info, BsaPathKind::Texture, maxBytes)) return true;
    }
    return false;
}

} // namespace fo3assets
