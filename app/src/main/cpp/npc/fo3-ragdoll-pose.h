#pragma once
#include "fo3-actor-animation.h"
#include "player/fo3-corpse-pose.h"
#include <functional>
namespace fo3rag {
// Physics uses rigid metre transforms; skinning uses NIF globals. Keep scale
// in geometry/pivots rather than feeding a scaled basis to Bullet.
inline bool Rigid(fo3anim::Matrix& m){
 for(int c=0;c<3;++c){float n=0;for(int r=0;r<3;++r)n+=m[c*4+r]*m[c*4+r];if(!std::isfinite(n)||n<1e-12f)return false;n=std::sqrt(n);for(int r=0;r<3;++r)m[c*4+r]/=n;}
 m[3]=m[7]=m[11]=0;m[15]=1;return true;
}
inline bool ApplyPose(const fo3anim::Skeleton& skeleton,fo3anim::Pose& pose,const fo3player::CorpsePose& corpse){
 if(!fo3player::ValidCorpsePose(corpse)||pose.local.size()!=skeleton.bones.size())return false;
 const size_t n=skeleton.bones.size();std::vector<int> overrides(n,-1);
 for(size_t i=0;i<corpse.bones.size();++i){const int bone=fo3anim::FindBone(skeleton,corpse.bones[i].name);if(bone<0)return false;overrides[bone]=int(i);}
 pose.global.resize(n);pose.delta.resize(n);std::vector<uint8_t> visited(n);
 std::function<bool(size_t)> compose=[&](size_t i){if(visited[i]==2)return true;if(visited[i]==1)return false;visited[i]=1;
  if(overrides[i]>=0)pose.global[i]=corpse.bones[size_t(overrides[i])].global;
  else{const int parent=skeleton.bones[i].parent;auto local=fo3anim::ToMatrix(pose.local[i]);if(parent>=0){if(size_t(parent)>=n||!compose(size_t(parent)))return false;local=fo3anim::Multiply(pose.global[size_t(parent)],local);}pose.global[i]=local;}
  pose.delta[i]=fo3anim::Multiply(pose.global[i],skeleton.inverseBind[i]);visited[i]=2;return true;};
 for(size_t i=0;i<n;++i)if(!compose(i))return false;
 return true;
}
}
