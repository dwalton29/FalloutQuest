#pragma once
// Transitional resource/command boundary. CPU asset decoders and gameplay do
// not depend on Vulkan headers. Disabled bridge is a no-op for normal GLES.
#include <cstddef>
#include <cstdint>
#include "data/fo3-texture-bsa.h"
namespace fqvk {
struct Hooks {
    void (*uniform)(uint32_t,int,int,int,const void*)=nullptr;
    void (*draw)(uint32_t,uint32_t,uint32_t,int,int,uint32_t,size_t,int)=nullptr;
    void (*texture)(uint32_t,int,int,const uint8_t*,unsigned)=nullptr;
    void (*mesh)(uint32_t,const float*,size_t,size_t,const uint32_t*,size_t)=nullptr;
    void (*program)(uint32_t,int)=nullptr;
    void (*skin)(uint32_t,uint32_t,const float*,size_t,const float*,size_t)=nullptr;
    void (*dynamicMesh)(uint32_t,const float*,size_t)=nullptr;
    void (*cube)(uint32_t,const Fo3RgbaCubeTexture&)=nullptr;
    void (*erase)(uint32_t,bool)=nullptr;
};
inline Hooks hooks;
inline bool recording=false;
inline const float* instanceMatrices=nullptr;
inline size_t instanceMatrixCount=0;
inline void Texture(uint32_t id,int w,int h,const uint8_t* rgba,bool repeat=true,bool mips=true) {
    if(hooks.texture)hooks.texture(id,w,h,rgba,(repeat?1u:0u)|(mips?2u:0u));
}
inline void Mesh(uint32_t vao,const float* vertices,size_t count,size_t stride,
                 const uint32_t* indices,size_t indexCount) {
    if(hooks.mesh)hooks.mesh(vao,vertices,count,stride,indices,indexCount);
}
inline void Skin(uint32_t vao,uint32_t texture,const float* attributes,size_t count,const float* palette,size_t rows) {
    if(hooks.skin)hooks.skin(vao,texture,attributes,count,palette,rows);
}
inline void DynamicMesh(uint32_t vao,const float* vertices,size_t count) {
    if(hooks.dynamicMesh)hooks.dynamicMesh(vao,vertices,count);
}
inline void Cube(uint32_t id,const Fo3RgbaCubeTexture& image) {
    if(hooks.cube)hooks.cube(id,image);
}
inline void Program(uint32_t program,int terrain) {
    if(hooks.program)hooks.program(program,terrain);
}
}
