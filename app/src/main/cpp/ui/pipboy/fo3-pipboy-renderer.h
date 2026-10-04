#pragma once
#include "fo3-pipboy-state.h"
#include "ui/interaction/fo3-hud-cached-state.h"
#include "ui/interaction/fo3-interaction-hud-renderer.h"
#include <chrono>
#include <unordered_map>
#include <unordered_set>
namespace fo3pipui {
constexpr int Width = 512,
              Height = 384; // original globals.xml 1024x768, half-resolution
struct Resources {
  bool attempted = false, ready = false;
  GLuint fbo = 0, texture = 0, program = 0, vao = 0, vbo = 0, white = 0,
         background = 0, scanlines = 0;
  GLuint fontTexture[2]{};
  fo3hudassets::Font fonts[2];
  GLint mvp = -1, tint = -1, tex = -1;
  fo3hudassets::TaiSprite buttons[2];
  GLuint buttonAtlas = 0;
  uint64_t renderQueries = 0;
  std::unordered_map<std::string, GLuint> art;
  std::unordered_set<std::string> pinnedArt;
  std::vector<fo3hudassets::Vertex> vertices;
  unsigned redraws = 0;
  double renderUs = 0;
};
inline Resources &State() {
  static Resources r;
  return r;
}
inline GLuint Art(const std::string &path, bool pin = false) {
  auto &r = State();
  if (pin)
    r.pinnedArt.insert(path);
  auto found = r.art.find(path);
  if (found != r.art.end())
    return found->second;
  if (r.art.size() >= 48) {
    for (auto it = r.art.begin(); it != r.art.end(); ++it) {
      if (!r.pinnedArt.count(it->first)) {
        if (it->second)
          glDeleteTextures(1, &it->second);
        r.art.erase(it);
        break;
      }
    }
  }
  Fo3RgbaTexture t;
  GLuint tex = 0;
  if (LoadFalloutTextureRgba(path, t))
    tex = fo3hudassets::UploadTexture(t.width, t.height, t.rgba.data());
  r.art.emplace(path, tex);
  return tex;
}
inline bool MenuAssets() {
  auto archive = fo3assets::GetBsaArchive(
      fo3assets::FalloutDataPath("Fallout - Misc.bsa"));
  if (!archive)
    return false;
  for (const auto *path :
       {"menus\\globals.xml", "menus\\main\\stats_menu.xml",
        "menus\\main\\inventory_menu.xml", "menus\\main\\map_menu.xml"}) {
    std::vector<uint8_t> bytes;
    if (!archive->Read(path, bytes))
      return false;
    std::string xml(bytes.begin(), bytes.end());
    if (xml.find(path == std::string("menus\\globals.xml")
                     ? "<_pipboy_width> 1024"
                     : "&pipboy;") == std::string::npos)
      return false;
  }
  return true;
}
inline bool Ensure() {
  auto &r = State();
  if (r.attempted)
    return r.ready;
  r.attempted = true;
  if (!MenuAssets() ||
      !fo3hudassets::ParseFont(r.fonts[0],
                               "Textures\\Fonts\\Monofonto_Large.fnt") ||
      !fo3hudassets::ParseFont(
          r.fonts[1], "Textures\\Fonts\\Monofonto_VeryLarge02_Dialogs2.fnt"))
    return false;
  r.program = fo3hudassets::BuildProgram();
  if (!r.program)
    return false;
  r.mvp = glGetUniformLocation(r.program, "uMvp");
  r.tint = glGetUniformLocation(r.program, "uTint");
  r.tex = glGetUniformLocation(r.program, "uTex");
  for (int i = 0; i < 2; ++i) {
    auto &f = r.fonts[i];
    r.fontTexture[i] = fo3hudassets::UploadTexture(
        f.textureWidth, f.textureHeight, f.rgba.data());
    f.rgba.clear();
  }
  uint8_t white[4] = {255, 255, 255, 255};
  r.white = fo3hudassets::UploadTexture(1, 1, white);
  r.background =
      Art("Textures\\Interface\\Shared\\Background\\pipboy.dds", true);
  if (fo3hudassets::ParseButtonTai(r.buttons[0]) &&
      fo3hudassets::ParseButtonTai(r.buttons[1], "glow_general_button_b.dds"))
    r.buttonAtlas = Art(r.buttons[0].atlasPath, true);
  r.scanlines = Art("Textures\\Pipboy3000\\pipboyscanlines.dds", true);
  if (r.scanlines) {
    glBindTexture(GL_TEXTURE_2D, r.scanlines);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  }
  glGenTextures(1, &r.texture);
  glBindTexture(GL_TEXTURE_2D, r.texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Width, Height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glGenFramebuffers(1, &r.fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, r.fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         r.texture, 0);
  r.ready = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
  glViewport(0, 0, Width, Height);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glGenVertexArrays(1, &r.vao);
  glGenBuffers(1, &r.vbo);
  glBindVertexArray(r.vao);
  glBindBuffer(GL_ARRAY_BUFFER, r.vbo);
  glBufferData(GL_ARRAY_BUFFER, 12000 * sizeof(fo3hudassets::Vertex), nullptr,
               GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(fo3hudassets::Vertex),
                        nullptr);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(fo3hudassets::Vertex),
                        reinterpret_cast<void *>(3 * sizeof(float)));
  r.vertices.reserve(12000);
  __android_log_print(
      ANDROID_LOG_INFO, "FalloutQuest",
      "PIPBOY UI ASSETS: ready=%d target=512x384 original=1024x768 font2=%.1f "
      "font4=%.1f background=%d scanlines=%d",
      r.ready, r.fonts[0].baseLine, r.fonts[1].baseLine, r.background != 0,
      r.scanlines != 0);
  return r.ready;
}
inline void Quad(float x, float y, float w, float h, float u0 = 0, float v0 = 0,
                 float u1 = 1, float v1 = 1) {
  auto &v = State().vertices;
  v.insert(v.end(), {{x, y, 0, u0, v0},
                     {x + w, y, 0, u1, v0},
                     {x, y + h, 0, u0, v1},
                     {x, y + h, 0, u0, v1},
                     {x + w, y, 0, u1, v0},
                     {x + w, y + h, 0, u1, v1}});
}
inline void Draw(GLuint texture, float alpha = 1, bool untinted = false) {
  auto &r = State();
  if (r.vertices.empty() || !texture) {
    r.vertices.clear();
    return;
  }
  glBindTexture(GL_TEXTURE_2D, texture);
  glUniform4f(r.tint, untinted ? 1 : 26.f / 255, 1, untinted ? 1 : 128.f / 255,
              alpha);
  glBufferSubData(GL_ARRAY_BUFFER, 0,
                  r.vertices.size() * sizeof(fo3hudassets::Vertex),
                  r.vertices.data());
  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(r.vertices.size()));
  r.vertices.clear();
}
inline void Text(std::string_view text, float x, float y, int font = 0,
                 float width = 850) {
  auto &r = State();
  auto &f = r.fonts[font];
  text = text.substr(0, fo3font::FitText(f, text, width));
  fo3font::AppendText(
      f, text, x, y,
      [&](uint8_t, const fo3font::Glyph &g, const fo3font::Quad &q) {
        auto &v = r.vertices;
        v.insert(v.end(), {{q.left, q.top, 0, g.uv[0], g.uv[1]},
                           {q.right, q.top, 0, g.uv[2], g.uv[3]},
                           {q.left, q.bottom, 0, g.uv[4], g.uv[5]},
                           {q.left, q.bottom, 0, g.uv[4], g.uv[5]},
                           {q.right, q.top, 0, g.uv[2], g.uv[3]},
                           {q.right, q.bottom, 0, g.uv[6], g.uv[7]}});
      });
}
inline std::string Label(const fo3player::Player &p, const char *key,
                         const char *recovered) {
  auto i = p.Definitions().strings.find(key);
  return i == p.Definitions().strings.end() ? recovered : i->second;
}
inline std::string Number(float a) {
  char b[40];
  std::snprintf(b, sizeof(b), "%.0f", a);
  return b;
}
inline std::string Decimal(double a) {
  char b[40];
  std::snprintf(b, sizeof(b), "%.1f", a);
  std::string value = b;
  if (value.size() > 2 && value.substr(value.size() - 2) == ".0")
    value.resize(value.size() - 2);
  return value;
}
inline std::string WeightPair(const fo3player::Player &p) {
  return Decimal(p.InventoryWeight()) + "/" + Number(p.CarryCapacity());
}
inline std::string Pair(float a, float b) {
  return Number(a) + "/" + Number(b);
}
inline const char *Page(fo3pip::Tab tab, int page) {
  static const char *names[3][5] = {
      {"Status", "S.P.E.C.I.A.L.", "Skills", "Perks", "General"},
      {"Weapons", "Apparel", "Aid", "Misc", "Ammo"},
      {"Local Map", "World Map", "Quests", "Notes", "Radio"}};
  return names[int(tab)][page];
}
// Only called before the eye renderers, which explicitly bind their framebuffer
// and viewport. The FBO/viewport are owned here until this function returns.
inline void Render(fo3pip::Menu &menu, const fo3player::Player &p, bool focus) {
  auto &r = State();
  if (!r.ready || !menu.NeedsRedraw(focus))
    return;
  const auto started = std::chrono::steady_clock::now();
  const auto queries = fqgl::counters.queries;
  fo3hudrenderer::CachedStateGuard guard;
  glBindFramebuffer(GL_FRAMEBUFFER, r.fbo);
  glViewport(0, 0, Width, Height);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                      GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(r.program);
  float mvp[16] = {2.f / 1024, 0, 0, 0, 0,  -2.f / 768, 0, 0,
                   0,          0, 1, 0, -1, 1,          0, 1};
  glUniformMatrix4fv(r.mvp, 1, GL_FALSE, mvp);
  glUniform1i(r.tex, 0);
  glActiveTexture(GL_TEXTURE0);
  glBindVertexArray(r.vao);
  glBindBuffer(GL_ARRAY_BUFFER, r.vbo);
  if (r.background) {
    Quad(0, 0, 1024, 768);
    Draw(r.background, .9f);
  }
  const char *titles[] = {"STATS", "ITEMS", "DATA"};
  const char *titleKeys[] = {"sStats", "sInventoryItems", "sData"};
  Text(Label(p, titleKeys[int(menu.tab)], titles[int(menu.tab)]), 120, 50, 1);
  Draw(r.fontTexture[1]);
  const auto &s = p.Snapshot();
  if (menu.tab == fo3pip::Tab::Stats) {
    Text("LVL " + Number(s.level), 270, 50);
    Text("HP " + Pair(p.Health(), p.MaxHealth()), 380, 50, 0, 155);
    Text("AP " + Pair(p.ActionPoints(), p.MaxActionPoints()), 545, 50, 0, 155);
  }
  if (menu.tab == fo3pip::Tab::Items) {
    Text("Wg " + WeightPair(p), 265, 50, 0, 155);
    Text("HP " + Pair(p.Health(), p.MaxHealth()), 430, 50, 0, 155);
    int caps = 0;
    for (const auto &stack : s.inventory) {
      auto i = p.Definitions().items.find(stack.formId);
      if (i != p.Definitions().items.end() && i->second.editorId == "Caps001")
        caps += stack.count;
    }
    Text("Caps " + std::to_string(caps), 715, 50, 0, 190);
  }
  Draw(r.fontTexture[0]);
  Quad(50, 98, 905, 2);
  Quad(50, 642, 905, 2);
  Draw(r.white, .85f);
  // The original tabline at the bottom, preserving the original five pages.
  for (int i = 0; i < 5; ++i) {
    float x = 55.f + i * 184;
    Text(Page(menu.tab, i), x, 657, 0, 178);
    if (i == menu.page) {
      Draw(r.fontTexture[0]);
      Quad(x - 5, 650, 178, 2);
      Quad(x - 5, 695, 178, 2);
      Draw(r.white);
    }
  }
  Draw(r.fontTexture[0]);
  if (menu.tab == fo3pip::Tab::Items) {
    size_t first = menu.selected >= 8 ? menu.selected - 7 : 0;
    size_t end = std::min(menu.rows.size(), first + 8);
    if (menu.rows.empty())
      Text("No items", 90, 190);
    for (size_t row = first; row < end; ++row) {
      const auto *stack = fo3pip::Menu::Stack(p, menu.rows[row]);
      if (!stack)
        continue;
      const auto &item = p.Definitions().items.at(stack->formId);
      float y = 145.f + (row - first) * 51;
      if (row == menu.selected) {
        Draw(r.fontTexture[0]);
        Quad(75, y - 4, 425, 2);
        Quad(75, y + 41, 425, 2);
        Draw(r.white, menu.inPage ? 1.f : .45f);
      }
      Text((stack->equipped ? "[+] " : "") + item.name +
               (stack->count > 1 ? " (" + std::to_string(stack->count) + ")"
                                 : ""),
           90, y, 0, 395);
    }
    Draw(r.fontTexture[0]);
    if (menu.selected < menu.rows.size()) {
      const auto *stack = fo3pip::Menu::Stack(p, menu.rows[menu.selected]);
      if (stack) {
        const auto &item = p.Definitions().items.at(stack->formId);
        if (!item.icon.empty()) {
          auto art = Art(item.icon);
          if (art) {
            Quad(565, 145, 230, 230);
            Draw(art);
          }
        }
        Text(item.name, 540, 400, 0, 360);
        Text("WG " + Decimal(item.weight), 540, 451);
        if (item.valueKnown)
          Text("VAL " + std::to_string(item.value), 700, 451);
        if (item.maxCondition > 0)
          Text("CND " + Number(stack->condition * 100) + "%", 540, 500);
        Text(fo3pip::Equippable(item)
                 ? (stack->equipped ? "A  Unequip" : "A  Equip")
                 : "",
             540, 555);
        Draw(r.fontTexture[0]);
      }
    }
  } else if (menu.tab == fo3pip::Tab::Stats) {
    if (menu.page == 0) {
      GLuint art = Art("Textures\\Interface\\Icons\\PipboyImages\\Derived "
                       "Statistics\\hit_points.dds");
      if (art) {
        Quad(600, 175, 235, 235);
        Draw(art);
      }
      Text("HP " + Pair(p.Health(), p.MaxHealth()), 120, 175);
      Text("AP " + Pair(p.ActionPoints(), p.MaxActionPoints()), 120, 235);
      Text("Wg " + WeightPair(p), 120, 295);
      Text("Karma " + Number(s.karma), 120, 355);
    } else if (menu.page == 1) {
      const char *names[] = {"Strength", "Perception",   "Endurance",
                             "Charisma", "Intelligence", "Agility",
                             "Luck"};
      for (int i = 0; i < 7; ++i) {
        Text(names[i], 120, 145 + i * 58);
        Text(Number(s.special[i]), 450, 145 + i * 58);
      }
    } else if (menu.page == 2) {
      const char *names[] = {"Barter",        "Big Guns", "Energy Weapons",
                             "Explosives",    "Lockpick", "Medicine",
                             "Melee Weapons", "Repair",   "Science",
                             "Small Guns",    "Sneak",    "Speech",
                             "Unarmed"};
      size_t first = menu.selected >= 8 ? menu.selected - 7 : 0;
      for (size_t i = first; i < std::min(size_t(13), first + 8); ++i) {
        Text(names[i], 120, 145 + (i - first) * 51);
        Text(Number(s.skills[i == 12 ? 13 : i]), 470, 145 + (i - first) * 51);
      }
    } else
      Text("Unavailable", 120, 190);
    Draw(r.fontTexture[0]);
  } else {
    Text("Unavailable", 120, 190);
    Draw(r.fontTexture[0]);
  }
  if (r.buttonAtlas) {
    for (int i = 0; i < 2; ++i) {
      const auto &button = r.buttons[i];
      Quad(i == 0 ? 65 : 445, 706, 45, 45, button.u, button.v,
           button.u + button.w, button.v + button.h);
    }
    Draw(r.buttonAtlas);
  }
  Text("Select", 120, 712);
  Text("Back", 495, 712);
  Text("STATS   ITEMS   DATA", 615, 712, 0, 355);
  Draw(r.fontTexture[0]);
  // Original recovered static CRT overlay. No invented animated noise or
  // steady redraw timer; authored glass is a separate world NIF shape.
  if (r.scanlines) {
    glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_ZERO, GL_ONE);
    Quad(0, 0, 1024, 768, 0, 0, 1, 50); // FALLOUT.INI fScanlineScalePipboy=50
    Draw(r.scanlines, 1, true);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  menu.dirty = false;
  ++r.redraws;
  r.renderUs = std::chrono::duration<double, std::micro>(
                   std::chrono::steady_clock::now() - started)
                   .count();
  r.renderQueries = fqgl::counters.queries - queries;
  if (r.renderQueries)
    __android_log_print(ANDROID_LOG_ERROR, "FalloutQuest",
                        "PIPBOY UI: unexpected GL driver query");
}
inline void Shutdown() {
  auto &r = State();
  for (auto &art : r.art)
    if (art.second)
      glDeleteTextures(1, &art.second);
  for (auto &texture : r.fontTexture)
    if (texture)
      glDeleteTextures(1, &texture);
  for (auto texture : {r.texture, r.white})
    if (texture)
      glDeleteTextures(1, &texture);
  if (r.fbo)
    glDeleteFramebuffers(1, &r.fbo);
  if (r.vbo)
    glDeleteBuffers(1, &r.vbo);
  if (r.vao)
    glDeleteVertexArrays(1, &r.vao);
  if (r.program)
    glDeleteProgram(r.program);
  r = {};
}
} // namespace fo3pipui
