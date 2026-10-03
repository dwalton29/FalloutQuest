#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace fo3loadingpose {
constexpr float PanelDistance = 2.5f;
constexpr float ModelDistance = 2.1f;
constexpr float RotationRadiansPerSecond = 0.20f;
struct Matrix { float m[16]{}; };
inline Matrix Identity() { Matrix a; a.m[0]=a.m[5]=a.m[10]=a.m[15]=1; return a; }
inline Matrix Multiply(const Matrix& a,const Matrix& b) {
    Matrix c;
    for(int j=0;j<4;++j) for(int i=0;i<4;++i)
        for(int k=0;k<4;++k) c.m[j*4+i]+=a.m[k*4+i]*b.m[j*4+k];
    return c;
}
inline Matrix Placement(float x,float y,float z,float yaw,float scale=1) {
    Matrix a=Identity(); float c=std::cos(yaw)*scale,s=std::sin(yaw)*scale;
    a.m[0]=c;a.m[2]=-s;a.m[5]=scale;a.m[8]=s;a.m[10]=c;
    a.m[12]=x;a.m[13]=y;a.m[14]=z;return a;
}
// Capture in tracking space, once per loading generation. Locomotion/door yaw
// changes must never move the loading presentation between the two eyes.
inline Matrix Anchor(float x,float y,float z,float qx,float qy,float qz,float qw) {
    float yaw=std::atan2(2*(qw*qy+qx*qz),1-2*(qy*qy+qx*qx));
    return Placement(x-std::sin(yaw)*PanelDistance,y,z-std::cos(yaw)*PanelDistance,yaw);
}
inline uint64_t Mix(uint64_t x) {
    x+=0x9e3779b97f4a7c15ull;x=(x^(x>>30))*0xbf58476d1ce4e5b9ull;
    x=(x^(x>>27))*0x94d049bb133111ebull;return x^(x>>31);
}
} // namespace fo3loadingpose
