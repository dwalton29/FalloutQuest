#pragma once
#include "fo3-interaction-hud-renderer.h"
namespace fo3ammoui {
struct State{GLuint vao=0,vbo=0;GLsizei count=0;char text[48]{};};
inline State&Get(){static State s;return s;}
// HUDMainMenu uses the original Monofonto text templates and HUD systemcolour.
// The count-only weapon-relative placement and physical scale are VR policy.
inline void Prepare(const char*text){
  auto&s=Get();if(!text||std::strcmp(text,s.text)==0)return;
  fo3hudrenderer::CachedStateGuard guard;if(!fo3hudrenderer::EnsureResources())return;
  auto&font=fo3hudrenderer::State();
  std::array<fo3hudassets::Vertex,48*6> vertices{};size_t count=0;
  const auto view=std::string_view(text);const float width=fo3font::MeasureText(font,view);
  fo3font::AppendText(font,view,-width*.5f,0,[&](uint8_t,const fo3font::Glyph&g,const fo3font::Quad&q){
    constexpr float scale=.00055f;
    const fo3hudassets::Vertex quad[6]{{q.left*scale,-q.top*scale+.08f,-.04f,g.uv[0],g.uv[1]},
      {q.right*scale,-q.top*scale+.08f,-.04f,g.uv[2],g.uv[3]}, {q.left*scale,-q.bottom*scale+.08f,-.04f,g.uv[4],g.uv[5]},
      {q.left*scale,-q.bottom*scale+.08f,-.04f,g.uv[4],g.uv[5]}, {q.right*scale,-q.top*scale+.08f,-.04f,g.uv[2],g.uv[3]},
      {q.right*scale,-q.bottom*scale+.08f,-.04f,g.uv[6],g.uv[7]}};
    if(count+6<=vertices.size())for(auto&v:quad)vertices[count++]=v;
  });
  if(!s.vao)glGenVertexArrays(1,&s.vao);
  if(!s.vbo)glGenBuffers(1,&s.vbo);
  glBindVertexArray(s.vao);glBindBuffer(GL_ARRAY_BUFFER,s.vbo);
  glBufferData(GL_ARRAY_BUFFER,count*sizeof(fo3hudassets::Vertex),vertices.data(),GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),nullptr);
  glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),reinterpret_cast<void*>(3*sizeof(float)));
  s.count=count;std::snprintf(s.text,sizeof(s.text),"%s",text);
}
inline void Render(const float*mvp){
  auto&s=Get();auto&font=fo3hudrenderer::State();if(!mvp||!s.count||!font.ready)return;
  fo3hudrenderer::CachedStateGuard guard;
  glEnable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glUseProgram(font.program);
  glUniformMatrix4fv(font.mvpLoc,1,GL_FALSE,mvp);glUniform4f(font.tintLoc,26.f/255,1,128.f/255,1);glUniform1i(font.texLoc,0);
  glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,font.fontTexture);glBindVertexArray(s.vao);glDrawArrays(GL_TRIANGLES,0,s.count);
}
inline void Shutdown(){auto&s=Get();if(s.vbo)glDeleteBuffers(1,&s.vbo);if(s.vao)glDeleteVertexArrays(1,&s.vao);s={};}
}
