#include <android/log.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <zlib.h>

namespace {

constexpr const char* TAG = "FalloutQuest";
constexpr const char* ESM_PATH = "/data/user/0/com.falloutquest.app/files/Fallout3/Data/Fallout3.esm";
constexpr uint32_t TARGET_CELL_FORM_ID = 0x000151E3u;
constexpr uint32_t FLAG_COMPRESSED = 0x00040000u;
constexpr uint64_t HEADER_SIZE = 24;
constexpr uint32_t MAX_RECORD_BYTES = 64u * 1024u * 1024u;

struct GroupFrame {
    uint64_t end = 0;
    uint32_t label = 0;
    uint32_t type = 0;
};

struct Placement {
    uint32_t refFormId = 0;
    uint32_t baseFormId = 0;
    std::string recordType;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float rx = 0.0f;
    float ry = 0.0f;
    float rz = 0.0f;
    bool hasTransform = false;
};

struct BaseRecord {
    uint32_t formId = 0;
    std::string recordType;
    std::string edid;
    std::string model;
};

uint16_t ReadLe16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8);
}

uint32_t ReadLe32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

float ReadLeFloat(const uint8_t* p) {
    const uint32_t bits = ReadLe32(p);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool ReadExact(FILE* file, void* dst, size_t size) {
    return std::fread(dst, 1, size, file) == size;
}

int64_t FileSize(FILE* file) {
    const off_t current = ftello(file);
    if (current < 0) return -1;
    if (fseeko(file, 0, SEEK_END) != 0) return -1;
    const off_t end = ftello(file);
    fseeko(file, current, SEEK_SET);
    return static_cast<int64_t>(end);
}

std::string FourCC(const uint8_t* p) {
    char value[5]{static_cast<char>(p[0]), static_cast<char>(p[1]),
                  static_cast<char>(p[2]), static_cast<char>(p[3]), 0};
    return std::string(value);
}

bool InflateRecord(const std::vector<uint8_t>& stored, std::vector<uint8_t>& out) {
    if (stored.size() < 4) return false;
    const uint32_t inflatedSize = ReadLe32(stored.data());
    if (inflatedSize == 0 || inflatedSize > MAX_RECORD_BYTES) return false;

    out.resize(inflatedSize);
    uLongf destLen = static_cast<uLongf>(out.size());
    const int result = uncompress(reinterpret_cast<Bytef*>(out.data()), &destLen,
                                  reinterpret_cast<const Bytef*>(stored.data() + 4),
                                  static_cast<uLong>(stored.size() - 4));
    if (result != Z_OK || destLen != inflatedSize) {
        out.clear();
        return false;
    }
    return true;
}

bool ReadPayload(FILE* file, uint32_t storedSize, uint32_t flags,
                 std::vector<uint8_t>& payload) {
    if (storedSize == 0 || storedSize > MAX_RECORD_BYTES) return false;

    std::vector<uint8_t> stored(storedSize);
    if (!ReadExact(file, stored.data(), stored.size())) return false;

    if ((flags & FLAG_COMPRESSED) == 0) {
        payload.swap(stored);
        return true;
    }

    return InflateRecord(stored, payload);
}

bool IsMegatonChildGroup(uint32_t label, uint32_t type) {
    return label == TARGET_CELL_FORM_ID &&
           (type == 6u || type == 8u || type == 9u || type == 10u);
}

bool IsPlacementType(const uint8_t* type) {
    return std::memcmp(type, "REFR", 4) == 0 ||
           std::memcmp(type, "ACHR", 4) == 0 ||
           std::memcmp(type, "ACRE", 4) == 0 ||
           std::memcmp(type, "PGRE", 4) == 0 ||
           std::memcmp(type, "PMIS", 4) == 0;
}

bool InMegatonChildren(const std::vector<GroupFrame>& groups) {
    for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
        if (IsMegatonChildGroup(it->label, it->type)) return true;
    }
    return false;
}

