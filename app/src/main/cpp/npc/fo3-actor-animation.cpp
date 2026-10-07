#include <chrono>
#include "fo3-actor-animation.h"
#include <algorithm>
#include <cmath>
#include <functional>

namespace fo3anim {
Matrix Identity() { return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; }
Matrix Multiply(const Matrix &a, const Matrix &b) {
  Matrix m{};
  for (int c = 0; c < 4; ++c)
    for (int r = 0; r < 4; ++r)
      for (int k = 0; k < 4; ++k)
        m[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
  return m;
}
static std::array<float, 4> Normalize(std::array<float, 4> q) {
  float n = 0;
  for (float v : q)
    n += v * v;
  if (!std::isfinite(n) || n < 1e-12f)
    return {1, 0, 0, 0};
  for (float &v : q)
    v /= std::sqrt(n);
  return q;
}
Matrix ToMatrix(const Transform &t) {
  auto q = Normalize(t.rotation);
  float w = q[0], x = q[1], y = q[2], z = q[3], s = t.scale;
  return {(1 - 2 * y * y - 2 * z * z) * s,
          (2 * x * y + 2 * w * z) * s,
          (2 * x * z - 2 * w * y) * s,
          0,
          (2 * x * y - 2 * w * z) * s,
          (1 - 2 * x * x - 2 * z * z) * s,
          (2 * y * z + 2 * w * x) * s,
          0,
          (2 * x * z + 2 * w * y) * s,
          (2 * y * z - 2 * w * x) * s,
          (1 - 2 * x * x - 2 * y * y) * s,
          0,
          t.translation[0],
          t.translation[1],
          t.translation[2],
          1};
}
bool Inverse(const Matrix &a, Matrix &m) {
  float d = a[0] * (a[5] * a[10] - a[9] * a[6]) -
            a[4] * (a[1] * a[10] - a[9] * a[2]) +
            a[8] * (a[1] * a[6] - a[5] * a[2]);
  if (!std::isfinite(d) || std::fabs(d) < 1e-12f)
    return false;
  m = Identity();
  m[0] = (a[5] * a[10] - a[9] * a[6]) / d;
  m[4] = (a[8] * a[6] - a[4] * a[10]) / d;
  m[8] = (a[4] * a[9] - a[8] * a[5]) / d;
  m[1] = (a[9] * a[2] - a[1] * a[10]) / d;
  m[5] = (a[0] * a[10] - a[8] * a[2]) / d;
  m[9] = (a[8] * a[1] - a[0] * a[9]) / d;
  m[2] = (a[1] * a[6] - a[5] * a[2]) / d;
  m[6] = (a[4] * a[2] - a[0] * a[6]) / d;
  m[10] = (a[0] * a[5] - a[4] * a[1]) / d;
  for (int r = 0; r < 3; ++r)
    m[12 + r] = -(m[r] * a[12] + m[4 + r] * a[13] + m[8 + r] * a[14]);
  return true;
}
std::array<float, 3> Point(const Matrix &m, const std::array<float, 3> &p,
                           bool direction) {
  std::array<float, 3> v{};
  for (int r = 0; r < 3; ++r)
    v[r] = m[r] * p[0] + m[4 + r] * p[1] + m[8 + r] * p[2] +
           (direction ? 0 : m[12 + r]);
  return v;
}
std::array<float, 4> QuaternionFromMatrix(const float *m) {
  std::array<float, 4> q{};
  float trace = m[0] + m[4] + m[8];
  if (trace > 0) {
    float s = std::sqrt(trace + 1) * 2;
    q = {s / 4, (m[7] - m[5]) / s, (m[2] - m[6]) / s, (m[3] - m[1]) / s};
  } else {
    int i = m[4] > m[0] ? 1 : 0;
    if (m[8] > m[i * 3 + i])
      i = 2;
    int j = (i + 1) % 3, k = (i + 2) % 3;
    float s = std::sqrt(std::max(0.0f, 1 + m[i * 3 + i] - m[j * 3 + j] -
                                           m[k * 3 + k])) *
              2;
    if (s < 1e-8f)
      return {1, 0, 0, 0};
    q[i + 1] = s / 4;
    q[0] = (m[k * 3 + j] - m[j * 3 + k]) / s;
    q[j + 1] = (m[j * 3 + i] + m[i * 3 + j]) / s;
    q[k + 1] = (m[k * 3 + i] + m[i * 3 + k]) / s;
  }
  return Normalize(q);
}
bool FinalizeSkeleton(Skeleton &s) {
  if (s.bones.empty() || s.bones.size() > 4096)
    return false;
  s.bindGlobal.resize(s.bones.size());
  s.inverseBind.resize(s.bones.size());
  std::vector<uint8_t> state(s.bones.size());
  std::function<bool(size_t)> visit = [&](size_t i) {
    if (state[i] == 2)
      return true;
    if (state[i] == 1)
      return false;
    state[i] = 1;
    int p = s.bones[i].parent;
    if (p >= 0 && (static_cast<size_t>(p) >= s.bones.size() || !visit(p)))
      return false;
    s.bindGlobal[i] = ToMatrix(s.bones[i].bind);
    if (p >= 0)
      s.bindGlobal[i] = Multiply(s.bindGlobal[p], s.bindGlobal[i]);
    if (!Inverse(s.bindGlobal[i], s.inverseBind[i]))
      return false;
    state[i] = 2;
    return true;
  };
  for (size_t i = 0; i < s.bones.size(); ++i)
    if (!visit(i))
      return false;
  return true;
}
int FindBone(const Skeleton &s, const std::string &name) {
  for (size_t i = 0; i < s.bones.size(); ++i)
    if (s.bones[i].name == name)
      return static_cast<int>(i);
  return -1;
}
void BindClip(const Skeleton &s, const Clip &c, Pose &p) {
  p.accumulation=FindBone(s,c.accumulationRoot);
  p.accumulationRoot=c.accumulationRoot;
  p.local.resize(s.bones.size());
  p.global.resize(s.bones.size());
  p.delta.resize(s.bones.size());
  p.evaluated.resize(s.bones.size());
  p.trackBones.clear();
  for (const Track &t : c.tracks)
    p.trackBones.push_back(FindBone(s, t.bone));
}
static std::array<float, 4> Slerp(std::array<float, 4> a,
                                  std::array<float, 4> b, float u) {
  a = Normalize(a);
  b = Normalize(b);
  float d = 0;
  for (int i = 0; i < 4; ++i)
    d += a[i] * b[i];
  if (d < 0) {
    for (float &v : b)
      v = -v;
    d = -d;
  }
  float x = 1 - u, y = u;
  if (d < 0.9995f) {
    float angle = std::acos(std::clamp(d, -1.0f, 1.0f)), sn = std::sin(angle);
    x = std::sin((1 - u) * angle) / sn;
    y = std::sin(u * angle) / sn;
  }
  for (int i = 0; i < 4; ++i)
    a[i] = a[i] * x + b[i] * y;
  return Normalize(a);
}
// de Boor for an open, uniform cubic spline. Only four control points
// contribute.
static std::array<float, 4> Spline(const Channel &c, float u) {
  int n = static_cast<int>(c.controls.size() / c.dimensions), degree = 3;
  u = std::clamp(u, 0.0f, 1.0f);
  float x = u * (n - degree);
  int span = u >= 1 ? n - 1 : degree + static_cast<int>(x);
  auto knot = [&](int i) {
    return i <= degree ? 0.0f
                       : (i >= n ? static_cast<float>(n - degree)
                                 : static_cast<float>(i - degree));
  };
  std::array<std::array<float, 4>, 4> d{};
  for (int j = 0; j <= degree; ++j)
    for (int k = 0; k < c.dimensions; ++k)
      d[j][k] = c.controls[(span - degree + j) * c.dimensions + k];
  for (int r = 1; r <= degree; ++r)
    for (int j = degree; j >= r; --j) {
      int i = span - degree + j;
      float den = knot(i + degree - r + 1) - knot(i),
            a = den > 0 ? (x - knot(i)) / den : 0;
      for (int k = 0; k < c.dimensions; ++k)
        d[j][k] = (1 - a) * d[j - 1][k] + a * d[j][k];
    }
  return d[degree];
}
static std::array<float, 4> Evaluate(const Channel &c, float time, float start,
                                     float stop, bool quaternion) {
  if (!c.controls.empty()) {
    auto v = Spline(c, stop > start ? (time - start) / (stop - start) : 0);
    return quaternion ? Normalize(v) : v;
  }
  if (c.keys.empty())
    return {};
  if (time <= c.keys.front().time)
    return c.keys.front().value;
  if (time >= c.keys.back().time)
    return c.keys.back().value;
  auto hi = std::upper_bound(c.keys.begin(), c.keys.end(), time,
                             [](float t, const Key &k) { return t < k.time; });
  const Key &b = *hi, &a = *(hi - 1);
  float u = (time - a.time) / (b.time - a.time);
  if (quaternion)
    return Slerp(a.value, b.value, u);
  std::array<float, 4> v{};
  for (int i = 0; i < c.dimensions; ++i) {
    if (c.interpolation == 2) {
      float u2 = u * u, u3 = u2 * u;
      v[i] = (2 * u3 - 3 * u2 + 1) * a.value[i] +
             (u3 - 2 * u2 + u) * a.forward[i] +
             (-2 * u3 + 3 * u2) * b.value[i] + (u3 - u2) * b.backward[i];
    } else
      v[i] = a.value[i] * (1 - u) + b.value[i] * u;
  }
  return v;
}
bool Sample(const Skeleton &s, const Clip &c, double elapsed, Pose &p, SampleTimings* timings) {
  const auto started=std::chrono::steady_clock::now();
  if (p.trackBones.size() != c.tracks.size() ||
      p.local.size() != s.bones.size() ||
      p.accumulationRoot != c.accumulationRoot)
    BindClip(s, c, p);
  double d = c.stop - c.start, t = elapsed * c.frequency;
  if (!std::isfinite(t) || d < 0)
    return false;
  if (d > 0) {
    if (c.cycle == 0)
      t = std::fmod(std::max(0.0, t), d);
    else if (c.cycle == 1) {
      t = std::fmod(std::max(0.0, t), 2 * d);
      if (t > d)
        t = 2 * d - t;
    } else
      t = std::clamp(t, 0.0, d);
  } else
    t = 0;
  float time = c.start + static_cast<float>(t);
  for (size_t i = 0; i < s.bones.size(); ++i)
    p.local[i] = s.bones[i].bind;
  const int accumulation=p.accumulation;
  if (accumulation >= 0)
    p.local[accumulation] = Transform{};
  for (size_t i = 0; i < c.tracks.size(); ++i) {
    int bone = p.trackBones[i];
    if (bone < 0)
      continue;
    // KF accumulation belongs to the engine's actor/world motion. FalloutQuest
    // drives the resident actor root from NAVM + collision, so reapplying this
    // track would double root translation/rotation and lift/offset the skeleton.
    if (bone == accumulation)
      continue;
    const Track &track = c.tracks[i];
    Transform &local = p.local[bone];
    if (track.hasTranslation) {
      auto v =
          track.translation.keys.empty() && track.translation.controls.empty()
              ? std::array<float, 4>{track.base.translation[0],
                                     track.base.translation[1],
                                     track.base.translation[2], 0}
              : Evaluate(track.translation, time, track.start, track.stop,
                         false);
      std::copy_n(v.begin(), 3, local.translation.begin());
    }
    if (track.hasRotation) {
      if (track.rotation.interpolation == 4) {
        float angles[3]{};
        for (int axis = 0; axis < 3; ++axis)
          if (!track.xyzRotation[axis].keys.empty())
            angles[axis] = Evaluate(track.xyzRotation[axis], time, track.start,
                                    track.stop, false)[0];
        float cx = std::cos(angles[0] / 2), sx = std::sin(angles[0] / 2),
              cy = std::cos(angles[1] / 2), sy = std::sin(angles[1] / 2),
              cz = std::cos(angles[2] / 2), sz = std::sin(angles[2] / 2);
        local.rotation = {
            cx * cy * cz - sx * sy * sz, sx * cy * cz + cx * sy * sz,
            cx * sy * cz - sx * cy * sz, cx * cy * sz + sx * sy * cz};
      } else
        local.rotation =
            track.rotation.keys.empty() && track.rotation.controls.empty()
                ? track.base.rotation
                : Evaluate(track.rotation, time, track.start, track.stop, true);
    }
    if (track.hasScale)
      local.scale =
          track.scale.keys.empty() && track.scale.controls.empty()
              ? track.base.scale
              : Evaluate(track.scale, time, track.start, track.stop, false)[0];
  }
  const auto skeletonStarted=std::chrono::steady_clock::now();
  if(timings) timings->clipUs=std::chrono::duration<double,std::micro>(skeletonStarted-started).count();
  if(!ComposePose(s,p))return false;
  if(timings) timings->skeletonUs=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-skeletonStarted).count();
  return true;
}
bool ComposePose(const Skeleton& s,Pose& p) {
  if(p.local.size()!=s.bones.size())return false;
  // Skeleton decoder orders parents before children; externally built skeletons
  // are evaluated recursively too, so valid non-topological input is supported.
  std::fill(p.evaluated.begin(), p.evaluated.end(), 0);
  auto visit = [&](auto &&self, size_t i) -> void {
    if (p.evaluated[i])
      return;
    int parent = s.bones[i].parent;
    if (parent >= 0)
      self(self, parent);
    p.global[i] = ToMatrix(p.local[i]);
    if (parent >= 0)
      p.global[i] = Multiply(p.global[parent], p.global[i]);
    p.delta[i] = Multiply(p.global[i], s.inverseBind[i]);
    p.evaluated[i] = 1;
  };
  for (size_t i = 0; i < s.bones.size(); ++i)
    visit(visit, i);
  for (const Matrix &m : p.delta)
    for (float v : m)
      if (!std::isfinite(v))
        return false;
  return true;
}
void LookYaw(const Skeleton& s,Pose& p,int bone,float radians) {
  if(bone<0||size_t(bone)>=p.global.size()||!std::isfinite(radians))return;
  auto turn=Identity();turn[0]=turn[5]=std::cos(radians);turn[1]=std::sin(radians);turn[4]=-turn[1];
  auto pivot=p.global[bone];turn[12]=pivot[12]-turn[0]*pivot[12]-turn[4]*pivot[13];
  turn[13]=pivot[13]-turn[1]*pivot[12]-turn[5]*pivot[13];
  for(size_t i=0;i<s.bones.size();++i) {
    int parent=int(i);bool affected=false;
    while(parent>=0){if(parent==bone){affected=true;break;}parent=s.bones[parent].parent;}
    if(affected){p.global[i]=Multiply(turn,p.global[i]);p.delta[i]=Multiply(p.global[i],s.inverseBind[i]);}
  }
}
} // namespace fo3anim
