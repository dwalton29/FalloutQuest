#pragma once
#include "world/interaction/fo3-interaction-ray.h"
#include <string>
#include <cctype>
namespace fo3weapon {
enum class Region { Unknown, Head, Torso, LeftArm, RightArm, LeftLeg, RightLeg };
// Humanoid adapter for original Bip01 bone names. Creature names stay unknown.
inline Region BoneRegion(std::string name) {
 for(auto& c:name)c=char(std::tolower((unsigned char)c));
 if(name.find("bip01")==std::string::npos)return Region::Unknown;
 if(name.find("head")!=std::string::npos||name.find("neck")!=std::string::npos)return Region::Head;
 const bool left=name.find(" l ")!=std::string::npos,right=name.find(" r ")!=std::string::npos;
 if(left||right){
  if(name.find("arm")!=std::string::npos||name.find("hand")!=std::string::npos||name.find("finger")!=std::string::npos||name.find("clavicle")!=std::string::npos)return left?Region::LeftArm:Region::RightArm;
  if(name.find("thigh")!=std::string::npos||name.find("calf")!=std::string::npos||name.find("foot")!=std::string::npos||name.find("toe")!=std::string::npos)return left?Region::LeftLeg:Region::RightLeg;
 }
 if(name.find("spine")!=std::string::npos||name.find("pelvis")!=std::string::npos)return Region::Torso;
 return Region::Unknown;
}
// Returns distance, unlike visibility's boolean triangle test. This is shared
// by authored skinned actor surfaces and projectile swept segments.
inline bool Surface(const fo3interaction::Point&o,const fo3interaction::Point&d,
 const fo3interaction::Point&a,const fo3interaction::Point&b,const fo3interaction::Point&c,float&limit) {
 return fo3interaction::Surface(o,d,a,b,c,limit);
}
}
