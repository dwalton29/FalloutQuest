#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace fqquality {
constexpr float RenderScale = 0.90f;
constexpr float FogFarUnits = 24.0f * 4096.0f;
constexpr float FogNearUnits = 16.0f * 4096.0f;
// Retain the 16 > 8 > 4 > LAND hierarchy; remove only the coarsest tier.
inline bool CoarseDesired(int level, int32_t x, int32_t y, int32_t px, int32_t py) {
    if (level == 32) return false;
    const int distance = std::max(std::abs(x + level/2 - px),
                                  std::abs(y + level/2 - py));
    return level == 16 ? distance <= 72 : level == 8 && distance <= 44;
}
inline void Fog(uint32_t world, bool interior, bool valid, float& near, float& far) {
    if (world != 0x3Cu || interior || !valid) return;
    // Never make authored dense weather clearer. Keep source records untouched.
    if (!std::isfinite(far) || !std::isfinite(near) || far <= near + 1.0f) {
        near = FogNearUnits; far = FogFarUnits;
        return;
    }
    far = std::min(far, FogFarUnits);
    near = std::max(0.0f, std::min({near, FogNearUnits, far - 1.0f}));
}
struct Rect { int x, y, w, h; };
// Five disjoint rectangles cover the viewport once, including odd dimensions.
// Central 70% of each axis is full rate; the surrounding four bands use 2x2.
inline Rect Region(unsigned index, Rect v) {
    const int mx = (v.w * 15 / 100) & ~1;
    const int my = (v.h * 15 / 100) & ~1;
    switch (index) {
        case 0: return {v.x+mx,v.y+my,v.w-2*mx,v.h-2*my};
        case 1: return {v.x,v.y,v.w,my};
        case 2: return {v.x,v.y+v.h-my,v.w,my};
        case 3: return {v.x,v.y+my,mx,v.h-2*my};
        default:return {v.x+v.w-mx,v.y+my,mx,v.h-2*my};
    }
}
inline Rect Intersect(Rect a, Rect b) {
    const int x=std::max(a.x,b.x), y=std::max(a.y,b.y);
    return {x,y,std::max(0,std::min(a.x+a.w,b.x+b.w)-x),
                std::max(0,std::min(a.y+a.h,b.y+b.h)-y)};
}
}
