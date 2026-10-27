#pragma once
// Shared CPU rendering contract. No GLES, EGL, Vulkan or OpenXR dependency.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
namespace fqrender {
using Handle=uint32_t;
inline bool nativeOnly=false;
struct Capabilities {int textureSize=0,vertexAttributes=0,vertexTextures=0;};
inline Capabilities capabilities;
inline Handle nextHandle=1;
inline uint64_t driverCalls=0;
inline std::unordered_map<Handle,std::unordered_map<std::string,int>> uniformNames;
inline Handle Allocate(){return nextHandle++;}
inline int UniformLocation(Handle program,const char* name) {
    if(!program||!name)return -1;
    std::string key=name;int element=0;
    const auto bracket=key.find('[');
    if(bracket!=std::string::npos){element=std::stoi(key.substr(bracket+1));key.resize(bracket);}
    auto& names=uniformNames[program];auto found=names.find(key);
    if(found==names.end())found=names.emplace(key,int(names.size()*64)).first;
    return found->second+element;
}
struct DrawState {
    bool depth=true,write=true,blend=false,cull=false,clockwise=false,offset=false;
    uint32_t depthFunc=0x0201,cullFace=0x0405,src=1,dst=0,srcAlpha=1,dstAlpha=0,equation=0x8006,equationAlpha=0x8006;
    float offsetUnits=0,offsetFactor=0;
    std::array<Handle,16> textures{},cubes{};
};
struct DrawCommand {
    Handle program=0,mesh=0;
    uint32_t topology=0,indexType=0;
    int first=0,count=0,instances=1;
    size_t indexOffset=0;
    DrawState state;
    std::vector<float> transforms;
};
using LocationLookup=int(*)(Handle,const char*);
inline LocationLookup locationLookup=nullptr;
}
