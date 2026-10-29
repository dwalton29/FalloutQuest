#include "physics/fo3-rigid-world.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
using namespace fo3rigid;
static Asset Box(float x,float y,float z,float mass){
    Asset a;a.material.mass=mass;
    for(float px:{-x,x})for(float py:{-y,y})for(float pz:{-z,z}){
        if(a.hulls.empty())a.hulls.resize(1);
        a.hulls[0].push_back({px,py,pz});
    }return a;
}
static M Pose(float x,float y,float angle=0){auto m=Identity();m[0]=m[10]=std::cos(angle);m[8]=std::sin(angle);m[2]=-m[8];m[12]=x;m[13]=y;return m;}
static void Floor(World& w){w.Static(100,{{{-10,0,-10},{10,0,10},{10,0,-10}},{{-10,0,-10},{-10,0,10},{10,0,10}}});}
int main(){
    constexpr float dt=1.f/120;
    // Identical tracking motion must not make authored heavier props feel glued
    // to the hand. Free-space following and stopped settling are measured apart.
    for(float mass:{.2f,1.f,10.f,30.f}){
        World w;assert(w.Add(1,Box(.1f,.1f,.1f,mass),Pose(0,1)));
        assert(w.Grab(1,0,Pose(0,1),{0,1,0}));M p;float lag=0;
        for(int i=1;i<=120;++i){const float x=i*dt;w.Target(0,Pose(x,1));w.Step(dt);w.Pose(1,p);if(i>30)lag=std::max(lag,std::abs(x-p[12]));}
        float overshoot=0;
        for(int i=0;i<60;++i){w.Target(0,Pose(1,1));w.Step(dt);w.Pose(1,p);overshoot=std::max(overshoot,p[12]-1);}
        V velocity;w.Pose(1,p,&velocity);
        std::cout<<"mass="<<mass<<" trackingLag="<<lag<<" settledError="<<std::abs(p[12]-1)<<" speed="<<std::abs(velocity[0])<<std::endl;
        assert(lag<.025f&&overshoot<.01f&&std::abs(p[12]-1)<.002f&&std::abs(velocity[0])<.01f);
    }
    // Tracking is sampled at common headset rates, not at the solver's rate.
    for(int hz:{72,90,120}){
        World w;assert(w.Add(1,Box(.1f,.1f,.1f,10),Pose(0,1)));assert(w.Grab(1,0,Pose(0,1),{0,1,0}));
        M p;for(int frame=1;frame<=hz;++frame){w.Target(0,Pose(float(frame)/hz,1));w.Step(1.f/hz);}
        w.Pose(1,p);assert(std::abs(p[12]-1)<.03f);
    }
    // Actual player run speed is 4.84 m/s, above the 4 m/s hand correction
    // cap. The locomotion frame carries a held object; it must not chase the
    // moving player, nor transport unrelated world props.
    for(int hz:{30,36,72,90}){
        World w;auto gun=Box(.35f,.05f,.04f,6);
        assert(w.Add(1,gun,Pose(0,1))&&w.Add(2,gun,Pose(0,3)));
        assert(w.Grab(1,1,Pose(-.2f,1),{-.2f,1,0}));M p,previousApp=Identity();
        const float frameDt=1.f/hz;float worst=0;
        for(int frame=1;frame<=hz*2;++frame){const float x=4.84f*frame*frameDt;auto app=Pose(x,0);
            w.MoveHeld(previousApp,app,frameDt);previousApp=app;
            w.TargetBody(1,Pose(x,1));w.Step(frameDt);assert(w.Pose(1,p));
            worst=std::max(worst,std::abs(p[12]-x));
        }
        std::cout<<"run hz="<<hz<<" carriedLag="<<worst<<std::endl;assert(worst<.025f);
        assert(w.Pose(2,p)&&std::abs(p[12])<.001f);
        w.Release(1);V velocity;assert(w.Pose(1,p,&velocity));assert(std::abs(velocity[0]-4.84f)<.05f);
        const float released=p[12];w.MoveHeld(previousApp,Pose(20,0),frameDt);w.Pose(1,p);assert(p[12]==released);
    }
    // Both held bodies share the locomotion frame, so neither should collide
    // with the other's old position while their common transform is applied.
    {
        World w;auto box=Box(.1f,.1f,.1f,1);
        assert(w.Add(1,box,Pose(0,1))&&w.Add(2,box,Pose(.5f,1)));
        assert(w.Grab(1,0,Pose(0,1),{0,1,0})&&w.Grab(2,1,Pose(.5f,1),{.5f,1,0}));
        w.MoveHeld(Identity(),Pose(1,0),1.f/36);M p;
        assert(w.Pose(1,p)&&std::abs(p[12]-1)<.001f);
        assert(w.Pose(2,p)&&std::abs(p[12]-1.5f)<.001f);
    }
    // Snap-turn around the player transports the grip and orientation once.
    {
        World w;assert(w.Add(1,Box(.1f,.1f,.1f,2),Pose(1,1)));
        assert(w.Grab(1,0,Pose(1,1),{1,1,0}));
        w.MoveHeld(Identity(),Pose(0,0,.5235987756f),1.f/36);M p;
        assert(w.Pose(1,p)&&std::abs(p[12]-.8660254f)<.001f&&std::abs(p[14]+.5f)<.001f);
        M palm;assert(w.GripPalm(0,palm)&&std::abs(palm[12]-p[12])<.001f);
    }
    // Carry still respects a wall. A high-speed locomotion step cannot move a
    // held body through it, while unobstructed tangential travel can continue.
    {
        World w;w.Static(100,{{{.5f,0,-10},{.5f,3,10},{.5f,0,10}},{{.5f,0,-10},{.5f,3,-10},{.5f,3,10}}});
        assert(w.Add(1,Box(.1f,.1f,.1f,2),Pose(0,1)));assert(w.Grab(1,0,Pose(0,1),{0,1,0}));
        auto end=Pose(1,0);end[14]=.5f;w.MoveHeld(Identity(),end,1.f/36);M p;
        assert(w.Pose(1,p));std::cout<<"carry wall="<<p[12]<<","<<p[14]<<std::endl;
        assert(p[12]<.41f&&p[12]>.35f&&p[14]>.45f);
    }
    // A discontinuous tracking target has finite correction speed/effort.
    {
        World w;assert(w.Add(1,Box(.1f,.1f,.1f,10),Pose(0,1)));assert(w.Grab(1,0,Pose(0,1),{0,1,0}));
        M p;V velocity;
        for(int i=0;i<180;++i){w.Target(0,Pose(5,1));w.Step(dt);w.Pose(1,p,&velocity);assert(std::abs(velocity[0])<4.1f&&p[12]<5.01f);}
        assert(p[12]>4.9f);
    }
    // Long gun with an off-centre grip: rapid angular changes must settle without
    // a pendulum swing or rotating the muzzle wildly through the requested aim.
    {
        World w;auto gun=Box(.45f,.06f,.04f,6);assert(w.Add(1,gun,Pose(0,1)));
        assert(w.Grab(1,1,Pose(-.3f,1),{-.3f,1,0}));M p;float worst=0;
        for(int i=0;i<180;++i){float angle=i<60?.5f*std::sin(i*dt*8):.25f;w.TargetBody(1,Pose(0,1,angle));w.Step(dt);w.Pose(1,p);if(i>=90)worst=std::max(worst,std::abs(std::atan2(p[8],p[0])-angle));}
        std::cout<<"gun settledAngularError="<<worst<<std::endl;assert(worst<.01f);
    }
    // Start touching the floor, then translate while yawing. Tangential contact
    // must not make the conservative rotation sweep freeze the entire body.
    {
        World w;Floor(w);auto box=Box(.15f,.05f,.15f,2);assert(w.Add(1,box,Pose(0,.06f)));
        for(int i=0;i<240;++i)w.Step(dt);
        M p;w.Pose(1,p);float y=p[13];assert(w.Grab(1,0,Pose(0,y),{0,y,0}));
        for(int i=1;i<=120;++i){w.TargetBody(1,Pose(i*dt*.4f,y,i*dt*.5f));w.Step(dt);}
        w.Pose(1,p);std::cout<<"surfaceSlide="<<p[12]<<std::endl;assert(p[12]>.36f&&p[13]>.045f);
        // Gently lifting and tilting a plate from rest should need no pull threshold.
        w.Release(0);w.ClearBodies();auto plate=Box(.2f,.01f,.2f,1);assert(w.Add(2,plate,Pose(0,.02f)));
        for(int i=0;i<240;++i)w.Step(dt);
        w.Pose(2,p);y=p[13];assert(w.Grab(2,0,Pose(.2f,y),{.2f,y,0}));
        for(int i=1;i<=60;++i){auto t=Pose(0,y+i*dt*.2f);const float a=i*dt*.2f;t[0]=t[5]=std::cos(a);t[1]=std::sin(a);t[4]=-t[1];w.TargetBody(2,t);w.Step(dt);}
        w.Pose(2,p);std::cout<<"gentleLift="<<p[13]-y<<std::endl;assert(p[13]-y>.075f);
    }
    // Tiny resting tilt/position differences must not change tangential motion
    // into a latch. The discrete solver resolves sub-contact-slop corrections.
    for(int seed=0;seed<12;++seed){
        World w;Floor(w);auto start=Pose(.02f*seed,.06f);
        const float a=(seed-6)*.0002f;start[5]=start[10]=std::cos(a);start[6]=std::sin(a);start[9]=-start[6];
        assert(w.Add(1,Box(.15f,.05f,.15f,2),start));
        for(int i=0;i<240;++i)w.Step(dt);
        M p;w.Pose(1,p);const float x=p[12],y=p[13];assert(w.Grab(1,0,Pose(x,y),{x,y,0}));
        for(int i=1;i<=120;++i){w.TargetBody(1,Pose(x+i*dt*.4f,y,i*dt*.5f));w.Step(dt);}
        w.Pose(1,p);std::cout<<"seed="<<seed<<" x="<<p[12]-x<<" y="<<p[13]<<std::endl;assert(p[12]-x>.36f&&p[13]>.045f);
    }
    // Sliding around one obstruction must still stop at a second wall.
    {
        World w;Floor(w);
        w.Static(101,{{{.5f,0,-2},{.5f,3,2},{.5f,0,2}},{{.5f,0,-2},{.5f,3,-2},{.5f,3,2}}});
        w.Static(102,{{{-2,0,.5f},{2,0,.5f},{2,3,.5f}},{{-2,0,.5f},{2,3,.5f},{-2,3,.5f}}});
        assert(w.Add(1,Box(.1f,.05f,.1f,2),Pose(0,.06f)));
        for(int i=0;i<240;++i)w.Step(dt);
        M p;w.Pose(1,p);const float y=p[13];assert(w.Grab(1,0,Pose(0,y),{0,y,0}));
        for(int i=1;i<=120;++i){auto t=Pose(i*dt*.8f,y,.15f);t[14]=i*dt*.8f;w.TargetBody(1,t);w.Step(dt);}
        w.Pose(1,p);std::cout<<"corner="<<p[12]<<","<<p[13]<<","<<p[14]<<std::endl;assert(p[12]>.25f&&p[14]>.25f&&p[12]<.43f&&p[14]<.43f&&p[13]>.045f);
    }
    // Fast rotation still cannot sweep a thin plate through the ground.
    {
        World w;Floor(w);auto plate=Box(.3f,.01f,.3f,1);assert(w.Add(1,plate,Pose(0,.08f)));
        w.Place(1,Pose(0,.08f),{0,-2,0},{20,0,0});M p;
        for(int i=0;i<240;++i){w.Step(dt);w.Pose(1,p);assert(p[13]>-.002f);}
    }
}
