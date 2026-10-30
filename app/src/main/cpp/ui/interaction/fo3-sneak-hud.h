#pragma once
#include "fo3-health-bar-hud.h"
#include "npc/fo3-detection.h"
namespace fo3sneakui {
struct State{GLuint vao=0,vbo=0;fo3healthui::Sprite bracket;bool ready=false,attempted=false,active=false;GLsizei count=0;std::string text;};
inline State& Get(){static State s;return s;}
inline bool Ensure(){auto& s=Get();if(s.attempted)return s.ready;s.attempted=true;if(!fo3hudrenderer::EnsureResources()||!fo3healthui::LoadSprite(s.bracket,"glow_stealth_metre_seperator.dds",true))return false;
 glGenVertexArrays(1,&s.vao);glGenBuffers(1,&s.vbo);glBindVertexArray(s.vao);glBindBuffer(GL_ARRAY_BUFFER,s.vbo);glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),nullptr);glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),reinterpret_cast<void*>(3*sizeof(float)));return s.ready=s.vao&&s.vbo;
}
inline void PrepareLabel(bool active,const std::string& text){
 auto& s=Get();s.active=active;if(!s.active)return;
 if(s.ready&&s.text==text)return;
 fo3hudrenderer::CachedStateGuard guard;if(!Ensure())return;auto& font=fo3hudrenderer::State();
 // Original SneakMeter 356x34 window; bottom-centre head-relative placement
 // and physical size are Quest VR policy. Original font, label and DDS remain.
 constexpr float width=.42f,height=34.f/356.f*width,centreY=-.31f;
 std::vector<fo3hudassets::Vertex> vertices;const auto& u=s.bracket.uv;
 auto quad=[&](float l,float r,float top,float bottom,float u0,float v0,float u1,float v1){vertices.insert(vertices.end(),{{l,top,-1.3f,u0,v0},{r,top,-1.3f,u1,v0},{l,bottom,-1.3f,u0,v1},{l,bottom,-1.3f,u0,v1},{r,top,-1.3f,u1,v0},{r,bottom,-1.3f,u1,v1}});};
 quad(-width*.5f,width*.5f,centreY+height*.5f,centreY-height*.5f,u.u,u.v,u.u+u.w,u.v+u.h);
 float lo=INFINITY,hi=-INFINITY,top=INFINITY,bottom=-INFINITY;
 fo3font::AppendText(font,text,0,0,[&](uint8_t,const fo3font::Glyph&,const fo3font::Quad& q){lo=std::min(lo,q.left);hi=std::max(hi,q.right);top=std::min(top,q.top);bottom=std::max(bottom,q.bottom);});
 if(hi>lo&&bottom>top){const float scale=std::min(width*.85f/(hi-lo),height*.85f/(bottom-top));const float x=(lo+hi)*.5f,y=(top+bottom)*.5f;
  fo3font::AppendText(font,text,0,0,[&](uint8_t,const fo3font::Glyph& g,const fo3font::Quad& q){const float l=(q.left-x)*scale,r=(q.right-x)*scale,t=centreY-(q.top-y)*scale,b=centreY-(q.bottom-y)*scale;
   const fo3hudassets::Vertex v[6]{{l,t,-1.3f,g.uv[0],g.uv[1]},{r,t,-1.3f,g.uv[2],g.uv[3]},{l,b,-1.3f,g.uv[4],g.uv[5]},{l,b,-1.3f,g.uv[4],g.uv[5]},{r,t,-1.3f,g.uv[2],g.uv[3]},{r,b,-1.3f,g.uv[6],g.uv[7]}};vertices.insert(vertices.end(),std::begin(v),std::end(v));});
 }
 glBindVertexArray(s.vao);glBindBuffer(GL_ARRAY_BUFFER,s.vbo);glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(vertices[0]),vertices.data(),GL_DYNAMIC_DRAW);fo3hudassets::UploadVertices(s.vao,vertices.data(),vertices.size());s.count=GLsizei(vertices.size());s.text=text;
}
inline void Prepare(const fo3player::Player& p,fo3sneak::Indicator indicator){
 static const char* keys[]={"sSneakHidden","sSneakDetected","sSneakCaution","sSneakDanger"};
 const auto original=p.Definitions().strings.find(keys[unsigned(indicator)]);
 PrepareLabel(p.IsSneaking(),original==p.Definitions().strings.end()?fo3sneak::Label(unsigned(indicator)):original->second);
}
inline void Render(const float* mvp){auto& s=Get();if(!s.ready||!s.active||!mvp)return;auto& font=fo3hudrenderer::State();fo3hudrenderer::CachedStateGuard guard;
 glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glUseProgram(font.program);glUniformMatrix4fv(font.mvpLoc,1,GL_FALSE,mvp);glUniform4f(font.tintLoc,fo3uicolor::Main[0],fo3uicolor::Main[1],fo3uicolor::Main[2],1);glUniform1i(font.texLoc,0);glActiveTexture(GL_TEXTURE0);glBindVertexArray(s.vao);
 glBindTexture(GL_TEXTURE_2D,s.bracket.texture);glDrawArrays(GL_TRIANGLES,0,6);glBindTexture(GL_TEXTURE_2D,font.fontTexture);glDrawArrays(GL_TRIANGLES,6,s.count-6);
}
inline void Shutdown(){auto& s=Get();if(s.vao)glDeleteVertexArrays(1,&s.vao);if(s.vbo)glDeleteBuffers(1,&s.vbo);if(s.bracket.ownsTexture&&s.bracket.texture)glDeleteTextures(1,&s.bracket.texture);s={};}
}
