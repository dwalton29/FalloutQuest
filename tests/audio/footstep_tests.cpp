#include "audio/fo3-footsteps.h"
#include "audio/fo3-audio-catalog.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace fo3audio;
static FootstepGait Gait(float duration,float units,float left,float right) {
    fo3anim::Clip clip;clip.stop=duration;clip.accumulationRoot="root";
    clip.textKeys={{left,"m:L"},{right,"m:R"}};
    fo3anim::Track root;root.bone="root";fo3anim::Key a,b;b.time=duration;b.value[1]=units;
    root.translation.keys={a,b};clip.tracks={root};FootstepGait gait;assert(gait.Load(clip));return gait;
}
int main(int argc,char** argv) {
    auto walk=Gait(1.2f,102.308f,.4f,1.033333f);
    auto run=Gait(.8f,283.106f,.333332f,.733332f);
    FootstepClock clock;
    // Original cadence at original root-motion speed, including wraparound.
    for(int cycle=0;cycle<3;++cycle) {
        int left=0,right=0;
        for(int frame=0;frame<120;++frame) {
            const int step=clock.Advance(walk.stride/120.f,.01f,true,walk,run);
            left+=step==0;right+=step==1;
        }
        assert(left==1&&right==1);
    }
    clock.Reset();int l=0,r=0;
    for(int i=0;i<80;++i){int step=clock.Advance(run.stride/80.f,.01f,true,walk,run);l+=step==3;r+=step==2;}
    assert(l==1&&r==1);
    // Crossing the walk/run threshold must preserve the foot-contact interval.
    clock.Reset();int previous=-1;
    for(int frame=0;frame<240;++frame) {
        const bool fast=frame>=42&&frame<140;
        const int action=clock.Advance(fast?run.stride/80.f:walk.stride/120.f,.01f,true,walk,run);
        if(action<0)continue;
        const int foot=(action==0||action==3)?0:1;
        assert(foot!=previous);previous=foot;
    }

    assert(clock.Advance(0,.01f,true,walk,run)==-1&&clock.phase==0); // wall / stationary
    assert(clock.Advance(.1f,.01f,false,walk,run)==-1&&clock.phase==0); // focus/menu
    assert(clock.Advance(5,.01f,true,walk,run)==-1&&clock.phase==0); // teleport
    assert(clock.Advance(.1f,1,true,walk,run)==-1&&clock.phase==0); // no catch-up bursts
    assert(clock.Advance(std::numeric_limits<float>::quiet_NaN(),.01f,true,walk,run)==-1);
    fo3anim::Clip empty;FootstepGait missing;assert(!missing.Load(empty));
    assert(clock.Advance(.1f,.01f,true,missing,run)==-1);
    Catalog c;
    auto sound=[&](const char* name,uint32_t id){c.names[name]=id;c.sounds[id]={};};
    sound("FSTConcSolidWalkR",1);sound("FSTDirtWalkL",2);sound("FSTMetalSolidRunL",3);
    sound("FSTWoodStairsL",4);sound("FSTMetalHollowRunL",5);
    assert(c.Footstep(2,0)==2);assert(c.Footstep(18,0)==2); // Sand -> Dirt
    assert(c.Footstep(4,0)==2); // Grass -> Dirt
    assert(c.Footstep(5,3)==3);assert(c.Footstep(16,3)==5);
    assert(c.Footstep(17,3)==5); // Sheet -> Hollow
    assert(c.Footstep(9|64,0)==4);assert(c.Footstep(9|32,1)==1);
    assert(c.Footstep(8,1)==1);assert(c.Footstep(31,8)==1);assert(c.Footstep(0,99)==0);
    if(argc>1) {
        Catalog original;std::string error;assert(LoadCatalog(argv[1],original,error));
        for(unsigned material=0;material<32;++material)for(unsigned action=0;action<8;++action) {
            const auto id=original.Footstep(material,action);assert(id&&original.sounds.count(id));
        }
        const auto dirt=original.sounds.at(original.Footstep(2,0)).path;
        const auto metal=original.sounds.at(original.Footstep(5,3)).path;
        assert(dirt.find("dirt")!=std::string::npos&&metal.find("metal")!=std::string::npos);
        std::cout<<"Original ESM resolves every material/action combination\n";
    }
    std::cout<<"Original contact cadence, movement gates and material fallback passed\n";
}
