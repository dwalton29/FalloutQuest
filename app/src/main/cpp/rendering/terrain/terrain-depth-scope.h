#pragma once
#include <GLES3/gl3.h>
namespace fo3terrain {
// LAND is a geometry pass, even if its caller just restored sky/UI state.
// Painted layers share base depth exactly, hence LEQUAL for both passes.
class DepthScope {
    GLboolean enabled_,write_;
    GLint function_;
public:
    DepthScope() {
        enabled_=glIsEnabled(GL_DEPTH_TEST);
        glGetBooleanv(GL_DEPTH_WRITEMASK,&write_);
        glGetIntegerv(GL_DEPTH_FUNC,&function_);
        glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDepthFunc(GL_LEQUAL);
    }
    ~DepthScope() {
        glDepthMask(write_);glDepthFunc(static_cast<GLenum>(function_));
        if(enabled_)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);
    }
    DepthScope(const DepthScope&)=delete;
    DepthScope& operator=(const DepthScope&)=delete;
};
}
