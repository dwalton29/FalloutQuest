#pragma once
#include "fo3-title-layout.h"
#include "ui/loading/fo3-loading-pose.h"
#include "ui/menu/fo3-menu-player.h"
namespace fo3titleui {
inline fo3menuui::Document document;
inline fo3menuui::Surface background;
inline fo3menu::Layout backgroundLayout;
inline bool backgroundAttempted = false, anchored = false;
inline int backgroundDimmed = -1;
inline fo3loadingpose::Matrix anchor;
inline void Recenter() { anchored = false; }
inline void Head(float x, float y, float z, float qx, float qy, float qz,
                 float qw) {
  if (!anchored) {
    anchor = fo3loadingpose::Anchor(x, y, z, qx, qy, qz, qw);
    anchored = true;
  }
}
inline void Prepare(const fo3title::Model &model) {
  if (!backgroundAttempted) {
    backgroundAttempted = true;
    // The verified original static background is 1024x512. Preserve its
    // aspect on the 1280x960 menu canvas; no invented 3D environment.
    auto read = [](const std::string &p, std::string &out) {
      if (p == "menus\\title-background.xml") {
        out = "<menu name='Background'><image "
              "name='Art'><filename>Interface\\Main\\main_background.dds</"
              "filename><x>0</x><y>160</y><width>1280</width><height>640</"
              "height><systemcolor>0</systemcolor></image></menu>";
        return true;
      }
      return fo3menuui::Read(p, out);
    };
    backgroundLayout.width = 1280;
    backgroundLayout.height = 960;
    fo3menuui::Measure(backgroundLayout);
    if (backgroundLayout.Load("menus\\title-background.xml", read) &&
        !background.Build(backgroundLayout))
      __android_log_print(
          ANDROID_LOG_WARN, "FalloutQuest",
          "TITLE missing original Interface/Main/main_background.dds");
  }
  const int dimmed = model.page != fo3title::Model::Page::Main;
  if (backgroundAttempted && backgroundDimmed != dimmed) {
    backgroundDimmed = dimmed;
    backgroundLayout.Set("Art", "brightness", dimmed ? 46. : 255.);
    background.Build(backgroundLayout);
  }
  if (document.Ensure("menus\\options\\start_menu.xml", 1280, 960) &&
      document.revision != model.revision) {
    document.revision = model.revision;
    fo3title::Bind(document.layout, model);
    document.Build();
  }
}
inline void Render(const float *vp) {
  if (!anchored)
    return;
  fo3loadingpose::Matrix view;
  std::copy(vp, vp + 16, view.m);
  auto m = fo3loadingpose::Multiply(view, anchor);
  // A comfortable 2.56 x 1.92 metre screen at 2.5 metres, fixed in tracking
  // space until recentered; both eyes share the same geometry and anchor.
  for (int i = 0; i < 12; ++i)
    m.m[i] *= 2;
  fo3menuui::World(background, backgroundLayout, m.m);
  fo3menuui::World(document.surface, document.layout, m.m);
}
inline void Shutdown() {
  document.surface.Shutdown();
  document = {};
  background.Shutdown();
  backgroundLayout = {};
  backgroundAttempted = false;
  anchored = false;
  backgroundDimmed = -1;
}
} // namespace fo3titleui
