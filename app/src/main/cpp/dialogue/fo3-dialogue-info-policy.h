#pragma once
#include "pipboy/fo3-pipboy-data.h"

namespace fo3dialogue {
// Static structure shared by live selection and original-master evidence.
// Conditions, ordering, quest state, speaker and voice checks remain separate.
inline bool SupportedInfoStructure(const fo3pipdata::Info& i) noexcept {
  return (i.type==0||i.type==1||i.type==3)&&!(i.recordFlags&0x20)&&!i.nextSpeaker&&
    !i.flags2&&!(i.flags&0x80)&&!i.responses.empty();
}
}
