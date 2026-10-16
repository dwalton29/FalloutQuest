#pragma once
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace fo3scene {
// Necessary authored-data guard, not permission to dispatch Enable/Disable.
// Immutable worker output is bound to the complete logical affected set.
struct ResidentAuthoredAdmission {
    uint32_t cell=0;
    std::vector<uint32_t> affected;
    bool Matches(uint32_t expectedCell,const std::vector<uint32_t>& ids) const noexcept;
};
bool PrepareResidentAuthoredAdmission(const std::string& master,uint32_t cell,
    const std::vector<uint32_t>& affected,ResidentAuthoredAdmission& out,
    const std::atomic<bool>& cancelled,std::string& error);
}
