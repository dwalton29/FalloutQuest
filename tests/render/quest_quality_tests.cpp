#include "../../app/src/main/cpp/rendering/quest-quality-policy.h"
#include <cassert>
#include <limits>
#include <vector>
#include <iostream>
int main() {
    using namespace fqquality;
    static_assert(RenderScale==0.9f);
    assert(!CoarseDesired(32,0,0,0,0));
    assert(CoarseDesired(16,-16,-16,-1,-1));
    assert(CoarseDesired(8,0,0,0,0));
    assert(!CoarseDesired(16,80,0,0,0));
    assert(!CoarseDesired(8,48,0,0,0));
    float n=100000,f=200000;
    Fog(0x3C,false,true,n,f);
    assert(n==FogNearUnits && f==FogFarUnits && n<f);
    n=500;f=5000;Fog(0x3C,false,true,n,f);
    assert(n==500 && f==5000); // Preserve dense authored weather.
    n=100000;f=200000;Fog(0xA74,false,true,n,f);
    assert(n==100000 && f==200000); // Megaton unchanged.
    Fog(0x3C,true,true,n,f);assert(n==100000 && f==200000);
    Fog(0x3C,false,false,n,f);assert(n==100000 && f==200000);
    n=0;f=std::numeric_limits<float>::quiet_NaN();
    Fog(0x3C,false,true,n,f);assert(n==FogNearUnits && f==FogFarUnits);
    // Exhaustive raster coverage: each pixel exactly once, never outside.
    // Include tiny/odd viewports and a non-zero viewport origin.
    for(int w : {1,2,13,101,1512})
    for(int h : {1,3,17,103}) {
        Rect v{7,11,w,h};
        std::vector<unsigned> pixels(w*h);
        for(unsigned i=0;i<5;++i) {
            auto r=Region(i,v);
            assert(r.x>=v.x && r.y>=v.y && r.x+r.w<=v.x+w && r.y+r.h<=v.y+h);
            for(int y=r.y;y<r.y+r.h;++y)
            for(int x=r.x;x<r.x+r.w;++x) ++pixels[(y-v.y)*w+x-v.x];
        }
        for(auto count:pixels)assert(count==1);
        Rect clip{v.x+w/3,v.y+h/3,w/2,h/2};
        std::fill(pixels.begin(),pixels.end(),0);
        for(unsigned i=0;i<5;++i) {
            auto r=Intersect(Region(i,v),clip);
            for(int y=r.y;y<r.y+r.h;++y)
            for(int x=r.x;x<r.x+r.w;++x) ++pixels[(y-v.y)*w+x-v.x];
        }
        for(int y=0;y<h;++y)
        for(int x=0;x<w;++x) {
            const bool inside=x+v.x>=clip.x && x+v.x<clip.x+clip.w &&
                              y+v.y>=clip.y && y+v.y<clip.y+clip.h;
            assert(pixels[y*w+x]==unsigned(inside));
        }
    }
    std::cout<<"Quest quality: fog isolation, LOD selection and foveation coverage passed\n";
}
