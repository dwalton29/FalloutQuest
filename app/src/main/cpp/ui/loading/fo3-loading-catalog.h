#pragma once
#include "fo3-loading-pose.h"
#include <zlib.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// CPU-only ESM loading catalogue. Owned by the presentation worker, published
// through Preparation before any reader accesses these immutable vectors.
namespace fo3loading {
constexpr const char* ESM_PATH =
    "/data/user/0/com.falloutquest.app/files/Fallout3/Data/Fallout3.esm";
constexpr size_t RECORD_HEADER = 24u;
constexpr uint32_t COMPRESSED_RECORD = 0x00040000u;

struct LoadingScreen {
    uint32_t formId = 0u;
    std::string editorId;
    std::string description;
    std::string iconPath;
    std::vector<uint32_t> locations;
};

inline std::vector<LoadingScreen> gScreens;
inline std::vector<std::string> gModels;
inline bool gScreensPrepared = false;
inline uint16_t Read16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           (static_cast<uint16_t>(p[1]) << 8u);
}

inline uint32_t Read32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8u) |
           (static_cast<uint32_t>(p[2]) << 16u) |
           (static_cast<uint32_t>(p[3]) << 24u);
}

inline std::string ReadString(const uint8_t* p, size_t n) {
    while (n > 0u && p[n - 1u] == 0u) --n;
    return std::string(reinterpret_cast<const char*>(p), n);
}

inline bool ReadExact(FILE* f, void* dst, size_t bytes) {
    return f && std::fread(dst, 1u, bytes, f) == bytes;
}

inline bool InflateRecord(const std::vector<uint8_t>& stored,
                          uint32_t flags,
                          std::vector<uint8_t>& payload) {
    if ((flags & COMPRESSED_RECORD) == 0u) {
        payload = stored;
        return true;
    }
    if (stored.size() < 5u) return false;
    const uint32_t wanted = Read32(stored.data());
    if (wanted == 0u || wanted > 8u * 1024u * 1024u) return false;
    payload.resize(wanted);
    uLongf outputBytes = static_cast<uLongf>(wanted);
    const int z = uncompress(payload.data(), &outputBytes,
                             stored.data() + 4u,
                             static_cast<uLong>(stored.size() - 4u));
    if (z != Z_OK || outputBytes != wanted) {
        payload.clear();
        return false;
    }
    return true;
}

inline void ParseLoadingRecord(uint32_t formId,
                               const std::vector<uint8_t>& payload) {
    LoadingScreen screen;
    screen.formId = formId;
    size_t pos = 0u;
    while (pos + 6u <= payload.size()) {
        char type[5]{
            static_cast<char>(payload[pos]), static_cast<char>(payload[pos + 1u]),
            static_cast<char>(payload[pos + 2u]), static_cast<char>(payload[pos + 3u]), 0};
        uint32_t size = Read16(payload.data() + pos + 4u);
        pos += 6u;

        if (std::memcmp(type, "XXXX", 4u) == 0) {
            if (size != 4u || pos + 4u + 6u > payload.size()) break;
            const uint32_t extended = Read32(payload.data() + pos);
            pos += 4u;
            type[0] = static_cast<char>(payload[pos]);
            type[1] = static_cast<char>(payload[pos + 1u]);
            type[2] = static_cast<char>(payload[pos + 2u]);
            type[3] = static_cast<char>(payload[pos + 3u]);
            type[4] = 0;
            pos += 6u; // next subrecord's 16-bit size is ignored by XXXX semantics
            size = extended;
        }

        if (pos + size > payload.size()) break;
        const uint8_t* data = payload.data() + pos;
        if (std::memcmp(type, "EDID", 4u) == 0) {
            screen.editorId = ReadString(data, size);
        } else if (std::memcmp(type, "DESC", 4u) == 0) {
            screen.description = ReadString(data, size);
        } else if (std::memcmp(type, "ICON", 4u) == 0) {
            screen.iconPath = ReadString(data, size);
        } else if (std::memcmp(type, "LNAM", 4u) == 0 && size >= 4u) {
            screen.locations.push_back(Read32(data));
        }
        pos += size;
    }
    if (!screen.iconPath.empty()) gScreens.push_back(std::move(screen));
}

