#pragma once
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <string>
#include <strings.h>
#include <sys/stat.h>

namespace fo3assets {
// Java sets this before NativeActivity loads the library. Immutable for the
// process: archive and ESM indexes must never be shared between installations.
inline const std::string& FalloutDataRoot() {
    static const std::string root = [] {
        const char* configured = std::getenv("FALLOUTQUEST_DATA_ROOT");
        std::string result = configured && *configured ? configured :
            "/data/user/0/com.falloutquest.app/files/Fallout3/Data";
        while (!result.empty() && result.back() == '/') result.pop_back();
        return result + '/';
    }();
    return root;
}

inline std::string InstallFilePath(const std::string& filename) {
    const auto& root = FalloutDataRoot();
    const auto exact = root + filename;
    struct stat info{};
    if (filename.empty() || stat(exact.c_str(), &info) == 0) return exact;
    // Windows Steam installations use a case-insensitive filesystem. Resolve
    // the top-level archive/master spelling after copying to Android.
    if (filename.find('/') == std::string::npos) {
        if (DIR* directory = opendir(root.c_str())) {
            std::string match;
            while (auto* entry = readdir(directory)) {
                if (strcasecmp(entry->d_name, filename.c_str()) == 0) {
                    match = root + entry->d_name;
                    break;
                }
            }
            closedir(directory);
            if (!match.empty()) return match;
        }
    }
    return exact;
}

inline const std::string& FalloutMasterPath() {
    static const std::string path = InstallFilePath("Fallout3.esm");
    return path;
}
} // namespace fo3assets
