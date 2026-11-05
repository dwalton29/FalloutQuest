#pragma once
#include "fo3-interaction-hud-renderer.h"
#include "player/fo3-player-state.h"
#include <random>

namespace fo3bloodui {
struct Spot {float x,y,size,opacity;double began;};
inline std::vector<Spot> spots;
inline GLuint texture=0,vao=0,vbo=0;
inline float duration=0,fadeStart=0;
inline bool textureAttempted=false;
inline void Clear(){spots.clear();}
inline void Trigger(uint32_t count,double now,const fo3player::Catalog& catalog){
  auto setting=[&](const char* name){const auto i=catalog.barterSettings.find(name);return i==catalog.barterSettings.end()?0.f:i->second;};
  duration=setting("fBloodSplatterDuration");fadeStart=setting("fBloodSplatterFadeStart");
  const float lo=setting("fBloodSplatterMinSize"),hi=setting("fBloodSplatterMaxSize");
  const float amin=setting("fBloodSplatterMinOpacity"),amax=setting("fBloodSplatterMaxOpacity");
  if(duration<=0||lo<=0||hi<lo||catalog.screenBloodMaxCount>128)return;
  static std::mt19937 rng{std::random_device{}()};std::uniform_real_distribution<float> unit(0,1);
  while(count--&&spots.size()<catalog.screenBloodMaxCount)
    spots.push_back({unit(rng)*2-1,unit(rng)*2-1,lo+(hi-lo)*unit(rng),amin+(amax-amin)*unit(rng),now});
}
inline void Render(double now){
  spots.erase(std::remove_if(spots.begin(),spots.end(),[&](const auto& s){return now-s.began>=duration;}),spots.end());
  if(spots.empty())return;
  fo3hudrenderer::CachedStateGuard guard;if(!fo3hudrenderer::EnsureResources())return;
  if(!texture){
    if(textureAttempted)return;textureAttempted=true;
    // Original executable's screen-splatter texture defaults. Assets remain in
    // the user's archive; a missing texture never becomes fabricated artwork.
    Fo3RgbaTexture colour,alpha;
    if(!LoadFalloutTextureRgba("Textures\\Gore\\ScreenBloodColor01OPT.dds",colour)||
       !LoadFalloutTextureRgba("Textures\\Gore\\ScreenBloodAlpha01OPT.dds",alpha)||
       colour.width!=alpha.width||colour.height!=alpha.height)return;
    for(size_t n=0;n<colour.rgba.size();n+=4)colour.rgba[n+3]=alpha.rgba[n];
    texture=fo3hudassets::UploadTexture(colour.width,colour.height,colour.rgba.data());
    glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),nullptr);
    glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),reinterpret_cast<void*>(3*sizeof(float)));
  }
  const float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};auto& font=fo3hudrenderer::State();
  glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(font.program);glUniformMatrix4fv(font.mvpLoc,1,GL_FALSE,identity);glUniform1i(font.texLoc,0);
  glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
  for(const auto& s:spots){
    const float elapsed=float((now-s.began)/duration),fade=elapsed<fadeStart?1.f:std::clamp((1-elapsed)/std::max(.001f,1-fadeStart),0.f,1.f);
    const float l=s.x-s.size,r=s.x+s.size,t=s.y+s.size,b=s.y-s.size;
    const fo3hudassets::Vertex v[]{{l,t,0,0,0},{r,t,0,1,0},{l,b,0,0,1},{l,b,0,0,1},{r,t,0,1,0},{r,b,0,1,1}};
    glBufferData(GL_ARRAY_BUFFER,sizeof(v),v,GL_DYNAMIC_DRAW);fo3hudassets::UploadVertices(vao,v,6);
    glUniform4f(font.tintLoc,1,1,1,s.opacity*fade);glDrawArrays(GL_TRIANGLES,0,6);
  }
}
inline void Shutdown(){Clear();if(vao)glDeleteVertexArrays(1,&vao);if(vbo)glDeleteBuffers(1,&vbo);if(texture)glDeleteTextures(1,&texture);vao=vbo=texture=0;textureAttempted=false;}
} // namespace fo3bloodui
