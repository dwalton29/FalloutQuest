#include "world/interaction/fo3-door-activation.h"
#include "physics/fo3-nif-collision.h"
#include "rendering/mesh/fo3-static-nif.h"
#include "npc/fo3-actor-animation.h"
#include <cassert>
#include <fstream>
#include <iostream>

bool LoadFalloutMeshFile(const std::string& path,std::vector<uint8_t>& bytes,std::string* resolved){
    std::ifstream file(path,std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(file),{});
    if(resolved)*resolved=path;
    return !bytes.empty();
}
static fo3interaction::Point Point(const fo3anim::Matrix& m,fo3interaction::Point p,bool direction=false){
    return fo3anim::Point(m,p,direction);
}
static void Synthetic(){
    fo3doorpick::Bounds box;box.valid=true;box.lo={-70,-7,0};box.hi={70,7,210};
    auto placement=fo3anim::Identity();const float angle=.71f,c=std::cos(angle),s=std::sin(angle);
    placement[0]=2*c;placement[2]=-2*s;placement[8]=2*s;placement[10]=2*c;placement[5]=2;
    placement[12]=123;placement[13]=-45;placement[14]=67;
    fo3anim::Matrix inverse;assert(fo3anim::Inverse(placement,inverse));
    const auto origin=Point(placement,{0,-1,1});auto direction=Point(placement,{0,1,0},true);
    for(float& value:direction)value/=2;
    float distance=-1;
    assert(fo3doorpick::Ray(box,inverse.data(),70,origin,direction,3,distance));
    assert(std::fabs(distance-1.8f)<1e-4f);
    assert(!fo3doorpick::Ray(box,inverse.data(),70,origin,direction,1,distance));
    assert(!fo3doorpick::Ray(box,inverse.data(),70,Point(placement,{3,-1,1}),direction,3,distance));
    assert(fo3doorpick::Ray(box,inverse.data(),70,Point(placement,{0,0,1}),direction,3,distance)&&distance==0);
    assert(!fo3doorpick::Ray({},inverse.data(),70,origin,direction,3,distance));
    // Picking a load-door volume does not waive ordinary interaction occlusion.
    const fo3interaction::Point wallA{origin[0]-1,origin[1]+.5f,origin[2]-1};
    const fo3interaction::Point wallB{origin[0]+1,origin[1]+.5f,origin[2]-1};
    const fo3interaction::Point wallC{origin[0],origin[1]+.5f,origin[2]+1};
    assert(fo3interaction::Triangle(origin,direction,wallA,wallB,wallC,1.8f));
    std::cout<<"Authored door volume preserves rotation, scale, range and occlusion passed\n";
}
int main(int argc,char** argv){
    Synthetic();
    if(argc>1){
        std::vector<Fo3StaticNifMesh> meshes;assert(LoadFo3StaticNifMeshes(argv[1],meshes,true,true));
        std::vector<Fo3NifCollisionShapeQ6F> shapes;assert(LoadFo3NifCollisionShapesQ6F(argv[1],shapes));
        const auto visual=fo3doorpick::FromShapes(meshes),activation=fo3doorpick::FromShapes(shapes);
        assert(visual.valid&&activation.valid);
        assert(visual.hi[2]-visual.lo[2]<.01f&&activation.hi[2]-activation.lo[2]>200);
        // Same game-axis-to-scene basis as the placed runtime, with a shifted origin.
        fo3anim::Matrix placement{1,0,0,0, 0,0,-1,0, 0,1,0,0, 123,4,-56,1},inverse;
        assert(fo3anim::Inverse(placement,inverse));
        const auto origin=Point(placement,{0,activation.hi[1]/70+2,1.6f});
        const auto direction=Point(placement,{0,-1,0},true);
        float distance;
        auto padded=visual;for(size_t i=0;i<3;++i){padded.lo[i]-=.08f*70;padded.hi[i]+=.08f*70;}
        assert(!fo3doorpick::Ray(padded,inverse.data(),70,origin,direction,3,distance));
        assert(fo3doorpick::Ray(activation,inverse.data(),70,origin,direction,3,distance));
        assert(std::fabs(distance-2)<1e-4f);
        std::cout<<"Original Megaton entrance: marker misses, authored BHK hits at "<<distance<<" metres\n";
    }
}
