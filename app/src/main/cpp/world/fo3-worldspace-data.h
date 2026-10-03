#pragma once

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

// Shared CPU-only ESM helpers used by placement selection and LAND decoding.
// The immutable placement/door index stays private to worldspace-runtime.cpp.
namespace fo3world_detail {

inline constexpr const char* ESM_PATH_Q75 =
    "/data/user/0/com.falloutquest.app/files/Fallout3/Data/Fallout3.esm";
inline constexpr uint64_t HEADER_SIZE_Q75 = 24u;
inline constexpr float EXTERIOR_CELL_SIZE_Q75 = 4096.0f;
inline constexpr size_t SMALL_WORLDSPACE_CELL_LIMIT_Q75 = 64u;

struct GroupFrameQ75 {
    uint64_t end = 0;
    uint32_t label = 0;
    uint32_t type = 0;
};

struct CellInfoQ75 {
    uint32_t formId = 0;
    int32_t gridX = 0;
    int32_t gridY = 0;
    uint32_t forceHideLandQ720 = 0u;
    bool hasGrid = false;
    std::string editorId;
};

uint16_t ReadLe16Q75(const uint8_t* p);
uint32_t ReadLe32Q75(const uint8_t* p);
float ReadLeFloatQ75(const uint8_t* p);
bool ReadExactQ75(FILE* file, void* dst, size_t size);
int64_t FileSizeQ75(FILE* file);
bool ReadPayloadQ75(FILE* file, uint32_t storedSize, uint32_t flags,
                    std::vector<uint8_t>& out);
void WalkSubrecordsQ75(const uint8_t* data, size_t size,
    const std::function<void(const char*, const uint8_t*, uint32_t)>& visitor);
bool InWorldspaceQ75(const std::vector<GroupFrameQ75>& groups, uint32_t worldspace);
uint32_t OwningSelectedCellQ75(const std::vector<GroupFrameQ75>& groups,
    const std::unordered_set<uint32_t>& selectedCells);
bool DiscoverWorldspaceCellsQ75(uint32_t worldspace, std::vector<CellInfoQ75>& out);

} // namespace fo3world_detail
