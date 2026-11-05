#include "data/fo3-bsa-archive.h"
#include "ui/title/fo3-title-layout.h"
#include "ui/title/fo3-title-save-pose.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <unistd.h>
using namespace fo3title;
static void Navigation() {
  Model m;
  auto silent = [](const char *) {};
  assert(m.Input(true, false, 0, 0, 1, true, silent) == Model::Action::None);
  m.Input(false, false, 0, 0, 2, true, silent);
  assert(m.neutral);
  assert(m.Input(true, false, 0, 0, 3, true, silent) == Model::Action::None);
  assert(m.page == Model::Page::Notice);
  m.Back();
  m.saves = {{"old.fqps", "Old", 1}, {"new.fqps", "New", 2}};
  m.Input(false, false, 0, 0, 4, true, silent);
  assert(m.Input(true, false, 0, 0, 5, true, silent) ==
         Model::Action::Continue);
  assert(m.loadPath == "old.fqps");
  assert(m.Input(true, false, 0, 0, 6, true, silent) == Model::Action::None);
  m.Input(true, false, 0, 0, 7, false, silent);
  assert(!m.neutral);
  assert(m.Input(true, false, 0, 0, 8, true, silent) == Model::Action::None);
  m.Input(false, false, 0, 0, 9, true, silent);
  m.selected = 1;
  m.Confirm();
  assert(m.page == Model::Page::ConfirmNew);
  m.selected = 1;
  m.Confirm();
  assert(m.page == Model::Page::Main);
  m.selected = 1;
  m.Confirm();
  assert(m.Confirm() == Model::Action::NewGame);
  m.Go(Model::Page::Audio);
  for (int i = 0; i < 50; ++i)
    m.Adjust(-1);
  assert(m.volume[0] == 0);
  for (int i = 0; i < 50; ++i)
    m.Adjust(1);
  assert(m.volume[0] == 1);
  m.selected = 4;
  assert(m.Confirm() == Model::Action::Volume);
  assert(m.volume[1] == .3f);
  m.Go(Model::Page::Load);
  m.selected = 1;
  assert(m.Confirm() == Model::Action::Continue);
  assert(m.loadPath == "new.fqps");
  m.Go(Model::Page::Main);
  m.selected = 5;
  m.Confirm();
  assert(m.selected == 1);
  assert(m.Confirm() == Model::Action::None);
}
static void Persistence() {
  char pattern[] = "/tmp/fq-title-XXXXXX";
  const char *root = mkdtemp(pattern);
  assert(root);
  auto save = std::string(root) + "/player-state.fqps";
  {
    std::ofstream f(save, std::ios::binary);
    f.write("FQPS1234567890123456", 20);
  }
  auto entries = Saves(root);
  assert(entries.size() == 1 && entries[0].path == save);
  auto newPath = NewPath(root, 123);
  assert(newPath != save);
  {
    std::ofstream f(newPath);
    f << "separate state";
  }
  assert(NewPath(root, 123) != newPath);
  assert(Saves(root).size() == 2);
  Pose p{0x123, 0x3c, 12, 34, 56, .8f, true}, out;
  assert(WritePose(save, p));
  assert(ReadPose(save, out));
  assert(out.cell == p.cell && out.x == p.x && out.z == p.z);
  // Changing the canonical save invalidates an older arrival even if a crash
  // occurs between the two atomic replacements. It must never teleport into
  // a location from a different revision/playthrough.
  {
    std::fstream f(save, std::ios::in | std::ios::out | std::ios::binary);
    f.seekp(16);
    f.put('X');
  }
  assert(!ReadPose(save, out));
  Model m;
  m.volume = {.5f, .2f, .7f, .8f};
  assert(WriteVolumes(m, root));
  Model loaded;
  ReadVolumes(loaded, root);
  assert(std::abs(loaded.volume[3] - .8f) < .001f);
  {
    std::ofstream f(std::string(root) + "/menu-audio.cfg");
    f << "nan 1 1 1";
  }
  auto before = loaded.volume;
  ReadVolumes(loaded, root);
  assert(loaded.volume == before);
  for (const auto &name : {save, save + ".arrival", newPath,
                           std::string(root) + "/menu-audio.cfg"})
    std::remove(name.c_str());
  rmdir(root);
}
static void Original(const char *path) {
  fo3assets::BsaArchive archive(path);
  fo3menu::Layout l;
  l.width = 1280;
  l.height = 960;
  auto read = [&](const std::string &p, std::string &out) {
    std::vector<uint8_t> b;
    if (!archive.Read(p, b))
      return false;
    out.assign(b.begin(), b.end());
    return true;
  };
  assert(l.Load("menus\\options\\start_menu.xml", read));
  l.measure = [](int, const std::string &text, float width) {
    return fo3menu::Box{
        0, 0, std::min(float(text.size() * 16), width > 0 ? width : 100000.f),
        32};
  };
  l.imageSize = [](const std::string &, const std::string &) {
    return fo3menu::Box{0, 0, 256, 64};
  };
  Model m;
  m.saves = {{"a.fqps", "Current game", 1}};
  for (auto page :
       {Model::Page::Main, Model::Page::Load, Model::Page::Settings,
        Model::Page::Audio, Model::Page::Controls, Model::Page::ConfirmNew,
        Model::Page::ConfirmQuit, Model::Page::Notice, Model::Page::Credits}) {
    m.Go(page);
    m.notice = "Load failed";
    m.credits = {"Original credits"};
    Bind(l, m);
    auto draw = l.Paint();
    bool text = false, title = false;
    for (const auto &d : draw) {
      if (d.type == "text" && !d.text.empty())
        text = true;
      if (d.file.find("main_title.dds") != std::string::npos)
        title = true;
    }
    if (!text) {
      std::cerr << "No text for page " << int(page) << "\n";
      assert(false);
    }
    assert(l.N("pause_background", "visible") == 0);
    assert(l.N("main_press_start", "visible") == 0);
    assert(l.Unsupported().empty());
    if(page==Model::Page::ConfirmNew){
      bool warning=false;
      for(const auto& d:draw)if(d.node==l.Find("confirm_warning"))warning=d.alpha>0&&d.clip.w>0&&d.clip.h>0;
      assert(warning);
    }
    if (page == Model::Page::Main)
      assert(title);
  }
  // Repeat bindings cannot accumulate dynamic rows; all entries remain in
  // the viewport, and selecting long lists scrolls the authored highlight.
  m.Go(Model::Page::Load);
  for (int i = 0; i < 30; ++i)
    m.saves.push_back({"slot", "Game " + std::to_string(i), i});
  m.selected = 25;
  Bind(l, m);
  auto count = l.Nodes().size();
  l.Paint();
  Bind(l, m);
  assert(count == l.Nodes().size());
  assert(l.N(l.Find("lb_scrollbar", l.Find("main_container")),
             "_current_value") > 0);
}
int main(int argc, char **argv) {
  Navigation();
  Persistence();
  if (argc > 1)
    Original(argv[1]);
  std::cout << "Title menu checks passed\n";
}
