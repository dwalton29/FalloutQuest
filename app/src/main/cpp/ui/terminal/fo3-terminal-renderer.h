#pragma once
#include "fo3-terminal-state.h"
#include "fo3-terminal-surface.h"
#include "world/fo3-scene-preparation.h"
#include "ui/interaction/fo3-interaction-hud-renderer.h"
#include "audio/fo3-audio-assets.h"
#include "data/fo3-install-paths.h"
#include <fstream>
namespace fo3terminalui {
struct Assets {fo3hudassets::Font font;std::array<std::vector<std::string>,13> dictionary;};
inline fo3scene::Preparation<Assets> preparation;
inline Assets assets;inline bool started=false,ready=false,failed=false;
inline GLuint vao=0,vbo=0,solid=0,fontTexture=0;inline GLsizei background=0,highlight=0,text=0;
inline uint64_t revision=UINT64_MAX;inline const fo3terminal::Surface* surface=nullptr;
inline void Advance(){
  if(!started){started=true;preparation.Start([](Assets& out,const std::atomic<bool>& cancelled){
    if(!fo3hudassets::ParseFont(out.font,"Textures\\Fonts\\Fixedsys_Comp_uniform_width.fnt")||cancelled.load())return false;
    std::vector<uint8_t> dictionary;
    auto loose=fo3audio::LooseAt(fo3assets::FalloutDataRoot(),"menus/falloutdict.txt");
    if(!loose.empty()){std::ifstream f(loose,std::ios::binary);dictionary.assign(std::istreambuf_iterator<char>(f),{});}
    if(dictionary.empty())fo3assets::GetBsaArchive(fo3assets::FalloutDataPath("Fallout - Misc.bsa"))->Read("menus/falloutdict.txt",dictionary,nullptr,fo3assets::BsaPathKind::Exact,1024*1024);
    if(dictionary.empty()||cancelled.load())return false;
    out.dictionary=fo3hacking::Dictionary(std::string(dictionary.begin(),dictionary.end()));return true;
  });}
  if(ready||failed||!preparation.Ready())return;
  if(!preparation.Successful()){failed=true;__android_log_print(ANDROID_LOG_ERROR,"FalloutQuest","TERMINAL ASSET MISS: original Fixedsys font or Menus/FalloutDict.txt");return;}
  assets=std::move(preparation.Get());preparation.Reset();
  fo3hudrenderer::GlStateGuard guard;
  glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);const uint8_t white[]={255,255,255,255};solid=fo3hudassets::UploadTexture(1,1,white);
  fontTexture=fo3hudassets::UploadTexture(assets.font.textureWidth,assets.font.textureHeight,assets.font.rgba.data());ready=vao&&vbo&&solid&&fontTexture;
}
inline fo3scene::Preparation<Fo3RgbaTexture> imagePreparation;
inline std::string imageRequest;inline GLuint imageTexture=0;inline int imageWidth=0,imageHeight=0;
inline GLsizei imageFirst=0,imageCount=0;
inline void AdvanceImage(const std::string& request){
  if(request!=imageRequest){
    if(imagePreparation.Active()&&!imagePreparation.TryReset())return;
    if(imageTexture)glDeleteTextures(1,&imageTexture);imageTexture=0;imageWidth=imageHeight=0;
    imageRequest=request;revision=UINT64_MAX;
    if(!request.empty())imagePreparation.Start([request](Fo3RgbaTexture& texture,const std::atomic<bool>& cancel){return !cancel.load()&&LoadFalloutTextureRgba(request,texture)&&!cancel.load();});
  }
  if(imagePreparation.Ready()){
    if(imagePreparation.Successful()){
      const auto& texture=imagePreparation.Get();imageWidth=texture.width;imageHeight=texture.height;
      fo3hudrenderer::GlStateGuard guard;imageTexture=fo3hudassets::UploadTexture(texture.width,texture.height,texture.rgba.data());
    }else __android_log_print(ANDROID_LOG_ERROR,"FalloutQuest","TERMINAL IMAGE MISS: %s",imageRequest.c_str());
    imagePreparation.Reset();revision=UINT64_MAX;
  }
}
inline void Shutdown(){preparation.Reset();imagePreparation.Reset();if(imageTexture)glDeleteTextures(1,&imageTexture);imageTexture=0;imageRequest.clear();if(vbo)glDeleteBuffers(1,&vbo);if(vao)glDeleteVertexArrays(1,&vao);
  if(solid)glDeleteTextures(1,&solid);if(fontTexture)glDeleteTextures(1,&fontTexture);
  vao=vbo=solid=fontTexture=0;assets={};started=ready=failed=false;revision=UINT64_MAX;surface=nullptr;}
