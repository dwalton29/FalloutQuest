#pragma once
#include "fo3-npc-appearance.h"
#include "fo3-pipboy-state.h"
#include "rendering/mesh/fo3-static-nif.h"
#include <array>
#include <limits>
namespace fo3pip {
// Conjugate authored game rotation into the exact mesh upload coordinate frame,
// including the renderer's floor offset. Units affect translations only.
inline fo3anim::Matrix BindInRenderCoordinates(fo3anim::Matrix bind,
                                               float units, float floor,
                                               float forward) {
  for (int i = 0; i < 3; ++i)
    bind[12 + i] /= units;
  auto c = fo3anim::Identity();
  c[5] = 0;
  c[6] = -1;
  c[9] = 1;
  c[10] = 0;
  c[13] = floor;
  c[14] = forward;
  fo3anim::Matrix inverse;
  fo3anim::Inverse(c, inverse);
  return fo3anim::Multiply(c, fo3anim::Multiply(bind, inverse));
}
inline bool Screen(const Fo3StaticNifMesh &m) {
  return fo3appearance::SameModel(m.shapeName, "pipboyscreen:0") &&
         fo3appearance::SameModel(m.diffuseTexturePath,
                                  "textures/pipboy3000/screen.dds") &&
         m.noLighting && !m.alphaBlend;
}
struct Surface {
  V center{}, normal{};
  std::array<float, 2> lo{}, hi{};
  bool valid = false;
};
inline Surface Inspect(const Fo3StaticNifMesh &m) {
  Surface s;
  if (!Screen(m) || m.positions.empty() ||
      m.texcoords.size() * 3 != m.positions.size() * 2 ||
      m.normals.size() != m.positions.size())
    return s;
  s.lo = {INFINITY, INFINITY};
  s.hi = {-INFINITY, -INFINITY};
  size_t n = m.positions.size() / 3;
  for (size_t i = 0; i < n; ++i) {
    s.center.x += m.positions[i * 3];
    s.center.y += m.positions[i * 3 + 1];
    s.center.z += m.positions[i * 3 + 2];
    s.normal.x += m.normals[i * 3];
    s.normal.y += m.normals[i * 3 + 1];
    s.normal.z += m.normals[i * 3 + 2];
    for (int c = 0; c < 2; ++c) {
      s.lo[c] = std::min(s.lo[c], m.texcoords[i * 2 + c]);
      s.hi[c] = std::max(s.hi[c], m.texcoords[i * 2 + c]);
    }
  }
  s.center = {s.center.x / n, s.center.y / n, s.center.z / n};
  s.normal = Unit(s.normal);
  s.valid = s.hi[0] > s.lo[0] && s.hi[1] > s.lo[1] && Length(s.normal) > .9f;
  return s;
}
inline void ScreenUvs(Fo3StaticNifMesh &m, const Surface &s) {
  if (!s.valid)
    return;
  for (size_t i = 0; i < m.texcoords.size() / 2; ++i) {
    m.texcoords[i * 2] = (m.texcoords[i * 2] - s.lo[0]) / (s.hi[0] - s.lo[0]);
    m.texcoords[i * 2 + 1] =
        (s.hi[1] - m.texcoords[i * 2 + 1]) / (s.hi[1] - s.lo[1]);
  }
}
} // namespace fo3pip