void WalkSubrecords(const uint8_t* data, size_t size,
                    const std::function<void(const char*, const uint8_t*, uint32_t)>& visitor) {
    size_t pos = 0;
    uint32_t extendedSize = 0;

    while (pos + 6 <= size) {
        const char* type = reinterpret_cast<const char*>(data + pos);
        const uint16_t size16 = ReadLe16(data + pos + 4);
        pos += 6;

        if (std::memcmp(type, "XXXX", 4) == 0) {
            if (size16 != 4 || pos + 4 > size) return;
            extendedSize = ReadLe32(data + pos);
            pos += 4;
            continue;
        }

        const uint32_t subSize = extendedSize ? extendedSize : size16;
        extendedSize = 0;
        if (subSize > size - pos) return;

        visitor(type, data + pos, subSize);
        pos += subSize;
    }
}

bool ParsePlacement(const std::vector<uint8_t>& payload, Placement& out) {
    bool haveBase = false;
    WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "NAME", 4) == 0 && size >= 4) {
            out.baseFormId = ReadLe32(bytes);
            haveBase = out.baseFormId != 0;
        } else if (std::memcmp(type, "DATA", 4) == 0 && size >= 24) {
            out.x = ReadLeFloat(bytes + 0);
            out.y = ReadLeFloat(bytes + 4);
            out.z = ReadLeFloat(bytes + 8);
            out.rx = ReadLeFloat(bytes + 12);
            out.ry = ReadLeFloat(bytes + 16);
            out.rz = ReadLeFloat(bytes + 20);
            out.hasTransform = true;
        }
    });
    return haveBase;
}

BaseRecord ParseBaseRecord(uint32_t formId, const std::string& recordType,
                           const std::vector<uint8_t>& payload) {
    BaseRecord result;
    result.formId = formId;
    result.recordType = recordType;

    WalkSubrecords(payload.data(), payload.size(),
                   [&](const char* type, const uint8_t* bytes, uint32_t size) {
        if (std::memcmp(type, "EDID", 4) == 0 && result.edid.empty()) {
            size_t len = 0;
            while (len < size && bytes[len] != 0) ++len;
            result.edid.assign(reinterpret_cast<const char*>(bytes), len);
        } else if (std::memcmp(type, "MODL", 4) == 0 && result.model.empty()) {
            size_t len = 0;
            while (len < size && bytes[len] != 0) ++len;
            result.model.assign(reinterpret_cast<const char*>(bytes), len);
        }
    });
    return result;
}

bool CollectMegatonPlacements(std::vector<Placement>& placements,
                              uint32_t& childGroupCount) {
    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) return false;
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        return false;
    }

    std::vector<GroupFrame> groups;

    while (true) {
        const off_t offsetRaw = ftello(file);
        if (offsetRaw < 0) break;
        const uint64_t offset = static_cast<uint64_t>(offsetRaw);

        while (!groups.empty() && offset >= groups.back().end) groups.pop_back();
        if (offset + HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[HEADER_SIZE]{};
        if (!ReadExact(file, header, sizeof(header))) break;
        const uint32_t sizeField = ReadLe32(header + 4);

        if (std::memcmp(header, "GRUP", 4) == 0) {
            if (sizeField < HEADER_SIZE || offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            const uint32_t label = ReadLe32(header + 8);
            const uint32_t groupType = ReadLe32(header + 12);
            groups.push_back(GroupFrame{offset + sizeField, label, groupType});
            if (IsMegatonChildGroup(label, groupType)) {
                ++childGroupCount;
                __android_log_print(ANDROID_LOG_INFO, TAG,
                                    "Q5C entered Megaton child GRUP type=%u at 0x%llX size=%u",
                                    groupType, static_cast<unsigned long long>(offset), sizeField);
            }
            continue;
        }

        const uint32_t flags = ReadLe32(header + 8);
        const uint32_t formId = ReadLe32(header + 12);
        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;

        const bool wanted = InMegatonChildren(groups) && IsPlacementType(header);
        if (!wanted) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, flags, payload)) break;

        Placement placement;
        placement.refFormId = formId;
        placement.recordType = FourCC(header);
        if (ParsePlacement(payload, placement)) placements.push_back(std::move(placement));
    }

    std::fclose(file);
    return true;
}

