#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
namespace fqvk {
// Fixed lens-centred foveation: full rate centrally, 2x2 at the edges,
// 4x4 only in the extreme corners. Identical maps for the two stereo layers.
inline std::vector<uint8_t> DensityPixels(uint32_t w,uint32_t h) {
    std::vector<uint8_t> pixels(size_t(w)*h*4);
    for(unsigned e=0;e<2;++e)for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x){
        const float nx=2.f*(x+.5f)/w-1,ny=2.f*(y+.5f)/h-1;
        const float radius=std::sqrt(nx*nx+ny*ny);
        const uint8_t density=radius<=.75f?255:radius<=1.2f?128:64;
        const size_t at=(size_t(e)*w*h+y*w+x)*2;pixels[at]=pixels[at+1]=density;
    }
    return pixels;
}
}
