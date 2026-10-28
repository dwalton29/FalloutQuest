#pragma once
// Original HUDMainMenu HitPoints / EnemyHealth: tiled hud_tick_mark and
// their distinct authored brackets. No solid replacement artwork is embedded.
#include "fo3-interaction-hud-renderer.h"
#include "fo3-health-bar-state.h"
#include "fo3-health-bar-layout.h"
#include <array>
#include <cmath>

namespace fo3healthui {
struct Sprite {
  GLuint texture=0;
  bool ownsTexture=false;
  fo3hudassets::TaiSprite uv;
};
constexpr size_t kVertexCapacity=6*(1+38);
struct State {
  GLuint vao=0,vbo=0;
  bool ready=false,attempted=false;
  Sprite tick,playerBracket,enemyBracket;
};
inline State& Get(){static State s;return s;}
inline bool LoadSprite(Sprite& out,const char* name,bool atlas,const Sprite* previous=nullptr){
  auto& hud=fo3hudrenderer::State();
  if(atlas&&fo3hudassets::ParseButtonTai(out.uv,name)){
    if(out.uv.atlasPath==hud.button.atlasPath)out.texture=hud.interfaceTexture;
    else if(previous&&previous->texture&&out.uv.atlasPath==previous->uv.atlasPath)
      out.texture=previous->texture;
    else {
      Fo3RgbaTexture image;
      if(LoadFalloutTextureRgba(out.uv.atlasPath,image)){
        out.texture=fo3hudassets::UploadTexture(image.width,image.height,image.rgba.data());
        out.ownsTexture=out.texture!=0;
      }
    }
    if(out.texture)return true;
  }
  // Original loose DDS also supports installs without a usable atlas entry.
  out.uv={};out.uv.w=out.uv.h=1;
  Fo3RgbaTexture image;
  if(!LoadFalloutTextureRgba(std::string("Textures\\Interface\\HUD\\")+name,image))return false;
  out.texture=fo3hudassets::UploadTexture(image.width,image.height,image.rgba.data());
  out.ownsTexture=out.texture!=0;
  return out.texture!=0;
}
inline bool Ensure(){
  auto& s=Get();
  if(s.ready)return true;
  if(s.attempted)return false;
  s.attempted=true;
  if(!fo3hudrenderer::EnsureResources())return false;
  if(!LoadSprite(s.tick,"hud_tick_mark.dds",false)||
     !LoadSprite(s.playerBracket,"glow_hud_left_seperatorglow.dds",true)||
     !LoadSprite(s.enemyBracket,"glow_enemy_health_seperator.dds",true,&s.playerBracket)){
    __android_log_print(ANDROID_LOG_WARN,fo3hudrenderer::kTag,
        "HEALTH HUD ORIGINAL ASSET MISSING: tick or HP/EnemyHealth bracket");
    return false;
  }
  glGenVertexArrays(1,&s.vao);glGenBuffers(1,&s.vbo);
  glBindVertexArray(s.vao);glBindBuffer(GL_ARRAY_BUFFER,s.vbo);
  glBufferData(GL_ARRAY_BUFFER,kVertexCapacity*sizeof(fo3hudassets::Vertex),nullptr,GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),nullptr);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),
    reinterpret_cast<void*>(3*sizeof(float)));
  s.ready=s.vao&&s.vbo;return s.ready;
}
using Vertices=std::array<fo3hudassets::Vertex,kVertexCapacity>;
inline void Quad(Vertices& out,size_t at,
    const std::array<float,3>& pos,const std::array<float,3>& right,
    float x0,float x1,float y0,float y1,const fo3hudassets::TaiSprite& sprite){
  const float u0=sprite.u,v0=sprite.v,u1=u0+sprite.w,v1=v0+sprite.h;
  const auto v=[&](float x,float y,float u,float t){
    return fo3hudassets::Vertex{pos[0]+right[0]*x,pos[1]+y,pos[2]+right[2]*x,u,t};
  };
  out[at+0]=v(x0,y1,u0,v0);out[at+1]=v(x1,y1,u1,v0);out[at+2]=v(x0,y0,u0,v1);
  out[at+3]=out[at+2];out[at+4]=out[at+1];out[at+5]=v(x1,y0,u1,v1);
}
inline size_t BuildVertices(Vertices& vertices,Style style,float fraction,float width,
    const std::array<float,3>& center,const std::array<float,3>& right,
    const fo3hudassets::TaiSprite& bracket,const fo3hudassets::TaiSprite& tick){
  const auto layout=AuthoredLayout(style);
  const float scale=width/layout.totalWidth;
  const float originX=style==Style::Enemy ? layout.originalX : layout.originalX+layout.totalWidth*.5f;
  const float originY=layout.meterY+kTickHeight*.5f;
  const auto quad=[&](size_t at,float x,float y,float w,float h,const fo3hudassets::TaiSprite& uv){
    Quad(vertices,at,center,right,(x-originX)*scale,(x+w-originX)*scale,
         (originY-y-h)*scale,(originY-y)*scale,uv);
  };
  quad(0,layout.bracketX,layout.bracketY,layout.bracketWidth,layout.bracketHeight,bracket);
  const float fill=FillWidth(style,fraction),x=MeterX(style,fill);
  size_t count=6;
  for(float offset=0;offset<fill;offset+=kTickWidth){
    quad(count,x+offset,layout.meterY,kTickWidth,kTickHeight,tick);count+=6;
  }
  return count;
}
inline void RenderBar(const float* mvp,const std::array<float,3>& center,
    const std::array<float,3>& eye,float width,const fo3health::Bar& bar,double now,
    bool depthTest,Style style){
  const float alpha=bar.Alpha(now);
  if(!mvp||alpha<=0||!std::isfinite(width)||width<=0)return;
  const float dx=eye[0]-center[0],dz=eye[2]-center[2];
  const float distance=std::hypot(dx,dz);
  if(distance<.10f||!std::isfinite(distance))return;
  fo3hudrenderer::CachedStateGuard guard;
  if(!Ensure())return;
  auto& s=Get();auto& hud=fo3hudrenderer::State();
  const auto& bracket=style==Style::Player ? s.playerBracket : s.enemyBracket;
  const std::array<float,3> right{dz/distance,0,-dx/distance};
  Vertices vertices{};
  const size_t count=BuildVertices(vertices,style,bar.Fraction(),width,center,right,bracket.uv,s.tick.uv);
  if(depthTest)glEnable(GL_DEPTH_TEST);
  else glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);
  glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
  glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(hud.program);glUniformMatrix4fv(hud.mvpLoc,1,GL_FALSE,mvp);
  glUniform1i(hud.texLoc,0);glActiveTexture(GL_TEXTURE0);
  glBindVertexArray(s.vao);glBindBuffer(GL_ARRAY_BUFFER,s.vbo);
  glBufferSubData(GL_ARRAY_BUFFER,0,count*sizeof(vertices[0]),vertices.data());
  fo3hudassets::UploadVertices(s.vao,vertices.data(),count);
  glBindTexture(GL_TEXTURE_2D,bracket.texture);
  glUniform4f(hud.tintLoc,26.f/255.f,1.f,128.f/255.f,
              alpha*(style==Style::Player ? 1.f : kMeterAlpha));
  glDrawArrays(GL_TRIANGLES,0,6);
  if(count>6){
    glBindTexture(GL_TEXTURE_2D,s.tick.texture);
    glUniform4f(hud.tintLoc,26.f/255.f,1.f,128.f/255.f,alpha*kMeterAlpha);
    glDrawArrays(GL_TRIANGLES,6,static_cast<GLsizei>(count-6));
  }
}
inline void Shutdown(){
  auto& s=Get();
  if(s.vbo)glDeleteBuffers(1,&s.vbo);
  if(s.vao)glDeleteVertexArrays(1,&s.vao);
  for(const auto* sprite:{&s.tick,&s.playerBracket,&s.enemyBracket})
    if(sprite->ownsTexture&&sprite->texture)glDeleteTextures(1,&sprite->texture);
  s={};
}
} // namespace fo3healthui
