#include "rendering/gl-state-cache.h"
#include <cassert>
#include <iostream>

int main() {
    fqgl::Reset(); driverCalls.clear();
    for(int i=0;i<100;++i) {
        glUseProgram(7);glBindVertexArray(9);glBindBuffer(GL_ARRAY_BUFFER,11);
        fqgl::BindTextureUnit(GL_TEXTURE0,GL_TEXTURE_2D,21);
        fqgl::BindTextureUnit(GL_TEXTURE0+1,GL_TEXTURE_2D,22);
        glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDepthFunc(GL_LEQUAL);
        glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glFrontFace(GL_CCW);
        glDisable(GL_BLEND);glDisable(GL_POLYGON_OFFSET_FILL);
        glUniform1f(0,0.75f);glUniform3f(1,1,2,3);
    }
    assert(driverCalls["glUseProgram"]==1 && driverCalls["glBindVertexArray"]==1);
    assert(driverCalls["glBindBuffer"]==1 && driverCalls["glBindTexture"]==2);
    assert(driverCalls["glActiveTexture"]==2 && driverCalls["glUniform1f"]==1);
    assert(driverCalls["glUniform3f"]==1 && fqgl::counters.queries==0);
    // Queries of known state return the tracked driver state with no driver read.
    GLint front=0;glGetIntegerv(GL_FRONT_FACE,&front);
    assert(front==GL_CCW && glIsEnabled(GL_CULL_FACE)==GL_TRUE);
    assert(fqgl::counters.queries==0);
    // Authored two-sided/reversed and mirror transitions remain distinct.
    glDisable(GL_CULL_FACE);glDisable(GL_CULL_FACE);
    glFrontFace(GL_CW);glFrontFace(GL_CW);
    assert(driverCalls["glFrontFace"]==2 && !fqgl::state.cull.value);
    glEnable(GL_CULL_FACE);glFrontFace(GL_CCW);
    // Decal -> decal -> ordinary does not repeat the offset state.
    for(int i=0;i<2;++i) {glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(-0.65f,-1);}
    assert(driverCalls["glPolygonOffset"]==1);
    glDisable(GL_POLYGON_OFFSET_FILL);
    // Equal uniforms deduplicate across overloads, but remain program-specific.
    const float same[]{1,2,3};glUniform3fv(1,1,same);
    assert(driverCalls["glUniform3fv"]==0);
    glUniform1f(0,0.5f);assert(driverCalls["glUniform1f"]==2);
    glUseProgram(8);glUniform1f(0,0.5f);assert(driverCalls["glUniform1f"]==3);
    glUseProgram(7);glUniform1f(0,0.5f);assert(driverCalls["glUniform1f"]==3);
    glDeleteProgram(8);glUseProgram(8);glUniform1f(0,0.5f);
    assert(driverCalls["glUniform1f"]==4);
    // Resource deletion/name reuse must force the next bind.
    GLuint texture=21;glDeleteTextures(1,&texture);
    fqgl::BindTextureUnit(GL_TEXTURE0,GL_TEXTURE_2D,21);
    assert(driverCalls["glBindTexture"]==3);
    GLuint vao=9, buffer=11;glDeleteVertexArrays(1,&vao);glDeleteBuffers(1,&buffer);
    glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,buffer);
    assert(driverCalls["glBindVertexArray"]==2 && driverCalls["glBindBuffer"]==2);
    // Element-array bindings are VAO-local and cannot use the global buffer cache.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,4);glBindVertexArray(10);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,4);assert(driverCalls["glBindBuffer"]==4);
    GLfloat values[8]{};glUniform1f(3,0.25f);
    glUniform4fv(2,2,values);glUniform1f(3,0.25f);
    assert(driverCalls["glUniform1f"]==6);
    fqgl::Invalidate();glUseProgram(8);assert(driverCalls["glUseProgram"]==5);
    std::cout << "GL state/uniform cache regressions passed\n";
}
