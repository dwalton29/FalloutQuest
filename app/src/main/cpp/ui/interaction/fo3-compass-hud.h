#pragma once
#include "ui/fo3-system-colour.h"
#include "fo3-compass-state.h"
#include "fo3-health-bar-hud.h"
namespace fo3compassui {
struct Draw {size_t sprite=0,first=0,count=0;};
struct State {GLuint vao=0,vbo=0;bool attempted=false,ready=false;std::array<fo3healthui::Sprite,6> sprites;std::vector<Draw> draws;};
inline State& Get(){static State s;return s;}
inline bool Ensure(){auto& s=Get();if(s.attempted)return s.ready;s.attempted=true;
  if(!fo3hudrenderer::EnsureResources())return false;
  const char* names[]={"glow_hud_comp_direction_strip.dds","glow_hud_compass_landmark.dds","glow_hud_compass_landmark_discovered.dds","glow_hud_compass_objective_marker.dds","glow_hud_compass_pc_marker.dds","glow_hud_tick_mark.dds"};
  for(size_t i=0;i<s.sprites.size();++i)if(!fo3healthui::LoadSprite(s.sprites[i],names[i],true,i?&s.sprites[i-1]:nullptr)){__android_log_print(ANDROID_LOG_WARN,fo3hudrenderer::kTag,"COMPASS ORIGINAL ASSET MISSING: %s",names[i]);return false;}
  glGenVertexArrays(1,&s.vao);glGenBuffers(1,&s.vbo);glBindVertexArray(s.vao);glBindBuffer(GL_ARRAY_BUFFER,s.vbo);
  glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),nullptr);
  glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),reinterpret_cast<void*>(3*sizeof(float)));
  s.ready=s.vao&&s.vbo;return s.ready;
}
inline void Prepare(const fo3compass::Frame& frame){
  fo3hudrenderer::CachedStateGuard guard;if(!Ensure())return;auto& s=Get();s.draws.clear();std::vector<fo3hudassets::Vertex> vertices;
  // Authored 345x64 compass window and 40x50 markers; physical size/anchoring
  // are VR presentation policy. The DDS strip begins and ends at North.
  constexpr float width=.55f,unit=width/345.f,bottom=-.48f;
  auto quad=[&](size_t sprite,float x,float y,float w,float h,float u0=0,float u1=1){const auto& uv=s.sprites[sprite].uv;
    const float l=uv.u+u0*uv.w,r=uv.u+u1*uv.w,t=uv.v,b=uv.v+uv.h;const auto at=vertices.size();
    vertices.insert(vertices.end(),{{x,y,-1.3f,l,t},{x+w,y,-1.3f,r,t},{x,y-h,-1.3f,l,b},{x,y-h,-1.3f,l,b},{x+w,y,-1.3f,r,t},{x+w,y-h,-1.3f,r,b}});
    s.draws.push_back({sprite,at,6});};
  constexpr float fraction=345.f/1024.f;float start=frame.bearing/(2*fo3compass::Pi)-fraction*.5f;start-=std::floor(start);
  const float first=std::min(fraction,1-start),part=first/fraction;
  quad(0,-width*.5f,bottom,width*part,64*unit,start,start+first);
  if(part<1)quad(0,-width*.5f+width*part,bottom,width*(1-part),64*unit,0,fraction-first);
  for(const auto& icon:frame.icons){const size_t sprite=1+size_t(icon.kind);const float x=icon.x*width;const float iconWidth=40*unit;
    // Clip complete marker quads against the same compass window.
    const float left=std::max(-width*.5f,x-iconWidth*.5f),right=std::min(width*.5f,x+iconWidth*.5f);
    if(right>left)quad(sprite,left,bottom+.055f,right-left,50*unit,(left-x+iconWidth*.5f)/iconWidth,(right-x+iconWidth*.5f)/iconWidth);
  }
  glBindVertexArray(s.vao);glBindBuffer(GL_ARRAY_BUFFER,s.vbo);glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(vertices[0]),vertices.data(),GL_DYNAMIC_DRAW);
  fo3hudassets::UploadVertices(s.vao,vertices.data(),vertices.size());
}
inline void Render(const float* mvp){auto& s=Get();if(!s.ready||!mvp)return;auto& hud=fo3hudrenderer::State();fo3hudrenderer::CachedStateGuard guard;
  glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(hud.program);glUniformMatrix4fv(hud.mvpLoc,1,GL_FALSE,mvp);glUniform1i(hud.texLoc,0);glActiveTexture(GL_TEXTURE0);glBindVertexArray(s.vao);
  glUniform4f(hud.tintLoc,fo3uicolor::Main[0],fo3uicolor::Main[1],fo3uicolor::Main[2],1);
  for(const auto& d:s.draws){glBindTexture(GL_TEXTURE_2D,s.sprites[d.sprite].texture);glDrawArrays(GL_TRIANGLES,GLint(d.first),GLsizei(d.count));}
}
inline void Shutdown(){auto& s=Get();if(s.vao)glDeleteVertexArrays(1,&s.vao);if(s.vbo)glDeleteBuffers(1,&s.vbo);for(auto& sprite:s.sprites)if(sprite.ownsTexture&&sprite.texture)glDeleteTextures(1,&sprite.texture);s={};}
}
