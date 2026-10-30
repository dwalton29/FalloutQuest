#pragma once
#include "ui/fo3-system-colour.h"
#include "ui/pipboy/fo3-pipboy-assets.h"
#include "fo3-pipboy-state.h"
#include "ui/menu/fo3-menu-renderer.h"
#include "ui/interaction/fo3-hud-cached-state.h"
#include "ui/interaction/fo3-interaction-hud-renderer.h"
#include <chrono>
#include <unordered_map>
#include <unordered_set>
namespace fo3pipui {
constexpr int Width = 1024,
              Height = 768; // original globals.xml render surface
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
  std::shared_ptr<const fo3pipdata::LocalMap> localMap;
  GLuint localTexture=0;
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
    if (!fo3pip::MenuAssetValid(path,xml))
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
    auto &shared = fo3menuui::Shared().fonts[i == 0 ? 2 : 4];
    if (!shared.texture) {
      shared.texture = fo3hudassets::UploadTexture(
          f.textureWidth, f.textureHeight, f.rgba.data());
      shared.metrics = f;
      shared.metrics.rgba.clear();
      shared.owned = true;
    }
    r.fontTexture[i] = shared.texture;
    f.rgba.clear();
  }
  uint8_t white[4] = {255, 255, 255, 255};
  r.white = fo3hudassets::UploadTexture(1, 1, white);
  r.scanlines = Art("Textures\\Pipboy3000\\pipboyscanlines.dds", true);
  if (r.scanlines) {
    glBindTexture(GL_TEXTURE_2D, r.scanlines);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
#ifdef FQ_ENABLE_VULKAN
    fqvk::RepeatT(r.scanlines);
#endif
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
      "PIPBOY UI ASSETS: ready=%d target=1024x768 original=1024x768 font2=%.1f "
      "font4=%.1f authored-menu-assets=lazy scanlines=%d",
      r.ready, r.fonts[0].baseLine, r.fonts[1].baseLine,
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
inline void Draw(GLuint texture, float alpha = 1, bool untinted = false, bool dark = false) {
  auto &r = State();
  if (r.vertices.empty() || !texture) {
    r.vertices.clear();
    return;
  }
  glBindTexture(GL_TEXTURE_2D, texture);
  glUniform4f(r.tint, dark?0:untinted ? 1 : fo3uicolor::Main[0], dark?0:1, dark?0:untinted ? 1 : fo3uicolor::Main[2],
              alpha);
  glBufferSubData(GL_ARRAY_BUFFER, 0,
                  r.vertices.size() * sizeof(fo3hudassets::Vertex),
                  r.vertices.data());
  fo3hudassets::UploadVertices(r.vao,r.vertices.data(),r.vertices.size());
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
} // namespace fo3pipui
#include "fo3-pipboy-data-pages.h"
#include "ui/menu/fo3-menu-player.h"
namespace fo3pipui {
// Only called before the eye renderers, which explicitly bind their framebuffer
// and viewport. The FBO/viewport are owned here until this function returns.
inline void Render(fo3pip::Menu &menu, const fo3player::Player &p, bool focus) {
  auto &r = State();
  if (!r.ready || !menu.NeedsRedraw(focus))
    return;
  const auto started = std::chrono::steady_clock::now();
  const auto queries = fqgl::counters.queries;
  fo3hudrenderer::CachedStateGuard guard;
#ifdef FQ_ENABLE_VULKAN
  fqvk::UiTargetScope nativeScreen(r.texture,Width,Height);
#endif
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
  if (menu.tab == fo3pip::Tab::Data && menu.page == 0 && menu.map.local &&
      !menu.map.local->rgba.empty() && r.localMap != menu.map.local) {
    if (r.localTexture)
      glDeleteTextures(1, &r.localTexture);
    r.localTexture =
        fo3hudassets::UploadTexture(256, 256, menu.map.local->rgba.data());
    r.localMap = menu.map.local;
  }
  if (!fo3menuui::Pipboy(menu, p, mvp, r.localTexture))
    __android_log_print(ANDROID_LOG_ERROR, "FalloutQuest",
                        "PIPBOY authored menu unavailable");
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
  // Font GPU ownership is shared with dialogue/barter; menu shutdown releases it.
  for (auto texture : {r.texture, r.white, r.localTexture})
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
