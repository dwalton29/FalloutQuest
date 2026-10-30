#pragma once
#include <array>
#include <cmath>
#include <string>
#include <vector>
#include <unordered_set>
namespace fo3player {
struct CorpseBone {
 std::string name;std::array<float,16> global{};
 std::array<float,3> linear{},angular{}; // NIF units/s and radians/s in actor axes.
 bool operator==(const CorpseBone& other)const{return name==other.name&&global==other.global&&linear==other.linear&&angular==other.angular;}
};
struct CorpsePose {
 std::string model;std::vector<CorpseBone> bones;bool sleeping=false;
 bool operator==(const CorpsePose& other)const{return model==other.model&&bones==other.bones&&sleeping==other.sleeping;}
};
inline bool ValidCorpsePose(const CorpsePose& p){
 if(p.model.empty()||p.model.size()>512||p.model.find('\0')!=p.model.npos||p.bones.size()<2||p.bones.size()>64)return false;
 std::unordered_set<std::string> names;
 for(const auto& b:p.bones){if(b.name.empty()||b.name.size()>128||b.name.find('\0')!=b.name.npos||!names.insert(b.name).second)return false;
  for(const auto* velocity:{&b.linear,&b.angular})for(float v:*velocity)if(!std::isfinite(v)||std::abs(v)>1e6f)return false;
  for(float v:b.global)if(!std::isfinite(v)||std::abs(v)>1e6f)return false;
  if(std::abs(b.global[3])+std::abs(b.global[7])+std::abs(b.global[11])+std::abs(b.global[15]-1)>.001f)return false;
  float lengths[3]{};
  for(int axis=0;axis<3;++axis)for(int r=0;r<3;++r)lengths[axis]+=b.global[axis*4+r]*b.global[axis*4+r];
  if(lengths[0]<.0001f||lengths[0]>10000)return false;
  for(int axis=1;axis<3;++axis)if(std::abs(lengths[axis]/lengths[0]-1)>.005f)return false;
  for(int a=0;a<3;++a)for(int c=a+1;c<3;++c){float dot=0;for(int r=0;r<3;++r)dot+=b.global[a*4+r]*b.global[c*4+r];if(std::abs(dot)/lengths[0]>.005f)return false;}
  const auto& m=b.global;const float determinant=m[0]*(m[5]*m[10]-m[9]*m[6])-m[4]*(m[1]*m[10]-m[9]*m[2])+m[8]*(m[1]*m[6]-m[5]*m[2]);if(determinant<=0)return false;
 }
 return true;
}
}
