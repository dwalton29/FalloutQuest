#pragma once
// Original HUDMainMenu HitPoints / EnemyHealth and meter.xml _SolidMeter.
// The InterfaceShared.tai "solid.dds" entry supplies Bethesda's solid sprite;
// floating placement, billboarding and metres-per-unit are Quest VR policy.
#include "fo3-interaction-hud-renderer.h"
#include "fo3-health-bar-state.h"
#include <array>
#include <cmath>

namespace fo3healthui {
struct State {
  GLuint vao=0,vbo=0,texture=0;
  bool ownsTexture=false,ready=false;
  fo3hudassets::TaiSprite sprite;
};
inline State& Get(){static State s;return s;}
inline bool Ensure(){
  auto& s=Get();
  if(s.ready)return true;
  if(!fo3hudrenderer::EnsureResources())return false;
  fo3hudassets::TaiSprite shared;
  if(!fo3hudassets::ParseButtonTai(s.sprite,"solid.dds")||
     !fo3hudassets::ParseButtonTai(shared,"glow_general_button_a.dds"))return false;
  if(s.sprite.atlasPath==shared.atlasPath)
    s.texture=fo3hudrenderer::State().interfaceTexture;
  else {
    Fo3RgbaTexture atlas;
    if(!LoadFalloutTextureRgba(s.sprite.atlasPath,atlas))return false;
    s.texture=fo3hudassets::UploadTexture(atlas.width,atlas.height,atlas.pixels.data());
    s.ownsTexture=true;
  }
  if(!s.texture)return false;
  glGenVertexArrays(1,&s.vao);glGenBuffers(1,&s.vbo);
  glBindVertexArray(s.vao);glBindBuffer(GL_ARRAY_BUFFER,s.vbo);
  glBufferData(GL_ARRAY_BUFFER,12*sizeof(fo3hudassets::Vertex),nullptr,GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),nullptr);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),
    reinterpret_cast<void*>(3*sizeof(float)));
  s.ready=true;return true;
}
inline void Quad(std::array<fo3hudassets::Vertex,12>& out,size_t at,
    const std::array<float,3>& pos,const std::array<float,3>& right,
    float x0,float x1,float y0,float y1,const fo3hudassets::TaiSprite& sprite){
  const float u0=sprite.u,v0=sprite.v,u1=u0+sprite.w,v1=v0+sprite.h;
  const auto v=[&](float x,float y,float u,float t){
    return fo3hudassets::Vertex{pos[0]+right[0]*x,pos[1]+y,pos[2]+right[2]*x,u,t};
  };
  out[at+0]=v(x0,y1,u0,v0);out[at+1]=v(x1,y1,u1,v0);out[at+2]=v(x0,y0,u0,v1);
  out[at+3]=out[at+2];out[at+4]=out[at+1];out[at+5]=v(x1,y0,u1,v1);
}
inline void RenderBar(const float* mvp,const std::array<float,3>& center,
    const std::array<float,3>& eye,float width,const fo3health::Bar& bar,double now,
    bool depthTest){
  const float alpha=bar.Alpha(now);
  if(!mvp||alpha<=0||!std::isfinite(width)||width<=0)return;
  const float dx=eye[0]-center[0],dz=eye[2]-center[2];
  const float distance=std::hypot(dx,dz);
  if(distance<.10f||!std::isfinite(distance))return;
  fo3hudrenderer::CachedStateGuard guard;
  if(!Ensure())return;
  auto& s=Get();auto& hud=fo3hudrenderer::State();
  const std::array<float,3> right{dz/distance,0,-dx/distance};
  const float half=width*.5f,halfHeight=width*.05f;
  const float filled=-half+width*bar.Fraction();
  std::array<fo3hudassets::Vertex,12> vertices{};
  Quad(vertices,0,center,right,-half,half,-halfHeight,halfHeight,s.sprite);
  Quad(vertices,6,center,right,-half,filled,-halfHeight,halfHeight,s.sprite);
  if(depthTest){glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);}
  else glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);
  glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
  glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(hud.program);glUniformMatrix4fv(hud.mvpLoc,1,GL_FALSE,mvp);
  glUniform1i(hud.texLoc,0);
  glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,s.texture);
  glBindVertexArray(s.vao);glBindBuffer(GL_ARRAY_BUFFER,s.vbo);
  glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(vertices),vertices.data());
  // Both quads use original HUDMainMenu &hudmain; colour from Fallout.ini.
  glUniform4f(hud.tintLoc,26.f/255.f,1.f,128.f/255.f,alpha*.24f);
  glDrawArrays(GL_TRIANGLES,0,6);
  if(filled>-half){
    glUniform4f(hud.tintLoc,26.f/255.f,1.f,128.f/255.f,alpha);
    glDrawArrays(GL_TRIANGLES,6,6);
  }
}
inline void Shutdown(){
  auto& s=Get();
  if(s.vbo)glDeleteBuffers(1,&s.vbo);
  if(s.vao)glDeleteVertexArrays(1,&s.vao);
  if(s.ownsTexture&&s.texture)glDeleteTextures(1,&s.texture);
  s={};
}
} // namespace fo3healthui
