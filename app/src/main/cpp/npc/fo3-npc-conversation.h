#pragma once
#include "dialogue/fo3-dialogue-conditions.h"
#include <cstdint>
#include <string>
namespace fo3npcchat {
struct Line {
  uint32_t topic=0,info=0,speaker=0,listener=0;
  std::string audio,subtitle;
  bool Valid() const {return info&&speaker&&listener&&!audio.empty();}
};
// Read-only, fail-closed authored HELLO. Does not run GREETING activation,
// quest scripts, INFO results, or manipulate player dialogue/quest state.
Line SelectHello(const fo3pipdata::Definitions&,const fo3player::Player&,
                 const fo3dialogue::Context&);
}
