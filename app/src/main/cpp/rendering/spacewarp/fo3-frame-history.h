#pragma once
// Application SpaceWarp history is owned by successfully submitted stereo
// frames. It is independent of physics ticks, eye draws and asset publications.
#include "rendering/actor-skinning.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>
namespace fqspacewarp {
using Matrix=fqskin::Matrix;
struct Frame {
  uint64_t serial=0,scene=0;
  int64_t displayTime=0;
  uint32_t world=0;
  std::array<float,3> origin{};
  Matrix appToWorld=fqskin::Identity();
  std::array<Matrix,2> viewProjection{fqskin::Identity(),fqskin::Identity()};
  bool sceneReady=false,loading=false,focused=false,modal=false;
};
inline bool Finite(const Matrix& m){for(float f:m)if(!std::isfinite(f))return false;return true;}
inline bool Eligible(const Frame& f){
  if(f.world!=0x3cu||!f.sceneReady||f.loading||!f.focused||f.modal||f.displayTime<=0||!Finite(f.appToWorld))return false;
  for(float v:f.origin)if(!std::isfinite(v))return false;
  return Finite(f.viewProjection[0])&&Finite(f.viewProjection[1]);
}
inline bool CameraCut(const Matrix& a,const Matrix& b){
  float distance=0,rotation=0;
  for(int i=0;i<3;++i){const float d=a[12+i]-b[12+i];distance+=d*d;}
  for(int i:{0,1,2,4,5,6,8,9,10})rotation+=a[i]*b[i];
  // VR policy: a >2 m jump or >30 degree turn invalidates temporal history.
  return distance>4.f||rotation<1.f+2.f*std::cos(.5235987756f);
}
struct Drawable {
  uint64_t geometry=0;
  Matrix model=fqskin::Identity();
  std::vector<fqskin::PaletteRow> palette;
  std::vector<float> deformedPositions;
  size_t Bytes()const{return sizeof(Drawable)+palette.size()*sizeof(fqskin::PaletteRow)+deformedPositions.size()*sizeof(float);}
  bool operator==(const Drawable& b)const{return geometry==b.geometry&&model==b.model&&palette==b.palette&&deformedPositions==b.deformedPositions;}
};
struct Coverage {
  uint64_t serial=0;
  bool motion=false,depth=false,staticWorld=false,cutouts=false,hands=false,weapons=false;
  uint32_t dynamicExpected=0,dynamicWritten=0;
  bool Complete(uint64_t frame)const{return serial==frame&&motion&&depth&&staticWorld&&cutouts&&hands&&weapons&&dynamicExpected==dynamicWritten;}
};
class History {
  Frame previous_{},current_{};
  std::unordered_map<uint64_t,Drawable> committed_,pending_;
  std::array<bool,2> eyes_{};
  size_t pendingBytes_=0;
  unsigned submitted_=0;
  bool candidate_=false,consistent_=true;
public:
  static constexpr size_t MaxHistoryBytes=8u*1024u*1024u,MaxDrawables=2048;
  void Reset(){committed_.clear();pending_.clear();submitted_=0;candidate_=false;eyes_={};pendingBytes_=0;consistent_=true;}
  bool Begin(const Frame& f){
    if(!Eligible(f)){Reset();return false;}
    if(submitted_&&(f.scene!=previous_.scene||f.origin!=previous_.origin||f.displayTime<=previous_.displayTime||CameraCut(previous_.appToWorld,f.appToWorld)))Reset();
    current_=f;pending_.clear();pendingBytes_=0;eyes_={};consistent_=true;candidate_=true;return true;
  }
  bool Record(uint64_t identity,const Drawable& d){
    if(!candidate_)return false;
    if(!identity||!d.geometry||!Finite(d.model)){consistent_=false;return false;}
    for(const auto& row:d.palette)for(float f:row)if(!std::isfinite(f)){consistent_=false;return false;}
    for(float f:d.deformedPositions)if(!std::isfinite(f)){consistent_=false;return false;}
    auto old=pending_.find(identity);
    if(old!=pending_.end()){if(!(old->second==d))consistent_=false;return consistent_;}
    if(pending_.size()>=MaxDrawables||d.Bytes()>MaxHistoryBytes-pendingBytes_){consistent_=false;return false;}
    pendingBytes_+=d.Bytes();pending_.emplace(identity,d);return true;
  }
  const Drawable* Previous(uint64_t identity,uint64_t geometry)const{
    const auto it=committed_.find(identity);
    return it==committed_.end()||it->second.geometry!=geometry?nullptr:&it->second;
  }
  void EyeRendered(unsigned eye){if(candidate_&&eye<2)eyes_[eye]=true;}
  bool Ready()const{return candidate_&&consistent_&&submitted_>=2;}
  bool Active()const{return candidate_;}
  const Frame& CurrentFrame()const{return current_;}
  const Drawable* Current(uint64_t identity)const{const auto it=pending_.find(identity);return it==pending_.end()?nullptr:&it->second;}
  bool CanSynthesize(const std::array<Coverage,2>& coverage)const{return Ready()&&eyes_[0]&&eyes_[1]&&coverage[0].Complete(current_.serial)&&coverage[1].Complete(current_.serial)&&pending_.size()>=std::max(coverage[0].dynamicExpected,coverage[1].dynamicExpected);}
  bool Finish(bool submitted){
    if(!submitted){Reset();return false;}
    if(!candidate_)return false;
    if(!submitted||!eyes_[0]||!eyes_[1]||!consistent_){Reset();return false;}
    committed_.swap(pending_);pending_.clear();previous_=current_;submitted_=std::min(2u,submitted_+1);candidate_=false;return true;
  }
  const Frame& PreviousFrame()const{return previous_;}
  unsigned SubmittedFrames()const{return submitted_;}
};
} // namespace fqspacewarp
