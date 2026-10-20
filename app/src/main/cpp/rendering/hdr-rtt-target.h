#pragma once
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <cstring>

namespace fqhdr {
inline bool HasExtension(const char* list,const char* name) {
    if(!list)return false;
    const size_t length=std::strlen(name);
    const char* at=list;
    while((at=std::strstr(at,name))){
        if((at==list||at[-1]==' ')&&(at[length]==' '||at[length]=='\0'))return true;
        at+=length;
    }
    return false;
}
// Experimental colour-only MRTT. Depth stays in an EXT multisample
// renderbuffer and is explicitly copied before the final colour is sampled.
// Never resume world/water rendering after that copy/implicit resolve.
struct RttTarget {
    GLuint fbo=0,depth=0,colour=0;
    GLsizei width=0,height=0;
    GLenum format=0;
    bool attempted=false,ready=false;
    PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC attach=nullptr;
    PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC storage=nullptr;
    void Reset(){
        if(fbo)glDeleteFramebuffers(1,&fbo);
        if(depth)glDeleteRenderbuffers(1,&depth);
        fbo=depth=colour=0;width=height=0;format=0;attempted=ready=false;
    }
    bool Ensure(GLsizei w,GLsizei h,GLuint texture,GLenum colourFormat,GLuint resolvedFbo){
        if(attempted&&w==width&&h==height&&texture==colour&&format==colourFormat)return ready;
        Reset();attempted=true;width=w;height=h;colour=texture;format=colourFormat;
        if(w<=0||h<=0||!texture||!resolvedFbo||colourFormat!=GL_RGBA16F)return false;
        const char* extensions=reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        if(!HasExtension(extensions,"GL_EXT_multisampled_render_to_texture"))return false;
        attach=reinterpret_cast<PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC>(eglGetProcAddress("glFramebufferTexture2DMultisampleEXT"));
        storage=reinterpret_cast<PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC>(eglGetProcAddress("glRenderbufferStorageMultisampleEXT"));
        if(!attach||!storage)return false;
        while(glGetError()!=GL_NO_ERROR){}
        GLint previousRenderbuffer=0;glGetIntegerv(GL_RENDERBUFFER_BINDING,&previousRenderbuffer);
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        attach(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0,2);
        glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);
        storage(GL_RENDERBUFFER,2,GL_DEPTH_COMPONENT24,w,h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
        GLint actual=0;glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_SAMPLES,&actual);
        GLint colourSamples=0;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT,&colourSamples);
        ready=actual==2&&colourSamples==2&&glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE&&glGetError()==GL_NO_ERROR;
        if(ready){
            // Probe the exact depth-copy operation before any live-world use.
            GLboolean write=GL_TRUE;GLfloat clear=1;
            glGetBooleanv(GL_DEPTH_WRITEMASK,&write);glGetFloatv(GL_DEPTH_CLEAR_VALUE,&clear);
            glDepthMask(GL_TRUE);glClearDepthf(1);glClear(GL_DEPTH_BUFFER_BIT);
            glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,resolvedFbo);
            glBlitFramebuffer(0,0,w,h,0,0,w,h,GL_DEPTH_BUFFER_BIT,GL_NEAREST);
            ready=glGetError()==GL_NO_ERROR;
            glClearDepthf(clear);glDepthMask(write);
        }
        if(!ready){
            if(fbo)glDeleteFramebuffers(1,&fbo);
            if(depth)glDeleteRenderbuffers(1,&depth);
            fbo=depth=0;
            while(glGetError()!=GL_NO_ERROR){}
        }
        glBindRenderbuffer(GL_RENDERBUFFER,static_cast<GLuint>(previousRenderbuffer));
        return ready;
    }
};
} // namespace fqhdr
