#pragma once
#include <cmath>
#include <cstdint>

namespace questrender {
struct Bounds { float minX, maxX, minY, maxY, minZ, maxZ; };
inline bool Visible(const float* m, const Bounds& object, float padding = 1.0f) {
    if (object.minX > object.maxX || object.minY > object.maxY || object.minZ > object.maxZ) return true;
    const float cx = 0.5f * (object.minX + object.maxX);
    const float cy = 0.5f * (object.minY + object.maxY);
    const float cz = 0.5f * (object.minZ + object.maxZ);
    const float ex = 0.5f * (object.maxX - object.minX);
    const float ey = 0.5f * (object.maxY - object.minY);
    const float ez = 0.5f * (object.maxZ - object.minZ);

    const float planes[6][4] = {
        {m[3] + m[0],  m[7] + m[4],  m[11] + m[8],  m[15] + m[12]},
        {m[3] - m[0],  m[7] - m[4],  m[11] - m[8],  m[15] - m[12]},
        {m[3] + m[1],  m[7] + m[5],  m[11] + m[9],  m[15] + m[13]},
        {m[3] - m[1],  m[7] - m[5],  m[11] - m[9],  m[15] - m[13]},
        {m[3] + m[2],  m[7] + m[6],  m[11] + m[10], m[15] + m[14]},
        {m[3] - m[2],  m[7] - m[6],  m[11] - m[10], m[15] - m[14]},
    };

    for (const auto& p : planes) {
        const float len =
            std::sqrt(p[0]*p[0] + p[1]*p[1] + p[2]*p[2]);
        if (len < 1e-6f) continue;
        const float inv = 1.0f / len;
        const float a = p[0] * inv;
        const float b = p[1] * inv;
        const float c = p[2] * inv;
        const float d = p[3] * inv;
        const float distance = a*cx + b*cy + c*cz + d;
        const float radius =
            std::fabs(a)*ex + std::fabs(b)*ey + std::fabs(c)*ez;
        if (distance + radius < -padding) {
            return false;
        }
    }
    return true;
}

inline bool ReuseReflection(uint64_t frame, uint64_t cachedFrame, bool sameScene,
                            float planeDelta, float matrixDelta, float eyeDelta) {
    if (cachedFrame == ~uint64_t{0} || !sameScene || std::fabs(planeDelta) >= 0.001f) return false;
    return frame == cachedFrame || (frame > cachedFrame && frame-cachedFrame < 3u &&
                                    matrixDelta < 0.002f && eyeDelta < 0.01f);
}
} // namespace questrender
