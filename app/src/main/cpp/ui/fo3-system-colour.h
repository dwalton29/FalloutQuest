#pragma once
#include <cmath>
namespace fo3uicolor {
// FALLOUT.INI Interface: HUDMain/System/Pipboy RGB(26,255,128), HUDAlt
// RGB(255,67,42). Authored channel values are sRGB display values.
inline constexpr float Main[3]{26.f/255.f,1.f,128.f/255.f};
inline constexpr float Crime[3]{1.f,67.f/255.f,42.f/255.f};
inline float Linear(float value){return value<=.04045f?value/12.92f:std::pow((value+.055f)/1.055f,2.4f);}
}
