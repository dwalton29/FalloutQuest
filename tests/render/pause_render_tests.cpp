#include "ui/pause/fo3-pause-renderer.h"
#include <cassert>
#include <iostream>
bool LoadFalloutTextureRgba(const std::string&,Fo3RgbaTexture&){assert(false&&"warm pause must not decode textures");return false;}
namespace fo3assets {bool LoadTextureFile(const std::string&,std::vector<uint8_t>&,BsaFileInfo*,size_t){assert(false&&"warm pause must not load font files");return false;}}
int main(){
  auto& font=fo3hudrenderer::State();font.attempted=font.ready=true; font.program=30;font.fontTexture=31;font.textureCount=1;font.baseLine=20;
  for(auto& glyph:font.glyphs){glyph.width=10;glyph.height=20;glyph.uv={0,0,1,0,0,1,1,1};}
  auto& r=fo3pauseui::State();r.attempted=true;r.vao=40;r.vbo=50;r.solid=41;r.art=42;r.darken=43;
  fo3debug::Menu menu;menu.panel.visible=true;menu.panel.revision=1;menu.panel.choices={"Continue","Save","Load","Settings","Quit","Debug"};
  fqgl::Invalidate();glUseProgram(91);glBindVertexArray(92);glBindBuffer(GL_ARRAY_BUFFER,93);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,94);
  glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glDisable(GL_BLEND);glDepthMask(GL_TRUE);
  fo3pauseui::Prepare(menu);const auto uploads=driverCalls["glBufferData"];assert(uploads>0&&r.draws.size()>8);
  const float mvp[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  fo3pauseui::Prepare(menu);
  for(int eye=0;eye<2;++eye){fo3pauseui::Render(mvp);assert(fqgl::state.program.value==91&&fqgl::state.vao.value==92&&fqgl::state.texture2d[0].value==94);assert(fqgl::state.depth.value&&fqgl::state.depthWrite.value&&fqgl::state.cull.value&&!fqgl::state.blend.value);}
  assert(driverCalls["glBufferData"]==uploads);menu.panel.revision++;menu.selected=5;fo3pauseui::Prepare(menu);assert(driverCalls["glBufferData"]==uploads+1);
  std::cout<<"Pause stereo reuse and GL state restoration passed\n";
}
