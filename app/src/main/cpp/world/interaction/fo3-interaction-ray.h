#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace fo3interaction {
using Point = std::array<float, 3>;
inline bool ClipBoxAxis(float origin, float direction, float lo, float hi,
                        float& entryDistance, float& exitDistance) {
  if (!std::isfinite(origin) || !std::isfinite(direction) ||
      !std::isfinite(lo) || !std::isfinite(hi) || lo > hi) return false;
  if (std::fabs(direction) < 1e-6f) return origin >= lo && origin <= hi;
  float a = (lo - origin) / direction, b = (hi - origin) / direction;
  if (a > b) std::swap(a, b);
  entryDistance = std::max(entryDistance, a);
  exitDistance = std::min(exitDistance, b);
  return entryDistance <= exitDistance;
}
inline bool Box(const Point &o, const Point &d, const Point &lo,
                const Point &hi, float limit, float &distance) {
  float entryDistance = 0, exitDistance = limit;
  for (int i = 0; i < 3; ++i)
    if (!ClipBoxAxis(o[i], d[i], lo[i], hi[i], entryDistance, exitDistance)) return false;
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
// Exact first-surface parameter, also used by swept projectiles.
inline bool Surface(const Point& o,const Point& d,const Point& a,const Point& b,const Point& c,float& limit){
  const auto e1=Sub(b,a),e2=Sub(c,a),p=Cross(d,e2);const float det=Dot(e1,p);
  if(std::fabs(det)<1e-7f)return false;
  const auto t=Sub(o,a);const float u=Dot(t,p)/det;if(u<0||u>1)return false;
  const auto q=Cross(t,e1);const float v=Dot(d,q)/det;if(v<0||u+v>1)return false;
  const float distance=Dot(e2,q)/det;
  if(!std::isfinite(distance)||distance<0||distance>limit)return false;
  limit=distance;return true;
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
