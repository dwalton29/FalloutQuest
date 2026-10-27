#define FQ_ENABLE_VULKAN 1
#include "rendering/gl-state-cache.h"
#include <cassert>
#include <cstring>
int main() {
    fqgl::Reset();driverCalls.clear();unsigned writes=0,draws=0;
    static unsigned* uniformWrites=&writes;static unsigned* drawWrites=&draws;
    fqvk::hooks.uniform=[](uint32_t program,int location,int,int bytes,const void* data){
        assert(program==7&&location==3&&bytes==4);float value=0;std::memcpy(&value,data,4);assert(value==.5f);++*uniformWrites;
    };
    fqvk::hooks.draw=[](uint32_t p,uint32_t v,uint32_t,int first,int n,uint32_t,size_t,int instances){assert(p==7&&v==9&&first==0&&n==6&&instances==1);++*drawWrites;};
    glUseProgram(7);glBindVertexArray(9);
    glUniform1f(3,.5f);glUniform1f(3,.5f);
    assert(writes==2&&driverCalls["glUniform1f"]==1); // mirror sees cache hits too.
    glDrawArrays(GL_TRIANGLES,0,6);assert(draws==0);
    fqvk::recording=true;glDrawArrays(GL_TRIANGLES,0,6);assert(draws==1);
    fqvk::gameplayPresentation=true;
    glDrawArrays(GL_TRIANGLES,0,6);assert(draws==2&&driverCalls["glDrawArrays"]==2);
    fqvk::gameplayPresentation=false;
    fqvk::recording=false;fqvk::hooks={};glDrawArrays(GL_TRIANGLES,0,6);
    assert(draws==2&&driverCalls["glDrawArrays"]==3); // GLES remains usable in all modes.
}
