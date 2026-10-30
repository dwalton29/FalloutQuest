#include "data/fo3-bsa-archive.h"
#include "ui/menu/fo3-menu-layout.h"
#include "ui/menu/fo3-menu-strings.h"
#include <cassert>
#include <iostream>
using namespace fo3menu;
static bool Near(float a, float b) { return std::abs(a - b) < .01f; }
static void Synthetic() {
  std::unordered_map<std::string, std::string> files{
      {"menus\\globals.xml",
       "<rect name='Strings'><_pipboy_width>1024</_pipboy_width></rect>"},
      {"menus\\test.xml",
       "<menu name='Test'><user0>0</user0><rect "
       "name='Frame'><locus>&true;</locus><x>50</x><width>800</width><image "
       "name='Rule'><include src='rule.xml'/><width><copy src='parent()' "
       "trait='width'/><sub>20</sub></width></image></rect><template "
       "name='Row'><hotrect name='Row'><text name='Label'><string><copy "
       "src='parent()' "
       "trait='string'/></string></text></hotrect></template></menu>"},
      {"menus\\prefabs\\rule.xml",
       "<width>10</width><height>2</height><visible><copy src='io()' "
       "trait='user0'/><eq>0</eq></visible>"}};
  auto read = [&](const std::string &p, std::string &out) {
    auto i = files.find(p);
    if (i == files.end())
      return false;
    out = i->second;
    return true;
  };
  Layout l;
  assert(l.Load("menus\\test.xml", read));
  int rule = l.Find("Rule"), frame = l.Find("Frame");
  assert(Near(l.N(rule, "width"), 780) && Near(l.Bounds(rule).x, 50));
  l.Set(frame, "width", 500.);
  assert(Near(l.N(rule, "width"), 480));
  l.Set(l.Root(), "user0", 1.);
  assert(!l.N(rule, "visible"));
  auto initial = l.Nodes().size();
  int row = l.Instantiate("Row", frame);
  l.Set(row, "string", "Original topic");
  assert(l.S(l.Find("Label", row), "string") == "Original topic");
  l.Reset();
  assert(l.Nodes().size() == initial && l.N(rule, "visible"));
  l.imageSize = [](const std::string &, const std::string &) {
    return Box{0, 0, 64, 32};
  };
  l.Set(rule, "zoom", 150.);
  assert(Near(l.N(rule, "filewidth"), 96) && Near(l.N(rule, "fileheight"), 48));
  std::vector<Xml> malformed;
  std::string error;
  assert(!Parse("<menu><text></menu>", malformed, error));
  assert(StringDefaults().at("sInventoryItems") == "ITEMS");
}
int main(int argc, char **argv) {
  Synthetic();
  if (argc > 1) {
    fo3assets::BsaArchive archive(argv[1]);
    auto read = [&](const std::string &p, std::string &s) {
      std::vector<uint8_t> b;
      if (!archive.Read(p, b))
        return false;
      s.assign(b.begin(), b.end());
      return true;
    };
    for (const auto *path :
         {"menus\\main\\stats_menu.xml", "menus\\main\\inventory_menu.xml",
          "menus\\main\\map_menu.xml", "menus\\barter_menu.xml",
          "menus\\dialog\\dialog_menu.xml", "menus\\message_menu.xml"}) {
      Layout l;
      assert(l.Load(path, read));
      l.strings = [](const std::string &key) -> Value {
        auto i = StringDefaults().find(key);
        return i == StringDefaults().end() ? Value{} : Value(i->second);
      };
      l.measure = [](int, const std::string &s, float) {
        return Box{0, 0, float(s.size() * 20), 32};
      };
      std::string name = l.Nodes()[l.Root()].name;
      if (name == "InventoryMenu") {
        auto b = l.Bounds(l.Find("IM_ItemIcon"));
        assert(Near(b.x, 500) && Near(b.y, 70) && Near(b.w, 400) &&
               Near(b.h, 400));
        auto row = l.Instantiate("IM_InventoryListTemplate",
                                 l.Find("IM_InventoryList"));
        assert(row >= 0);
        l.Set(row, "string", "10mm Pistol");
        l.Set(row, "listindex", 0.);
        l.Set(row, "height", 60.);
        assert(l.S(l.Find("ListItemText", row), "string") == "10mm Pistol");
      }
      if (name == "DialogMenu") {
        l.width = 1280;
        l.height = 1024;
        l.Set(l.Root(), "_DialogVisible", 1.);
        l.Set(l.Root(), "_ShowingText", 0.);
        auto background = l.Bounds(l.Find("DM_TextBackground"));
        assert(Near(background.x, 100) && Near(background.w, 1080));
        auto row = l.Instantiate("DM_TopicTemplate", l.Find("DM_TopicList"));
        assert(row >= 0);
        l.Set(row, "string", "Where is my father?");
        l.Set(row, "_line_alpha", 255.);
        l.Set(row, "listindex", 0.);
        l.Set(row, "height", 64.);
        assert(l.N(l.Find("ListItemText", row), "font") == 6);
      }
      if (name == "BarterMenu") {
        l.width = 1280;
        l.height = 1024;
        auto b = l.Bounds(l.Find("BM_MainRect"));
        assert(Near(b.x, 128) && Near(b.y, 112) && Near(b.w, 1024) &&
               Near(b.h, 800));
        assert(l.Instantiate("BM_list_template",
                             l.Find("BM_Items_InventoryList")) >= 0);
        assert(l.Instantiate("BM_list_template",
                             l.Find("BM_Container_InventoryList")) >= 0);
      }
      if (name == "StatsMenu") {
        l.Set(l.Root(), "user0", 2.);
        assert(l.N(l.Find("stats_skills_container"), "visible") &&
               !l.N(l.Find("stats_special_container"), "visible"));
        assert(l.Instantiate("stats_list_template",
                             l.Find("stats_skills_container")) >= 0);
      }
      if (name == "MapMenu") {
        assert(l.N("GLOW_BRANCH", "systemcolor") == 1);
        auto b = l.Bounds(l.Find("MM_LocalMap_ClipWindow"));
        assert(Near(b.w, 855) && Near(b.h, 500));
        assert(l.Instantiate("TabButtonTemplate", l.Find("MM_Tabline")) >= 0);
      }
      const auto draw = l.Paint();
      assert(!draw.empty() && l.Unsupported().empty());
      for (const auto &d : draw)
        assert(std::isfinite(d.box.x) && std::isfinite(d.box.y) &&
               std::isfinite(d.box.w) && std::isfinite(d.box.h));
      std::cout << name
                << " original hierarchy, prefabs, expressions and native "
                   "template slots passed\n";
    }
  }
  std::cout << "Menu layout tests passed\n";
}