// Fallout 3 LSCR records only author pictures. The rotating exhibit uses
// original WEAP/MISC MODL records rather than pretending LSCR contains a model.
inline void ParseModelRecord(const std::vector<uint8_t>& payload) {
    size_t p=0;
    while(p+6<=payload.size()) {
        const uint8_t* h=payload.data()+p;
        size_t size=Read16(h+4); p+=6;
        if(size>payload.size()-p) return;
        if(std::memcmp(h,"MODL",4)==0) {
            std::string path=ReadString(payload.data()+p,size);
            std::string normalized=path;
            for(char& c:normalized) c=c=='/'?'\\':static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if((normalized.find("weapons\\")==0 || normalized.find("clutter\\junk\\")==0 ||
                normalized.find("clutter\\teddybear\\")==0) &&
                std::find(gModels.begin(),gModels.end(),path)==gModels.end())
                gModels.push_back(std::move(path));
        }
        p+=size;
    }
}

inline void Prepare(const char* path = ESM_PATH, const std::atomic<bool>* cancelled = nullptr) {
    if (gScreensPrepared) return;
    gScreensPrepared = true;

    FILE* f = std::fopen(path, "rb");
    if (!f) {
        return;
    }

    uint8_t header[RECORD_HEADER]{};
    while ((!cancelled || !cancelled->load()) && ReadExact(f, header, sizeof(header))) {
        const uint32_t sizeField = Read32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            // Group size includes this 24-byte header. Children immediately
            // follow, so continue sequentially instead of skipping the group.
            continue;
        }
        if (sizeField > 64u * 1024u * 1024u) break;

        const uint32_t flags = Read32(header + 8u);
        const uint32_t formId = Read32(header + 12u);
        if (std::memcmp(header, "LSCR", 4u) == 0 ||
            std::memcmp(header, "WEAP", 4u) == 0 ||
            std::memcmp(header, "MISC", 4u) == 0) {
            std::vector<uint8_t> stored(sizeField);
            if (!ReadExact(f, stored.data(), stored.size())) break;
            std::vector<uint8_t> payload;
            if (InflateRecord(stored, flags, payload)) {
                if (std::memcmp(header, "LSCR", 4u) == 0) {
                    ParseLoadingRecord(formId, payload);
                } else ParseModelRecord(payload);
            }
        } else if (std::fseek(f, static_cast<long>(sizeField), SEEK_CUR) != 0) {
            break;
        }
    }
    std::fclose(f);


}

inline bool ContainsLocation(const LoadingScreen& screen, uint32_t formId) {
    return formId != 0u &&
           std::find(screen.locations.begin(), screen.locations.end(), formId) !=
               screen.locations.end();
}

inline const LoadingScreen* Select(uint32_t cellFormId, uint32_t worldspaceFormId, uint64_t random = 0) {
    if (gScreens.empty()) return nullptr;
    std::vector<const LoadingScreen*> exact;
    std::vector<const LoadingScreen*> world;
    std::vector<const LoadingScreen*> generic;
    for (const LoadingScreen& screen : gScreens) {
        if (ContainsLocation(screen, cellFormId)) exact.push_back(&screen);
        else if (ContainsLocation(screen, worldspaceFormId)) world.push_back(&screen);
        else if (screen.locations.empty()) generic.push_back(&screen);
    }
    const std::vector<const LoadingScreen*>* choices = nullptr;
    if (!exact.empty()) choices = &exact;
    else if (!world.empty()) choices = &world;
    else if (!generic.empty()) choices = &generic;
    if (!choices || choices->empty()) choices = nullptr;
    if (!choices) return &gScreens[fo3loadingpose::Mix(random ^ cellFormId ^ worldspaceFormId) % gScreens.size()];
    const uint32_t hash = cellFormId * 1664525u + worldspaceFormId * 1013904223u;
    return (*choices)[fo3loadingpose::Mix(random ^ hash) % choices->size()];
}

} // namespace fo3loading
