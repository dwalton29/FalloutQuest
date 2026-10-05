#pragma once
#include "world/interaction/fo3-interaction-ray.h"
namespace fo3weapon {
// Returns distance, unlike visibility's boolean triangle test. This is shared
// by authored skinned actor surfaces and projectile swept segments.
inline bool Surface(const fo3interaction::Point&o,const fo3interaction::Point&d,
 const fo3interaction::Point&a,const fo3interaction::Point&b,const fo3interaction::Point&c,float&limit) {
 using namespace fo3interaction;
 const auto e1=Sub(b,a),e2=Sub(c,a),p=Cross(d,e2);const float det=Dot(e1,p);
 if(std::fabs(det)<1e-7f)return false;
 const auto t=Sub(o,a);const float u=Dot(t,p)/det;if(u<0||u>1)return false;
 const auto q=Cross(t,e1);const float v=Dot(d,q)/det;if(v<0||u+v>1)return false;
 const float distance=Dot(e2,q)/det;
 if(!std::isfinite(distance)||distance<0||distance>limit)return false;
 limit=distance;return true;
}
}
