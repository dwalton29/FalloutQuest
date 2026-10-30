#include <GLES3/gl3.h>
inline GLuint nextMenuObject = 300;
inline void glGenVertexArrays(GLsizei n, GLuint *ids) {
  ++driverCalls["glGenVertexArrays"];
  while (n--)
    *ids++ = nextMenuObject++;
}
inline void glGenBuffers(GLsizei n, GLuint *ids) {
  ++driverCalls["glGenBuffers"];
  while (n--)
    *ids++ = nextMenuObject++;
}
#include "ui/pipboy/fo3-pipboy-renderer.h"
#include <cassert>
#include <fstream>
#include <iostream>
bool LoadFalloutTextureRgba(const std::string &, Fo3RgbaTexture &) {
  return false;
}
namespace fo3assets {
bool LoadTextureFile(const std::string &, std::vector<uint8_t> &, BsaFileInfo *,
                     size_t) {
  return false;
}
} // namespace fo3assets
namespace fo3assets {
std::shared_ptr<BsaArchive> GetBsaArchive(const std::string &) { return {}; }
std::string FalloutDataPath(const std::string &filename) { return filename; }
const std::array<std::string, 2> &TextureArchivePaths() {
  static const std::array<std::string, 2> paths{"mock-textures",
                                                "mock-fallback"};
  return paths;
}
} // namespace fo3assets
int main(int argc, char **argv) {
  fo3player::Catalog catalog;
  std::string error;
  if (argc > 3)
    assert(fo3player::LoadCatalog(argv[3], catalog, error));
  fo3player::Player p(std::move(catalog));
  fo3pip::Menu menu;
  menu.tab = fo3pip::Tab::Data;
  menu.page = 2;
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
  for (auto &glyph : resources.fonts[0].glyphs)
    glyph.width = 10;
  auto wrapped = fo3pipui::Wrap("abc def\r\nghi", 35);
  assert(wrapped.size() == 3 && wrapped[0] == "abc" && wrapped[1] == "def" &&
         wrapped[2] == "ghi");
  const std::string longNote(10000, 'x');
  wrapped = fo3pipui::Wrap(longNote, 100);
  size_t retained = 0;
  for (auto &line : wrapped)
    retained += line.size();
  assert(retained == longNote.size());
  auto &document = fo3menuui::PipDocument(2);
  document.attempted = document.ready = true;
  auto original =
      argc > 1 ? std::make_shared<fo3assets::BsaArchive>(argv[1]) : nullptr;
  assert(document.layout.Load(
      "menus\\main\\map_menu.xml",
      [&](const std::string &path, std::string &out) {
        if (original) {
          std::vector<uint8_t> bytes;
          if (!original->Read(path, bytes))
            return false;
          out.assign(bytes.begin(), bytes.end());
          return true;
        }
        out = path == "menus\\globals.xml"
                  ? "<rect name='Strings'/>"
                  : "<menu name='MapMenu'><rect name='MM_Tabline'/></menu>";
        return true;
      }));
  fo3menuui::Measure(document.layout);
  fo3menuui::Shared().program = 35;
  for (int index : {2, 4, 6, 7}) {
    auto &font = fo3menuui::Shared().fonts[index];
    font.texture = GLuint(100 + index);
    font.metrics.baseLine = 31;
    for (auto &glyph : font.metrics.glyphs) {
      glyph.width = 18;
      glyph.height = 24;
      glyph.topEdge = 24;
      glyph.spacing = 2;
      glyph.uv = {0, 0, 1, 0, 0, 1, 1, 1};
    }
    font.metrics.textureCount = 1;
    if (argc > 2) {
      const char *files[]{
          "monofonto_large.fnt", "monofonto_verylarge02_dialogs2.fnt",
          "glow_monofonto_vl_dialogs.fnt", "baked-in_monofonto_large.fnt"};
      int ordinal = index == 2 ? 0 : index == 4 ? 1 : index == 6 ? 2 : 3;
      std::ifstream input(std::string(argv[2]) + "/" + files[ordinal],
                          std::ios::binary);
      std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
      assert(fo3font::ParseFalloutFont(bytes, font.metrics));
      fo3font::ApplyFalloutLoadSemantics(font.metrics);
    }
  }
  // Real image decoding is covered separately; this fixture checks authored
  // layout binding, glyph geometry, clipping and GL state, with resident IDs.
  if (original) {
    auto commands = document.layout.Paint();
    for (const auto &command : commands)
      if (!command.file.empty())
        fo3menuui::Shared().textures[command.atlas + "|" + command.file] = {
            200};
  }
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
  if (original)
    assert(!document.surface.vertices.empty() &&
           !document.surface.batches.empty());
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
  const auto beforeClock = fo3pip::PresentationRevision(p);
  assert(p.AdvanceGameClock(1) &&
         fo3pip::PresentationRevision(p) == beforeClock);
  if (original) {
    auto &conversation = fo3menuui::DialogueDocument();
    conversation.attempted = conversation.ready = true;
    assert(conversation.layout.Load(
        "menus\\dialog\\dialog_menu.xml",
        [&](const std::string &path, std::string &out) {
          std::vector<uint8_t> bytes;
          if (!original->Read(path, bytes))
            return false;
          out.assign(bytes.begin(), bytes.end());
          return true;
        }));
    fo3menuui::Measure(conversation.layout);
    fo3dialogue::Panel panel;
    panel.visible = true;
    panel.title = "Speaker";
    panel.subtitle = "Original response";
    panel.choices = {"First original topic", "Second original topic"};
    panel.selected = 0;
    panel.revision = 1;
    const float identity[]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    fo3menuui::Conversation(identity, panel, p);
    assert(conversation.selection == 0 &&
           !conversation.surface.vertices.empty());
    panel.selected = 1;
    fo3menuui::Conversation(identity, panel, p);
    assert(conversation.selection == 1);
    auto &layout = conversation.layout;
    assert(layout.N(layout.Find("DM_SpeakerText"), "visible") == 0);
    const auto count = driverCalls["glBufferData"];
    fo3menuui::Conversation(identity, panel, p);
    assert(driverCalls["glBufferData"] == count);
    panel.title = "Another speaker";
    fo3menuui::Conversation(identity, panel, p);
    assert(layout.S("DM_SpeakerNameLabel", "string") == panel.title);
    panel.choices.clear();
    panel.revision++;
    fo3menuui::Conversation(identity, panel, p);
    assert(layout.N(layout.Find("DM_SpeakerText"), "visible") == 1);
  }
  if (original && argc > 3) {
    auto prime = [&](fo3menuui::Document &doc, const char *path, float w = 1024,
                     float h = 768) {
      doc.attempted = doc.ready = true;
      doc.layout.width = w;
      doc.layout.height = h;
      assert(
          doc.layout.Load(path, [&](const std::string &path, std::string &out) {
            std::vector<uint8_t> bytes;
            if (!original->Read(path, bytes))
              return false;
            out.assign(bytes.begin(), bytes.end());
            return true;
          }));
      fo3menuui::Measure(doc.layout);
    };
    const float identity[]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    prime(fo3menuui::PipDocument(0), "menus\\main\\stats_menu.xml");
    prime(fo3menuui::PipDocument(1), "menus\\main\\inventory_menu.xml");
    prime(fo3menuui::MessageDocument(), "menus\\message_menu.xml");
    prime(fo3menuui::BarterDocument(), "menus\\barter_menu.xml", 1280, 1024);
    assert(p.Add(0xf, 1000) && p.Add(0x434f, 1) && p.Add(0x4241, 37));
    assert(p.Definitions().items.at(0x4241).shortName == "10mm");
    for (int tab = 0; tab < 3; ++tab)
      for (int page = 0; page < 5; ++page) {
        menu.tab = fo3pip::Tab(tab);
        menu.page = page;
        menu.revision = UINT64_MAX;
        menu.Refresh(p);
        assert(fo3menuui::Pipboy(menu, p, identity, 99));
        const auto &surface = fo3menuui::PipDocument(tab).surface;
        assert(!surface.vertices.empty());
        for (const auto &v : surface.vertices)
          assert(std::isfinite(v.x) && std::isfinite(v.y));
        assert(fo3menuui::PipDocument(tab).layout.Unsupported().empty());
      }
    // Native map data uses the same projection as gameplay. Original XML
    // controls clipping, selected enlargement and the marker centre offset.
    assert(p.Discover(0x62743));
    const auto &megaton = p.Definitions().pipboy.markers.at(0x62743);
    menu.tab = fo3pip::Tab::Data;
    menu.page = 1;
    menu.map.mapWorld = 0x3c;
    menu.map.playerKnown = true;
    menu.map.player = fo3pipdata::MapProject(
        p.Definitions().pipboy, megaton.world, megaton.x, megaton.y);
    menu.map.centre = menu.map.player;
    menu.selectedMarker = 0x62743;
    menu.travelPrompt = true;
    menu.travelMarker = 0x62743;
    fo3menuui::Pipboy(menu, p, identity);
    auto &map = fo3menuui::PipDocument(2).layout;
    assert(map.N("MM_WorldMap_ClipWindow", "visible") == 1);
    int marker = map.Find("MapMarker");
    assert(marker >= 0 && std::abs(map.N(marker, "width") - 52.5f) < .01f);
    auto markerBox = map.Bounds(marker),
         viewBox = map.Bounds(map.Find("MM_WorldMap_ClipWindow"));
    assert(std::abs(markerBox.x + markerBox.w * .5f - viewBox.x -
                    viewBox.w * .5f) < .1f);
    assert(fo3menuui::MessageDocument()
               .layout.S("MM_MessageText", "string")
               .find("Megaton") != std::string::npos);
    menu.travelPrompt = false;
    menu.tab = fo3pip::Tab::Items;
    menu.page = 0;
    menu.revision = UINT64_MAX;
    menu.Refresh(p);
    fo3menuui::Pipboy(menu, p, identity);
    auto &inventory = fo3menuui::PipDocument(1).layout;
    assert(!menu.rows.empty());
    assert(inventory.S("IM_Headline_PlayerCapsInfo", "_Value") == "1000");
    auto *weapon = fo3pip::Menu::Stack(p, menu.rows.front());
    assert(weapon && p.WeaponDamage(*weapon) == p.WeaponDamage(weapon->id));
    auto ammo = inventory.Find("AmmoInfo", inventory.Find("IM_ItemInfoRect"));
    assert(inventory.S(ammo, "_Title") == "10mm");
    p.RegisterOriginalActors();
    auto moira = p.Snapshot().actors.at(0x2d2bc);
    moira.package = 0x4153;
    assert(p.UpdateActor(0x2d2bc, moira));
    fo3player::BarterSession session;
    assert(p.OpenBarter(0x2d2bc, 0, session));
    fo3menuui::Barter(identity, p, session, true, 0, 20, "");
    auto &barter = fo3menuui::BarterDocument();
    assert(!barter.surface.vertices.empty());
    assert(barter.layout.S("BM_ContainerTitle", "string") == "Moira Brown");
    assert(barter.layout.S("BM_Items_CapsLabel", "string") == "1000");
    const auto &nodes = barter.layout.Nodes();
    auto list = barter.layout.Find("BM_Container_InventoryList");
    size_t count = 0;
    for (auto child : nodes[list].children)
      count += nodes[child].dynamic;
    size_t eligible = 0;
    for (const auto &stack : session.merchant) {
      float price = 0;
      if (p.BarterItemAllowed(session.actor, stack) &&
          p.BarterPrice(stack, true, 0, price))
        ++eligible;
    }
    assert(count == eligible && count > 0 &&
           barter.layout.Unsupported().empty());
    fo3dialogue::Panel message;
    message.visible = true;
    message.title = "Fixture";
    message.subtitle = "Save and return?";
    message.choices = {"Yes", "No"};
    message.revision = 2;
    message.selected = 1;
    fo3menuui::Notice(identity, "Fixture notice", p, false);
    fo3menuui::Message(identity, message, p);
    auto &notice = fo3menuui::MessageDocument();
    assert(notice.layout.width == 1280 && notice.layout.height == 1024);
    assert(notice.layout.S("MM_MessageText", "string") == message.subtitle);
    assert(!notice.surface.vertices.empty() &&
           notice.layout.Unsupported().empty());
    const auto before = fo3pip::PresentationRevision(p);
    assert(p.Add(0x4241, 1) && fo3pip::PresentationRevision(p) != before);
    std::cout << "Original-data Pip-Boy pages, inventory/ammo, Moira barter "
                 "and message bindings passed\n";
  }
  std::cout << "Pip-Boy warm renderer: zero queries; cached GL state restored; "
               "dormant/static frames produce zero draws/uploads\n";
}
