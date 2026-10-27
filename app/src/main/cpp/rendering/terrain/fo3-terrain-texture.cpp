#include "fo3-install-paths.h"
// Q7.18 landscape material resolver.
// Textually included by fo3-cell-spawn-q74.cpp after the proven Q7.5/Q7.9 ESM
// helpers. Q7.11 resolved only LAND BTXT base layers. Q7.18 also preserves the
// authored ATXT/VTXT paint layers so heavily painted Megaton ground does not
// collapse to one base texture or the brown fallback.

#include <array>
#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct Q711LtexInfo {
    uint32_t textureSetFormId = 0u;
    uint8_t havokMaterial = 0u;
    std::string iconPath;
};

struct Q718TerrainAlphaLayer {
    uint32_t ltexFormId = 0u;
    uint8_t quadrant = 0u;
    uint16_t layer = 0u;
    std::array<float, 17u * 17u> opacity{};
    size_t authoredVertices = 0u;
};

bool q711TextureIndexAttempted = false;
std::unordered_map<uint32_t, std::array<uint32_t, 4>> q711LandBaseTextures;
std::unordered_map<uint32_t, std::vector<Q718TerrainAlphaLayer>> q718LandAlphaTextures;
std::unordered_map<uint32_t, Q711LtexInfo> q711LandscapeTextures;
std::unordered_map<uint32_t, std::string> q711TextureSetDiffuse;

std::string Q711CString(const uint8_t* bytes, uint32_t size) {
    if (!bytes || size == 0u) return {};
    size_t length = 0u;
    while (length < size && bytes[length] != 0u) ++length;
    return std::string(reinterpret_cast<const char*>(bytes), length);
}

std::string ResolveQ718LtexDiffuse(uint32_t ltexFormId) {
    if (ltexFormId == 0u) return {};
    const auto ltexIt = q711LandscapeTextures.find(ltexFormId);
    if (ltexIt == q711LandscapeTextures.end()) return {};
    if (ltexIt->second.textureSetFormId != 0u) {
        const auto txstIt = q711TextureSetDiffuse.find(ltexIt->second.textureSetFormId);
        if (txstIt != q711TextureSetDiffuse.end()) return txstIt->second;
    }
    return ltexIt->second.iconPath;
}

