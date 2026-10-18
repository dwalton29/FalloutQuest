#pragma once
#include <cstdint>

namespace fo3experience {
// Fallout 3 requires 200 XP for level 2; subsequent level requirements
// increase by iXPBumpBase (150 in the unmodified Fallout3.esm).
constexpr uint32_t FirstLevelRequirement = 200;
constexpr uint32_t VanillaXPBump = 150;
constexpr uint16_t BaseGameLevelCap = 20;

// Cumulative experience required to REACH a level; never wraps uint32_t.
inline uint32_t ThresholdForLevel(uint16_t level, uint32_t bump = VanillaXPBump) {
  if (level <= 1) return 0;
  const uint64_t steps = uint64_t(level) - 1;
  const uint64_t result = steps * FirstLevelRequirement +
      (steps * (steps - 1) / 2) * uint64_t(bump);
  return result > UINT32_MAX ? UINT32_MAX : uint32_t(result);
}

struct Progress {
  uint16_t level = 1, maxLevel = BaseGameLevelCap, pendingLevels = 0;
  uint32_t earned = 0, currentThreshold = 0, nextThreshold = 0;
  bool ready = false;
};

// Level represents committed skill/perk allocation. Pending levels must not be
// silently committed until the original level-up choices can be made.
inline Progress Evaluate(uint16_t committedLevel, uint32_t experience,
                         uint32_t bump = VanillaXPBump,
                         uint16_t maxLevel = BaseGameLevelCap) {
  Progress p;
  p.maxLevel = maxLevel ? maxLevel : 1;
  p.level = committedLevel ? committedLevel : 1;
  if (p.level > p.maxLevel) p.level = p.maxLevel;
  p.earned = experience;
  p.currentThreshold = ThresholdForLevel(p.level, bump);
  p.nextThreshold = ThresholdForLevel(
      p.level == p.maxLevel ? p.level : uint16_t(p.level + 1), bump);
  uint16_t reached = p.level;
  while (reached < p.maxLevel &&
         experience >= ThresholdForLevel(uint16_t(reached + 1), bump)) ++reached;
  p.pendingLevels = uint16_t(reached - p.level);
  p.ready = p.pendingLevels != 0;
  return p;
}
} // namespace fo3experience
