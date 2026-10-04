#include "../../app/src/main/cpp/rendering/quest-render-policy.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace questrender;
    float left[16]{1,0,0,0, 0,1,0,0, 0,0,1,0, 0.2f,0,0,1};
    float right[16]{1,0,0,0, 0,1,0,0, 0,0,1,0, -0.2f,0,0,1};
    Bounds rightOnly{1.05f,1.1f,-0.1f,0.1f,-0.1f,0.1f};
    assert(!Visible(left,rightOnly,0));
    assert(Visible(right,rightOnly,0));
    assert(Visible(left,rightOnly,0) || Visible(right,rightOnly,0));
    Bounds leftOnly{-1.1f,-1.05f,-0.1f,0.1f,-0.1f,0.1f};
    assert(Visible(left,leftOnly,0)); assert(!Visible(right,leftOnly,0));
    Bounds distant{10,11,10,11,10,11};
    assert(!Visible(left,distant)); assert(!Visible(right,distant));
    Bounds crossing{-2,2,-2,2,-2,2}; assert(Visible(left,crossing,0));
    Bounds invalid{2,-2,0,1,0,1}; assert(Visible(left,invalid,0));
    Bounds marginal{1.4f,1.5f,0,0.1f,0,0.1f};
    assert(Visible(left,marginal,1)); assert(!Visible(left,marginal,0.1f));
    assert(!ReuseReflection(1,~uint64_t{0},true,0,0,0));
    assert(ReuseReflection(10,10,true,0,1,0.064f)); // other eye uses same coordinates
    assert(ReuseReflection(11,10,true,0,0.001f,0.005f));
    assert(ReuseReflection(12,10,true,0,0.001f,0.005f));
    assert(!ReuseReflection(13,10,true,0,0,0)); // bounded age, even while stationary
    assert(!ReuseReflection(11,10,false,0,0,0)); // cell/scene/target changed
    assert(!ReuseReflection(11,10,true,0.1f,0,0));
    assert(!ReuseReflection(11,10,true,0,0.003f,0));
    assert(!ReuseReflection(11,10,true,0,0,0.02f));
    assert(!ReuseReflection(9,10,true,0,0,0));
    std::cout << "Stereo frustum and reflection invalidation tests passed\n";
}