inline void Invalidate(){revision=UINT64_MAX;surface=nullptr;}
inline void Render(const float* mvp,const fo3terminal::Surface& screen,const fo3terminal::Display& display,bool amber=false,bool imageVisible=false){
  if(!ready||!screen.Ready()||!fo3hudrenderer::EnsureResources())return;
  fo3hudrenderer::GlStateGuard guard;
  if(surface!=&screen||revision!=display.revision){surface=&screen;revision=display.revision;
    std::vector<fo3hudassets::Vertex> vertices;
    for(auto i:screen.triangles){auto p=fo3terminal::Add(screen.points[i],fo3terminal::Scale(screen.normal,.001f));vertices.push_back({p[0],p[1],p[2],0,0});}
    background=GLsizei(vertices.size());
    auto quad=[&](float x0,float y0,float x1,float y1,const std::array<float,8>& uv,float depth){
      const auto a=screen.At(x0,y0,depth),b=screen.At(x1,y0,depth),c=screen.At(x0,y1,depth),d=screen.At(x1,y1,depth);
      auto emit=[&](const auto& p,int u){vertices.push_back({p[0],p[1],p[2],uv[size_t(u)],uv[size_t(u+1)]});};
      emit(a,0);emit(c,4);emit(b,2);emit(b,2);emit(c,4);emit(d,6);
    };
    constexpr float cw=.90f/fo3terminal::Display::Columns,ch=.90f/fo3terminal::Display::Rows;
    for(int y=0;y<fo3terminal::Display::Rows;++y)for(int x=0;x<fo3terminal::Display::Columns;++x)if(display.cells[size_t(y*fo3terminal::Display::Columns+x)].selected)
      quad(.05f+x*cw,.05f+y*ch,.05f+(x+1)*cw,.05f+(y+1)*ch,{0,0,1,0,0,1,1,1},.0018f);
    highlight=GLsizei(vertices.size())-background;
    imageFirst=GLsizei(vertices.size());
    if(imageVisible&&imageTexture&&imageHeight>0){
      const float aspect=float(imageWidth)/imageHeight;float w=.85f,h=w*screen.width/(aspect*screen.height);
      if(h>.75f){h=.75f;w=h*aspect*screen.height/screen.width;}
      quad(.5f-w/2,.52f-h/2,.5f+w/2,.52f+h/2,{0,0,1,0,0,1,1,1},.002f);
    }
    imageCount=GLsizei(vertices.size())-imageFirst;
    const auto& font=assets.font;const float glyphWidth=std::max(1.f,fo3font::GlyphRenderAdvance(font.glyphs['M']));
    for(int y=0;y<fo3terminal::Display::Rows;++y)for(int x=0;x<fo3terminal::Display::Columns;++x){
      const auto& cell=display.cells[size_t(y*fo3terminal::Display::Columns+x)];if(cell.glyph==' ')continue;
      const auto& g=font.glyphs[fo3font::GlyphIndex(uint8_t(cell.glyph))];if(!fo3font::Drawable(font,g))continue;
      auto q=fo3font::GlyphQuad(font,g,0,0);const float x0=.05f+x*cw,y0=.05f+y*ch;
      quad(x0+q.left/glyphWidth*cw,y0+q.top/font.baseLine*ch,x0+q.right/glyphWidth*cw,y0+q.bottom/font.baseLine*ch,g.uv,.0025f);
    }
    text=GLsizei(vertices.size())-background-highlight-imageCount;
    glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(vertices[0]),vertices.data(),GL_DYNAMIC_DRAW);
    fo3hudassets::UploadVertices(vao,vertices.data(),vertices.size());
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(vertices[0]),nullptr);
    glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(vertices[0]),reinterpret_cast<const void*>(3*sizeof(float)));
  }
  auto& program=fo3hudrenderer::State();
  glEnable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);
  glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(program.program);glUniformMatrix4fv(program.mvpLoc,1,GL_FALSE,mvp);glUniform1i(program.texLoc,0);glActiveTexture(GL_TEXTURE0);glBindVertexArray(vao);
  glBindTexture(GL_TEXTURE_2D,solid);glUniform4f(program.tintLoc,.005f,.009f,.006f,1);glDrawArrays(GL_TRIANGLES,0,background);
  const float r=amber?1.f:33.f/255,g=amber?67.f/255:231.f/255,b=amber?42.f/255:121.f/255;
  glUniform4f(program.tintLoc,r,g,b,.35f);glDrawArrays(GL_TRIANGLES,background,highlight);
  if(imageCount){glBindTexture(GL_TEXTURE_2D,imageTexture);glUniform4f(program.tintLoc,1,1,1,1);glDrawArrays(GL_TRIANGLES,imageFirst,imageCount);}
  glBindTexture(GL_TEXTURE_2D,fontTexture);glUniform4f(program.tintLoc,r,g,b,1);glDrawArrays(GL_TRIANGLES,background+highlight+imageCount,text);
}
}
