#pragma once
#include <string>

// Paths observed in original human and creature animation trees. Resolve the
// real file through the asset loader; never replace an absent creature clip
// with a humanoid animation.
inline std::string Fo3ActorClipPath(const std::string& skeleton,const char* clip,
                                  bool creature,bool female=false,bool locomotion=false){
    const auto slash=skeleton.find_last_of("\\/");
    const auto root=slash==std::string::npos?std::string{}:skeleton.substr(0,slash+1);
    if(creature)return root+(locomotion?"locomotion\\":"")+clip+".kf";
    return root+"locomotion\\"+(locomotion?(female?"female\\":"male\\"):"")+clip+".kf";
}
