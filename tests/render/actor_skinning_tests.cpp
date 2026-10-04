#include "rendering/actor-skinning.h"
#include "npc/fo3-animation-bounds.h"
#include <cassert>
#include <iostream>
using namespace fqskin;
int main() {
    const std::vector<uint16_t> indices={4,4,65535,0, 1,65535,0,0};
    const std::vector<float> weights={.4f,.2f,.3f,0, 1.2f,0,0,0};
    const auto player=Map(indices,weights,5,true),npc=Map(indices,weights,5,false);
    assert(player.sources.size()==2 && player.sources[0]==4 && player.sources[1]==1);
    assert(player.attributes[2]==0 && npc.attributes[2]==.3f && npc.attributes[6]==0);
    std::vector<PaletteRow> palette(3);
    auto pos=Identity(),dir=Identity();pos[0]=1.5f;pos[12]=2;
    for(auto& row:palette) Pack(row,Identity(),Identity());
    Pack(palette[1],pos,dir);Pack(palette[2],pos,dir);
    const float point[3]={2,3,4};
    auto blended=Blend(player.attributes.data(),palette,true);
    auto p=Transform(blended.data(),point,false);
    assert(std::abs(p[0]-3.8f)<1e-5); // .6*(1.5*2+2) + .4*2
    auto normal=Transform(blended.data()+16,point,true);
    assert(std::abs(normal[0]-2)<1e-5); // directions do not axially stretch
    blended=Blend(npc.attributes.data(),palette,false);
    p=Transform(blended.data(),point,false);
    assert(std::abs(p[0]-4)<1e-5); // .6 transformed + .3 identity, divided by .9
    blended=Blend(player.attributes.data()+8,palette,true);
    p=Transform(blended.data(),point,false);assert(std::abs(p[0]-6)<1e-5); // overweight preserved
    std::vector<uint16_t> bad(4,65535);std::vector<float> zero(4,0);
    auto empty=Map(bad,zero,5,false);assert(empty.sources.empty());
    blended=Blend(empty.attributes.data(),palette,false);assert(blended[0]==1);
    assert(Map({0},{1},1,true).attributes.empty());
    // Verify NPC scene-space palette conjugation against the former game-space path.
    auto placement=Identity();placement[0]=placement[5]=placement[10]=2;placement[12]=3000;placement[13]=-8000;placement[14]=100;
    auto scene=Identity();scene[0]=.01f;scene[5]=scene[10]=0;scene[6]=-.01f;scene[9]=.01f;scene[12]=-20;scene[13]=-1;scene[14]=40;
    Matrix invScene{},invPlacement{};assert(fo3anim::Inverse(scene,invScene));assert(fo3anim::Inverse(placement,invPlacement));
    auto delta=Identity();delta[0]=0;delta[1]=1;delta[4]=-1;delta[5]=0;delta[12]=.5f;
    const auto game=fo3anim::Multiply(placement,fo3anim::Multiply(delta,invPlacement));
    const auto gpu=fo3anim::Multiply(scene,fo3anim::Multiply(game,invScene));
    const std::array<float,3> gamePoint={3004,-7994,109};
    const auto old=fo3anim::Point(scene,fo3anim::Point(game,gamePoint));
    const auto now=fo3anim::Point(gpu,fo3anim::Point(scene,gamePoint));
    for(int i=0;i<3;++i) assert(std::abs(old[i]-now[i])<1e-4);
    const std::array<float,3> tangent={.3f,.4f,.5f};
    const auto oldT=fo3anim::Point(scene,fo3anim::Point(game,tangent,true),true);
    const auto newT=fo3anim::Point(gpu,fo3anim::Point(scene,tangent,true),true);
    for(int i=0;i<3;++i) assert(std::abs(oldT[i]-newT[i])<1e-5);
    // Conservative animation bounds must contain Hermite overshoot, scaling and hierarchy.
    fo3anim::Skeleton skeleton;
    fo3anim::Bone root;root.name="root";root.bind.translation={2,0,0};root.bind.scale=2;
    fo3anim::Bone child;child.name="head";child.parent=0;child.bind.translation={0,3,0};
    skeleton.bones={root,child};assert(fo3anim::FinalizeSkeleton(skeleton));
    fo3anim::Clip clip;clip.start=0;clip.stop=1;clip.cycle=2;
    fo3anim::Track track;track.bone="head";track.hasTranslation=true;
    track.translation.dimensions=3;track.translation.interpolation=2;
    fo3anim::Key a,b;a.time=0;a.value={0,3,0,0};a.forward={20,0,0,0};b.time=1;b.value={0,3,0,0};b.backward={-20,0,0,0};
    track.translation.keys={a,b};clip.tracks={track};
    const auto envelope=fo3anim::ClipEnvelope(skeleton,clip);
    const double radius=fo3anim::DeltaRadius(skeleton,envelope,1,10);
    fo3anim::Pose pose;
    for(int sample=0;sample<=100;++sample) {
        fo3anim::SampleTimings timings;
        assert(fo3anim::Sample(skeleton,clip,sample/100.0,pose,&timings));
        const auto q=fo3anim::Point(pose.delta[1],{3,4,5});
        assert(std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2])<=radius);
        assert(timings.clipUs>=0 && timings.skeletonUs>=0);
    }
    std::cout<<"Palette mapping, weight policy, direction, scene conjugation and animation-envelope tests passed\n";
}
