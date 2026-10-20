#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>
#include "rendering/opaque-front-to-back.h"
using namespace fqopaqueorder;
struct Draw { int id; SortKey key; };
static Draw P(int id,bool safe,float metres,uint64_t mat) {
    return {id,MakeKey(safe,metres*metres,mat,static_cast<size_t>(id))};
}
int main(){
    const Bounds b{-2.f,2.f,-3.f,3.f,-4.f,4.f};
    assert(NearestBoxDistanceSquared(0,0,0,b)==0.f);
    assert(NearestBoxDistanceSquared(2,3,9,b)==25.f);
    assert(!std::isfinite(NearestBoxDistanceSquared(0,0,0,{2,1,0,1,0,1})));
    assert(!MakeKey(true,std::numeric_limits<float>::infinity(),1,0).safe);
    assert(MakeKey(true,225.f,0,0).bucket==3u);
    // Material batching is allowed only within equal depth bands.
    std::vector<Draw> draws{
        P(0,true,22.f,1),P(1,true,4.f,9),P(2,true,5.f,2),
        P(3,false,0.f,0), // unsafe draw: fixed authoring barrier
        P(4,true,21.f,2),P(5,true,3.f,99),P(6,true,20.f,1),
        P(7,false,0.f,0),P(8,true,2.f,1)};
    SortSafeRuns(draws);
    const int expected[]{2,1,0,3,5,6,4,7,8};
    for(size_t i=0;i<draws.size();++i)assert(draws[i].id==expected[i]);
    const auto unchanged=draws;SortSafeRuns(draws);
    for(size_t i=0;i<draws.size();++i)assert(draws[i].id==unchanged[i].id);
    std::vector<Draw> guarded{P(0,false,0,0),P(1,true,40,0),P(2,true,1,0),P(3,false,0,0)};
    SortSafeRuns(guarded);
    assert(guarded[0].id==0&&guarded[1].id==2&&guarded[2].id==1&&guarded[3].id==3);
    return 0;
}
