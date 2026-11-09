#include "data/fo3-bsa-archive.h"
#include "ui/menu/fo3-menu-appearance-layout.h"
#include "ui/menu/fo3-menu-layout.h"
#include "ui/menu/fo3-menu-vats-layout.h"
#include "ui/menu/fo3-menu-strings.h"
#include "ui/menu/fo3-menu-texture-paths.h"
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
  TexturePaths paths;
  paths.Add("textures/interface/shared/solid.dds");
  paths.Add("textures/interface/shared/solid.dds");
  assert(paths.Resolve("SOLID.DDS") ==
         "textures\\interface\\shared\\solid.dds");
  paths.Add("textures/interface/another/solid.dds");
  assert(paths.Resolve("solid.dds").empty());
  paths.Add("textures/clutter/only.dds");
  assert(paths.Resolve("only.dds").empty());
  assert(paths.Resolve("../solid.dds").empty());
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
          "menus\\dialog\\dialog_menu.xml", "menus\\message_menu.xml",
          "menus\\quantity_menu.xml", "menus\\chargen\\race_sex_menu.xml", "menus\\vats_menu.xml"}) {
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
      if (name == "StatsMenu")
        assert(l.S("stats_title", "string") == "STATS");
      if (name == "QuantityMenu") {
        l.Set("QM_AmountMeter", "user0", 10.);
        l.Set("QM_AmountMeter", "user2", 4.);
        assert(Near(l.N("QM_AmountMeter", "_Value"), .4f));
        assert(l.S("QM_AmountChosen", "string") == "4");
      }
      if (name == "MapMenu") {
        assert(l.N("GLOW_BRANCH", "systemcolor") == 1);
        auto b = l.Bounds(l.Find("MM_LocalMap_ClipWindow"));
        assert(Near(b.w, 855) && Near(b.h, 500));
        assert(l.Instantiate("TabButtonTemplate", l.Find("MM_Tabline")) >= 0);
      }
      if (name == "RaceSexMenu") {
        using fo3chargen::Kind;
        using fo3chargen::Row;
        l.width = 1280;
        l.height = 1080;
        l.imageSize=[](const std::string&,const std::string&){return Box{0,0,20,20};};
        const auto lcd=l.Bounds(l.Find("RSM_Background"));
        assert(Near(lcd.x,930)&&Near(lcd.y,550)&&Near(lcd.w,340)&&Near(lcd.h,500));
        std::vector<Row> rows{{"Preset:",Kind::Preset,0,false,"2",1,5,2},
                              {"Customize >",Kind::Page,0,false,{}},
                              {"Randomize",Kind::Randomize,0,false,{}},
                              {"Back",Kind::Back,0,false,{}},
                              {"Next",Kind::Next,0,false,{}}};
        BindAppearance(l,"3. Face",rows,0,2);
        const auto face=l.Paint();
        for(const auto* label:{"3. Face","Preset:","2","Customize >","Randomize","Back","Next"}){
          const auto text=std::find_if(face.begin(),face.end(),[&](const Draw& d){
            return d.type=="text"&&d.text==label&&d.font==5&&d.colour==TerminalColour&&d.alpha>0;
          });
          if(text==face.end())std::cerr<<"Missing native gene projection text "<<label<<'\n';
          assert(text!=face.end());
        }
        const auto marker=std::find_if(face.begin(),face.end(),[&](const Draw& d){
          return l.Nodes()[d.node].name=="RSM_SM_text"&&d.text=="|"&&d.box.w>0;
        });
        assert(marker!=face.end());
        const auto slider=l.Nodes()[marker->node].parent;
        const auto row=l.Nodes()[slider].parent;
        const auto bar=l.Bounds(l.Find("RSM_slider_bar",row));
        assert(Near(l.Bounds(slider).x+8,bar.x+.25f*bar.w));
        l.Reset();
        rows.clear();
        for(size_t i=0;i<15;++i)rows.push_back({"Race "+std::to_string(i),Kind::Race,i,i==10,{}});
        BindAppearance(l,"2. Race",rows,10,1);
        const auto race=l.Paint();
        assert(l.N("RSM_scroll_up_target","visible")&&l.N("RSM_scroll_down_target","visible"));
        assert(std::any_of(race.begin(),race.end(),[&](const Draw& d){
          return l.Nodes()[d.node].name=="RSM_selection_indicator"&&d.box.w>0;
        }));
      }
      if(name=="VATSMenu"){
        l.width=420;l.height=260;
        const auto head=BindVatsLimb(l,{"Head",73,.6f,true,"up"});
        const auto marker=l.Find("chance_bracket",head);
        assert(l.S(marker,"filename")=="Interface\\VATS\\vats_head_divider.dds");
        assert(l.S(l.Find("chance_to_hit",head),"string")=="73%");
        assert(l.S(l.Find("limb_name",head),"string")=="Head");
        assert(Near(l.Bounds(l.Find("chance_to_hit",head)).x,l.Bounds(l.Find("limb_name",head)).x));
        assert(l.N(l.Find("chance_to_hit",head),"systemcolor")==1);
        assert(Near(l.N(l.Find("meter",head),"_Value"),.6f));
        l.Reset();
        l.width=1280;l.height=900;
        BindVatsChrome(l,"HP 100","AP 58/75",1,1,.23f,"Raider",.7f,{"Shot  Head"},"",true);
        const auto buttonText=[&](const char* name){return l.Find("button_text",l.Find(name));};
        assert(Near(l.Bounds(buttonText("Select_button")).x,l.Bounds(buttonText("accept_button")).x));
        assert(Near(l.Bounds(buttonText("accept_button")).x,l.Bounds(buttonText("return_button")).x));
        assert(l.N(buttonText("accept_button"),"systemcolor")==1);
        const auto vats=l.Paint();
        for(const auto* art:{"glow_general_button_rt.dds","glow_general_button_a.dds","glow_general_button_b.dds"})
          assert(std::any_of(vats.begin(),vats.end(),[&](const auto& d){
            return d.file==art&&d.alpha>0&&d.box.x>=0&&d.box.x+d.box.w<=l.width;
          }));
        for(const auto* text:{"AP 58/75","HP 100","Raider","Shot  Head","Accept","Return"}){
          const bool present=std::any_of(vats.begin(),vats.end(),[&](const auto& d){return d.text==text&&d.alpha>0;});
          if(!present){
            std::cerr<<"Missing original VATS text "<<text<<'\n';
            for(const auto& d:vats)if(d.type=="text")
              std::cerr<<l.Nodes()[d.node].name<<" ["<<d.text<<"] alpha="<<d.alpha<<" xy="<<d.box.x<<","<<d.box.y<<'\n';
          }
          assert(present);
        }
        assert(std::any_of(vats.begin(),vats.end(),[](const auto& d){
          return d.file=="Interface\\VATS\\vats_bar.dds"&&d.box.w>0;
        }));
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
