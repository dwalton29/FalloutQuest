#pragma once
#include <array>
#include <cmath>
namespace fo3scale {
inline bool Valid(float scale){return std::isfinite(scale)&&scale>=.1f&&scale<=10.f;}
// Local OpenXR space has an eye-level origin. Scale around the calibrated
// physical floor so shrink/grow never changes the authored world floor.
inline std::array<float,3> Local(std::array<float,3> point,float scale,float floor){return {point[0]*scale,floor+(point[1]-floor)*scale,point[2]*scale};}
}
