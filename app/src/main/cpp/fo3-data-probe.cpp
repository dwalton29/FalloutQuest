#include <android/log.h>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

namespace {

constexpr const char* TAG = "FalloutQuest";
constexpr const char* DATA_ROOT = "/data/user/0/com.falloutquest.app/files/Fallout3/Data";
constexpr const char* TARGET_CELL_EDID = "MegatonPlayerHouse";
constexpr unsigned TARGET_CELL_FORM_ID = 0x000151E3u;

struct FileCheck {
    const char* name;
    const char* magic;
    size_t magicLen;
};

long long FileSize(const char* path) {
    struct stat st{};
    return stat(path, &st) == 0 ? static_cast<long long>(st.st_size) : -1;
}

bool HasMagic(const char* path, const char* expected, size_t len) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char buf[8]{};
    const size_t read = std::fread(buf, 1, len, f);
    std::fclose(f);
    return read == len && std::memcmp(buf, expected, len) == 0;
}

void ProbeFo3Data() {
    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "Q5 target locked: %s (FormID %08X)",
                        TARGET_CELL_EDID, TARGET_CELL_FORM_ID);
    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "Q5 data root: %s", DATA_ROOT);

    const FileCheck required[] = {
        {"Fallout3.esm", "TES4", 4},
        {"Fallout - Meshes.bsa", "BSA\0", 4},
        {"Fallout - Textures.bsa", "BSA\0", 4},
        {"Fallout - Textures2.bsa", "BSA\0", 4},
    };

    bool allReady = true;
    for (const auto& item : required) {
        char path[512]{};
        std::snprintf(path, sizeof(path), "%s/%s", DATA_ROOT, item.name);
        const long long size = FileSize(path);
        const bool magicOk = size > 0 && HasMagic(path, item.magic, item.magicLen);
        __android_log_print(magicOk ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, TAG,
                            "Q5 data %s: %s (%lld bytes)",
                            item.name, magicOk ? "READY" : "MISSING/INVALID", size);
        allReady &= magicOk;
    }

    if (allReady) {
        __android_log_print(ANDROID_LOG_INFO, TAG,
                            "Q5 DATA READY: real Fallout 3 ESM/BSA files are readable on Quest");
    } else {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "Q5 DATA NOT READY: stage Steam Fallout 3 Data files into app-private storage");
    }
}

__attribute__((constructor)) void Fallout3DataProbeConstructor() {
    ProbeFo3Data();
}

} // namespace
