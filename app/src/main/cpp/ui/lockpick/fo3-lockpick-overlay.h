#pragma once
#include "rendering/gl-state-cache.h"
namespace fo3lockoverlay {
inline GLuint depth=0;
inline GLsizei depthWidth=0,depthHeight=0;
struct DepthScope {
    GLboolean scissor=glIsEnabled(GL_SCISSOR_TEST),stencil=glIsEnabled(GL_STENCIL_TEST);
    fqgl::Value<GLenum> previousFunc=fqgl::state.depthFunc;
    bool offset=fqgl::state.offset.known&&fqgl::state.offset.value;
    bool attached=false,valid=true;
    DepthScope(GLsizei width,GLsizei height) {
        glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_POLYGON_OFFSET_FILL);
        if(fqgl::UseDriver()) {
            GLint previous=0;glGetIntegerv(GL_RENDERBUFFER_BINDING,&previous);
            if(!depth)glGenRenderbuffers(1,&depth);
            glBindRenderbuffer(GL_RENDERBUFFER,depth);
            if(width!=depthWidth||height!=depthHeight) {
                glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);
                depthWidth=width;depthHeight=height;
            }
            // Caller binds the final eye colour framebuffer, which has no
            // scene depth attachment. Keep the world/post depth intact.
            glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
            glBindRenderbuffer(GL_RENDERBUFFER,GLuint(previous));attached=true;
        }
        glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_TRUE);
        glClearDepthf(1);glClear(GL_DEPTH_BUFFER_BIT);
#ifdef FQ_ENABLE_VULKAN
        valid=fqvk::BeginModalDepth();
#endif
    }
    ~DepthScope() {
        if(attached)glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,0);
        if(scissor)glEnable(GL_SCISSOR_TEST);
        if(stencil)glEnable(GL_STENCIL_TEST);
        if(offset)glEnable(GL_POLYGON_OFFSET_FILL);
        if(previousFunc.known)glDepthFunc(previousFunc.value);
    }
};
inline void Shutdown(){if(depth)glDeleteRenderbuffers(1,&depth);depth=0;depthWidth=depthHeight=0;}
} // namespace fo3lockoverlay
