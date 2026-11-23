#pragma once
#include <cstddef>
#include <cstdint>

namespace fqspacewarp {
// Snapshot of the first rejected record in a frame, before the history resets.
// Reason strings are static literals so a buffered capture owns no draw pointers.
struct HistoryRejection {
    const char* reason=nullptr;
    uint64_t identity=0;
    uint32_t reference=0,mesh=0;
    std::size_t objects=0,usedBytes=0,recordBytes=0;
};
}
