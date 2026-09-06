#include <android/log.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <zlib.h>

namespace {

constexpr const char* TAG = "FalloutQuest";
constexpr const char* ESM_PATH = "/data/user/0/com.falloutquest.app/files/Fallout3/Data/Fallout3.esm";
constexpr uint32_t TARGET_CELL_FORM_ID = 0x000151E3u;
constexpr const char* TARGET_CELL_EDID = "MegatonPlayerHouse";
constexpr uint32_t FLAG_COMPRESSED = 0x00040000u;
constexpr uint64_t HEADER_SIZE = 24;
constexpr uint32_t MAX_TARGET_RECORD_BYTES = 64u * 1024u * 1024u;

uint16_t ReadLe16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           static_cast<uint16_t>(p[1] << 8);
}

uint32_t ReadLe32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
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

bool InflateRecord(const std::vector<uint8_t>& stored, std::vector<uint8_t>& out) {
    if (stored.size() < 4) return false;
    const uint32_t inflatedSize = ReadLe32(stored.data());
    if (inflatedSize == 0 || inflatedSize > MAX_TARGET_RECORD_BYTES) {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "Q5B compressed CELL claims invalid inflated size %u", inflatedSize);
        return false;
    }

    out.resize(inflatedSize);
    uLongf destLen = static_cast<uLongf>(out.size());
    const Bytef* compressed = reinterpret_cast<const Bytef*>(stored.data() + 4);
    const uLong compressedLen = static_cast<uLong>(stored.size() - 4);
    const int result = uncompress(reinterpret_cast<Bytef*>(out.data()), &destLen,
                                  compressed, compressedLen);
    if (result != Z_OK || destLen != inflatedSize) {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "Q5B zlib inflate failed: result=%d expected=%u actual=%lu",
                            result, inflatedSize, static_cast<unsigned long>(destLen));
        out.clear();
        return false;
    }
    return true;
}

std::string ReadEditorId(const uint8_t* data, size_t size) {
    size_t pos = 0;
    uint32_t extendedSize = 0;

    while (pos + 6 <= size) {
        const char* type = reinterpret_cast<const char*>(data + pos);
        const uint16_t size16 = ReadLe16(data + pos + 4);
        pos += 6;

        if (std::memcmp(type, "XXXX", 4) == 0) {
            if (size16 != 4 || pos + 4 > size) return {};
            extendedSize = ReadLe32(data + pos);
            pos += 4;
            continue;
        }

        const uint32_t subSize = extendedSize ? extendedSize : size16;
        extendedSize = 0;
        if (subSize > size - pos) return {};

        if (std::memcmp(type, "EDID", 4) == 0) {
            size_t len = 0;
            while (len < subSize && data[pos + len] != 0) ++len;
            return std::string(reinterpret_cast<const char*>(data + pos), len);
        }

        pos += subSize;
    }
    return {};
}

