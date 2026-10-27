#pragma once
// Compatibility frontend for existing scene preparation. In native mode these
// entry points update CPU state/handles only. No GL shader, texture, buffer or
// framebuffer is created. Decoded assets and DrawCommand feed the native backend.
// GLES fallback calls the real driver with unchanged arguments.
#include "render-submission.h"
#include <algorithm>
#include <cstring>
#include <utility>
namespace fqfrontend {
struct State {
    std::unordered_map<unsigned,bool> capabilities;
    unsigned readFramebuffer=0,drawFramebuffer=0,renderbuffer=0;
    std::array<int,4> viewport{0,0,0,0},scissor{0,0,0,0};
    std::array<float,4> clear{0,0,0,0};
    std::array<unsigned char,4> colourMask{1,1,1,1};
};
inline State state;
inline bool Native(){return fqrender::nativeOnly;}
inline void Reset(){state={};}
inline void NativeIntegers(unsigned name,int* out) {
    // Native device limits come from the actual Vulkan physical device.
    switch(name) {
        case 0x0ba2:std::copy(state.viewport.begin(),state.viewport.end(),out);return;
        case 0x0c10:std::copy(state.scissor.begin(),state.scissor.end(),out);return;
        case 0x8caa:*out=int(state.readFramebuffer);return;
        case 0x8ca6:*out=int(state.drawFramebuffer);return;
        case 0x8ca7:*out=int(state.renderbuffer);return;
        case 0x0d33:*out=fqrender::capabilities.textureSize;return;
        case 0x8869:*out=fqrender::capabilities.vertexAttributes;return;
        case 0x8b4c:*out=fqrender::capabilities.vertexTextures;return;
        case 0x8d57:*out=1;return; // Actual native path is currently single-sample.
        default:*out=0;return;
    }
}
inline void NativeFloats(unsigned name,float* out){if(name==0x0c22)std::copy(state.clear.begin(),state.clear.end(),out);else *out=0;}
inline void NativeBooleans(unsigned name,unsigned char* out){if(name==0x0c23)std::copy(state.colourMask.begin(),state.colourMask.end(),out);else *out=0;}
template<class... T> inline void AttachShader(T... args) {if(!Native()){++fqrender::driverCalls;glAttachShader(args...);}}
template<class... T> inline void BufferData(T... args) {if(!Native()){++fqrender::driverCalls;glBufferData(args...);}}
template<class... T> inline void BufferSubData(T... args) {if(!Native()){++fqrender::driverCalls;glBufferSubData(args...);}}
template<class... T> inline void CompileShader(T... args) {if(!Native()){++fqrender::driverCalls;glCompileShader(args...);}}
template<class... T> inline void DeleteFramebuffers(T... args) {if(!Native()){++fqrender::driverCalls;glDeleteFramebuffers(args...);}}
template<class... T> inline void DeleteRenderbuffers(T... args) {if(!Native()){++fqrender::driverCalls;glDeleteRenderbuffers(args...);}}
template<class... T> inline void DeleteShader(T... args) {if(!Native()){++fqrender::driverCalls;glDeleteShader(args...);}}
template<class... T> inline void DeleteSync(T... args) {if(!Native()){++fqrender::driverCalls;glDeleteSync(args...);}}
template<class... T> inline void DisableVertexAttribArray(T... args) {if(!Native()){++fqrender::driverCalls;glDisableVertexAttribArray(args...);}}
template<class... T> inline void DrawBuffers(T... args) {if(!Native()){++fqrender::driverCalls;glDrawBuffers(args...);}}
template<class... T> inline void EnableVertexAttribArray(T... args) {if(!Native()){++fqrender::driverCalls;glEnableVertexAttribArray(args...);}}
template<class... T> inline void Flush(T... args) {if(!Native()){++fqrender::driverCalls;glFlush(args...);}}
template<class... T> inline void FramebufferRenderbuffer(T... args) {if(!Native()){++fqrender::driverCalls;glFramebufferRenderbuffer(args...);}}
template<class... T> inline void FramebufferTexture2D(T... args) {if(!Native()){++fqrender::driverCalls;glFramebufferTexture2D(args...);}}
template<class... T> inline void GenerateMipmap(T... args) {if(!Native()){++fqrender::driverCalls;glGenerateMipmap(args...);}}
template<class... T> inline void InvalidateFramebuffer(T... args) {if(!Native()){++fqrender::driverCalls;glInvalidateFramebuffer(args...);}}
template<class... T> inline void LineWidth(T... args) {if(!Native()){++fqrender::driverCalls;glLineWidth(args...);}}
template<class... T> inline void LinkProgram(T... args) {if(!Native()){++fqrender::driverCalls;glLinkProgram(args...);}}
template<class... T> inline void ReadBuffer(T... args) {if(!Native()){++fqrender::driverCalls;glReadBuffer(args...);}}
template<class... T> inline void RenderbufferStorage(T... args) {if(!Native()){++fqrender::driverCalls;glRenderbufferStorage(args...);}}
template<class... T> inline void RenderbufferStorageMultisample(T... args) {if(!Native()){++fqrender::driverCalls;glRenderbufferStorageMultisample(args...);}}
template<class... T> inline void ShaderSource(T... args) {if(!Native()){++fqrender::driverCalls;glShaderSource(args...);}}
template<class... T> inline void TexImage2D(T... args) {if(!Native()){++fqrender::driverCalls;glTexImage2D(args...);}}
template<class... T> inline void TexParameterf(T... args) {if(!Native()){++fqrender::driverCalls;glTexParameterf(args...);}}
template<class... T> inline void TexParameteri(T... args) {if(!Native()){++fqrender::driverCalls;glTexParameteri(args...);}}
template<class... T> inline void TexStorage2D(T... args) {if(!Native()){++fqrender::driverCalls;glTexStorage2D(args...);}}
template<class... T> inline void TexSubImage2D(T... args) {if(!Native()){++fqrender::driverCalls;glTexSubImage2D(args...);}}
template<class... T> inline void VertexAttribDivisor(T... args) {if(!Native()){++fqrender::driverCalls;glVertexAttribDivisor(args...);}}
template<class... T> inline void VertexAttribPointer(T... args) {if(!Native()){++fqrender::driverCalls;glVertexAttribPointer(args...);}}
template<class... T> inline void BlitFramebuffer(T... args) {if(!Native()){++fqrender::driverCalls;glBlitFramebuffer(args...);}}
template<class... T> inline void Clear(T... args) {if(!Native()){++fqrender::driverCalls;glClear(args...);}}
template<class... T> inline void ClearDepthf(T... args) {if(!Native()){++fqrender::driverCalls;glClearDepthf(args...);}}
inline void GenBuffers(int n,unsigned* out) {if(Native()){for(int i=0;i<n;++i)out[i]=fqrender::Allocate();}else{++fqrender::driverCalls;glGenBuffers(n,out);}}
inline void GenFramebuffers(int n,unsigned* out) {if(Native()){for(int i=0;i<n;++i)out[i]=fqrender::Allocate();}else{++fqrender::driverCalls;glGenFramebuffers(n,out);}}
inline void GenRenderbuffers(int n,unsigned* out) {if(Native()){for(int i=0;i<n;++i)out[i]=fqrender::Allocate();}else{++fqrender::driverCalls;glGenRenderbuffers(n,out);}}
inline void GenTextures(int n,unsigned* out) {if(Native()){for(int i=0;i<n;++i)out[i]=fqrender::Allocate();}else{++fqrender::driverCalls;glGenTextures(n,out);}}
inline void GenVertexArrays(int n,unsigned* out) {if(Native()){for(int i=0;i<n;++i)out[i]=fqrender::Allocate();}else{++fqrender::driverCalls;glGenVertexArrays(n,out);}}
inline unsigned CreateShader(unsigned type){if(Native())return fqrender::Allocate();++fqrender::driverCalls;return glCreateShader(type);}
inline unsigned CreateProgram(){if(Native())return fqrender::Allocate();++fqrender::driverCalls;return glCreateProgram();}
inline int GetUniformLocation(unsigned id,const char* name){if(Native())return fqrender::UniformLocation(id,name);++fqrender::driverCalls;return glGetUniformLocation(id,name);}
inline void GetShaderiv(unsigned id,unsigned name,int* out){if(Native()){*out=name==0x8b81?1:0;return;}++fqrender::driverCalls;glGetShaderiv(id,name,out);}
inline void GetProgramiv(unsigned id,unsigned name,int* out){if(Native()){*out=name==0x8b82?1:0;return;}++fqrender::driverCalls;glGetProgramiv(id,name,out);}
inline void GetShaderInfoLog(unsigned id,int size,int* n,char* out){if(Native()){if(n)*n=0;if(size&&out)*out=0;return;}++fqrender::driverCalls;glGetShaderInfoLog(id,size,n,out);}
inline void GetProgramInfoLog(unsigned id,int size,int* n,char* out){if(Native()){if(n)*n=0;if(size&&out)*out=0;return;}++fqrender::driverCalls;glGetProgramInfoLog(id,size,n,out);}
inline const unsigned char* GetString(unsigned name){if(Native())return reinterpret_cast<const unsigned char*>(name==0x1f03?"":"FalloutQuest native Vulkan CPU frontend");++fqrender::driverCalls;return glGetString(name);}
inline unsigned GetError(){if(Native())return 0;++fqrender::driverCalls;return glGetError();}
inline unsigned CheckFramebufferStatus(unsigned target){if(Native())return 0x8cd5; /* no GL FBO: Vulkan validates its own targets */ ++fqrender::driverCalls;return glCheckFramebufferStatus(target);}
inline void BindFramebuffer(unsigned target,unsigned id){if(Native()){if(target!=0x8ca8)state.drawFramebuffer=id;if(target!=0x8ca9)state.readFramebuffer=id;return;}++fqrender::driverCalls;glBindFramebuffer(target,id);}
inline void BindRenderbuffer(unsigned target,unsigned id){if(Native()){state.renderbuffer=id;return;}++fqrender::driverCalls;glBindRenderbuffer(target,id);}
inline void Viewport(int x,int y,int w,int h){if(Native()){state.viewport={x,y,w,h};return;}++fqrender::driverCalls;glViewport(x,y,w,h);}
inline void Scissor(int x,int y,int w,int h){if(Native()){state.scissor={x,y,w,h};return;}++fqrender::driverCalls;glScissor(x,y,w,h);}
inline void ClearColor(float r,float g,float b,float a){if(Native()){state.clear={r,g,b,a};return;}++fqrender::driverCalls;glClearColor(r,g,b,a);}
inline void ColorMask(unsigned char r,unsigned char g,unsigned char b,unsigned char a){if(Native()){state.colourMask={r,g,b,a};return;}++fqrender::driverCalls;glColorMask(r,g,b,a);}
inline void GetRenderbufferParameteriv(unsigned target,unsigned name,int* out){if(Native()){*out=1;return;}++fqrender::driverCalls;glGetRenderbufferParameteriv(target,name,out);}
inline void GetFramebufferAttachmentParameteriv(unsigned target,unsigned attachment,unsigned name,int* out){if(Native()){*out=1;return;}++fqrender::driverCalls;glGetFramebufferAttachmentParameteriv(target,attachment,name,out);}
// The current native upload path completes before returning to the shared loader.
// Replace this completion token when uploads become asynchronous.
inline GLsync FenceSync(unsigned condition,unsigned flags){if(Native())return reinterpret_cast<GLsync>(uintptr_t(1));++fqrender::driverCalls;return glFenceSync(condition,flags);}
inline unsigned ClientWaitSync(GLsync sync,unsigned flags,uint64_t timeout){if(Native())return 0x911a;++fqrender::driverCalls;return glClientWaitSync(sync,flags,timeout);}
}
#define glAttachShader fqfrontend::AttachShader
#define glBufferData fqfrontend::BufferData
#define glBufferSubData fqfrontend::BufferSubData
#define glCompileShader fqfrontend::CompileShader
#define glDeleteFramebuffers fqfrontend::DeleteFramebuffers
#define glDeleteRenderbuffers fqfrontend::DeleteRenderbuffers
#define glDeleteShader fqfrontend::DeleteShader
#define glDeleteSync fqfrontend::DeleteSync
#define glDisableVertexAttribArray fqfrontend::DisableVertexAttribArray
#define glDrawBuffers fqfrontend::DrawBuffers
#define glEnableVertexAttribArray fqfrontend::EnableVertexAttribArray
#define glFlush fqfrontend::Flush
#define glFramebufferRenderbuffer fqfrontend::FramebufferRenderbuffer
#define glFramebufferTexture2D fqfrontend::FramebufferTexture2D
#define glGenerateMipmap fqfrontend::GenerateMipmap
#define glInvalidateFramebuffer fqfrontend::InvalidateFramebuffer
#define glLineWidth fqfrontend::LineWidth
#define glLinkProgram fqfrontend::LinkProgram
#define glReadBuffer fqfrontend::ReadBuffer
#define glRenderbufferStorage fqfrontend::RenderbufferStorage
#define glRenderbufferStorageMultisample fqfrontend::RenderbufferStorageMultisample
#define glShaderSource fqfrontend::ShaderSource
#define glTexImage2D fqfrontend::TexImage2D
#define glTexParameterf fqfrontend::TexParameterf
#define glTexParameteri fqfrontend::TexParameteri
#define glTexStorage2D fqfrontend::TexStorage2D
#define glTexSubImage2D fqfrontend::TexSubImage2D
#define glVertexAttribDivisor fqfrontend::VertexAttribDivisor
#define glVertexAttribPointer fqfrontend::VertexAttribPointer
#define glBlitFramebuffer fqfrontend::BlitFramebuffer
#define glClear fqfrontend::Clear
#define glClearDepthf fqfrontend::ClearDepthf
#define glGenBuffers fqfrontend::GenBuffers
#define glGenFramebuffers fqfrontend::GenFramebuffers
#define glGenRenderbuffers fqfrontend::GenRenderbuffers
#define glGenTextures fqfrontend::GenTextures
#define glGenVertexArrays fqfrontend::GenVertexArrays
#define glCreateShader fqfrontend::CreateShader
#define glCreateProgram fqfrontend::CreateProgram
#define glGetUniformLocation fqfrontend::GetUniformLocation
#define glGetShaderiv fqfrontend::GetShaderiv
#define glGetProgramiv fqfrontend::GetProgramiv
#define glGetShaderInfoLog fqfrontend::GetShaderInfoLog
#define glGetProgramInfoLog fqfrontend::GetProgramInfoLog
#define glGetString fqfrontend::GetString
#define glGetError fqfrontend::GetError
#define glCheckFramebufferStatus fqfrontend::CheckFramebufferStatus
#define glBindFramebuffer fqfrontend::BindFramebuffer
#define glBindRenderbuffer fqfrontend::BindRenderbuffer
#define glViewport fqfrontend::Viewport
#define glScissor fqfrontend::Scissor
#define glClearColor fqfrontend::ClearColor
#define glColorMask fqfrontend::ColorMask
#define glGetRenderbufferParameteriv fqfrontend::GetRenderbufferParameteriv
#define glGetFramebufferAttachmentParameteriv fqfrontend::GetFramebufferAttachmentParameteriv
#define glFenceSync fqfrontend::FenceSync
#define glClientWaitSync fqfrontend::ClientWaitSync
