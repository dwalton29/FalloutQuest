#include "rendering/gl-state-cache.h"
#include "rendering/terrain/terrain-depth-scope.h"
#include <cassert>
int main() {
    // RenderEye disables depth for sky. The environment pass restores that
    // state before LAND, which must still occlude against existing buildings.
    for(bool incomingDepth:{false,true})for(GLenum incomingFunction:{GL_LESS,GL_LEQUAL})for(bool incomingWrite:{false,true}) {
        fqgl::Reset();
        if(incomingDepth)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);
        glDepthMask(incomingWrite);glDepthFunc(incomingFunction);
        const auto queries=fqgl::counters.queries;
        {
            fo3terrain::DepthScope land;
            assert(glIsEnabled(GL_DEPTH_TEST));assert(fqgl::state.depthWrite.value);
            assert(fqgl::state.depthFunc.value==GL_LEQUAL);
            glDepthMask(GL_FALSE); // authored paint writes color only
            assert(glIsEnabled(GL_DEPTH_TEST)&&fqgl::state.depthFunc.value==GL_LEQUAL);
        }
        assert(bool(glIsEnabled(GL_DEPTH_TEST))==incomingDepth);
        assert(bool(fqgl::state.depthWrite.value)==incomingWrite);
        assert(fqgl::state.depthFunc.value==incomingFunction);
        assert(fqgl::counters.queries==queries);
    }
}
