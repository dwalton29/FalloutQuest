#pragma once
#include "npc/fo3-npc-state.h"
#include "world/interaction/fo3-loot-cursor.h"
#include "ui/interaction/fo3-font-layout.h"
#include <string>
#include <vector>
namespace fo3dialogue {
struct Panel {
  bool visible=false;
  std::array<float,3> anchor{};
  float yaw=0;
  std::string title,subtitle;
  std::vector<std::string> choices;
  size_t selected=0;
  uint64_t revision=0;
};
// Uses the canonical authored glyph advances. Preserve every non-whitespace byte.
inline std::vector<std::string> Wrap(const fo3font::Metrics& font,const std::string& text,float width) {
  std::vector<std::string> lines;size_t start=0;
  while(start<text.size()) {
    auto end=text.find('\n',start);if(end==std::string::npos)end=text.size();
    auto remaining=std::string_view(text).substr(start,end-start);
    size_t count=fo3font::FitText(font,remaining,width);
    if(count<remaining.size()) {
      count=std::max(size_t(1),count);auto space=remaining.rfind(' ',count);
      if(space!=std::string_view::npos&&space>0)count=space;
    }
    lines.push_back(text.substr(start,count));start+=count;
    while(start<text.size()&&(text[start]==' '||text[start]=='\r'))++start;
    if(start<text.size()&&text[start]=='\n')++start;
  }
  return lines;
}
inline void SmoothAnchor(Panel& panel,const std::array<float,3>& target,float dt,bool first) {
  const float blend=first?1.f:1-std::exp(-std::max(0.f,dt)/fo3npc::DialoguePolicy::AnchorTime);
  for(size_t i=0;i<3;++i)panel.anchor[i]+=(target[i]-panel.anchor[i])*blend;
}
}
