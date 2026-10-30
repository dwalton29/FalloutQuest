#pragma once
#include "fo3-interaction-hud-renderer.h"
namespace fo3blinkui {
struct State {GLuint vao=0,vbo=0,white=0;};
inline State& Get(){static State s;return s;}
inline void Render(float alpha){
 if(alpha<=0)return;
 auto& s=Get();auto& font=fo3hudrenderer::State();
 fo3hudrenderer::CachedStateGuard guard;if(!fo3hudrenderer::EnsureResources())return;
 if(!s.vao){const uint8_t white[4]{255,255,255,255};s.white=fo3hudassets::UploadTexture(1,1,white);
  const fo3hudassets::Vertex v[]{{-1,1,0,0,0},{1,1,0,1,0},{-1,-1,0,0,1},{-1,-1,0,0,1},{1,1,0,1,0},{1,-1,0,1,1}};
  glGenVertexArrays(1,&s.vao);glGenBuffers(1,&s.vbo);glBindVertexArray(s.vao);glBindBuffer(GL_ARRAY_BUFFER,s.vbo);glBufferData(GL_ARRAY_BUFFER,sizeof(v),v,GL_DYNAMIC_DRAW);fo3hudassets::UploadVertices(s.vao,v,6);glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(v[0]),nullptr);glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(v[0]),reinterpret_cast<void*>(3*sizeof(float)));
 }
 const float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
 glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glUseProgram(font.program);glUniformMatrix4fv(font.mvpLoc,1,GL_FALSE,identity);glUniform4f(font.tintLoc,0,0,0,std::clamp(alpha,0.f,1.f));glUniform1i(font.texLoc,0);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,s.white);glBindVertexArray(s.vao);glDrawArrays(GL_TRIANGLES,0,6);
}
inline void Shutdown(){auto& s=Get();if(s.vao)glDeleteVertexArrays(1,&s.vao);if(s.vbo)glDeleteBuffers(1,&s.vbo);if(s.white)glDeleteTextures(1,&s.white);s={};}
}
