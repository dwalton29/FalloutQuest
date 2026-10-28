#pragma once
// HUDTemplates.xml and Fallout3.exe 1.7.0.4 HUD meter helper (0x936a40).
// Native screen units; only their conversion to world metres is VR policy.
#include <algorithm>
#include <cmath>

namespace fo3healthui {
enum class Style { Player, Enemy };
struct Layout {
  float totalWidth, originalX, meterY;
  float bracketX, bracketY, bracketWidth, bracketHeight;
};
inline constexpr Layout AuthoredLayout(Style style){
  return style==Style::Player ? Layout{300,20,41,0,0,512,256}
                              : Layout{240,171,3,-7,3,500,64};
}
constexpr float kTickWidth=8, kTickHeight=20;
constexpr float kMeterAlpha=175.f/255.f;
// The game truncates to whole ticks, or pairs of ticks for centered enemy HP.
// Living actors retain at least one tick; dead actors have no fill.
inline float FillWidth(Style style,float fraction){
  if(!std::isfinite(fraction)||fraction<=0)return 0;
  const float step=style==Style::Enemy ? 16.f : kTickWidth;
  return std::max(kTickWidth,std::floor(AuthoredLayout(style).totalWidth*
                   std::min(fraction,1.f)/step)*step);
}
inline float MeterX(Style style,float fillWidth){
  return AuthoredLayout(style).originalX-(style==Style::Enemy ? fillWidth*.5f : 0.f);
}
} // namespace fo3healthui
