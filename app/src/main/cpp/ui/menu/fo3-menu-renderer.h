#pragma once
#include "dialogue/fo3-dialogue-panel.h"
#include "fo3-menu-layout.h"
#include "fo3-menu-strings.h"
#include "fo3-menu-texture-paths.h"
#include "player/fo3-player-state.h"
#include "ui/fo3-system-colour.h"
#include "ui/interaction/fo3-interaction-hud-renderer.h"
#include <cctype>
#include <memory>
namespace fo3menuui {
struct Font {
  fo3hudassets::Font metrics;
  GLuint texture = 0;
  bool owned = true;
};
struct Texture {
  GLuint id = 0;
  float u = 0, v = 0, w = 1, h = 1;
  float pixelWidth = 0, pixelHeight = 0;
};
struct Assets {
  GLuint program = 0;
  GLint mvp = -1, tint = -1, tex = -1;
  std::map<int, Font> fonts;
  std::map<std::string, Texture> textures;
  std::map<std::string, Texture> images;
  std::map<std::string, GLuint> dynamicImages;
  fo3menu::TexturePaths aliases;
  bool aliasesReady = false;
  std::string AliasPath(const std::string &file) {
    if (file.find_first_of("/\\") != file.npos)
      return {};
    if (!aliasesReady) {
      aliasesReady = true;
      for (const auto &path : fo3assets::TextureArchivePaths()) {
        auto archive = fo3assets::GetBsaArchive(path);
        std::vector<fo3assets::BsaFileInfo> entries;
        if (archive && archive->List("interface\\", entries,
                                     fo3assets::BsaPathKind::Texture))
          for (const auto &entry : entries)
            aliases.Add(entry.path);
      }
    }
    return aliases.Resolve(file);
  }
  bool Ensure() {
    if (program)
      return true;
    program = fo3hudassets::BuildProgram();
    if (!program)
      return false;
    mvp = glGetUniformLocation(program, "uMvp");
    tint = glGetUniformLocation(program, "uTint");
    tex = glGetUniformLocation(program, "uTex");
    return true;
  }
  Font *GetFont(int id) {
    auto i = fonts.find(id);
    if (i != fonts.end())
      return i->second.texture ? &i->second : nullptr;
    const char *path = nullptr;
    switch (id) {
    case 1:
      path = "Glow_Monofonto_Large.fnt";
      break;
    case 2:
      path = "Monofonto_Large.fnt";
      break;
    case 3:
      path = "Glow_Monofonto_Medium.fnt";
      break;
    case 4:
      path = "Monofonto_VeryLarge02_Dialogs2.fnt";
      break;
    case 6:
      path = "Glow_Monofonto_VL_dialogs.fnt";
      break;
    case 7:
      path = "Baked-in_Monofonto_Large.fnt";
      break;
    case 8:
      path = "Glow_Futura_Caps_Large.fnt";
      break;
    default:
      return nullptr;
    }
    Font f;
    if (fo3hudassets::ParseFont(
            f.metrics, (std::string("Textures\\Fonts\\") + path).c_str())) {
      f.texture = fo3hudassets::UploadTexture(f.metrics.textureWidth,
                                              f.metrics.textureHeight,
                                              f.metrics.rgba.data());
      f.metrics.rgba.clear();
      f.metrics.rgba.shrink_to_fit();
    }
    auto row = fonts.emplace(id, std::move(f));
    return row.first->second.texture ? &row.first->second : nullptr;
  }
  Texture Image(std::string file, const std::string &atlas) {
    if (file.empty())
      return {};
    auto generated = dynamicImages.find(file);
    if (generated != dynamicImages.end())
      return Texture{generated->second, 0, 0, 1, 1, 256, 256};
    auto key = atlas + "|" + file;
    auto old = textures.find(key);
    if (old != textures.end())
      return old->second;
    Texture t;
    std::string path = file;
    if (!atlas.empty()) {
      fo3hudassets::TaiSprite sprite;
      const auto slash = file.find_last_of("/\\");
      const auto alias = file.substr(slash == file.npos ? 0 : slash + 1);
      if (fo3hudassets::ParseButtonTai(sprite, alias.c_str())) {
        path = sprite.atlasPath;
        t.u = sprite.u;
        t.v = sprite.v;
        t.w = sprite.w;
        t.h = sprite.h;
      }
    }
    if (path == file) {
      const auto resolved = AliasPath(file);
      if (!resolved.empty())
        path = resolved;
    }
    if (path.rfind("Textures", 0) != 0 && path.rfind("textures", 0) != 0)
      path = "Textures\\" + path;
    auto image = images.find(path);
    if (image != images.end()) {
      t.id = image->second.id;
      t.pixelWidth = image->second.pixelWidth * t.w;
      t.pixelHeight = image->second.pixelHeight * t.h;
    } else {
      Fo3RgbaTexture pixels;
      if (LoadFalloutTextureRgba(path, pixels)) {
        t.id = fo3hudassets::UploadTexture(pixels.width, pixels.height,
                                           pixels.rgba.data());
        t.pixelWidth = pixels.width;
        t.pixelHeight = pixels.height;
      }
      Texture full = t;
      full.u = full.v = 0;
      full.w = full.h = 1;
      images.emplace(path, full);
      t.pixelWidth *= t.w;
      t.pixelHeight *= t.h;
    }
    textures.emplace(key, t);
    return t;
  }
  void Shutdown() {
    for (auto &p : fonts)
      if (p.second.texture && p.second.owned)
        glDeleteTextures(1, &p.second.texture);
    for (auto &p : images)
      if (p.second.id)
        glDeleteTextures(1, &p.second.id);
    if (program)
      glDeleteProgram(program);
    *this = {};
  }
};
inline Assets &Shared() {
  static Assets a;
  return a;
}
inline bool Read(const std::string &path, std::string &text) {
  auto archive = fo3assets::GetBsaArchive(
      fo3assets::FalloutDataPath("Fallout - Misc.bsa"));
  std::vector<uint8_t> bytes;
  if (!archive || !archive->Read(path, bytes))
    return false;
  text.assign(bytes.begin(), bytes.end());
  return true;
}
inline void Measure(fo3menu::Layout &layout) {
  layout.imageSize = [](const std::string &file, const std::string &atlas) {
    const auto texture = Shared().Image(file, atlas);
    return fo3menu::Box{0, 0, texture.pixelWidth, texture.pixelHeight};
  };
  layout.measure = [](int id, const std::string &text, float width) {
    auto *f = Shared().GetFont(id);
    if (!f)
      return fo3menu::Box{};
    const auto lines =
        fo3dialogue::Wrap(f->metrics, text, width > 0 ? width : 100000.f);
    float maximum = 0;
    for (const auto &line : lines) {
      float length = fo3font::MeasureText(f->metrics, line);
      maximum = std::max(maximum, length);
    }
    // B564AF loads post-load space.height for line measurement. XML padding
    // already positions the font cell; its Y anchor is centred on that cell.
    return fo3menu::Box{0, 0, maximum,
                        float(lines.size()) * f->metrics.glyphs[32].height};
  };
}
struct Batch {
  GLuint texture = 0;
  GLsizei first = 0, count = 0;
  float rgb[3]{}, alpha = 1;
};
struct Surface {
  GLuint vao = 0, vbo = 0;
  std::vector<Batch> batches;
  std::vector<fo3hudassets::Vertex> vertices;
  struct AnimatedUv {
    size_t vertex;
    float u;
  };
  std::vector<AnimatedUv> animated;
  float animationPhase = -1;
  void Animate(float phase) {
    if (animated.empty() || phase == animationPhase)
      return;
    fo3hudrenderer::CachedStateGuard guard;
    animationPhase = phase;
    for (const auto &point : animated)
      vertices[point.vertex].u = point.u + phase;
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(vertices[0]),
                    vertices.data());
    fo3hudassets::UploadVertices(vao, vertices.data(), vertices.size());
  }
  void Shutdown() {
    if (vao)
      glDeleteVertexArrays(1, &vao);
    if (vbo)
      glDeleteBuffers(1, &vbo);
    *this = {};
  }
  void Quad(fo3menu::Box b, const fo3menu::Box &clip, Texture texture, float z,
            float rotation = 0) {
    if (rotation != 0 && b.w > 0 && b.h > 0) {
      using Vertex = fo3hudassets::Vertex;
      float cx = b.x + b.w * .5f, cy = b.y + b.h * .5f, c = std::cos(rotation),
            s = std::sin(rotation);
      std::vector<Vertex> polygon{
          {b.x, b.y, z, texture.u, texture.v},
          {b.x + b.w, b.y, z, texture.u + texture.w, texture.v},
          {b.x + b.w, b.y + b.h, z, texture.u + texture.w,
           texture.v + texture.h},
          {b.x, b.y + b.h, z, texture.u, texture.v + texture.h}};
      for (auto &v : polygon) {
        float x = v.x - cx, y = v.y - cy;
        v.x = cx + x * c - y * s;
        v.y = cy + x * s + y * c;
      }
      for (int edge = 0; edge < 4 && !polygon.empty(); ++edge) {
        float limit = edge == 0   ? clip.x
                      : edge == 1 ? clip.x + clip.w
                      : edge == 2 ? clip.y
                                  : clip.y + clip.h;
        auto coordinate = [&](const Vertex &v) { return edge < 2 ? v.x : v.y; };
        auto inside = [&](const Vertex &v) {
          return edge % 2 ? coordinate(v) <= limit : coordinate(v) >= limit;
        };
        std::vector<Vertex> next;
        auto previous = polygon.back();
        bool was = inside(previous);
        for (const auto &v : polygon) {
          bool now = inside(v);
          if (now != was) {
            float t = (limit - coordinate(previous)) /
                      (coordinate(v) - coordinate(previous));
            next.push_back({previous.x + t * (v.x - previous.x),
                            previous.y + t * (v.y - previous.y), z,
                            previous.u + t * (v.u - previous.u),
                            previous.v + t * (v.v - previous.v)});
          }
          if (now)
            next.push_back(v);
          previous = v;
          was = now;
        }
        polygon = std::move(next);
      }
      for (size_t i = 1; i + 1 < polygon.size(); ++i)
        vertices.insert(vertices.end(),
                        {polygon[0], polygon[i], polygon[i + 1]});
      return;
    }
    float x = std::max(b.x, clip.x), y = std::max(b.y, clip.y),
          right = std::min(b.x + b.w, clip.x + clip.w),
          bottom = std::min(b.y + b.h, clip.y + clip.h);
    if (b.w <= 0 || b.h <= 0 || right <= x || bottom <= y)
      return;
    float u = texture.u + (x - b.x) / b.w * texture.w,
          v = texture.v + (y - b.y) / b.h * texture.h,
          ur = texture.u + (right - b.x) / b.w * texture.w,
          vb = texture.v + (bottom - b.y) / b.h * texture.h;
    vertices.insert(vertices.end(), {{x, y, z, u, v},
                                     {right, y, z, ur, v},
                                     {x, bottom, z, u, vb},
                                     {x, bottom, z, u, vb},
                                     {right, y, z, ur, v},
                                     {right, bottom, z, ur, vb}});
  }
  bool Build(fo3menu::Layout &layout) {
    fo3hudrenderer::CachedStateGuard guard;
    if (!Shared().Ensure())
      return false;
    batches.clear();
    vertices.clear();
    animated.clear();
    animationPhase = -1;
    auto draw = layout.Paint();
    bool complete = true;
    for (auto &d : draw) {
      if (d.alpha <= 0 || d.clip.w <= 0 || d.clip.h <= 0)
        continue;
      Texture texture;
      size_t first = vertices.size();
      if (d.type == "text") {
        auto *font = Shared().GetFont(d.font);
        if (!font) {
          complete = false;
          continue;
        }
        texture.id = font->texture;
        auto width = layout.N(d.node, "wrapwidth");
        auto lines = fo3dialogue::Wrap(font->metrics, d.text,
                                       width > 0 ? width : 100000.f);
        float y = d.box.y;
        const float glyphOrigin = font->metrics.glyphs[32].height * .5f;
        for (const auto &line : lines) {
          auto extent = layout.measure(d.font, line, 0);
          float x = d.box.x - (d.justify == 2   ? extent.w
                               : d.justify == 1 ? extent.w * .5f
                                                : 0);
          fo3font::AppendText(
              font->metrics, line, 0, glyphOrigin - font->metrics.baseLine,
              [&](uint8_t, const fo3font::Glyph &g, const fo3font::Quad &q) {
                Quad(
                    {x + q.left, y + q.top, q.right - q.left, q.bottom - q.top},
                    d.clip,
                    {texture.id, g.uv[0], g.uv[1], g.uv[2] - g.uv[0],
                     g.uv[5] - g.uv[1]},
                    0);
              });
          y += font->metrics.glyphs[32].height;
        }
      } else if (d.type == "image" && !d.file.empty()) {
        texture = Shared().Image(d.file, d.atlas);
        if (texture.id) {
          // Original image crop traits are offsets in zoomed texture pixels.
          const auto &traits = layout.Nodes()[d.node].traits;
          float width = layout.N(d.node, "filewidth"),
                height = layout.N(d.node, "fileheight");
          if (traits.count("cropx") && width > 0) {
            texture.u += texture.w * layout.N(d.node, "cropx") / width;
            texture.w *= d.box.w / width;
          }
          if (traits.count("cropy") && height > 0) {
            texture.v += texture.h * layout.N(d.node, "cropy") / height;
            texture.h *= d.box.h / height;
          }
          const bool scroll = layout.S(d.node, "animation") == "Forward";
          if (scroll && width > 0 && height > 0) {
            texture.w *= d.box.w / width;
            texture.h *= d.box.h / height;
            glBindTexture(GL_TEXTURE_2D, texture.id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
#ifdef FQ_ENABLE_VULKAN
            fqvk::RepeatS(texture.id);
#endif
          }
          Quad(d.box, d.clip, texture, 0, d.rotation);
          if (scroll)
            for (size_t i = first; i < vertices.size(); ++i)
              animated.push_back({i, vertices[i].u});
        } else
          complete = false;
      }
      if (vertices.size() == first)
        continue;
      Batch batch;
      batch.texture = texture.id;
      batch.first = GLsizei(first);
      batch.count = GLsizei(vertices.size() - first);
      batch.alpha = std::clamp(d.alpha, 0.f, 1.f);
      for (int c = 0; c < 3; ++c)
        batch.rgb[c] = (d.colour ? fo3uicolor::Main[c] : 1.f) *
                       std::clamp(d.brightness, 0.f, 1.f);
      if (!batches.empty()) {
        auto &last = batches.back();
        if (last.texture == batch.texture && last.alpha == batch.alpha &&
            last.first + last.count == batch.first &&
            std::equal(last.rgb, last.rgb + 3, batch.rgb)) {
          last.count += batch.count;
          continue;
        }
      }
      batches.push_back(batch);
    }
    if (!vao) {
      glGenVertexArrays(1, &vao);
      glGenBuffers(1, &vbo);
    }
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 vertices.size() * sizeof(fo3hudassets::Vertex),
                 vertices.data(), GL_DYNAMIC_DRAW);
    fo3hudassets::UploadVertices(vao, vertices.data(), vertices.size());
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          sizeof(fo3hudassets::Vertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                          sizeof(fo3hudassets::Vertex),
                          reinterpret_cast<void *>(3 * sizeof(float)));
    return complete;
  }
  void Render(const float *mvp) {
    if (!vao || !mvp || !Shared().program)
      return;
    fo3hudrenderer::CachedStateGuard guard;
    auto &a = Shared();
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                        GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(a.program);
    glUniformMatrix4fv(a.mvp, 1, GL_FALSE, mvp);
    glUniform1i(a.tex, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vao);
    for (const auto &batch : batches) {
      glBindTexture(GL_TEXTURE_2D, batch.texture);
      glUniform4f(a.tint, batch.rgb[0], batch.rgb[1], batch.rgb[2],
                  batch.alpha);
      glDrawArrays(GL_TRIANGLES, batch.first, batch.count);
    }
  }
};
inline bool SameSetting(const std::string &a, const std::string &b) {
  return a.size() == b.size() &&
         std::equal(a.begin(), a.end(), b.begin(),
                    [](unsigned char x, unsigned char y) {
                      return std::tolower(x) == std::tolower(y);
                    });
}
inline std::string Setting(const fo3player::Player &p, const std::string &key) {
  const auto &overrides = p.Definitions().strings;
  auto found = overrides.find(key);
  if (found != overrides.end())
    return found->second;
  for (const auto &entry : overrides)
    if (SameSetting(entry.first, key))
      return entry.second;
  const auto &defaults = fo3menu::StringDefaults();
  auto original = defaults.find(key);
  if (original != defaults.end())
    return original->second;
  for (const auto &entry : defaults)
    if (SameSetting(entry.first, key))
      return entry.second;
  return {};
}
inline void Strings(fo3menu::Layout &l, const fo3player::Player &p) {
  l.strings = [&p](const std::string &key) -> fo3menu::Value {
    return Setting(p, key);
  };
}

