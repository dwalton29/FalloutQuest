#define FQ_ENABLE_VULKAN 1
#include "rendering/gl-state-cache.h"
#include <cassert>
int main() {
    fqrender::nativeOnly=true;fqrender::driverCalls=0;fqgl::Reset();driverCalls.clear();
    fqrender::capabilities={8192,32,16};
    unsigned shader=glCreateShader(0x8b31);glShaderSource(shader,0,nullptr,nullptr);glCompileShader(shader);
    int accepted=0;glGetShaderiv(shader,0x8b81,&accepted);assert(accepted==1);glDeleteShader(shader);
    unsigned program=glCreateProgram(),texture=0,mesh=0,buffer=0;
    glGenTextures(1,&texture);glGenVertexArrays(1,&mesh);glGenBuffers(1,&buffer);
    assert(program&&texture&&mesh&&buffer&&texture!=mesh&&program!=texture);
    glUseProgram(program);glBindVertexArray(mesh);glBindBuffer(GL_ARRAY_BUFFER,buffer);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);
    glBufferData(GL_ARRAY_BUFFER,16,nullptr,0);
    const int first=glGetUniformLocation(program,"uPositions[0]");
    assert(glGetUniformLocation(program,"uPositions[3]")==first+3);
    assert(glGetUniformLocation(program,"uPositions")==first);
    glUniform1f(first,1);
    glEnable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDepthFunc(GL_LEQUAL);
    static fqrender::DrawCommand captured;
    fqvk::hooks.draw=[](const fqrender::DrawCommand& command){captured=command;};
    fqvk::recording=true;glDrawArrays(GL_TRIANGLES,0,6);fqvk::recording=false;
    glDisable(GL_DEPTH_TEST);glBindTexture(GL_TEXTURE_2D,0);
    assert(captured.state.depth&&!captured.state.write&&captured.state.depthFunc==GL_LEQUAL);
    assert(captured.state.textures[0]==texture&&captured.mesh==mesh&&captured.program==program);
    glViewport(2,3,640,480);int viewport[4]{};glGetIntegerv(0x0ba2,viewport);
    assert(viewport[0]==2&&viewport[1]==3&&viewport[2]==640&&viewport[3]==480);
    glClearColor(.1f,.2f,.3f,1);float clear[4]{};glGetFloatv(0x0c22,clear);assert(clear[2]==.3f);
    int limit=0;glGetIntegerv(0x0d33,&limit);assert(limit==8192);
    glGetIntegerv(0x8869,&limit);assert(limit==32);glGetIntegerv(0x8d57,&limit);assert(limit==1);
    assert(*glGetString(0x1f03)==0); // GL timers/extensions cannot load driver entry points.
    glDeleteTextures(1,&texture);glDeleteVertexArrays(1,&mesh);glDeleteBuffers(1,&buffer);glDeleteProgram(program);
    unsigned batch[2]{};glGenTextures(2,batch);
    static unsigned erased=0;
    fqvk::hooks.erase=[](uint32_t,bool){++erased;fqvk::hooks={};}; // renderer failure may disable callbacks mid-batch
    glDeleteTextures(2,batch);assert(erased==1);
    assert(driverCalls.empty()&&fqrender::driverCalls==0);
    fqvk::hooks={};fqrender::nativeOnly=false;fqgl::Reset();glUseProgram(7);glUniform1f(1,.2f);glDrawArrays(GL_TRIANGLES,0,6);
    assert(driverCalls["glUseProgram"]==1&&driverCalls["glUniform1f"]==1&&driverCalls["glDrawArrays"]==1);
}
