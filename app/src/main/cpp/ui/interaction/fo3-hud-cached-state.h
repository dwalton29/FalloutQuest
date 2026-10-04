#pragma once
#include "rendering/gl-state-cache.h"
namespace fo3hudrenderer {
// Snapshot renderer-owned values only. No driver readbacks, even for a cold
// cache. Unknown values remain under explicit ownership of the next renderer.
struct CachedStateGuard {
    fqgl::State before=fqgl::state;
    ~CachedStateGuard() {
        if(before.program.known) glUseProgram(before.program.value);
        if(before.vao.known) glBindVertexArray(before.vao.value);
        if(before.arrayBuffer.known) glBindBuffer(GL_ARRAY_BUFFER,before.arrayBuffer.value);
        if(before.texture2d[0].known) {
            glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,before.texture2d[0].value);
        }
        if(before.active.known) glActiveTexture(before.active.value);
        if(before.srcRgb.known && before.dstRgb.known && before.srcAlpha.known && before.dstAlpha.known)
            glBlendFuncSeparate(before.srcRgb.value,before.dstRgb.value,before.srcAlpha.value,before.dstAlpha.value);
        if(before.blendEquationRgb.known && before.blendEquationAlpha.known)
            glBlendEquationSeparate(before.blendEquationRgb.value,before.blendEquationAlpha.value);
        if(before.depthWrite.known) glDepthMask(before.depthWrite.value);
        if(before.unpackAlignment.known) glPixelStorei(GL_UNPACK_ALIGNMENT,before.unpackAlignment.value);
        if(before.depth.known) {if(before.depth.value)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);}
        if(before.blend.known) {if(before.blend.value)glEnable(GL_BLEND);else glDisable(GL_BLEND);}
        if(before.cull.known) {if(before.cull.value)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);}
    }
};
}