void ScanForMegatonPlayerHouse() {
    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "Q5B ESM SCAN START: seeking CELL %08X / %s",
                        TARGET_CELL_FORM_ID, TARGET_CELL_EDID);

    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "Q5B cannot open %s", ESM_PATH);
        return;
    }

    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "Q5B invalid ESM size: %lld", static_cast<long long>(fileSize));
        std::fclose(file);
        return;
    }

    uint64_t records = 0;
    uint64_t groups = 0;
    uint64_t cells = 0;
    bool found = false;

    while (true) {
        const off_t recordOffset = ftello(file);
        if (recordOffset < 0 || static_cast<int64_t>(recordOffset) + static_cast<int64_t>(HEADER_SIZE) > fileSize) {
            break;
        }

        uint8_t header[HEADER_SIZE]{};
        if (!ReadExact(file, header, sizeof(header))) break;

        const uint32_t sizeField = ReadLe32(header + 4);

        if (std::memcmp(header, "GRUP", 4) == 0) {
            ++groups;
            if (sizeField < HEADER_SIZE ||
                static_cast<uint64_t>(recordOffset) + sizeField > static_cast<uint64_t>(fileSize)) {
                __android_log_print(ANDROID_LOG_ERROR, TAG,
                                    "Q5B malformed GRUP at 0x%llX size=%u",
                                    static_cast<unsigned long long>(recordOffset), sizeField);
                break;
            }
            // GRUP size includes this 24-byte header. Deliberately do not
            // seek to its end: Bethesda groups contain the records we want,
            // so the next iteration enters the group body.
            continue;
        }

        ++records;
        const uint32_t flags = ReadLe32(header + 8);
        const uint32_t formId = ReadLe32(header + 12);
        const uint64_t payloadOffset = static_cast<uint64_t>(recordOffset) + HEADER_SIZE;
        const uint64_t payloadEnd = payloadOffset + sizeField;

        if (payloadEnd > static_cast<uint64_t>(fileSize)) {
            char type[5]{header[0], header[1], header[2], header[3], 0};
            __android_log_print(ANDROID_LOG_ERROR, TAG,
                                "Q5B malformed %s record at 0x%llX size=%u",
                                type, static_cast<unsigned long long>(recordOffset), sizeField);
            break;
        }

        const bool isCell = std::memcmp(header, "CELL", 4) == 0;
        if (isCell) ++cells;

        if (isCell && formId == TARGET_CELL_FORM_ID) {
            if (sizeField == 0 || sizeField > MAX_TARGET_RECORD_BYTES) {
                __android_log_print(ANDROID_LOG_ERROR, TAG,
                                    "Q5B target CELL has invalid stored size %u", sizeField);
                break;
            }

            std::vector<uint8_t> stored(sizeField);
            if (!ReadExact(file, stored.data(), stored.size())) {
                __android_log_print(ANDROID_LOG_ERROR, TAG,
                                    "Q5B failed reading target CELL payload");
                break;
            }

            const uint8_t* payload = stored.data();
            size_t payloadSize = stored.size();
            std::vector<uint8_t> inflated;
            if ((flags & FLAG_COMPRESSED) != 0) {
                if (!InflateRecord(stored, inflated)) break;
                payload = inflated.data();
                payloadSize = inflated.size();
            }

            const std::string edid = ReadEditorId(payload, payloadSize);
            __android_log_print(ANDROID_LOG_INFO, TAG,
                                "Q5B target CELL record hit at 0x%llX: stored=%u flags=%08X EDID=%s",
                                static_cast<unsigned long long>(recordOffset), sizeField, flags,
                                edid.empty() ? "<missing>" : edid.c_str());

            if (edid == TARGET_CELL_EDID) {
                __android_log_print(ANDROID_LOG_INFO, TAG,
                                    "Q5B MEGATON CELL FOUND: FormID %08X EDID %s",
                                    formId, edid.c_str());
                found = true;
            } else {
                __android_log_print(ANDROID_LOG_ERROR, TAG,
                                    "Q5B FormID matched but EDID did not: expected=%s actual=%s",
                                    TARGET_CELL_EDID, edid.empty() ? "<missing>" : edid.c_str());
            }
            break;
        }

        if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) {
            __android_log_print(ANDROID_LOG_ERROR, TAG,
                                "Q5B seek failed after record at 0x%llX",
                                static_cast<unsigned long long>(recordOffset));
            break;
        }
    }

    __android_log_print(found ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, TAG,
                        "Q5B ESM SCAN %s: records=%llu groups=%llu cells=%llu",
                        found ? "SUCCESS" : "FAILED",
                        static_cast<unsigned long long>(records),
                        static_cast<unsigned long long>(groups),
                        static_cast<unsigned long long>(cells));
    std::fclose(file);
}

__attribute__((constructor)) void Fallout3EsmScanConstructor() {
    ScanForMegatonPlayerHouse();
}

} // namespace