struct Row {
  std::string text, value;
  bool equipped = false, marked = false;
};
inline std::vector<int> List(fo3menu::Layout &l, const std::string &name,
                             const std::string &type,
                             const std::vector<Row> &rows, size_t selected) {
  std::vector<int> ids;
  int parent = l.Find(name);
  if (parent < 0)
    return ids;
  l.Set(parent, "_enabled", double(selected < rows.size()));
  float height = l.N(parent, "height"), total = 0;
  const auto declared = l.N(parent, "_number_of_visible_items");
  std::vector<float> heights;
  for (size_t i = 0; i < rows.size(); ++i) {
    int row = l.Instantiate(type, parent);
    if (row < 0)
      break;
    l.Set(row, "string", rows[i].text);
    l.Set(row, "_Value", rows[i].value);
    l.Set(row, "listindex", double(i));
    l.Set(row, "_selected", double(i == selected));
    l.Set(row, "mouseover", double(i == selected));
    l.Set(row, "_line_alpha", double(i == selected ? 255 : 128));
    l.Set(row, "_IsEquipped", double(rows[i].equipped && !rows[i].marked));
    l.Set(row, "_IsBarterSelected", double(rows[i].marked));
    l.Set(row, "user0", 0.);
    l.Set(row, "user1",
          rows[i].value.empty() ? fo3menu::Value(-1.)
                                : fo3menu::Value(rows[i].value));
    l.Set(row, "height", 0.);
    auto text = l.Find("ListItemText", row);
    float h =
        text >= 0 ? l.N(text, "height") + l.N(row, "_VerticalSpacing") : 0;
    if (h <= 0)
      h = l.N(parent, "_scroll_delta");
    if (h <= 0)
      h = 26;
    l.Set(row, "height", h);
    l.Set(row, "_y", total);
    heights.push_back(h);
    total += h;
    ids.push_back(row);
  }
  if (declared > 0 && !heights.empty()) {
    height = heights.front() * declared;
    l.Set(parent, "height", height);
  }
  float offset = 0, selectedY = 0, selectedH = 0;
  for (size_t i = 0; i < heights.size(); ++i) {
    if (i == selected) {
      selectedY = offset;
      selectedH = heights[i];
    }
    offset += heights[i];
  }
  const auto scroll = std::max(0.f, selectedY + selectedH - height);
  l.Set(parent, "_highlight_y",
        selected < ids.size() ? selectedY - scroll : -1.);
  l.Set(parent, "_selected_height", selectedH);
  l.Set(parent, "_num_filtered", double(rows.size()));
  l.Set(parent, "_scroll_delta", 1.);
  int bar = l.Find("lb_scrollbar", parent);
  l.Set(bar, "_current_value", scroll);
  l.Set(bar, "_total_height", total);
  l.Set(bar, "_number_of_items", double(rows.size()));
  return ids;
}
inline void ListScrollFix(fo3menu::Layout &l, int parent, size_t selected) {
  float y = 0, chosen = 0, h = 0, total = 0;
  for (auto c : l.Nodes()[parent].children)
    if (l.Nodes()[c].dynamic) {
      if (size_t(l.N(c, "listindex")) == selected) {
        chosen = y;
        h = l.N(c, "height");
      }
      y += l.N(c, "height");
    }
  total = y;
  float offset = std::max(0.f, chosen + h - l.N(parent, "height"));
  l.Set(parent, "_highlight_y", h ? chosen - offset : -1.);
  l.Set(parent, "_selected_height", h);
  int bar = l.Find("lb_scrollbar", parent);
  l.Set(bar, "_current_value", offset);
  l.Set(bar, "_total_height", total);
}
inline void World(Surface &surface, const fo3menu::Layout &layout,
                  const float *mvp) {
  // VR policy: one metre per 1000 authored UI pixels, centred on the existing
  // interaction anchor. All relative screen coordinates still come from XML.
  float local[16]{.001f,
                  0,
                  0,
                  0,
                  0,
                  -.001f,
                  0,
                  0,
                  0,
                  0,
                  1,
                  0,
                  -layout.width * .0005f,
                  layout.height * .0005f,
                  0,
                  1},
      result[16]{};
  for (int c = 0; c < 4; ++c)
    for (int r = 0; r < 4; ++r)
      for (int k = 0; k < 4; ++k)
        result[c * 4 + r] += mvp[k * 4 + r] * local[c * 4 + k];
  surface.Render(result);
}
inline void Tabs(fo3menu::Layout &l, const std::string &name,
                 const std::vector<std::string> &labels, int selected) {
  int parent = l.Find(name);
  l.Set(parent, "_ButtonCount", double(labels.size()));
  l.Set(parent, "_CurrentTab", double(selected));
  for (size_t i = 0; i < labels.size(); ++i) {
    int row = l.Instantiate("TabButtonTemplate", parent);
    l.Set(row, "string", labels[i]);
    l.Set(row, "listindex", double(i));
    l.Set(row, "_selected", double(int(i) == selected));
  }
  std::vector<int> tabs;
  float used = 0;
  for (auto c : l.Nodes()[parent].children)
    if (l.Nodes()[c].dynamic) {
      tabs.push_back(c);
      used += l.N(c, "width");
    }
  float gap = std::max(0.f, (l.N(parent, "width") - used) / (tabs.size() + 1));
  l.Set(parent, "_LeftLineLength", gap);
  float x = gap;
  for (auto tab : tabs) {
    l.Set(tab, "_x", x);
    x += l.N(tab, "width") + gap;
  }
}
} // namespace fo3menuui
