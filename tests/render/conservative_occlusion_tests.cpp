#include "rendering/conservative-occlusion.h"
#include <cassert>
#include <iostream>
#include <limits>
#include <random>
using namespace fo3occ;
static const float identity[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
int main() {
    const Point wall[]{{-.9f,-.9f,-.5f},{.9f,-.9f,-.5f},{.9f,.9f,-.5f},{-.9f,.9f,-.5f}};
    Polygon polygon;
    assert(ProjectPolygon(identity,wall,4,false,false,polygon));
    Rect behind;
    assert(ProjectBounds(identity,{-.1f,.1f,-.1f,.1f,.4f,.5f},behind));
    assert(polygon.Covers(behind));
    Rect inFront;
    assert(ProjectBounds(identity,{-.1f,.1f,-.1f,.1f,-.8f,-.7f},inFront));
    assert(!polygon.Covers(inFront));
    Rect crossing=behind;crossing.right=.901f;assert(!polygon.Covers(crossing));
    Rect touching=behind;touching.nearZ=polygon.farZ;assert(!polygon.Covers(touching));
    assert(!ProjectBounds(identity,{0,1,0,1,0,.5f},behind)); // Viewport edge fails open.
    assert(!ProjectBounds(identity,{-.1f,.1f,-.1f,.1f,-1,-.9f},behind)); // Near clip.
    auto malformed=questrender::Bounds{0,0,0,0,0,0};
    malformed.maxX=std::numeric_limits<float>::quiet_NaN();
    assert(!ProjectBounds(identity,malformed,behind));
    Point reversed[]{wall[3],wall[2],wall[1],wall[0]};
    assert(!ProjectPolygon(identity,reversed,4,false,false,polygon));
    assert(ProjectPolygon(identity,reversed,4,false,true,polygon));
    assert(ProjectPolygon(identity,reversed,4,true,false,polygon));
    Point folded[]{wall[0],wall[2],wall[1],wall[3]};
    assert(!ProjectPolygon(identity,folded,4,true,false,polygon));
    Point concave[]{wall[0],wall[1],{-.5f,-.5f,-.5f},wall[3]};
    assert(!ProjectPolygon(identity,concave,4,true,false,polygon));
    assert(!ProjectPolygon(identity,wall,2,true,false,polygon));
    Point degenerate[]{{0,0,0},{0,0,0},{0,0,0}};
    assert(!ProjectPolygon(identity,degenerate,3,true,false,polygon));
    const Triangle first{{wall[0],wall[1],wall[2]}},second{{wall[0],wall[2],wall[3]}};
    Surface combined;
    assert(Merge(first,second,combined));assert(combined.count==4);
    assert(ProjectPolygon(identity,combined.p,4,false,false,polygon));
    assert(ProjectBounds(identity,{-.1f,.1f,-.1f,.1f,.4f,.5f},behind));
    assert(polygon.Covers(behind)); // Shared triangle diagonal is not a hole.
    const Triangle unrelated{{{3,3,0},{4,3,0},{3,4,0}}};
    assert(!Merge(first,unrelated,combined));
    // A quad which slopes in depth must use the farthest vertex, not its
    // centre/nearest depth. This leaves intersecting objects visible.
    Point slope[]{{-.9f,-.9f,-.5f},{.9f,-.9f,.5f},{.9f,.9f,.5f},{-.9f,.9f,-.5f}};
    assert(ProjectPolygon(identity,slope,4,false,false,polygon));
    assert(ProjectBounds(identity,{-.1f,.1f,-.1f,.1f,0,.1f},behind));
    assert(!polygon.Covers(behind));
    // Stereo: a blocker in only one current eye never suppresses the object.
    float rightEye[16];std::copy(identity,identity+16,rightEye);rightEye[12]=.8f;
    assert(ProjectPolygon(identity,wall,4,false,false,polygon));
    assert(ProjectBounds(identity,{-.1f,.1f,-.1f,.1f,.4f,.5f},behind));
    Rect rightBounds;
    assert(polygon.Covers(behind));
    assert(!ProjectBounds(rightEye,{-.1f,.1f,-.1f,.1f,.4f,.5f},rightBounds));
    // Exact coverage is not inferred from a triangle's enclosing rectangle.
    const Point triangle[]{{-.9f,-.9f,-.5f},{.9f,-.9f,-.5f},{-.9f,.9f,-.5f}};
    assert(ProjectPolygon(identity,triangle,3,false,false,polygon));
    assert(!polygon.Covers({.4f,.5f,.4f,.5f,.8f}));
    Buffer buffer;buffer.Add(polygon);
    uint32_t tests=0;assert(!buffer.Hidden({.4f,.5f,.4f,.5f,.8f},tests,0));assert(tests==0);
    for(size_t i=0;i<Buffer::Capacity*2;++i)buffer.Add(polygon);
    assert(buffer.count==Buffer::Capacity);
    tests=0;assert(!buffer.Hidden({.4f,.5f,.4f,.5f,.8f},tests,7));assert(tests==7);
    uint8_t pixels[]{0,1,2,255,0,1,2,255};
    assert(OpaquePixels(pixels,sizeof(pixels)));pixels[7]=254;
    assert(!OpaquePixels(pixels,sizeof(pixels)));assert(!OpaquePixels(pixels,0));
    // Randomised coverage oracle: every declared-hidden rectangle must have
    // all of its densely sampled points strictly inside the actual triangle.
    std::mt19937 rng(241);std::uniform_real_distribution<float> random(-.95f,.95f);
    size_t rejected=0;
    for(int trial=0;trial<20000;++trial) {
        Point p[3]{{random(rng),random(rng),-.6f},{random(rng),random(rng),-.6f},{random(rng),random(rng),-.6f}};
        if(!ProjectPolygon(identity,p,3,true,false,polygon))continue;
        const float x=random(rng),y=random(rng);
        Rect r{x,x+.025f,y,y+.025f,.6f};
        if(!polygon.Covers(r))continue;
        ++rejected;
        const auto cross=[](Point a,Point b,float x,float y){return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);};
        const float sign=cross(p[0],p[1],p[2].x,p[2].y)>0?1.f:-1.f;
        for(int ix=0;ix<7;++ix)for(int iy=0;iy<7;++iy)
            for(int edge=0;edge<3;++edge)
                assert(sign*cross(p[edge],p[(edge+1)%3],r.left+(r.right-r.left)*ix/6,r.bottom+(r.top-r.bottom)*iy/6)>0);
    }
    assert(rejected>100);
    std::cout<<"Conservative occlusion: coverage, depth, holes, stereo and bounded work passed\n";
}
