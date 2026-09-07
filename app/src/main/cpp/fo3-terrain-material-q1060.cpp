// Q10.6: Resolve the authored normal/specular partner for LAND diffuse textures.
// This is textually included after fo3-terrain-texture-q711.cpp, so it reuses the
// proven Fallout3.esm record/subrecord helpers from the exterior CELL runtime.

namespace {

struct Q1060TerrainMaterialInfo {
    std::string normalPath;
    bool specularEnabled = true;
};

bool q1060TerrainMaterialsAttempted = false;
std::unordered_map<std::string, Q1060TerrainMaterialInfo> q1060TerrainMaterialByDiffuse;

std::string Q1060CString(const uint8_t* bytes, uint32_t size) {
    if (!bytes || size == 0u) return {};
    size_t length = 0u;
    while (length < size && bytes[length] != 0u) ++length;
    return std::string(reinterpret_cast<const char*>(bytes), length);
}

bool BuildFo3TerrainMaterialIndexQ1060() {
    if (q1060TerrainMaterialsAttempted) return !q1060TerrainMaterialByDiffuse.empty();
    q1060TerrainMaterialsAttempted = true;

    FILE* file = std::fopen(ESM_PATH_Q75, "rb");
    if (!file) {
        Q75_LOGE("Q10.6 TERRAIN MATERIAL INDEX FAILED: reason=esm-open");
        return false;
    }
    const int64_t fileSize = FileSizeQ75(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE_Q75)) {
        std::fclose(file);
        Q75_LOGE("Q10.6 TERRAIN MATERIAL INDEX FAILED: reason=esm-size");
        return false;
    }

    size_t txstSeen = 0u;
    size_t normalPairs = 0u;
    size_t noSpecular = 0u;
    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        if (offset + HEADER_SIZE_Q75 > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[HEADER_SIZE_Q75]{};
        if (!ReadExactQ75(file, header, sizeof(header))) break;
        const uint32_t sizeField = ReadLe32Q75(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < HEADER_SIZE_Q75 || offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            continue;
        }

        const uint64_t payloadEnd = offset + HEADER_SIZE_Q75 + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        if (std::memcmp(header, "TXST", 4u) != 0) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        const uint32_t flags = ReadLe32Q75(header + 8u);
        std::vector<uint8_t> payload;
        if (!ReadPayloadQ75(file, sizeField, flags, payload)) break;
        ++txstSeen;

        std::string diffuse;
        Q1060TerrainMaterialInfo info;
        WalkSubrecordsQ75(payload.data(), payload.size(),
                          [&](const char* type, const uint8_t* bytes, uint32_t subSize) {
            if (std::memcmp(type, "TX00", 4u) == 0 && diffuse.empty()) {
                diffuse = Q1060CString(bytes, subSize);
            } else if (std::memcmp(type, "TX01", 4u) == 0 && info.normalPath.empty()) {
                info.normalPath = Q1060CString(bytes, subSize);
            } else if (std::memcmp(type, "DNAM", 4u) == 0 && subSize >= 2u) {
                const uint16_t materialFlags = ReadLe16Q75(bytes);
                info.specularEnabled = (materialFlags & 0x0001u) == 0u;
            }
        });

        if (!diffuse.empty() && !info.normalPath.empty()) {
            if (!info.specularEnabled) ++noSpecular;
            q1060TerrainMaterialByDiffuse[diffuse] = std::move(info);
            ++normalPairs;
        }
    }

    std::fclose(file);
    Q75_LOGI("Q10.6 TERRAIN MATERIAL INDEX: TXST=%zu normalPairs=%zu noSpecular=%zu source=Fallout3.esm/TX01",
             txstSeen, normalPairs, noSpecular);
    return !q1060TerrainMaterialByDiffuse.empty();
}

} // namespace

std::string ResolveFo3TerrainNormalTextureQ1060(const std::string& diffusePath) {
    if (diffusePath.empty() || !BuildFo3TerrainMaterialIndexQ1060()) return {};
    const auto found = q1060TerrainMaterialByDiffuse.find(diffusePath);
    return found == q1060TerrainMaterialByDiffuse.end() ? std::string{} : found->second.normalPath;
}

bool IsFo3TerrainSpecularEnabledQ1060(const std::string& diffusePath) {
    if (diffusePath.empty() || !BuildFo3TerrainMaterialIndexQ1060()) return false;
    const auto found = q1060TerrainMaterialByDiffuse.find(diffusePath);
    return found != q1060TerrainMaterialByDiffuse.end() && found->second.specularEnabled;
}
