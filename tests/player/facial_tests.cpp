#include "npc/fo3-facial-data.h"
#include "audio/fo3-dialogue-face.h"
#include <cassert>
#include <fstream>
#include <iostream>
static std::vector<uint8_t> Read(const char* path){std::ifstream f(path,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
static void Put(std::vector<uint8_t>& b,uint32_t v){for(int i=0;i<4;++i)b.push_back(uint8_t(v>>(i*8)));}
int main(int argc,char** argv) {
  std::vector<uint8_t> bytes;Put(bytes,1);Put(bytes,156);Put(bytes,0);Put(bytes,1);Put(bytes,0);
  for(int i=0;i<33;++i)Put(bytes,i==10?0x3f800000:0);
  fo3face::Lip lip;std::string error;assert(fo3face::DecodeLip(bytes,lip,error));assert(lip.frames.size()==1);
  assert(fo3face::Sample(lip,0)[10]==1&&fo3face::Sample(lip,1)[10]==0);
  auto bad=bytes;bad.pop_back();assert(!fo3face::DecodeLip(bad,lip,error)&&lip.frames.empty());
  bad=bytes;bad[0]=2;assert(!fo3face::DecodeLip(bad,lip,error));
  bad=bytes;bad[12]=255;assert(!fo3face::DecodeLip(bad,lip,error));
  bad=bytes;bad[60]=0xff;bad[61]=0xff;bad[62]=0xff;bad[63]=0x7f;assert(!fo3face::DecodeLip(bad,lip,error));
  // Compressed equivalent: byte-wise zero runs, including zeros INSIDE floats.
  std::vector<uint8_t> packed(bytes.begin(),bytes.begin()+12);packed[8]=1;
  for(size_t i=12;i<bytes.size();)if(bytes[i])packed.push_back(bytes[i++]);else {
    size_t start=i;while(i<bytes.size()&&!bytes[i])++i;const auto n=i-start;
    packed.push_back(0);packed.push_back(uint8_t(n));packed.push_back(uint8_t(n>>8));
  }
  assert(fo3face::DecodeLip(packed,lip,error));
  bad=packed;bad.pop_back();assert(!fo3face::DecodeLip(bad,lip,error));
  bad=packed;bad.insert(bad.end(),{0,255,255});assert(!fo3face::DecodeLip(bad,lip,error));
  fo3audio::DialogueFaceMailbox mailbox;mailbox.Start(1);
  auto asset=std::make_shared<fo3face::Lip>();assert(fo3face::DecodeLip(bytes,*asset,error));
  mailbox.Asset(1,asset);mailbox.Position(1,123);assert(mailbox.Read(1).seconds==.123);
  mailbox.Start(2);mailbox.Asset(1,asset);mailbox.Position(1,999);assert(!mailbox.Read(2).lip&&mailbox.Read(2).seconds<0);
  mailbox.Asset(2,asset);mailbox.Position(2,456);assert(mailbox.Read(2).seconds==.456);mailbox.Start(0);assert(!mailbox.Read(2).lip);
  std::vector<float> bind(36,7),deformed;fo3face::ExpandedDeltas deltas;
  deltas[0]={{{1,2,3}},{{4,5,6}}};fo3face::Weights weights{};weights[0]=.5;
  assert(fo3face::BlendExpanded(bind,deltas,weights,deformed));
  assert(deformed[0]==7.5&&deformed[18]==9);
  for(size_t i=0;i<36;++i)if(i%18>=3)assert(deformed[i]==bind[i]);
  assert(fo3face::BlendExpanded(bind,deltas,{},deformed)&&deformed==bind);
  deltas[0].pop_back();assert(!fo3face::BlendExpanded(bind,deltas,weights,deformed));
  fo3face::Tri tri;assert(!fo3face::DecodeTri({},tri,error));
  if(argc>1) {
    auto original=Read(argv[1]);assert(fo3face::DecodeTri(original,tri,error));
    assert(tri.vertices==1211&&tri.morphs.size()==38&&tri.morphs.count("N")&&tri.morphs.count("BMP"));
    assert(tri.morphs.count("Ee")&&!tri.morphs.count("Eee"));
    std::cout<<"Original head TRI vertices="<<tri.vertices<<" morphs="<<tri.morphs.size()<<'\n';
    for(int i=2;i<argc;++i) {
      auto line=Read(argv[i]);assert(fo3face::DecodeLip(line,lip,error));
      bool nonzero=false;for(const auto& frame:lip.frames)for(float w:frame.speech)nonzero|=w!=0;assert(nonzero);
      float displacement=0;
      for(double time=0;time<lip.frames.size()/30.;time+=.1) {
        const auto weights=fo3face::Sample(lip,time);
        for(size_t c=0;c<16;++c){const auto morph=tri.morphs.find(fo3face::SpeechNames[c]);if(morph==tri.morphs.end())continue;
          for(const auto& d:morph->second)for(float axis:d)displacement+=std::fabs(weights[c]*axis);}
      }
      assert(displacement>0);const auto neutral=fo3face::Sample(lip,lip.frames.size()/30.+1);for(float w:neutral)assert(w==0);
      for(size_t n=0;n<std::min<size_t>(line.size(),128);++n){bad.assign(line.begin(),line.begin()+n);assert(!fo3face::DecodeLip(bad,lip,error));}
      assert(fo3face::DecodeLip(line,lip,error));
      std::cout<<"Original LIP "<<argv[i]<<" frames="<<lip.frames.size()<<" first="<<lip.firstFrame<<" sampledDisplacement="<<displacement<<'\n';
    }
    original.pop_back();assert(!fo3face::DecodeTri(original,tri,error));
  }
  std::cout<<"Facial decoding, bounded rejection, authored sampling and token isolation passed\n";
}
