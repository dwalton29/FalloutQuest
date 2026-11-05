#pragma once
#include "fo3-title-state.h"
#include "ui/menu/fo3-menu-layout.h"
#include "ui/menu/fo3-menu-strings.h"
namespace fo3title {
inline std::string Label(const std::string &key) {
  auto i = fo3menu::StringDefaults().find(key);
  return i == fo3menu::StringDefaults().end() ? key : i->second;
}
// Native bindings for StartMenu's source-defined dynamic list and warning
// slots. Geometry, text styling, highlights and scrolling remain in the
// original XML.
inline void Bind(fo3menu::Layout &l, const Model &m) {
  l.Reset();
  l.strings = [](const std::string &key) -> fo3menu::Value {
    return Label(key);
  };
  l.Set(l.Globals(), "_Has360Controller", 0.);
  l.Set(l.Root(), "alpha", 255.);
  l.Set(l.Root(), "user0", 0.);
  l.Set(l.Root(), "user5", 0.);
  for (const char *name :
       {"pause_background", "main_press_start", "main_version",
        "saveload_screenshot", "saveload_playername", "saveload_playerlevel",
        "saveload_playtime", "main_device_button", "main_defaults_button",
        "main_back_button", "settings_container", "options_container",
        "saveload_container", "confirm_container"})
    l.Set(name, "visible", 0.);
  l.Set("main_title", "visible", 1.);
  l.Set("main_title", "alpha", 255.);
  l.Set("main_container", "visible", 1.);
  l.Set("main_container", "alpha", 255.);
  l.Set("main_container", "_alpha", 255.);
  l.Set("main_container", "_enabled", 1.);
  std::vector<std::string> labels;
  std::string message;
  using Page = Model::Page;
  switch (m.page) {
  case Page::Main:
    for (const char *key : {"sContinue", "sNew", "sLoad", "sSettings"})
      labels.push_back(Label(key));
    labels.push_back("Credits");
    labels.push_back(Label("sQuit"));
    break;
  case Page::Load:
    for (const auto &save : m.saves)
      labels.push_back(save.label);
    break;
  case Page::Settings:
    labels = {Label("sAudio"), Label("sControls"), "Recenter"};
    break;
  case Page::Audio: {
    const char *names[] = {"sMasterVolume", "sMusicVolume", "sEffectsVolume",
                           "sVoiceVolume"};
    for (size_t i = 0; i < 4; ++i)
      labels.push_back(Label(names[i]) + ": " +
                       std::to_string(int(m.volume[i] * 100 + .5f)) + "%");
    labels.push_back(Label("sDefaults"));
    break;
  }
  case Page::Controls:
    message = "Right stick: select / adjust\nA: confirm    B: back\nLeft menu "
              "button: recenter title screen";
    labels = {Label("sBack")};
    break;
  case Page::ConfirmNew:
    message = "Start a new game? Your existing saves will be "
              "kept.\nBegin the original Vault 101 opening. Character creation "
              "and the childhood sequence are still in development.";
    labels = {Label("sYes"), Label("sNo")};
    break;
  case Page::ConfirmQuit:
    message = "Quit FalloutQuest?";
    labels = {Label("sYes"), Label("sNo")};
    break;
  case Page::Notice:
    message = m.notice;
    labels = {Label("sBack")};
    break;
  case Page::Credits:
    for (size_t i = m.creditsPage * 12;
         i < std::min(m.credits.size(), (m.creditsPage + 1) * 12); ++i)
      message += m.credits[i] + "\n";
    labels = {"Previous", "Next", Label("sBack")};
    break;
  }
  const int parent = l.Find("main_container");
  l.Set(parent, "width", m.page == Page::Load ? 460. : 300.);
  l.Set(parent, "_number_of_visible_items",
        double(std::min<size_t>(labels.size(), 8)));
  float offset = 0, chosen = 0;
  const float height = l.N(l.Root(), "_item_height");
  for (size_t i = 0; i < labels.size(); ++i) {
    const int row = l.Instantiate("lb_item_template", parent);
    l.Set(row, "string", labels[i]);
    l.Set(row, "listindex", double(i));
    l.Set(row, "_y", offset);
    l.Set(row, "_enabled",
          double(!(m.page == Page::Main && (i == 0 || i == 2) &&
                   m.saves.empty())));
    l.Set(row, "_selected", double(i == m.selected));
    l.Set(row, "mouseover", double(i == m.selected));
    if (i == m.selected)
      chosen = offset;
    offset += height;
  }
  const double scroll =
      std::max(0.f, chosen + height - float(l.N(parent, "height")));
  l.Set(parent, "_highlight_y", chosen - scroll);
  l.Set(parent, "_selected_height", height);
  l.Set(parent, "_scroll_delta", 1.);
  const int bar = l.Find("lb_scrollbar", parent);
  l.Set(bar, "_current_value", scroll);
  l.Set(bar, "_total_height", offset);
  l.Set(bar, "_number_of_items", double(labels.size()));
  // Reuse the original warning text tile for runtime notices and help. Its
  // position and width here are an explicit VR adaptation for longer text.
  if (!message.empty()) {
    l.Set("confirm_container", "visible", 1.);
    l.Set("confirm_container", "alpha", 255.);
    l.Set("confirm_container", "_alpha", 255.);
    l.Set("confirm_container", "x", 40.);
    l.Set("confirm_container", "y", 160.);
    l.Set("confirm_container", "width", 650.);
    l.Set("confirm_container", "height", 600.);
    l.Set("confirm_question", "visible", 0.);
    l.Set("confirm_warning", "visible", 1.);
    l.Set("confirm_warning", "alpha", 255.);
    l.Set("confirm_warning", "string", message);
    l.Set("confirm_warning", "x", 0.);
    l.Set("confirm_warning", "y", 0.);
    l.Set("confirm_warning", "wrapwidth", 650.);
    l.Set("main_title", "visible", 0.);
  }
}
} // namespace fo3title
