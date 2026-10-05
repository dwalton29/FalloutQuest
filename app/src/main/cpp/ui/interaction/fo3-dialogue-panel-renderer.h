#pragma once
#include "dialogue/fo3-dialogue-panel.h"
#include "fo3-interaction-hud-renderer.h"
namespace fo3dialogueui {
struct Resources {
  GLuint vao=0,vbo=0,solid=0;uint64_t revision=UINT64_MAX;size_t selected=SIZE_MAX;
  GLsizei textStart=0,textCount=0;bool highlight=false;
};
inline Resources& State(){static Resources r;return r;}
inline void Shutdown(){auto& r=State();if(r.vbo)glDeleteBuffers(1,&r.vbo);if(r.vao)glDeleteVertexArrays(1,&r.vao);if(r.solid)glDeleteTextures(1,&r.solid);r={};}
inline void Render(const float* mvp,const fo3dialogue::Panel& panel) {
  if(!panel.visible||!mvp)return;
  fo3hudrenderer::GlStateGuard guard;glActiveTexture(GL_TEXTURE0);
  if(!fo3hudrenderer::EnsureResources())return;
  auto& font=fo3hudrenderer::State();auto& r=State();
  if(!r.vao){glGenVertexArrays(1,&r.vao);glGenBuffers(1,&r.vbo);const uint8_t white[]{255,255,255,255};r.solid=fo3hudassets::UploadTexture(1,1,white);}
  if(!r.vao||!r.vbo||!r.solid)return;
  if(r.revision!=panel.revision||r.selected!=panel.selected) {
    using fo3hudrenderer::AppendPromptQuad;
    struct Line {std::string text;float y;};std::vector<Line> lines;
    const float scale=.023f/std::max(1.f,font.baseLine),rowStep=.03f,width=.66f;
    float y=0;float selectedTop=0,selectedBottom=0;
    auto append=[&](const std::string& text){for(auto& line:fo3dialogue::Wrap(font,text,width/scale)){lines.push_back({line,y});y-=rowStep;}};
    append(panel.title);y-=.01f;
    if(!panel.subtitle.empty()){append(panel.subtitle);y-=.01f;}
    const size_t start=panel.selected>=4?panel.selected-3:0;
    for(size_t i=start;i<panel.choices.size()&&i<start+4;++i){
      if(i==panel.selected)selectedTop=y+.007f;
      append(panel.choices[i]);if(i==panel.selected)selectedBottom=y+.002f;y-=.012f;
    }
    append(panel.choices.empty()?"B Exit":"A Select    B Exit");
    std::vector<fo3hudassets::Vertex> vertices;
    AppendPromptQuad(vertices,-.35f,.018f,.35f,y-.01f,0,0,0,1,0,0,1,1,1);
    r.highlight=!panel.choices.empty();
    if(r.highlight)AppendPromptQuad(vertices,-.34f,selectedTop,.34f,selectedBottom,.001f,0,0,1,0,0,1,1,1);
    r.textStart=GLsizei(vertices.size());
    for(auto& line:lines)fo3font::AppendText(font,line.text,0,0,[&](uint8_t,const fo3font::Glyph& g,const fo3font::Quad& q){
      AppendPromptQuad(vertices,-.33f+q.left*scale,line.y-q.top*scale,-.33f+q.right*scale,line.y-q.bottom*scale,.002f,
        g.uv[0],g.uv[1],g.uv[2],g.uv[3],g.uv[4],g.uv[5],g.uv[6],g.uv[7]);
    });
    r.textCount=GLsizei(vertices.size())-r.textStart;
    glBindVertexArray(r.vao);glBindBuffer(GL_ARRAY_BUFFER,r.vbo);
    glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(fo3hudassets::Vertex),vertices.data(),GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),nullptr);
    glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),reinterpret_cast<const void*>(3*sizeof(float)));
    r.revision=panel.revision;r.selected=panel.selected;
  }
  glEnable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glEnable(GL_BLEND);
  glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(font.program);glUniformMatrix4fv(font.mvpLoc,1,GL_FALSE,mvp);glUniform1i(font.texLoc,0);glBindVertexArray(r.vao);
  glBindTexture(GL_TEXTURE_2D,r.solid);glUniform4f(font.tintLoc,.015f,.035f,.025f,.87f);glDrawArrays(GL_TRIANGLES,0,6);
  if(r.highlight){glUniform4f(font.tintLoc,26.f/255,1,128.f/255,.22f);glDrawArrays(GL_TRIANGLES,6,6);}
  glBindTexture(GL_TEXTURE_2D,font.fontTexture);glUniform4f(font.tintLoc,26.f/255,1,128.f/255,1);glDrawArrays(GL_TRIANGLES,r.textStart,r.textCount);
}
}
