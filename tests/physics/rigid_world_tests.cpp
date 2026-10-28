#include "physics/fo3-rigid-world.h"
#include "physics/fo3-land-surface.h"
#include "physics/fo3-contact-limit.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace fo3rigid;
static Asset Box(float x,float y,float z){Asset a;a.hulls.resize(1);for(float px:{-x,x})for(float py:{-y,y})for(float pz:{-z,z})a.hulls[0].push_back({px,py,pz});return a;}
static M At(float x,float y,float z){auto m=Identity();m[12]=x;m[13]=y;m[14]=z;return m;}
int main(){
    // LAND's p00-p11 diagonal differs from a bilinear patch at its centre.
    assert(fo3land::Height(0,0,0,2,.5f,.5f)==1);
    assert(std::abs(fo3land::Height(0,0,0,2,.75f,.25f)-.5f)<1e-6f);
    assert(std::abs(fo3land::Height(0,0,0,2,.25f,.75f)-.5f)<1e-6f);
    assert(fo3contact::Limit([](float f){return f>.4f&&f<.6f;})>.39f);
    assert(fo3contact::Limit([](float f){return f>.4f&&f<.6f;})<=.4f);
    assert(fo3contact::Limit([](float){return false;})==1);
    assert(fo3contact::Limit([](float){return true;})==0);
    World w;w.Static(100,{{{-10,0,-10},{10,0,10},{10,0,-10}},{{-10,0,-10},{-10,0,10},{10,0,10}}});
    auto box=Box(.1f,.1f,.1f);box.material.restitution=0;
    assert(w.Add(1,box,At(0,2,0)));M p;V v;
    w.Step(1.f/60);assert(w.Pose(1,p,&v)&&v[1]<0&&p[13]<2);
    for(int i=0;i<240;++i)w.Step(1.f/120);
    w.Pose(1,p,&v);assert(p[13]>.09f&&p[13]<.13f&&std::abs(v[1])<.03f);
    // Dynamic contacts must support stacking, not pass through other objects.
    assert(w.Add(2,box,At(0,1,0)));
    for(int i=0;i<240;++i)w.Step(1.f/120);
    w.Pose(2,p);assert(p[13]>.28f&&p[13]<.34f);
    // Actual surface anchor: a thin plate held by its rim retains its centre.
    auto plate=Box(.2f,.01f,.2f);assert(w.Add(3,plate,At(1,1,0)));
    V surface,normal;float distance;
    assert(w.Closest(3,{1.205f,1,0},surface,normal,distance));
    assert(distance<.02f&&distance>0&&surface[0]>1.19f);
    assert(w.Grab(3,0,At(1.21f,1,0),surface));
    w.Pose(3,p);assert(std::abs(p[12]-1)<1e-6f);
    for(int i=0;i<120;++i){w.Target(0,At(1.21f,1.2f,0));w.Step(1.f/120);}
    w.Pose(3,p);assert(p[13]>1.1f&&p[13]<1.25f);
    assert(!w.Grab(3,1,At(1.21f,1,0),surface)); // One body cannot have two primary owners.
    assert(w.Intersects(3,{p[12],p[13],p[14]},.005f));
    assert(!w.Intersects(3,{p[12],p[13]+.2f,p[14]},.005f));
    V beforeRelease,afterRelease;w.Pose(3,p,&beforeRelease);
    w.Release(0);w.Pose(3,p,&afterRelease);assert(beforeRelease==afterRelease&&!w.Held(0));
    for(int i=0;i<240;++i)w.Step(1.f/120);
    w.Pose(3,p);assert(p[13]>.005f&&p[13]<.04f);
    // Continuous capsule landing: a large downward motion cannot skip a floor.
    V resolved,n;assert(w.SweepCapsule({3,4,0},{3,-4,0},.25f,1.7f,resolved,n));
    assert(resolved[1]>=-.003f&&resolved[1]<.03f&&n[1]>.9f);
    // A held object remains blocked by the same world as a dropped one.
    w.Static(101,{{{.5f,0,-2},{.5f,3,2},{.5f,0,2}},{{.5f,0,-2},{.5f,3,-2},{.5f,3,2}}});
    assert(w.Add(4,box,At(0,1,1)));assert(w.Grab(4,0,At(0,1,1),{0,1,1}));
    for(int i=0;i<90;++i){w.Target(0,At(1,1,1));w.Step(1.f/120);}
    w.Pose(4,p);assert(p[12]<.43f);w.Release(0);
    // Authored full inertia is diagonalized without changing model-space poses.
    auto inertia=box;inertia.material.tensor={.02f,.002f,0,.002f,.03f,0,0,0,.04f};
    auto initial=At(-2,1,0);assert(w.Add(5,inertia,initial));w.Pose(5,p);
    for(size_t i=0;i<16;++i)assert(std::abs(initial[i]-p[i])<1e-5f);
    // Visual contact uses the authored visible surface even when BHK is coarser.
    auto visual=Box(.2f,.2f,.2f);
    visual.surface={{{-.2f,0,-.2f},{.2f,0,.2f},{.2f,0,-.2f}},{{-.2f,0,-.2f},{-.2f,0,.2f},{.2f,0,.2f}}};
    assert(Surface(visual,At(4,1,0),{4,1.03f,0},surface,normal,distance));
    assert(std::abs(surface[1]-1)<1e-5f&&std::abs(distance-.03f)<1e-5f);
    assert(w.Add(6,visual,At(4,1,0)));
    assert(w.Intersects(6,{4,1.002f,0},.004f));
    assert(!w.Intersects(6,{4,1.05f,0},.004f));
    auto malformed=box;malformed.hulls[0][0][0]=std::nanf("");assert(!w.Add(7,malformed,At(0,0,0)));
    w.ClearBodies();assert(!w.Has(1)&&!w.Has(4)&&!w.Held(0));
    std::cout<<"Rigid world: gravity, stable landing/stacking, rim anchors, constrained held contacts, finger distances, capsule sweep and authored inertia passed\n";
}