bool BuildFo3TerrainTextureIndexQ711() {
    if (q711TextureIndexAttempted) {
        return !q711LandBaseTextures.empty() && !q711TextureSetDiffuse.empty();
    }
    q711TextureIndexAttempted = true;

    FILE* file = std::fopen(fo3assets::FalloutMasterPath().c_str(), "rb");
    if (!file) {
        Q75_LOGE("Q7.18 TERRAIN TEXTURE INDEX FAILED: reason=esm-open");
        return false;
    }
    const int64_t fileSize = FileSizeQ75(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE_Q75)) {
        std::fclose(file);
        Q75_LOGE("Q7.18 TERRAIN TEXTURE INDEX FAILED: reason=esm-size");
        return false;
    }

    size_t landRecords = 0u;
    size_t baseLayers = 0u;
    size_t alphaLayers = 0u;
    size_t alphaVertices = 0u;
    size_t invalidAlphaPositions = 0u;
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
            std::vector<Q718TerrainAlphaLayer> alpha;
            int currentAlpha = -1;

            WalkSubrecordsQ75(payload.data(), payload.size(),
                              [&](const char* type, const uint8_t* bytes, uint32_t subSize) {
                if (std::memcmp(type, "BTXT", 4u) == 0 && subSize >= 8u) {
                    const uint32_t texture = ReadLe32Q75(bytes);
                    const uint8_t quadrant = bytes[4u] & 3u;
                    if (texture != 0u) {
                        quadrants[quadrant] = texture;
                        ++baseLayers;
                    }
                    currentAlpha = -1;
                    return;
                }

                if (std::memcmp(type, "ATXT", 4u) == 0 && subSize >= 8u) {
                    Q718TerrainAlphaLayer layer;
                    layer.ltexFormId = ReadLe32Q75(bytes);
                    layer.quadrant = bytes[4u] & 3u;
                    layer.layer = static_cast<uint16_t>(ReadLe16Q75(bytes + 6u) & 7u);
                    if (layer.ltexFormId != 0u) {
                        alpha.push_back(layer);
                        currentAlpha = static_cast<int>(alpha.size() - 1u);
                        ++alphaLayers;
                    } else {
                        currentAlpha = -1;
                    }
                    return;
                }

                if (std::memcmp(type, "VTXT", 4u) == 0 && currentAlpha >= 0) {
                    Q718TerrainAlphaLayer& layer = alpha[static_cast<size_t>(currentAlpha)];
                    for (uint32_t pos = 0u; pos + 8u <= subSize; pos += 8u) {
                        const uint16_t vertex = ReadLe16Q75(bytes + pos);
                        if (vertex >= layer.opacity.size()) {
                            ++invalidAlphaPositions;
                            continue;
                        }
                        float opacity = ReadLeFloatQ75(bytes + pos + 4u);
                        if (!std::isfinite(opacity)) opacity = 0.0f;
                        opacity = std::clamp(opacity, 0.0f, 1.0f);
                        layer.opacity[vertex] = opacity;
                        if (opacity > 0.0001f) {
                            ++layer.authoredVertices;
                            ++alphaVertices;
                        }
                    }
                }
            });

            bool anyBase = false;
            for (uint32_t texture : quadrants) anyBase = anyBase || texture != 0u;
            if (anyBase) q711LandBaseTextures[formId] = quadrants;
            if (!alpha.empty()) q718LandAlphaTextures[formId] = std::move(alpha);
        } else if (isLtex) {
            ++ltexRecords;
            Q711LtexInfo info;
            WalkSubrecordsQ75(payload.data(), payload.size(),
                              [&](const char* type, const uint8_t* bytes, uint32_t subSize) {
                if (std::memcmp(type, "TNAM", 4u) == 0 && subSize >= 4u) {
                    info.textureSetFormId = ReadLe32Q75(bytes);
                } else if (std::memcmp(type, "HNAM", 4u) == 0 && subSize >= 1u) {
                    info.havokMaterial = bytes[0];
                } else if (std::memcmp(type, "ICON", 4u) == 0 && subSize > 0u) {
                    info.iconPath = Q711CString(bytes, subSize);
                }
            });
            q711LandscapeTextures[formId] = std::move(info);
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
    Q75_LOGI("Q7.18 TERRAIN TEXTURE INDEX: LAND=%zu baseLayers=%zu LANDwithBase=%zu alphaLayers=%zu LANDwithAlpha=%zu alphaVertices=%zu invalidAlphaPositions=%zu LTEX=%zu resolvedLTEX=%zu TXST=%zu diffuseTXST=%zu",
             landRecords, baseLayers, q711LandBaseTextures.size(),
             alphaLayers, q718LandAlphaTextures.size(), alphaVertices,
             invalidAlphaPositions, ltexRecords, q711LandscapeTextures.size(),
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
    return ResolveQ718LtexDiffuse(ltexFormId);
}

const std::vector<Q718TerrainAlphaLayer>& GetFo3TerrainAlphaLayersQ718(uint32_t landFormId) {
    static const std::vector<Q718TerrainAlphaLayer> empty;
    BuildFo3TerrainTextureIndexQ711();
    const auto found = q718LandAlphaTextures.find(landFormId);
    return found != q718LandAlphaTextures.end() ? found->second : empty;
}

std::string ResolveFo3TerrainAlphaTextureQ718(const Q718TerrainAlphaLayer& layer) {
    BuildFo3TerrainTextureIndexQ711();
    return ResolveQ718LtexDiffuse(layer.ltexFormId);
}

// Render-thread read of the existing immutable LAND/LTEX index; never opens
// the master or starts terrain preparation from the movement/audio path.
static uint32_t Fo3TerrainContactMaterial(uint32_t land,float localX,float localY) {
    const int q=(localX>=2048.f?1:0)+(localY>=2048.f?2:0);
    const float gx=std::clamp((localX-(q&1?2048.f:0.f))/128.f,0.f,16.f);
    const float gy=std::clamp((localY-(q&2?2048.f:0.f))/128.f,0.f,16.f);
    const int x=std::min(int(gx),15),y=std::min(int(gy),15);
    const float tx=gx-x,ty=gy-y;
    uint32_t texture=0;float strongest=0,remaining=1;
    const auto painted=q718LandAlphaTextures.find(land);
    if(painted!=q718LandAlphaTextures.end()) {
        // Painter order matches LAND rendering: later layers cover earlier ones.
        for(int layer=7;layer>=0;--layer)for(const auto& a:painted->second) {
            if(a.quadrant!=q||a.layer!=layer)continue;
            auto h=[&](int u,int v){return a.opacity[v*17+u];};
            const float alpha=std::clamp((h(x,y)*(1-tx)+h(x+1,y)*tx)*(1-ty)+
                (h(x,y+1)*(1-tx)+h(x+1,y+1)*tx)*ty,0.f,1.f);
            const float weight=remaining*alpha;remaining*=1-alpha;
            if(weight>strongest){strongest=weight;texture=a.ltexFormId;}
        }
    }
    const auto base=q711LandBaseTextures.find(land);
    if(remaining>strongest && base!=q711LandBaseTextures.end())texture=base->second[q];
    const auto info=q711LandscapeTextures.find(texture);
    return info==q711LandscapeTextures.end()?0u:info->second.havokMaterial;
}
