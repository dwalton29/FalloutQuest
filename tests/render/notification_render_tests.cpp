#include "ui/interaction/fo3-item-notification-renderer.h"
#include <cassert>
#include <iostream>

bool LoadFalloutTextureRgba(const std::string&,Fo3RgbaTexture&){assert(false&&"Warm notice must not load assets");return false;}
namespace fo3assets {bool LoadTextureFile(const std::string&,std::vector<uint8_t>&,BsaFileInfo*,size_t){assert(false&&"Warm notice must not load font files");return false;}}
int main(){
  auto& font=fo3hudrenderer::State();font.attempted=font.ready=true;
  font.program=30;font.fontTexture=31;font.textureCount=1;
  for(auto& glyph:font.glyphs){glyph.width=10;glyph.height=20;glyph.uv={0,0,1,0,0,1,1,1};}
  auto& resources=fo3notifyui::State();resources.vao=40;resources.vbo=50;resources.program=30;
  resources.mvp=0;resources.tint=1;resources.tex=2;resources.age=3;
  fqgl::Invalidate();glUseProgram(91);glBindVertexArray(92);glBindBuffer(GL_ARRAY_BUFFER,93);
  glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,94);
  glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glDisable(GL_BLEND);glDepthMask(GL_TRUE);
  glBlendFuncSeparate(1,2,3,4);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
  auto& queue=fo3notify::Notifications();queue.Advance(100);queue.EnqueueQuest("Quest added\nAuthored title","UIPopUpQuestNew");
  fo3notifyui::Prepare(100,true,true);assert(queue.TakeSound()=="UIPopUpQuestNew");
  fo3notifyui::Prepare(101,true,true);
  const float mvp[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  const auto queries=fqgl::counters.queries;const auto revision=queue.Revision();
  const auto uploads=driverCalls["glBufferData"];
  for(int eye=0;eye<2;++eye){
    const auto draws=driverCalls["glDrawArrays"];fo3notifyui::Render(mvp);
    assert(driverCalls["glDrawArrays"]==draws+1);
    assert(fqgl::state.program.value==91&&fqgl::state.vao.value==92);
    assert(fqgl::state.arrayBuffer.value==93&&fqgl::state.texture2d[0].value==94);
    assert(fqgl::state.depth.value&&fqgl::state.cull.value&&!fqgl::state.blend.value&&fqgl::state.depthWrite.value);
    assert(fqgl::state.srcRgb.value==1&&fqgl::state.dstRgb.value==2);
    assert(queue.Revision()==revision&&queue.TakeSound().empty());
  }
  assert(fqgl::counters.queries==queries&&driverCalls["glBufferData"]==uploads);
  fo3notifyui::Prepare(120,false,true);assert(queue.Alpha()==.5f&&queue.TakeSound().empty());
  std::cout<<"Warm quest HUD: both eyes draw; zero asset reads/queries/uploads; GL state restored; modal pause and single sound passed\n";
}
