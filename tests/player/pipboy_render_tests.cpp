#include "ui/pipboy/fo3-pipboy-renderer.h"
#include <cassert>
#include <iostream>
bool LoadFalloutTextureRgba(const std::string &, Fo3RgbaTexture &) {
  assert(false && "No asset I/O in a warm DATA render");
  return false;
}
int main() {
  fo3player::Player p(fo3player::Catalog{});
  fo3pip::Menu menu;
  menu.tab = fo3pip::Tab::Data;
  auto &resources = fo3pipui::State();
  resources.ready = true;
  resources.fbo = 10;
  resources.texture = 20;
  resources.program = 30;
  resources.vao = 40;
  resources.vbo = 50;
  resources.white = 60;
  resources.scanlines = 90;
  resources.fontTexture[0] = 70;
  resources.fontTexture[1] = 80;
  // Prime exactly the renderer-owned state and verify it survives UI work.
  fqgl::Invalidate();
  glUseProgram(91);
  glBindVertexArray(92);
  glBindBuffer(GL_ARRAY_BUFFER, 93);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, 94);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glBlendFuncSeparate(1, 2, 3, 4);
  glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
  auto queries = fqgl::counters.queries;
  fo3pipui::Render(menu, p, true);
  assert(resources.redraws == 1);
  assert(fqgl::counters.queries == queries);
  assert(fqgl::state.program.value == 91 && fqgl::state.vao.value == 92 &&
         fqgl::state.arrayBuffer.value == 93);
  assert(fqgl::state.depth.value && fqgl::state.cull.value &&
         !fqgl::state.blend.value && fqgl::state.depthWrite.value);
  auto buffers = driverCalls["glBufferSubData"],
       draws = driverCalls["glDrawArrays"];
  for (int i = 0; i < 100; ++i)
    fo3pipui::Render(menu, p, true);
  assert(resources.redraws == 1 && driverCalls["glBufferSubData"] == buffers &&
         driverCalls["glDrawArrays"] == draws);
  menu.dirty = true;
  for (int i = 0; i < 100; ++i)
    fo3pipui::Render(menu, p, false);
  assert(resources.redraws == 1 && menu.dirty);
  assert(fqgl::counters.queries == queries);
  std::cout << "Pip-Boy warm renderer: zero queries; cached GL state restored; "
               "dormant/static frames produce zero draws/uploads\n";
}
