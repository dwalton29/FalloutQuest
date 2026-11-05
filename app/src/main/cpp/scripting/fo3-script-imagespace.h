#pragma once
#include "data/fo3-esm-reader.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>
namespace fo3script {
struct ImageKey {float time=0;std::array<float,4> value{};};
struct ImageCurve {
  std::vector<ImageKey> keys;
  std::array<float,4> Sample(float time,std::array<float,4> fallback={}) const {
    if(keys.empty())return fallback;
    if(time<=keys.front().time)return keys.front().value;
    const auto hi=std::upper_bound(keys.begin(),keys.end(),time,[](float t,const ImageKey& k){return t<k.time;});
    if(hi==keys.end())return keys.back().value;
    const auto& lo=*(hi-1);const float u=(time-lo.time)/(hi->time-lo.time);auto v=lo.value;
    for(size_t i=0;i<4;++i)v[i]+=(hi->value[i]-v[i])*u;
    return v;
  }
};
struct ImageModifier {
  bool valid=false;uint32_t flags=0;float duration=0;
  ImageCurve blur,fade,tint,saturationMult,saturationAdd,brightnessMult,brightnessAdd,contrastMult,contrastAdd;
};
inline bool ReadImageCurve(const uint8_t* bytes,uint32_t size,int dimensions,ImageCurve& out){
  const auto stride=uint32_t(4*(dimensions+1));if(!size||size%stride||size/stride>4096)return false;
  float last=-1;for(uint32_t i=0;i<size;i+=stride){ImageKey k;k.time=fo3esm::ReadF32(bytes+i);
    if(!std::isfinite(k.time)||k.time<0||k.time<=last)return false;
    last=k.time;
    for(int d=0;d<dimensions;++d){k.value[d]=fo3esm::ReadF32(bytes+i+4*(d+1));if(!std::isfinite(k.value[d]))return false;}
    out.keys.push_back(k);
  }return true;
}
inline ImageModifier DecodeImageModifier(const std::vector<uint8_t>& payload){
  ImageModifier m;bool header=false,ok=true;
  fo3esm::WalkSubrecords(payload.data(),payload.size(),[&](const char* t,const uint8_t* b,uint32_t n){
    if(!std::memcmp(t,"DNAM",4)&&n>=8){m.flags=fo3esm::ReadU32(b);m.duration=fo3esm::ReadF32(b+4);header=true;return;}
    ImageCurve* c=nullptr;int dimensions=1;
    if(!std::memcmp(t,"BNAM",4))c=&m.blur;
    if(!std::memcmp(t,"NAM3",4)){c=&m.fade;dimensions=4;}
    if(!std::memcmp(t,"TNAM",4)){c=&m.tint;dimensions=4;}
    if(t[1]=='I'&&t[2]=='A'&&t[3]=='D')switch(uint8_t(t[0])){
      case 0x11:c=&m.saturationMult;break;case 'Q':c=&m.saturationAdd;break;
      case 0x12:c=&m.brightnessMult;break;case 'R':c=&m.brightnessAdd;break;
      case 0x13:c=&m.contrastMult;break;case 'S':c=&m.contrastAdd;break;default:break;
    }
    if(c){if(!c->keys.empty()||!ReadImageCurve(b,n,dimensions,*c))ok=false;}
  });m.valid=ok&&header&&std::isfinite(m.duration)&&m.duration>0;return m;
}
struct ImagePresentation {
  std::array<float,4> fade{},tint{};float blur=0,saturation=1,brightness=1,contrast=1;
};
inline ImagePresentation SampleImageModifier(const ImageModifier& m,double elapsed){
  ImagePresentation p;if(!m.valid||!std::isfinite(elapsed)||elapsed<0||((m.flags&1)&&elapsed>=m.duration))return p;
  const float t=(m.flags&1)?float(elapsed/m.duration):0.f;p.fade=m.fade.Sample(t);p.tint=m.tint.Sample(t);
  p.blur=std::clamp(m.blur.Sample(t)[0],0.f,32.f);
  const std::array<float,4> one{1,1,1,1};
  p.saturation=std::clamp(m.saturationMult.Sample(t,one)[0]+m.saturationAdd.Sample(t)[0],0.f,10.f);
  p.brightness=std::clamp(m.brightnessMult.Sample(t,one)[0]+m.brightnessAdd.Sample(t)[0],0.f,10.f);
  p.contrast=std::clamp(m.contrastMult.Sample(t,one)[0]+m.contrastAdd.Sample(t)[0],0.f,10.f);
  return p;
}
// Ordered independent modifiers: a static base remains until Remove, while
// animated entries expire on their original duration. Reapplying restarts one.
struct ImageStack {
  struct Entry {uint32_t form;ImageModifier modifier;double started;};
  std::vector<Entry> entries;
  void Apply(uint32_t form,const ImageModifier& modifier,double now){
    Remove(form);if(modifier.valid&&entries.size()<32)entries.push_back({form,modifier,now});
  }
  void Remove(uint32_t form){entries.erase(std::remove_if(entries.begin(),entries.end(),[&](const auto& e){return e.form==form;}),entries.end());}
  ImagePresentation Sample(double now){
    entries.erase(std::remove_if(entries.begin(),entries.end(),[&](const auto& e){return (e.modifier.flags&1)&&now-e.started>=e.modifier.duration;}),entries.end());
    ImagePresentation result;
    auto over=[](std::array<float,4>& dst,const std::array<float,4>& src){
      const float a=std::clamp(src[3],0.f,1.f),b=std::clamp(dst[3],0.f,1.f),out=a+b*(1-a);
      for(size_t c=0;c<3;++c)dst[c]=out>0?(src[c]*a+dst[c]*b*(1-a))/out:0;
      dst[3]=out;
    };
    for(const auto& e:entries){const auto p=SampleImageModifier(e.modifier,now-e.started);
      over(result.fade,p.fade);over(result.tint,p.tint);result.blur=std::max(result.blur,p.blur);
      result.saturation*=p.saturation;result.brightness*=p.brightness;result.contrast*=p.contrast;
    }return result;
  }
};

}
