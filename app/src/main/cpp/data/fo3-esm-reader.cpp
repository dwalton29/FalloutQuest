#include "fo3-esm-reader.h"

#include <cstring>
#include <zlib.h>

namespace fo3esm {

uint16_t ReadU16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8u);
}

uint32_t ReadU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8u) |
           (static_cast<uint32_t>(p[2]) << 16u) |
           (static_cast<uint32_t>(p[3]) << 24u);
}

float ReadF32(const uint8_t* p) {
    const uint32_t bits = ReadU32(p);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool ReadExact(FILE* file, void* dst, size_t size) {
    return file && std::fread(dst, 1u, size, file) == size;
}

int64_t FileSize(FILE* file) {
    if (!file) return -1;
    const off_t current = ftello(file);
    if (current < 0) return -1;
    if (fseeko(file, 0, SEEK_END) != 0) return -1;
    const off_t end = ftello(file);
    if (fseeko(file, current, SEEK_SET) != 0) return -1;
    return static_cast<int64_t>(end);
}

std::string FourCC(const uint8_t* p) {
    char value[5]{
        static_cast<char>(p[0]), static_cast<char>(p[1]),
        static_cast<char>(p[2]), static_cast<char>(p[3]), 0};
    return std::string(value);
}

std::string ZString(const uint8_t* p, uint32_t size) {
    size_t len = 0u;
    while (len < size && p[len] != 0u) ++len;
    return std::string(reinterpret_cast<const char*>(p), len);
}

bool InflateRecord(const std::vector<uint8_t>& stored,
                   std::vector<uint8_t>& out,
                   uint32_t maxBytes) {
    if (stored.size() < 4u) return false;
    const uint32_t inflatedSize = ReadU32(stored.data());
    if (inflatedSize == 0u || inflatedSize > maxBytes) return false;

    out.resize(inflatedSize);
    uLongf destLen = static_cast<uLongf>(out.size());
    const int result = uncompress(
        reinterpret_cast<Bytef*>(out.data()), &destLen,
        reinterpret_cast<const Bytef*>(stored.data() + 4u),
        static_cast<uLong>(stored.size() - 4u));
    if (result != Z_OK || destLen != inflatedSize) {
        out.clear();
        return false;
    }
    return true;
}

bool ReadPayload(FILE* file,
                 const RecordLocation& location,
                 std::vector<uint8_t>& out,
                 uint32_t maxBytes) {
    if (!file || location.storedSize == 0u ||
        location.storedSize > maxBytes) {
        return false;
    }
    if (fseeko(file, static_cast<off_t>(location.payloadOffset), SEEK_SET) != 0)
        return false;

    std::vector<uint8_t> stored(location.storedSize);
    if (!ReadExact(file, stored.data(), stored.size())) return false;
    if ((location.flags & FLAG_COMPRESSED) == 0u) {
        out.swap(stored);
        return true;
    }
    return InflateRecord(stored, out, maxBytes);
}

bool ReadPayloadCurrent(FILE* file,
                        uint32_t storedSize,
                        uint32_t flags,
                        std::vector<uint8_t>& out,
                        uint32_t maxBytes) {
    if (!file || storedSize == 0u || storedSize > maxBytes) return false;
    std::vector<uint8_t> stored(storedSize);
    if (!ReadExact(file, stored.data(), stored.size())) return false;
    if ((flags & FLAG_COMPRESSED) == 0u) {
        out.swap(stored);
        return true;
    }
    return InflateRecord(stored, out, maxBytes);
}

void WalkSubrecords(const uint8_t* data,
                    size_t size,
                    const SubrecordVisitor& visitor) {
    if (!data || !visitor) return;
    size_t pos = 0u;
    uint32_t extendedSize = 0u;
    while (pos + 6u <= size) {
        const char* type = reinterpret_cast<const char*>(data + pos);
        const uint16_t size16 = ReadU16(data + pos + 4u);
        pos += 6u;

        if (std::memcmp(type, "XXXX", 4u) == 0) {
            if (size16 != 4u || pos + 4u > size) return;
            extendedSize = ReadU32(data + pos);
            pos += 4u;
            continue;
        }

        const uint32_t subSize = extendedSize ? extendedSize : size16;
        extendedSize = 0u;
        if (subSize > size - pos) return;
        visitor(type, data + pos, subSize);
        pos += subSize;
    }
}

} // namespace fo3esm
