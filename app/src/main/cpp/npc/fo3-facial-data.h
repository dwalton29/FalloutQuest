#pragma once
#include <array>
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
namespace fo3face {
// Original Fallout3.exe: speech table 0x10FE230; LIP reader 0x62D2D0,
// frame reader 0x62D510, playback 0x62CFB0 (1/30 second per frame).
inline constexpr std::array<const char*,16> SpeechNames{
  "Aah","BigAah","BMP","ChJSh","DST","Eee","Eh","FV","I","K","N","Oh","OohQ","R","Th","W"};
// Original modifier table 0x10FE1E8, matched by 0x5FE222/0x5FE652
// (17 entries); LIP playback submits the separate 17-float block at 0x62D0AA.
inline constexpr std::array<const char*,33> MorphNames{
  "Aah","BigAah","BMP","ChJSh","DST","Eee","Eh","FV","I","K","N","Oh","OohQ","R","Th","W",
  "BlinkLeft","BlinkRight","BrowDownLeft","BrowDownRight","BrowInLeft","BrowInRight","BrowUpLeft","BrowUpRight",
  "LookDown","LookLeft","LookRight","LookUp","SquintLeft","SquintRight","HeadPitch","HeadRoll","HeadYaw"};
using Weights=std::array<float,MorphNames.size()>;
using ExpandedDeltas=std::array<std::vector<std::array<float,3>>,MorphNames.size()>;
// Only position components change; normals, UVs and skin attributes remain.
inline bool BlendExpanded(const std::vector<float>& bind,const ExpandedDeltas& deltas,const Weights& weights,std::vector<float>& out) {
  if(bind.size()%18)return false;
  for(size_t c=0;c<MorphNames.size();++c)if(!std::isfinite(weights[c])||(!deltas[c].empty()&&deltas[c].size()!=bind.size()/18))return false;
  out=bind;
  for(size_t c=0;c<MorphNames.size();++c)if(weights[c]!=0)
    for(size_t v=0;v<deltas[c].size();++v)for(size_t axis=0;axis<3;++axis)out[v*18+axis]+=deltas[c][v][axis]*weights[c];
  return true;
}
struct LipFrame { std::array<float,16> speech{};std::array<float,17> modifiers{}; };
struct Lip { int32_t firstFrame=0;std::vector<LipFrame> frames; };
inline uint32_t U32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline float F32(const uint8_t* p){auto u=U32(p);float f;std::memcpy(&f,&u,4);return f;}
inline bool DecodeLip(const std::vector<uint8_t>& b,Lip& out,std::string& error) {
  out={};error.clear();auto fail=[&](const char* s){error=s;return false;};
  if(b.size()<12||b.size()>8*1024*1024)return fail("LIP size");
  if(U32(b.data())!=1)return fail("LIP version");
  const auto allocation=U32(b.data()+4),flags=U32(b.data()+8);
  if(allocation<24||allocation>16*1024*1024||flags>1)return fail("LIP header");
  const size_t expected=allocation-16;std::vector<uint8_t> raw;raw.reserve(expected);
  if(flags==0)raw.assign(b.begin()+12,b.end());
  else for(size_t at=12;at<b.size();) {
    const auto c=b[at++];
    if(c){if(raw.size()>=expected)return fail("LIP overflow");raw.push_back(c);}
    else {
      if(b.size()-at<2)return fail("LIP truncated zero run");
      const size_t n=b[at]|(size_t(b[at+1])<<8);at+=2;
      if(n==0||n>expected-raw.size())return fail("LIP zero run overflow");
      raw.resize(raw.size()+n,0);
    }
  }
  if(raw.size()!=expected||raw.size()<8)return fail("LIP decoded size");
  const uint32_t count=U32(raw.data());const int32_t first=int32_t(U32(raw.data()+4));
  if(!count||count>100000||first>10000||first< -10000||raw.size()!=8+size_t(count)*132)return fail("LIP frame layout");
  Lip decoded;decoded.firstFrame=first;decoded.frames.resize(count);
  for(size_t i=0;i<count;++i)for(size_t c=0;c<33;++c) {
    const float v=F32(raw.data()+8+(i*33+c)*4);
    if(!std::isfinite(v)||std::fabs(v)>100)return fail("LIP nonfinite/out-of-range channel");
    if(c<16)decoded.frames[i].speech[c]=v;else decoded.frames[i].modifiers[c-16]=v;
  }
  out=std::move(decoded);return true;
}
inline Weights Sample(const Lip& lip,double seconds) {
  Weights out{};if(lip.frames.empty()||!std::isfinite(seconds)||seconds<0)return out;
  const double frame=seconds*30-lip.firstFrame;
  if(frame<0||frame>=lip.frames.size())return out;
  const size_t a=size_t(frame),b=std::min(a+1,lip.frames.size()-1);const float f=float(frame-a);
  for(size_t c=0;c<16;++c)out[c]=lip.frames[a].speech[c]*(1-f)+lip.frames[b].speech[c]*f;
  for(size_t c=0;c<17;++c)out[16+c]=lip.frames[a].modifiers[c]*(1-f)+lip.frames[b].modifiers[c]*f;
  return out;
}
struct Tri {
  uint32_t vertices=0,differentialCount=0,sparseCount=0;
  std::unordered_map<std::string,std::vector<std::array<float,3>>> morphs;
};
// FRTRI003 differential targets. Statistical EGM FaceGen stays in the bind
// mesh; these differential offsets are added to it, never replace its shape.
inline bool DecodeTri(const std::vector<uint8_t>& b,Tri& out,std::string& error) {
  out={};error.clear();auto fail=[&](const char* s){error=s;return false;};
  if(b.size()<64||b.size()>32*1024*1024||std::memcmp(b.data(),"FRTRI003",8))return fail("TRI header");
  const auto nv=U32(b.data()+8),nf=U32(b.data()+12),nq=U32(b.data()+16),uv=U32(b.data()+28),flags=U32(b.data()+32);
  const auto nm=U32(b.data()+36),mods=U32(b.data()+40),extra=U32(b.data()+44);
  if(!nv||nv>100000||nf>200000||nq||nm>256||mods>256||extra>100000||flags!=1||uv!=nv)return fail("TRI unsupported layout");
  if((size_t(nm)+mods)*nv>64*1024*1024/sizeof(std::array<float,3>))return fail("TRI decoded morph budget");
  size_t at=64+(size_t(nv)+extra)*12+size_t(nf)*12+size_t(uv)*8+size_t(nf)*12;
  if(at>b.size())return fail("TRI geometry truncated");
  Tri decoded;decoded.vertices=nv;decoded.differentialCount=nm;decoded.sparseCount=mods;
  std::vector<std::array<float,3>> base(nv),absolute(extra);
  size_t geometry=64;
  for(auto* points:{&base,&absolute})for(auto& point:*points)for(auto& c:point) {
    c=F32(b.data()+geometry);geometry+=4;
    if(!std::isfinite(c)||std::fabs(c)>1000000)return fail("TRI base/modifier coordinate");
  }
  auto name=[&](std::string& s){
    if(b.size()-at<4)return false;
    const auto n=U32(b.data()+at);at+=4;
    if(!n||n>256||n>b.size()-at||b[at+n-1]!=0)return false;
    s.assign(reinterpret_cast<const char*>(b.data()+at),n-1);at+=n;return !s.empty()&&s.find('\0')==std::string::npos;
  };
  for(uint32_t m=0;m<nm;++m) {
    std::string s;if(!name(s)||b.size()-at<4+size_t(nv)*6||decoded.morphs.count(s))return fail("TRI morph truncated/duplicate");
    const float scale=F32(b.data()+at);at+=4;if(!std::isfinite(scale)||scale<0||scale>1)return fail("TRI morph scale");
    auto& deltas=decoded.morphs[s];deltas.resize(nv);
    for(auto& delta:deltas)for(auto& c:delta){c=float(int16_t(uint16_t(b[at])|(uint16_t(b[at+1])<<8)))*scale;at+=2;}
  }
  // NifTools FRTRI003 ModifierRecord: names and base-vertex indices refer to
  // the absolute modifier vertices preceding the triangle/UV arrays.
  size_t used=0;
  for(uint32_t m=0;m<mods;++m) {
    std::string s;if(!name(s)||b.size()-at<4||decoded.morphs.count(s))return fail("TRI modifier name/count/duplicate");
    const auto count=U32(b.data()+at);at+=4;
    if(count>extra-used||size_t(count)*4>b.size()-at)return fail("TRI modifier vertices");
    auto& delta=decoded.morphs[s];delta.resize(nv);std::vector<bool> seen(nv,false);
    for(uint32_t i=0;i<count;++i){const auto vertex=U32(b.data()+at);at+=4;
      if(vertex>=nv||seen[vertex])return fail("TRI modifier index");
      seen[vertex]=true;
      for(size_t axis=0;axis<3;++axis)delta[vertex][axis]=absolute[used+i][axis]-base[vertex][axis];
    }
    used+=count;
  }
  if(used!=extra||at!=b.size())return fail("TRI trailing bytes/modifier count");
  out=std::move(decoded);return true;
}
}
