#pragma once

// One renderer-owned cache shared by all native translation units. GL runs on
// the render thread. Calls bypassing this header must invalidate at the boundary.
#include <GLES3/gl3.h>
#include <array>
#include <cstring>
#include <cstdint>

namespace fqgl {
struct Counters {
    uint64_t state=0, uniforms=0, textures=0, vaos=0, queries=0, draws=0, vertices=0;
};
inline Counters counters;
template<class T> struct Value {
    T value{}; bool known=false;
    bool Change(T next) {
        if (known && value==next) return false;
        value=next; known=true; return true;
    }
};
struct State {
    Value<GLuint> program, vao, arrayBuffer;
    Value<GLenum> active, depthFunc, cullFace, frontFace;
    Value<GLboolean> depthWrite;
    Value<bool> depth, blend, cull, offset, a2c;
    Value<GLfloat> offsetFactor, offsetUnits;
    Value<GLenum> srcRgb, dstRgb, srcAlpha, dstAlpha;
    std::array<Value<GLuint>,16> texture2d{}, textureCube{};
};
inline State state;
struct Uniform {
    std::array<unsigned char,64> bytes{};
    unsigned size=0, kind=0;
};
struct ProgramUniforms {
    GLuint program=0;
    std::array<Uniform,256> values{};
};
inline std::array<ProgramUniforms,24> programs{};
inline void Invalidate() { state=State{}; }
inline void Reset() { Invalidate(); programs={}; counters={}; }
inline Value<bool>* Capability(GLenum cap) {
    switch(cap) {
        case GL_DEPTH_TEST:return &state.depth;
        case GL_BLEND:return &state.blend;
        case GL_CULL_FACE:return &state.cull;
        case GL_POLYGON_OFFSET_FILL:return &state.offset;
        case GL_SAMPLE_ALPHA_TO_COVERAGE:return &state.a2c;
        default:return nullptr;
    }
}
inline void Enable(GLenum cap) {
    auto* v=Capability(cap); if (!v || v->Change(true)) { glEnable(cap); ++counters.state; }
}
inline void Disable(GLenum cap) {
    auto* v=Capability(cap); if (!v || v->Change(false)) { glDisable(cap); ++counters.state; }
}
inline GLboolean IsEnabled(GLenum cap) {
    auto* v=Capability(cap);
    if (v && v->known) return v->value ? GL_TRUE : GL_FALSE;
    ++counters.queries; const auto result=glIsEnabled(cap);
    if (v) v->Change(result==GL_TRUE);
    return result;
}
inline void UseProgram(GLuint id) {
    if(state.program.Change(id)) { glUseProgram(id); ++counters.state; }
}
inline void BindVertexArray(GLuint id) {
    if(state.vao.Change(id)) { glBindVertexArray(id); ++counters.vaos; ++counters.state; }
}
inline void BindBuffer(GLenum target,GLuint id) {
    // Element-array bindings belong to the VAO and deliberately bypass this cache.
    if(target!=GL_ARRAY_BUFFER || state.arrayBuffer.Change(id)) {
        glBindBuffer(target,id); ++counters.state;
    }
}
inline void ActiveTexture(GLenum unit) {
    if(state.active.Change(unit)) { glActiveTexture(unit); ++counters.state; }
}
inline Value<GLuint>* Texture(GLenum target) {
    if(!state.active.known || state.active.value<GL_TEXTURE0 ||
       state.active.value>=GL_TEXTURE0+state.texture2d.size()) return nullptr;
    const auto unit=state.active.value-GL_TEXTURE0;
    if(target==GL_TEXTURE_2D) return &state.texture2d[unit];
    if(target==GL_TEXTURE_CUBE_MAP) return &state.textureCube[unit];
    return nullptr;
}
inline void BindTexture(GLenum target,GLuint id) {
    auto* v=Texture(target);
    if(!v || v->Change(id)) { glBindTexture(target,id); ++counters.textures; ++counters.state; }
}
inline void BindTextureUnit(GLenum unit,GLenum target,GLuint id) {
    const auto index=unit-GL_TEXTURE0;
    if(index<state.texture2d.size()) {
        auto& v=target==GL_TEXTURE_CUBE_MAP ? state.textureCube[index] : state.texture2d[index];
        if(v.known && v.value==id) return;
    }
    ActiveTexture(unit); BindTexture(target,id);
}
inline void DepthMask(GLboolean mask) {
    if(state.depthWrite.Change(mask)) { glDepthMask(mask); ++counters.state; }
}
inline void DepthFunc(GLenum func) {
    if(state.depthFunc.Change(func)) { glDepthFunc(func); ++counters.state; }
}
inline void CullFace(GLenum face) {
    if(state.cullFace.Change(face)) { glCullFace(face); ++counters.state; }
}
inline void FrontFace(GLenum face) {
    if(state.frontFace.Change(face)) { glFrontFace(face); ++counters.state; }
}
inline void PolygonOffset(GLfloat factor,GLfloat units) {
    const bool f=state.offsetFactor.Change(factor), u=state.offsetUnits.Change(units);
    if(f || u) { glPolygonOffset(factor,units); ++counters.state; }
}
inline void BlendFuncSeparate(GLenum sr,GLenum dr,GLenum sa,GLenum da) {
    const bool a=state.srcRgb.Change(sr), b=state.dstRgb.Change(dr);
    const bool c=state.srcAlpha.Change(sa), d=state.dstAlpha.Change(da);
    if(a || b || c || d) { glBlendFuncSeparate(sr,dr,sa,da); ++counters.state; }
}
inline void BlendFunc(GLenum src,GLenum dst) { BlendFuncSeparate(src,dst,src,dst); }
inline void GetIntegerv(GLenum name,GLint* out) {
    Value<GLuint>* id=nullptr; Value<GLenum>* value=nullptr;
    switch(name) {
        case GL_CURRENT_PROGRAM:id=&state.program;break;
        case GL_VERTEX_ARRAY_BINDING:id=&state.vao;break;
        case GL_ARRAY_BUFFER_BINDING:id=&state.arrayBuffer;break;
        case GL_ACTIVE_TEXTURE:value=&state.active;break;
        case GL_DEPTH_FUNC:value=&state.depthFunc;break;
        case GL_FRONT_FACE:value=&state.frontFace;break;
        case GL_CULL_FACE_MODE:value=&state.cullFace;break;
        case GL_BLEND_SRC_RGB:value=&state.srcRgb;break;
        case GL_BLEND_DST_RGB:value=&state.dstRgb;break;
        case GL_BLEND_SRC_ALPHA:value=&state.srcAlpha;break;
        case GL_BLEND_DST_ALPHA:value=&state.dstAlpha;break;
        case GL_TEXTURE_BINDING_2D:id=Texture(GL_TEXTURE_2D);break;
        case GL_TEXTURE_BINDING_CUBE_MAP:id=Texture(GL_TEXTURE_CUBE_MAP);break;
    }
    if(id && id->known) { *out=static_cast<GLint>(id->value);return; }
    if(value && value->known) { *out=static_cast<GLint>(value->value);return; }
    ++counters.queries; glGetIntegerv(name,out);
    if(id) id->Change(static_cast<GLuint>(*out));
    if(value) value->Change(static_cast<GLenum>(*out));
}
inline void GetBooleanv(GLenum name,GLboolean* out) {
    if(name==GL_DEPTH_WRITEMASK && state.depthWrite.known) { *out=state.depthWrite.value;return; }
    ++counters.queries; glGetBooleanv(name,out);
    if(name==GL_DEPTH_WRITEMASK) state.depthWrite.Change(*out);
}
inline void GetFloatv(GLenum name,GLfloat* out) {
    auto* v=name==GL_POLYGON_OFFSET_FACTOR ? &state.offsetFactor :
        name==GL_POLYGON_OFFSET_UNITS ? &state.offsetUnits : nullptr;
    if(v && v->known) { *out=v->value;return; }
    ++counters.queries; glGetFloatv(name,out); if(v) v->Change(*out);
}
inline bool Upload(GLint location,unsigned kind,const void* data,unsigned size) {
    if(location<0) return false;
    if(state.program.known && state.program.value && location<256) {
        ProgramUniforms* bank=nullptr;
        for(auto& p:programs) if(p.program==state.program.value) { bank=&p;break; }
        if(!bank) for(auto& p:programs) if(!p.program) { p.program=state.program.value;bank=&p;break; }
        if(bank) {
            auto& v=bank->values[location];
            if(size>64) { v.size=0; ++counters.uniforms; return true; }
            if(v.size==size && v.kind==kind && std::memcmp(v.bytes.data(),data,size)==0) return false;
            v.size=size;v.kind=kind;std::memcpy(v.bytes.data(),data,size);
        }
    }
    ++counters.uniforms; return true;
}
// Array writes can overlap individually addressed element locations. Keep
// arrays outside the value cache and invalidate that program's scalar history.
inline bool UploadArray(GLint location,unsigned kind,const void* data,unsigned size,GLsizei count) {
    if(count==1) return Upload(location,kind,data,size);
    if(location<0) return false;
    for(auto& bank:programs) if(bank.program==state.program.value) bank.values={};
    ++counters.uniforms;return true;
}
inline void Uniform1f(GLint l,GLfloat a) { if(Upload(l,1,&a,sizeof(a))) glUniform1f(l,a); }
inline void Uniform1i(GLint l,GLint a) { if(Upload(l,2,&a,sizeof(a))) glUniform1i(l,a); }
inline void Uniform2f(GLint l,GLfloat a,GLfloat b) { GLfloat v[]{a,b};if(Upload(l,3,v,sizeof(v))) glUniform2f(l,a,b); }
inline void Uniform3f(GLint l,GLfloat a,GLfloat b,GLfloat c) { GLfloat v[]{a,b,c};if(Upload(l,4,v,sizeof(v))) glUniform3f(l,a,b,c); }
inline void Uniform4f(GLint l,GLfloat a,GLfloat b,GLfloat c,GLfloat d) { GLfloat v[]{a,b,c,d};if(Upload(l,5,v,sizeof(v))) glUniform4f(l,a,b,c,d); }
inline void Uniform1fv(GLint l,GLsizei n,const GLfloat* v) { if(UploadArray(l,1,v,n*sizeof(float),n)) glUniform1fv(l,n,v); }
inline void Uniform2fv(GLint l,GLsizei n,const GLfloat* v) { if(UploadArray(l,3,v,n*2*sizeof(float),n)) glUniform2fv(l,n,v); }
inline void Uniform3fv(GLint l,GLsizei n,const GLfloat* v) { if(UploadArray(l,4,v,n*3*sizeof(float),n)) glUniform3fv(l,n,v); }
inline void Uniform4fv(GLint l,GLsizei n,const GLfloat* v) { if(UploadArray(l,5,v,n*4*sizeof(float),n)) glUniform4fv(l,n,v); }
inline void UniformMatrix4fv(GLint l,GLsizei n,GLboolean t,const GLfloat* v) { if(UploadArray(l,6+t,v,n*16*sizeof(float),n)) glUniformMatrix4fv(l,n,t,v); }
inline void DrawArrays(GLenum m,GLint first,GLsizei n) { ++counters.draws;counters.vertices+=n;glDrawArrays(m,first,n); }
inline void DrawArraysInstanced(GLenum m,GLint first,GLsizei n,GLsizei count) { ++counters.draws;counters.vertices+=uint64_t(n)*count;glDrawArraysInstanced(m,first,n,count); }
inline void DeleteProgram(GLuint id) {
    for(auto& p:programs) if(p.program==id) p=ProgramUniforms{};
    glDeleteProgram(id); // A current deleted program remains current until unbound.
}
inline void DeleteTextures(GLsizei n,const GLuint* ids) {
    glDeleteTextures(n,ids);
    for(GLsizei i=0;i<n;++i) {
        for(auto& v:state.texture2d) if(v.known && v.value==ids[i]) v.Change(0);
        for(auto& v:state.textureCube) if(v.known && v.value==ids[i]) v.Change(0);
    }
}
inline void DeleteBuffers(GLsizei n,const GLuint* ids) {
    glDeleteBuffers(n,ids);
    for(GLsizei i=0;i<n;++i) if(state.arrayBuffer.known && state.arrayBuffer.value==ids[i]) state.arrayBuffer.Change(0);
}
inline void DeleteVertexArrays(GLsizei n,const GLuint* ids) {
    glDeleteVertexArrays(n,ids);
    for(GLsizei i=0;i<n;++i) if(state.vao.known && state.vao.value==ids[i]) state.vao.Change(0);
}
} // namespace fqgl

