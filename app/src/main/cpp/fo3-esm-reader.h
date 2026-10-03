#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace fo3esm {

constexpr uint32_t FLAG_COMPRESSED = 0x00040000u;
constexpr uint64_t HEADER_SIZE = 24u;
constexpr uint32_t DEFAULT_MAX_RECORD_BYTES = 64u * 1024u * 1024u;

struct RecordLocation {
    uint64_t payloadOffset = 0u;
    uint32_t storedSize = 0u;
    uint32_t flags = 0u;
    std::string type;
};

uint16_t ReadU16(const uint8_t* p);
uint32_t ReadU32(const uint8_t* p);
float ReadF32(const uint8_t* p);

bool ReadExact(FILE* file, void* dst, size_t size);
int64_t FileSize(FILE* file);

std::string FourCC(const uint8_t* p);
std::string ZString(const uint8_t* p, uint32_t size);

bool InflateRecord(const std::vector<uint8_t>& stored,
                   std::vector<uint8_t>& out,
                   uint32_t maxBytes = DEFAULT_MAX_RECORD_BYTES);

bool ReadPayload(FILE* file,
                 const RecordLocation& location,
                 std::vector<uint8_t>& out,
                 uint32_t maxBytes = DEFAULT_MAX_RECORD_BYTES);

bool ReadPayloadCurrent(FILE* file,
                        uint32_t storedSize,
                        uint32_t flags,
                        std::vector<uint8_t>& out,
                        uint32_t maxBytes = DEFAULT_MAX_RECORD_BYTES);

using SubrecordVisitor =
    std::function<void(const char*, const uint8_t*, uint32_t)>;

void WalkSubrecords(const uint8_t* data,
                    size_t size,
                    const SubrecordVisitor& visitor);

inline void WalkSubrecords(const std::vector<uint8_t>& data,
                           const SubrecordVisitor& visitor) {
    WalkSubrecords(data.data(), data.size(), visitor);
}

} // namespace fo3esm
