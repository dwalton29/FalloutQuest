// Q7.11 landscape material resolver.
// Textually included by fo3-cell-spawn-q74.cpp after the proven Q7.5/Q7.9 ESM
// helpers. LAND base layers point at LTEX records; Fallout 3 LTEX records point
// through TNAM to TXST records, whose TX00 is the diffuse DDS path.

#include <array>
#include <string>
#include <unordered_map>

namespace {

struct Q711LtexInfo {
    uint32_t textureSetFormId = 0u;
    std::string iconPath;
};

bool q711TextureIndexAttempted = false;
std::unordered_map<uint32_t, std::array<uint32_t, 4>> q711LandBaseTextures;
std::unordered_map<uint32_t, Q711LtexInfo> q711LandscapeTextures;
std::unordered_map<uint32_t, std::string> q711TextureSetDiffuse;

std::string Q711CString(const uint8_t* bytes, uint32_t size) {
    if (!bytes || size == 0u) return {};
    size_t length = 0u;
    while (length < size && bytes[length] != 0u) ++length;
    return std::string(reinterpret_cast<const char*>(bytes), length);
}

bool BuildFo3TerrainTextureIndexQ711() {
    if (q711TextureIndexAttempted) {
        return !q711LandBaseTextures.empty() && !q711TextureSetDiffuse.empty();
    }
    q711TextureIndexAttempted = true;

    FILE* file = std::fopen(ESM_PATH_Q75, "rb");
    if (!file) {
        Q75_LOGE("Q7.11 TERRAIN TEXTURE INDEX FAILED: reason=esm-open");
        return false;
    }
    const int64_t fileSize = FileSizeQ75(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE_Q75)) {
        std::fclose(file);
        Q75_LOGE("Q7.11 TERRAIN TEXTURE INDEX FAILED: reason=esm-size");
        return false;
    }

    size_t landRecords = 0u;
    size_t baseLayers = 0u;
    size_t ltexRecords = 0u;
    size_t txstRecords = 0u;

    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        if (offset + HEADER_SIZE_Q75 > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[HEADER_SIZE_Q75]{};
        if (!ReadExactQ75(file, header, sizeof(header))) break;
        const uint32_t sizeField = ReadLe32Q75(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < HEADER_SIZE_Q75 ||
                offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            continue;
        }

        const uint32_t flags = ReadLe32Q75(header + 8u);
        const uint32_t formId = ReadLe32Q75(header + 12u);
        const uint64_t payloadEnd = offset + HEADER_SIZE_Q75 + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;

        const bool isLand = std::memcmp(header, "LAND", 4u) == 0;
        const bool isLtex = std::memcmp(header, "LTEX", 4u) == 0;
        const bool isTxst = std::memcmp(header, "TXST", 4u) == 0;
        if (!isLand && !isLtex && !isTxst) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        std::vector<uint8_t> payload;
        if (!ReadPayloadQ75(file, sizeField, flags, payload)) break;

        if (isLand) {
            ++landRecords;
            std::array<uint32_t, 4> quadrants{};
            WalkSubrecordsQ75(payload.data(), payload.size(),
                              [&](const char* type, const uint8_t* bytes, uint32_t subSize) {
                if (std::memcmp(type, "BTXT", 4u) != 0 || subSize < 8u) return;
                const uint32_t texture = ReadLe32Q75(bytes);
                const uint8_t quadrant = bytes[4u];
                if (quadrant < quadrants.size() && texture != 0u) {
                    quadrants[quadrant] = texture;
                    ++baseLayers;
                }
            });
            bool any = false;
            for (uint32_t texture : quadrants) any = any || texture != 0u;
            if (any) q711LandBaseTextures[formId] = quadrants;
        } else if (isLtex) {
            ++ltexRecords;
            Q711LtexInfo info;
            WalkSubrecordsQ75(payload.data(), payload.size(),
                              [&](const char* type, const uint8_t* bytes, uint32_t subSize) {
                if (std::memcmp(type, "TNAM", 4u) == 0 && subSize >= 4u) {
                    info.textureSetFormId = ReadLe32Q75(bytes);
                } else if (std::memcmp(type, "ICON", 4u) == 0 && subSize > 0u) {
                    info.iconPath = Q711CString(bytes, subSize);
                }
            });
            if (info.textureSetFormId != 0u || !info.iconPath.empty()) {
                q711LandscapeTextures[formId] = std::move(info);
            }
        } else if (isTxst) {
            ++txstRecords;
            std::string diffuse;
            WalkSubrecordsQ75(payload.data(), payload.size(),
                              [&](const char* type, const uint8_t* bytes, uint32_t subSize) {
                if (std::memcmp(type, "TX00", 4u) == 0 && diffuse.empty()) {
                    diffuse = Q711CString(bytes, subSize);
                }
            });
            if (!diffuse.empty()) q711TextureSetDiffuse[formId] = std::move(diffuse);
        }
    }

    std::fclose(file);
    Q75_LOGI("Q7.11 TERRAIN TEXTURE INDEX: LAND=%zu baseLayers=%zu LANDwithBase=%zu LTEX=%zu resolvedLTEX=%zu TXST=%zu diffuseTXST=%zu",
             landRecords, baseLayers, q711LandBaseTextures.size(),
             ltexRecords, q711LandscapeTextures.size(),
             txstRecords, q711TextureSetDiffuse.size());
    return !q711LandBaseTextures.empty();
}

} // namespace

std::string ResolveFo3TerrainBaseTextureQ711(uint32_t landFormId, int quadrant) {
    if (quadrant < 0 || quadrant > 3) return {};
    if (!BuildFo3TerrainTextureIndexQ711()) return {};

    const auto landIt = q711LandBaseTextures.find(landFormId);
    if (landIt == q711LandBaseTextures.end()) return {};
    const uint32_t ltexFormId = landIt->second[static_cast<size_t>(quadrant)];
    if (ltexFormId == 0u) return {};

    const auto ltexIt = q711LandscapeTextures.find(ltexFormId);
    if (ltexIt == q711LandscapeTextures.end()) return {};
    if (ltexIt->second.textureSetFormId != 0u) {
        const auto txstIt = q711TextureSetDiffuse.find(ltexIt->second.textureSetFormId);
        if (txstIt != q711TextureSetDiffuse.end()) return txstIt->second;
    }
    // Some tools expose ICON directly on LTEX. Keep it as a conservative
    // fallback if a texture set is absent or malformed.
    return ltexIt->second.iconPath;
}
