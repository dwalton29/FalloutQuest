#pragma once
#include "fo3-interaction-hud-renderer.h"
#include "world/interaction/fo3-interaction.h"

// User-requested floating loot list. Original FO3 font/colour, explicit VR
// layout.
namespace fo3lootui {
struct Resources {
  GLuint vao = 0, vbo = 0, solid = 0;
  std::string key;
  GLsizei textStart = 0, textCount = 0, buttonStart = 0;
  bool highlight = false;
};
inline Resources &State() {
  static Resources r;
  return r;
}
inline void Shutdown() {
  auto &r = State();
  if (r.vbo)
    glDeleteBuffers(1, &r.vbo);
  if (r.vao)
    glDeleteVertexArrays(1, &r.vao);
  if (r.solid)
    glDeleteTextures(1, &r.solid);
  r = {};
}
inline void Render(const float *mvp, const Fo3LootPanel &panel) {
  if (!panel.reference || !mvp)
    return;
  fo3hudrenderer::GlStateGuard guard;
  glActiveTexture(GL_TEXTURE0);
  if (!fo3hudrenderer::EnsureResources())
    return;
  auto &font = fo3hudrenderer::State();
  auto &r = State();
  if (!r.vao) {
    glGenVertexArrays(1, &r.vao);
    glGenBuffers(1, &r.vbo);
    const std::vector<uint8_t> white{255, 255, 255, 255};
    r.solid = fo3hudassets::UploadTexture(1, 1, white.data());
  }
  if (!r.vao || !r.vbo || !r.solid)
    return;
  const size_t start = panel.selected >= 5 ? panel.selected - 4 : 0;
  std::string key = panel.title + "\n" + std::to_string(panel.selected);
  for (size_t i = start; i < panel.rows.size() && i < start + 5; ++i)
    key += '\n' + panel.rows[i].name + std::to_string(panel.rows[i].count) +
           (panel.rows[i].allowed ? "1" : "0");
  if (key != r.key) {
    using fo3hudrenderer::AppendPromptQuad;
    std::vector<fo3hudassets::Vertex> vertices;
    AppendPromptQuad(vertices, -.22f, .055f, .22f, -.25f, 0, 0, 0, 1, 0, 0, 1,
                      1, 1);
    r.highlight = !panel.rows.empty();
    if (r.highlight) {
      const float y =
          -.02f - static_cast<float>(panel.selected - start) * .037f;
      AppendPromptQuad(vertices, -.207f, y + .006f, .207f, y - .031f, .001f, 0,
                        0, 1, 0, 0, 1, 1, 1);
    }
    r.buttonStart = static_cast<GLsizei>(vertices.size());
    const auto &b = font.button;
    AppendPromptQuad(vertices, -.20f, -.204f, -.167f, -.237f, .002f, b.u, b.v,
                      b.u + b.w, b.v, b.u, b.v + b.h, b.u + b.w, b.v + b.h);
    r.textStart = static_cast<GLsizei>(vertices.size());
    const float scale = .025f / std::max(1.0f, font.baseLine);
    auto text = [&](std::string value, float x, float y) {
      const size_t length=fo3font::FitText(font,value,.38f/scale);
      fo3hudrenderer::LogTextDiagnostics(value.data(),length,
          fo3font::MeasureText(font,std::string_view(value.data(),length)));
      fo3font::AppendText(font,std::string_view(value.data(),length),0,0,
          [&](uint8_t,const fo3font::Glyph& g,const fo3font::Quad& q) {
              AppendPromptQuad(vertices,x+q.left*scale,y-q.top*scale,
                  x+q.right*scale,y-q.bottom*scale,.002f,
                  g.uv[0],g.uv[1],g.uv[2],g.uv[3],g.uv[4],g.uv[5],g.uv[6],g.uv[7]);
          });
    };
    text(panel.title, -.20f, .038f);
    if (panel.rows.empty())
      text("Empty", -.20f, -.025f);
    for (size_t i = start; i < panel.rows.size() && i < start + 5; ++i) {
      const auto &row = panel.rows[i];
      text(row.name +
               (row.count > 1 ? " (" + std::to_string(row.count) + ")" : ""),
           -.20f, -.02f - static_cast<float>(i - start) * .037f);
    }
    const bool take = !panel.rows.empty() && panel.rows[panel.selected].allowed;
    text(take ? "Take" : "", -.15f, -.209f);
    r.textCount = static_cast<GLsizei>(vertices.size()) - r.textStart;
    glBindVertexArray(r.vao);
    glBindBuffer(GL_ARRAY_BUFFER, r.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 vertices.size() * sizeof(fo3hudassets::Vertex),
                 vertices.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          sizeof(fo3hudassets::Vertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                          sizeof(fo3hudassets::Vertex),
                          reinterpret_cast<const void *>(3 * sizeof(float)));
    r.key = std::move(key);
  }
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                      GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(font.program);
  glUniformMatrix4fv(font.mvpLoc, 1, GL_FALSE, mvp);
  glUniform1i(font.texLoc, 0);
  glBindVertexArray(r.vao);
  glBindTexture(GL_TEXTURE_2D, r.solid);
  glUniform4f(font.tintLoc, .015f, .035f, .025f, .87f);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  if (r.highlight) {
    glUniform4f(font.tintLoc, 26.0f / 255, 1, 128.0f / 255, .22f);
    glDrawArrays(GL_TRIANGLES, 6, 6);
  }
  glUniform4f(font.tintLoc, 26.0f / 255, 1, 128.0f / 255, 1);
  if (!panel.rows.empty() && panel.rows[panel.selected].allowed) {
    glBindTexture(GL_TEXTURE_2D, font.interfaceTexture);
    glDrawArrays(GL_TRIANGLES, r.buttonStart, 6);
  }
  glBindTexture(GL_TEXTURE_2D, font.fontTexture);
  glDrawArrays(GL_TRIANGLES, r.textStart, r.textCount);
}
} // namespace fo3lootui
