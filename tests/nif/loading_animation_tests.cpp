#include "../../app/src/main/cpp/rendering/mesh/fo3-static-nif.cpp"
#include "../../app/src/main/cpp/ui/loading/fo3-loading-slideshow.h"
#include "../../app/src/main/cpp/world/interaction/fo3-door-nif-animation.h"
#include <cassert>
#include <fstream>
#include <iostream>
bool LoadFalloutMeshFile(const std::string& path,std::vector<uint8_t>& bytes,std::string* resolved) {
    std::ifstream f(path,std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(f),{});
    if(resolved)*resolved=path;
    return !bytes.empty();
}
int main(int argc,char** argv) {
    using namespace fo3anim;
    Skeleton scene;Bone root;root.name="slide";scene.bones.push_back(root);assert(FinalizeSkeleton(scene));
    std::vector<Clip> clips(3);
    const char* names[]={"Left","Forward","Backward"};
    for(int i=0;i<3;++i) {
        auto& c=clips[i];c.name=names[i];c.stop=i?2:1;c.cycle=2;
        Track track;track.bone="slide";track.hasTranslation=true;track.translation.dimensions=3;
        Key a,b;a.time=0;b.time=c.stop;b.value={2,0,0,0};track.translation.keys={a,b};c.tracks.push_back(track);
        c.textKeys={{0.1f,"sound: originalCue"}};
    }
    fo3slideshow::Player player;assert(player.Configure(scene,clips));player.Reset(0,4);
    assert(player.Advance(scene,clips,0,4));assert(player.sounds.empty());
    assert(player.Advance(scene,clips,0.5,4));assert(player.sounds.size()==1);
    assert(player.Advance(scene,clips,0.5,4));assert(player.sounds.empty()); // two-eye submission cannot duplicate keys
    assert(player.Advance(scene,clips,1,4));assert(player.waiting);
    assert(player.Advance(scene,clips,6.9,4));assert(player.current==player.left);
    assert(player.Advance(scene,clips,7,4));assert(player.current==player.forward && player.slots[1]==1);
    assert(player.Advance(scene,clips,9,4));assert(player.visible==1);
    assert(player.Advance(scene,clips,15,4));assert(player.current==player.backward && player.slots[0]==2);
    assert(player.Advance(scene,clips,17,4));assert(player.visible==0);
    assert(player.Advance(scene,clips,23,4));assert(player.current==player.forward && player.slots[1]==3);
    player.Reset(0,1);assert(player.Advance(scene,clips,1,1));assert(player.Advance(scene,clips,99,1));
    assert(player.current==player.left && player.slots[1]==0);
    player.Reset(0,4);assert(player.Advance(scene,clips,99,4));
    assert(player.sounds.size()==1); // no backlog of cycles on a long frame
    // Q24.1: object animation uses the same decoded hierarchy/clip semantics as
    // the slideshow. A child shape inherits the authored target-node delta, so
    // the hinge is the NIF pivot rather than the placed REFR origin.
    {
        Skeleton door;Bone root;root.name="DoorRoot";door.bones.push_back(root);
        Bone hinge;hinge.name="DoorLeaf";hinge.parent=0;
        hinge.bind.translation={8.00009155f,52.0f,0.07354069f};
        door.bones.push_back(hinge);
        Bone shape;shape.name="DoorShape";shape.parent=1;door.bones.push_back(shape);
        assert(FinalizeSkeleton(door));
        std::vector<int> doorMapping(8,-1);doorMapping[7]=2;
        Clip open;open.name="Open";open.stop=0.5f;open.frequency=1;open.cycle=2;
        Track ot;ot.bone="DoorLeaf";ot.hasRotation=true;ot.rotation.interpolation=4;
        ot.xyzRotation[2].interpolation=1;ot.xyzRotation[2].dimensions=1;
        Key o0,o1;o0.time=0;o0.value[0]=0;o1.time=0.5f;o1.value[0]=-1.658062696f;
        ot.xyzRotation[2].keys={o0,o1};open.tracks.push_back(ot);
        Clip close=open;close.name="Close";close.stop=0.46666664f;
        close.tracks[0].xyzRotation[2].keys[0].value[0]=-1.658062696f;
        close.tracks[0].xyzRotation[2].keys[1].time=0.46666664f;
        close.tracks[0].xyzRotation[2].keys[1].value[0]=0;
        fo3dooranim::Asset asset;
        assert(fo3dooranim::Configure(door,doorMapping,{open,close},asset));
        assert(std::fabs(fo3dooranim::Duration(asset,true)-0.5)<1e-6);
        assert(std::fabs(fo3dooranim::Duration(asset,false)-0.46666664)<1e-6);
        assert(asset.collisionBone==1&&fo3dooranim::ShapeBone(asset,7)==2);
        Pose doorPose;assert(fo3dooranim::Sample(asset,true,1.0,doorPose));
        const auto* delta=fo3dooranim::ShapeDelta(asset,doorPose,2);assert(delta);
        const std::array<float,3> pivot{8.00009155f,52.0f,0.07354069f};
        const auto pivotAfter=Point(*delta,pivot);
        for(int axis=0;axis<3;++axis)assert(std::fabs(pivotAfter[axis]-pivot[axis])<1e-3f);
        const auto pointAfter=Point(*delta,{108.00009155f,52.0f,0.07354069f});
        assert(std::fabs(pointAfter[0]-108.00009155f)>50.0f);
        assert(std::fabs(pointAfter[1]-52.0f)>50.0f);
    }

    std::vector<int> mapping;std::vector<Clip> decoded;Skeleton hierarchy;
    assert(!DecodeUiAnimation(std::vector<uint8_t>(64),hierarchy,mapping,decoded));
    if(argc>1) {
        std::vector<uint8_t> bytes;assert(LoadFalloutMeshFile(argv[1],bytes,nullptr));
        assert(DecodeUiAnimation(bytes,hierarchy,mapping,decoded));
        assert(decoded.size()==3);assert(player.Configure(hierarchy,decoded));
        std::vector<Fo3StaticNifMesh> meshes;assert(LoadFo3StaticNifMeshes(argv[1],meshes));
        assert(meshes.size()==8);
        for(const auto& m:meshes)assert(m.shapeBlock<mapping.size() && mapping[m.shapeBlock]>=0);
        for(const auto& c:decoded) {
            assert(c.tracks.size()==6 && c.textKeys.size()==3 && c.ignoredControllers==0);
            Pose pose;BindClip(hierarchy,c,pose);
            for(int bone:pose.trackBones)assert(bone>=0);
            for(int frame=0;frame<360;++frame)assert(Sample(hierarchy,c,frame/72.0,pose));
        }
        const int slide=FindBone(hierarchy,"Slide01");assert(slide>=0);
        Pose initial,end;assert(Sample(hierarchy,decoded[player.left],0,initial));
        assert(Sample(hierarchy,decoded[player.left],10,end));
        assert(std::fabs(initial.global[slide][13]-end.global[slide][13])>700);
        assert(std::fabs(end.global[slide][13])<1);
        player.Reset(0,4);
        for(int frame=0;frame<3600;++frame)assert(player.Advance(hierarchy,decoded,frame/72.0,4));
        for(size_t n:{size_t(0),size_t(40),bytes.size()/2,bytes.size()-8}) {
            auto truncated=bytes;truncated.resize(n);assert(!DecodeUiAnimation(truncated,hierarchy,mapping,decoded));
            assert(hierarchy.bones.empty() && mapping.empty() && decoded.empty());
        }
        std::cout<<"Original NIF: 3 sequences, 18 bound tracks, 8 textured layers, 3600 slideshow frames\n";
    }
    std::cout<<"Loading animation tests passed\n";
}
