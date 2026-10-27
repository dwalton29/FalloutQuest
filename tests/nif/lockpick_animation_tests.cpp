#include "../../app/src/main/cpp/rendering/mesh/fo3-static-nif.cpp"
#include "ui/lockpick/fo3-lockpick-pose.h"
#include <cassert>
#include <fstream>
#include <iostream>
bool LoadFalloutMeshFile(const std::string& path,std::vector<uint8_t>& bytes,std::string* resolved) {
    std::ifstream f(path,std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(f),{});
    if(resolved)*resolved=path;
    return !bytes.empty();
}
static bool Near(float a,float b){return std::fabs(a-b)<.002f;}
static void Synthetic() {
    fo3anim::Skeleton s;fo3anim::Bone bone;bone.name="root";s.bones={bone};assert(fo3anim::FinalizeSkeleton(s));
    fo3anim::Clip c;c.stop=1;c.cycle=0;fo3anim::Track track;track.bone="root";track.hasRotation=true;track.rotation.interpolation=4;
    for(int axis:{0,2}) {
        auto& channel=track.xyzRotation[axis];channel.dimensions=1;
        fo3anim::Key a,b;a.value[0]=axis==0?1.57079632679f:0.f;b.time=1;b.value[0]=1.57079632679f;
        channel.keys={a,b};
    }
    c.tracks={track};fo3anim::Pose p;
    assert(fo3lockpose::Seek(s,&c,1,p));
    const auto point=fo3anim::Point(p.global[0],{1,0,0},true);
    assert(Near(point[0],0)&&Near(point[1],1)&&Near(point[2],0)); // Rz*Ry*Rx
    const auto end=p.global;assert(fo3lockpose::Seek(s,&c,10,p)&&p.global==end); // LOOP flag cannot rewind menu endpoint
    assert(fo3anim::Sample(s,c,1,p));assert(Near(fo3anim::Point(p.global[0],{1,0,0},true)[0],1)); // ordinary loop unchanged
    for(float degrees:{-90.f,0.f,90.f}) {
        const auto pivot=fo3anim::Point(fo3lockpose::PinPlacement(degrees),{0,0,2.2f});
        assert(Near(pivot[0],0)&&Near(pivot[1],0)&&Near(pivot[2],.6f));
    }
}
static void Original(const char* path,bool pin) {
    std::vector<uint8_t> bytes;LoadFalloutMeshFile(path,bytes,nullptr);
    fo3anim::Skeleton s;std::vector<int> blocks;std::vector<fo3anim::Clip> clips;
    assert(fo3anim::DecodeUiAnimation(bytes,s,blocks,clips));
    const int bone=fo3anim::FindBone(s,pin?"bobbypin":"lock");assert(bone>=0);
    fo3anim::Pose pose;assert(fo3lockpose::Sample(s,clips,pin,10,0,1,-1,pose));
    const auto neutral=pose.global[bone];
    if(pin) {
        const auto up=fo3anim::Point(neutral,{1,0,0},true);assert(up[2]>.99f);
        for(float angle:{-90.f,0.f,90.f}) {
            const auto placed=fo3anim::Multiply(fo3lockpose::PinPlacement(angle),neutral);
            const auto dir=fo3anim::Point(placed,{1,0,0},true);
            if(angle>0)assert(dir[0]>.99f);
            if(angle<0)assert(dir[0]<-.99f);
        }
        for(float health:{.9f,.75f,.5f,.25f,0.f}) {
            assert(fo3lockpose::Sample(s,clips,true,10,0,health,-1,pose));
            const auto dir=fo3anim::Point(pose.global[bone],{1,0,0},true);
            assert(dir[2]>.85f); // original tension keys shake, not a 180deg tumble
        }
        assert(fo3lockpose::Sample(s,clips,true,10,0,0,fo3lockpose::Duration(fo3lockpose::Find(clips,"Left")),pose));
        const int piece=fo3anim::FindBone(s,"Pin2");assert(piece>=0);const auto fallen=pose.global[piece];
        assert(fo3lockpose::Sample(s,clips,true,10,0,0,10,pose));assert(pose.global[piece]==fallen);
        assert(fo3lockpose::Sample(s,clips,true,0,0,1,-1,pose)); // replacement restarts insertion
        assert(std::fabs(pose.global[bone][14]-neutral[14])>10);
    } else {
        for(float cylinder:{0.f,45.f,90.f}) {
            assert(fo3lockpose::Sample(s,clips,false,10,cylinder,1,-1,pose));
            const auto axis=fo3anim::Point(pose.global[bone],{0,1,0},true);
            assert(Near(axis[0],0)&&Near(axis[1],1)&&Near(axis[2],0)); // always face-on
            const auto direction=fo3anim::Point(pose.delta[bone],{1,0,0},true);
            const float a=cylinder*.01745329252f;
            assert(Near(direction[0],std::cos(a))&&Near(direction[2],-std::sin(a))); // clockwise
        }
    }
    std::cout<<path<<": original "<<(pin?"pin direction, pivot, tension, break and replacement":"clockwise cylinder and screwdriver controller")<<" passed\n";
}
int main(int argc,char** argv){Synthetic();if(argc==3){Original(argv[1],false);Original(argv[2],true);}std::cout<<"Lockpick animation tests passed\n";}