// Route existing renderers through the same ownership model, including terrain,
// water and UI in other translation units. The real GLES declarations precede
// these aliases, and implementations above always call the real entry points.
#define glEnable fqgl::Enable
#define glDisable fqgl::Disable
#define glIsEnabled fqgl::IsEnabled
#define glUseProgram fqgl::UseProgram
#define glBindVertexArray fqgl::BindVertexArray
#define glBindBuffer fqgl::BindBuffer
#define glActiveTexture fqgl::ActiveTexture
#define glBindTexture fqgl::BindTexture
#define glDepthMask fqgl::DepthMask
#define glDepthFunc fqgl::DepthFunc
#define glCullFace fqgl::CullFace
#define glFrontFace fqgl::FrontFace
#define glPolygonOffset fqgl::PolygonOffset
#define glBlendFunc fqgl::BlendFunc
#define glBlendFuncSeparate fqgl::BlendFuncSeparate
#define glGetIntegerv fqgl::GetIntegerv
#define glGetBooleanv fqgl::GetBooleanv
#define glGetFloatv fqgl::GetFloatv
#define glUniform1f fqgl::Uniform1f
#define glUniform1i fqgl::Uniform1i
#define glUniform2f fqgl::Uniform2f
#define glUniform3f fqgl::Uniform3f
#define glUniform4f fqgl::Uniform4f
#define glUniform1fv fqgl::Uniform1fv
#define glUniform2fv fqgl::Uniform2fv
#define glUniform3fv fqgl::Uniform3fv
#define glUniform4fv fqgl::Uniform4fv
#define glUniformMatrix4fv fqgl::UniformMatrix4fv
#define glDrawArrays fqgl::DrawArrays
#define glDrawArraysInstanced fqgl::DrawArraysInstanced
#define glDeleteProgram fqgl::DeleteProgram
#define glDeleteTextures fqgl::DeleteTextures
#define glDeleteBuffers fqgl::DeleteBuffers
#define glDeleteVertexArrays fqgl::DeleteVertexArrays
