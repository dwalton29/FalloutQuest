#include "rendering/quest-render-policy.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <random>
#include <vector>
using questrender::Bounds;
inline bool ReferenceVisible(const float* m, const Bounds& object, float padding = 1.0f) {
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

int main() {
    std::mt19937 random(223);
    std::uniform_real_distribution<float> coordinate(-100,100), coefficient(-3,3), extent(0,15);
    for(int camera=0;camera<150;++camera) {
        float m[16];for(float& x:m)x=coefficient(random);
        if(camera==0)for(float& x:m)x=0; // degenerate planes retain fail-open behaviour
        const questrender::Frustum frustum(m);
        for(int i=0;i<1500;++i) {
            float x=coordinate(random),y=coordinate(random),z=coordinate(random);
            Bounds b{x,x+extent(random),y,y+extent(random),z,z+extent(random)};
            if(i%100==0)b.maxX=b.minX-1; // invalid bounds
            const float padding=i%2?1.0f:0.1f;
            assert(ReferenceVisible(m,b,padding)==questrender::Visible(frustum,b,padding));
        }
    }
    // Same visible set and draw count, measured host CPU only. Both functions
    // see changing bounds and runtime camera data; neither loop can fold away.
    float m[16];for(float& x:m)x=coefficient(random);
    std::vector<Bounds> bounds(20000);
    for(auto& b:bounds){float x=coordinate(random),y=coordinate(random),z=coordinate(random);b={x,x+3,y,y+3,z,z+3};}
    volatile uint64_t oldCount=0,newCount=0;
    auto start=std::chrono::steady_clock::now();
    for(int pass=0;pass<30;++pass)for(const auto& b:bounds)oldCount+=ReferenceVisible(m,b);
    const auto before=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    start=std::chrono::steady_clock::now();
    for(int pass=0;pass<30;++pass){const questrender::Frustum frustum(m);for(const auto& b:bounds)newCount+=questrender::Visible(frustum,b);}
    const auto after=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    assert(oldCount==newCount);
    std::printf("Frustum: 225000 equivalence checks; 600000 bound tests oldMs=%.3f preparedMs=%.3f visible=%llu (host CPU, not Quest FPS)\n",before,after,(unsigned long long)newCount);
}