bool ResolveBaseRecords(const std::unordered_set<uint32_t>& wanted,
                        std::unordered_map<uint32_t, BaseRecord>& bases) {
    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) return false;
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        return false;
    }

    while (true) {
        const off_t offsetRaw = ftello(file);
        if (offsetRaw < 0) break;
        const uint64_t offset = static_cast<uint64_t>(offsetRaw);
        if (offset + HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[HEADER_SIZE]{};
        if (!ReadExact(file, header, sizeof(header))) break;
        const uint32_t sizeField = ReadLe32(header + 4);

        if (std::memcmp(header, "GRUP", 4) == 0) {
            if (sizeField < HEADER_SIZE || offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            continue;
        }

        const uint32_t flags = ReadLe32(header + 8);
        const uint32_t formId = ReadLe32(header + 12);
        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;

        if (wanted.find(formId) == wanted.end()) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, flags, payload)) break;
        bases[formId] = ParseBaseRecord(formId, FourCC(header), payload);
        if (bases.size() == wanted.size()) break;
    }

    std::fclose(file);
    return true;
}

void ScanMegatonContents() {
    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "Q5C MEGATON CONTENT SCAN START: CELL %08X", TARGET_CELL_FORM_ID);

    std::vector<Placement> placements;
    uint32_t childGroupCount = 0;
    if (!CollectMegatonPlacements(placements, childGroupCount)) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Q5C failed while collecting Megaton placements");
        return;
    }

    std::unordered_set<uint32_t> wantedBases;
    size_t refrCount = 0;
    size_t actorCount = 0;
    for (const auto& p : placements) {
        wantedBases.insert(p.baseFormId);
        if (p.recordType == "REFR") ++refrCount;
        if (p.recordType == "ACHR" || p.recordType == "ACRE") ++actorCount;
    }

    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "Q5C MEGATON CHILDREN: groups=%u placements=%zu REFR=%zu actors=%zu uniqueBases=%zu",
                        childGroupCount, placements.size(), refrCount, actorCount, wantedBases.size());

    std::unordered_map<uint32_t, BaseRecord> bases;
    if (!ResolveBaseRecords(wantedBases, bases)) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Q5C failed while resolving Megaton base records");
        return;
    }

    size_t withModel = 0;
    for (const auto& entry : bases) {
        if (!entry.second.model.empty()) ++withModel;
    }

    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "Q5C BASE RESOLUTION: resolved=%zu/%zu models=%zu",
                        bases.size(), wantedBases.size(), withModel);

    size_t logged = 0;
    std::unordered_set<uint32_t> loggedBases;
    for (const auto& p : placements) {
        if (logged >= 24) break;
        if (!loggedBases.insert(p.baseFormId).second) continue;
        const auto it = bases.find(p.baseFormId);
        if (it == bases.end() || it->second.model.empty()) continue;
        const BaseRecord& base = it->second;
        __android_log_print(ANDROID_LOG_INFO, TAG,
                            "Q5C MESH[%zu]: base=%08X type=%s EDID=%s MODL=%s",
                            logged, base.formId, base.recordType.c_str(),
                            base.edid.empty() ? "<none>" : base.edid.c_str(),
                            base.model.c_str());
        ++logged;
    }

    if (childGroupCount > 0 && !placements.empty() && withModel > 0) {
        __android_log_print(ANDROID_LOG_INFO, TAG,
                            "Q5C SUCCESS: MegatonPlayerHouse references resolved to real Fallout 3 model paths");
    } else {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "Q5C INCOMPLETE: groups=%u placements=%zu models=%zu",
                            childGroupCount, placements.size(), withModel);
    }
}

__attribute__((constructor)) void Fallout3MegatonContentsConstructor() {
    ScanMegatonContents();
}

} // namespace
