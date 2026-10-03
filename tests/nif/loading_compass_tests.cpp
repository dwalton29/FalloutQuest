#include "../../app/src/main/cpp/rendering/mesh/fo3-static-nif.cpp"
#include <cassert>
#include <fstream>
#include <iostream>
bool LoadFalloutMeshFile(const std::string& path,std::vector<uint8_t>& bytes,std::string* resolved) {
    std::ifstream f(path,std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(f),{});
    if(resolved)*resolved=path;
    return !bytes.empty();
}
int main(int argc,char** argv) {
    if(argc<2)return 0; // Original assets remain outside the repository.
    std::vector<uint8_t> bytes;assert(LoadFalloutMeshFile(argv[1],bytes,nullptr));
    fo3anim::Skeleton hierarchy;std::vector<int> blocks;std::vector<fo3anim::Clip> clips;
    assert(fo3anim::DecodeUiAnimation(bytes,hierarchy,blocks,clips));
    assert(clips.size()==1 && clips[0].name=="Idle" && clips[0].tracks.size()==2 && clips[0].cycle==0);
    const int dial=fo3anim::FindBone(hierarchy,"main_timer");
    const int pointer=fo3anim::FindBone(hierarchy,"Pointer:0");
    assert(dial>=0 && pointer>=0);
    fo3anim::Pose start,later,wrapped;
    assert(fo3anim::Sample(hierarchy,clips[0],0,start));
    assert(fo3anim::Sample(hierarchy,clips[0],1,later));
    assert(fo3anim::Sample(hierarchy,clips[0],clips[0].stop+0.000001,wrapped));
    float movement=0;
    for(int i=0;i<16;++i) {
        assert(std::fabs(start.delta[dial][i]-later.delta[dial][i])<0.0001f);
        assert(std::fabs(start.delta[pointer][i]-wrapped.delta[pointer][i])<0.001f);
        movement=std::max(movement,std::fabs(start.delta[pointer][i]-later.delta[pointer][i]));
    }
    assert(movement>0.1f);
    std::cout<<"Original compass: stationary dial, independently animated pointer, authored loop\n";
}
