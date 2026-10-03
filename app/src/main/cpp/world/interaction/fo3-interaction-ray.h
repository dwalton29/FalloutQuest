#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace fo3interaction {
using Point = std::array<float, 3>;
inline bool Box(const Point &o, const Point &d, const Point &lo,
                const Point &hi, float limit, float &distance) {
  float entryDistance = 0, exitDistance = limit;
  for (int i = 0; i < 3; ++i) {
    if (!std::isfinite(o[i]) || !std::isfinite(d[i]) || !std::isfinite(lo[i]) ||
        !std::isfinite(hi[i]) || lo[i] > hi[i])
      return false;
    if (std::fabs(d[i]) < 1e-6f) {
      if (o[i] < lo[i] || o[i] > hi[i])
        return false;
    } else {
      float a = (lo[i] - o[i]) / d[i], b = (hi[i] - o[i]) / d[i];
      if (a > b)
        std::swap(a, b);
      entryDistance = std::max(entryDistance, a);
      exitDistance = std::min(exitDistance, b);
      if (entryDistance > exitDistance)
        return false;
    }
  }
  distance = entryDistance;
  return exitDistance >= 0 && entryDistance <= limit;
}
inline Point Sub(const Point &a, const Point &b) {
  return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
inline Point Cross(const Point &a, const Point &b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
          a[0] * b[1] - a[1] * b[0]};
}
inline float Dot(const Point &a, const Point &b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
inline bool Triangle(const Point &o, const Point &d, const Point &a,
                     const Point &b, const Point &c, float limit) {
  const auto e1 = Sub(b, a), e2 = Sub(c, a), p = Cross(d, e2);
  const float determinant = Dot(e1, p);
  if (std::fabs(determinant) < 1e-7f)
    return false;
  const auto t = Sub(o, a);
  const float u = Dot(t, p) / determinant;
  if (u < 0 || u > 1)
    return false;
  const auto q = Cross(t, e1);
  const float v = Dot(d, q) / determinant;
  if (v < 0 || u + v > 1)
    return false;
  const float hit = Dot(e2, q) / determinant;
  return std::isfinite(hit) && hit > 0.005f && hit < limit - 0.01f;
}
} // namespace fo3interaction
