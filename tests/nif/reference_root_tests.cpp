#include <cassert>
#include <fstream>
#include <iostream>
#include "../../app/src/main/cpp/rendering/mesh/fo3-static-nif.cpp"
#include "audio/fo3-footsteps.h"
void TestReferenceDoorCollision(const std::string& path);
bool LoadFalloutMeshFile(const std::string& path,std::vector<uint8_t>& bytes,std::string* resolved) {
    std::ifstream f(path,std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(f),{});
    if(resolved)*resolved=path;
    return !bytes.empty();
}
static bool near(float a,float b){return std::fabs(a-b)<.001f;}
int main(int argc,char** argv) {
    Fo3StaticNifMesh mesh;mesh.positions={1,2,3};mesh.normals={0,1,0};
    mesh.tangents={1,0,0};mesh.bitangents={0,0,1};mesh.texcoords={.2f,.7f};
    NifTransform shape,child,root;shape.valid=child.valid=root.valid=true;
    child.translation[0]=7;root.translation[0]=100;root.scale=2;
    const float rotation[9]={1,0,0,0,0,-1,0,1,0};std::copy(rotation,rotation+9,root.rotation);
    auto standalone=mesh,reference=mesh;
    ApplyTransforms(standalone,shape,{child,root},nullptr);
    ApplyTransforms(reference,shape,{child,root},nullptr,true);
    assert(near(standalone.positions[0],116)&&near(standalone.positions[1],-6)&&near(standalone.positions[2],4));
    assert(reference.positions==std::vector<float>({8,2,3}));
    assert(reference.normals==mesh.normals&&reference.tangents==mesh.tangents);
    assert(reference.geometryDeltaToModel[4]==1&&standalone.geometryDeltaToModel[7]==2);
    assert(reference.texcoords==mesh.texcoords);
    auto fallback=mesh;ApplyTransforms(fallback,shape,{},&root,true);assert(fallback.positions==mesh.positions);
    // Optional original RanchDoor integration: its raw Y axis must become Z
    // after the Small Ranch REFR rotation, not acquire a second root X+90deg.
    if(argc>1) {
        std::vector<Fo3StaticNifMesh> ordinary,placed;
        assert(LoadFo3StaticNifMeshes(argv[1],ordinary,true));
        assert(LoadFo3StaticNifMeshes(argv[1],placed,true,true));
        assert(placed.size()==ordinary.size());
        float lo=1e9,hi=-1e9;
        for(size_t m=0;m<placed.size();++m) {
            assert(placed[m].texcoords==ordinary[m].texcoords);
            assert(placed[m].diffuseTexturePath==ordinary[m].diffuseTexturePath);
            for(size_t i=1;i<placed[m].positions.size();i+=3){lo=std::min(lo,placed[m].positions[i]);hi=std::max(hi,placed[m].positions[i]);}
        }
        TestReferenceDoorCollision(argv[1]);
        assert(hi-lo>150.f); // REFR 0009DF53 maps raw Y to game +Z.
        std::cout<<"Original Small Ranch door: upright height="<<hi-lo<<" UVs/material unchanged\n";
    }
    for(int i=2;i<argc;++i) {
        std::vector<uint8_t> b;LoadFalloutMeshFile(argv[i],b,nullptr);fo3anim::Clip clip;
        assert(fo3anim::DecodeClip(b,clip));fo3audio::FootstepGait gait;assert(gait.Load(clip));
        std::cout<<clip.name<<" stride="<<gait.stride<<" cycle="<<gait.duration<<" contacts="<<gait.contacts[0]<<","<<gait.contacts[1]<<"\n";
    }
}
