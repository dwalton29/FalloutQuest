#include "rendering/environment/fo3-interior-lighting.h"
#include "rendering/environment/fo3-imagespace-data.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <unordered_map>
using Bytes=std::vector<uint8_t>;
void u32(Bytes& b,size_t p,uint32_t v){if(b.size()<p+4)b.resize(p+4);std::memcpy(b.data()+p,&v,4);}
void f32(Bytes& b,size_t p,float v){if(b.size()<p+4)b.resize(p+4);std::memcpy(b.data()+p,&v,4);}
Bytes str(const char* s){return Bytes(s,s+std::strlen(s)+1);}
void sub(Bytes& b,const char* t,const Bytes& value){b.insert(b.end(),t,t+4);uint16_t n=value.size();b.push_back(n&255);b.push_back(n>>8);b.insert(b.end(),value.begin(),value.end());}
Bytes word(uint32_t v){Bytes b(4);u32(b,0,v);return b;}
Bytes record(const char* t,uint32_t id,const Bytes& b,uint32_t flags=0){Bytes out(24);std::memcpy(out.data(),t,4);u32(out,4,b.size());u32(out,8,flags);u32(out,12,id);out.insert(out.end(),b.begin(),b.end());return out;}
Bytes group(uint32_t id,uint32_t type,const Bytes& b){Bytes out(24);std::memcpy(out.data(),"GRUP",4);u32(out,4,b.size()+24);u32(out,8,id);u32(out,12,type);out.insert(out.end(),b.begin(),b.end());return out;}
void append(Bytes& a,const Bytes& b){a.insert(a.end(),b.begin(),b.end());}
Bytes lighting(){Bytes b(40);b[0]=32;b[1]=64;b[2]=96;b[4]=1;b[8]=10;f32(b,12,80);f32(b,16,900);u32(b,20,45);u32(b,24,30);f32(b,28,1);f32(b,32,900);f32(b,36,2);return b;}
Bytes cell(uint32_t image=0){Bytes b;sub(b,"DATA",{1});sub(b,"EDID",str("TestInterior"));sub(b,"XCLL",lighting());if(image)sub(b,"XCIM",word(image));return b;}
Bytes ref(uint32_t base,uint32_t parent=0,bool opposite=false){Bytes b;sub(b,"NAME",word(base));Bytes pos(24);f32(pos,0,140);f32(pos,4,210);f32(pos,8,280);sub(b,"DATA",pos);if(parent){Bytes v(8);u32(v,0,parent);v[4]=opposite;sub(b,"XESP",v);}return b;}
Bytes base(uint32_t flags=0){Bytes b;sub(b,"EDID",str("SyntheticLight"));Bytes data(32);u32(data,0,0xffffffff);u32(data,4,140);data[8]=128;data[9]=64;data[10]=32;u32(data,12,flags);f32(data,16,1);f32(data,20,90);sub(b,"DATA",data);Bytes fade;f32(fade,0,2);sub(b,"FNAM",fade);return b;}
void fixtures(){
 auto c=fo3interior::ParseCell(10,cell(99));assert(c.authored&&c.imageSpace==99&&c.rotationXY==45&&c.rotationZ==30&&c.fogPower==2);
 Bytes bad;sub(bad,"XCLL",Bytes(39));assert(!fo3interior::ParseCell(10,bad).authored);
 auto l=fo3interior::ParseLight(20,base(4));assert(l.flags==4&&l.radius==140&&l.fade==2&&l.fov==90);
 const auto p=fo3interior::ToScene({140,210,280},{70,70,140});assert(p[0]==1&&std::abs(p[1]-0.45f)<1e-5&&p[2]==-2);
 assert(fo3interior::Attenuation(1,2)==0.75f&&fo3interior::Attenuation(2,2)==0&&fo3interior::Attenuation(0,2)==1);
 for(size_t n:{132u,148u,152u}) {
   Bytes b,data(n);const size_t shift=n==152u?0u:4u;
   if(n==152u)f32(data,56,1.25f);
   f32(data,60-shift,3.0f);f32(data,64-shift,0.8f);f32(data,68-shift,0.2f);
   f32(data,96-shift,2.5f);
   f32(data,100-shift,0.9f);f32(data,104-shift,0.14f);f32(data,108-shift,1.2f);
   f32(data,112-shift,1.1f);f32(data,116-shift,0.7f);f32(data,120-shift,0.6f);
   f32(data,124-shift,0.3f);f32(data,128-shift,0.5f);
   if(n==152u)data[148]=7;
   sub(b,"DNAM",data);Fo3ImageSpace image;assert(fo3imagespace::ParseImageSpacePayload(b,image));
   assert(image.valid&&std::abs(image.bloomBlurRadius-3.0f)<1e-6&&
          std::abs(image.bloomAlphaInterior-0.8f)<1e-6&&std::abs(image.bloomAlphaExterior-0.2f)<1e-6);
   assert(std::abs(image.nightEyeBrightness-2.5f)<1e-6&&
          std::abs(image.cinematicSaturation-0.9f)<1e-6&&
          std::abs(image.cinematicContrastAvgLum-0.14f)<1e-6&&
          std::abs(image.cinematicContrast-1.2f)<1e-6&&
          std::abs(image.cinematicBrightness-1.1f)<1e-6&&
          std::abs(image.cinematicTint[0]-0.7f)<1e-6&&
          std::abs(image.cinematicTint[1]-0.6f)<1e-6&&
          std::abs(image.cinematicTint[2]-0.3f)<1e-6&&
          std::abs(image.cinematicTintValue-0.5f)<1e-6);
   assert(std::abs(image.hdrSkinDimmer-(n==152u?1.25f:1.0f))<1e-6);
 }
 Bytes templated=cell(99);sub(templated,"LTMP",word(50));sub(templated,"LNAM",word(1));
 Bytes stream;append(stream,record("CELL",10,templated));Bytes templateData;auto templateLighting=lighting();templateLighting[0]=128;sub(templateData,"DATA",templateLighting);append(stream,record("LGTM",50,templateData));append(stream,record("CELL",11,cell()));append(stream,record("LIGH",20,base()));append(stream,record("LIGH",21,base(4)));
 Bytes children;append(children,record("REFR",30,ref(20)));append(children,record("REFR",31,ref(20),0x800));append(children,record("REFR",32,ref(21,31,true)));append(children,record("REFR",33,ref(20,31)));append(children,record("REFR",34,ref(20,90)));append(children,record("REFR",35,ref(20,999,true)));
 append(stream,group(10,6,group(10,9,children)));append(stream,record("REFR",90,ref(20),0x800));
 Bytes pathBytes=stream;const std::string path="/tmp/falloutquest-synthetic-interior.esm";{std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<char*>(pathBytes.data()),pathBytes.size());}
 fo3interior::Snapshot snapshot;assert(fo3interior::Load(path,10,{70,70,140},snapshot));assert(snapshot.cell.ambient[0]==128.0f/255);assert(snapshot.total==6&&snapshot.lights.size()==2&&snapshot.disabled==4&&snapshot.unresolvedParents==1);
 assert(snapshot.lights[0].ref==30&&snapshot.lights[1].ref==32&&snapshot.lights[1].colour[0]<0);
 assert(snapshot.lights[0].position==p&&std::abs(snapshot.lights[0].colour[0]-256.0f/255)<1e-6);
 auto selection=fo3interior::Select(snapshot.lights,{0,-1,-3},{2,1,-1});assert(selection.count==2);
 assert(fo3interior::Select(snapshot.lights,{100,100,100},{101,101,101}).count==0);
 // Same destination container reused: previous room's environment/lights cannot survive.
 assert(fo3interior::Load(path,11,{0,0,0},snapshot)&&snapshot.lights.empty()&&snapshot.total==0);
 assert(!fo3interior::Load(path,999,{0,0,0},snapshot)&&snapshot.lights.empty()&&!snapshot.cell.authored);
 std::remove(path.c_str());
 std::vector<fo3interior::Light> lights(20);
 for(size_t i=0;i<lights.size();++i){lights[i].radius=4;lights[i].colour={float(i+1),0,0};}
 auto top=fo3interior::Select(lights,{-1,-1,-1},{1,1,1});assert(top.count==8&&top.indices[0]==19&&top.indices[7]==12);
 lights[19].flags=0x200;assert(fo3interior::Select(lights,{-1,-1,-1},{1,1,1}).indices[0]==18);
}
void original(const std::string& path){
 // Resolve by original EDID, not guessed runtime cell IDs.
 FILE* f=std::fopen(path.c_str(),"rb");assert(f);std::unordered_map<std::string,uint32_t> cells;
 auto size=fo3esm::FileSize(f);
 while(ftello(f)+24<=size){uint8_t h[24];assert(fo3esm::ReadExact(f,h,24));uint32_t n=fo3esm::ReadU32(h+4);if(std::memcmp(h,"GRUP",4)==0)continue;
 if(std::memcmp(h,"CELL",4)==0){Bytes b;assert(fo3esm::ReadPayloadCurrent(f,n,fo3esm::ReadU32(h+8),b));auto c=fo3interior::ParseCell(fo3esm::ReadU32(h+12),b);cells[c.editorId]=c.formId;}else assert(fseeko(f,n,SEEK_CUR)==0);}
 std::fclose(f);
 for(const char* edid:{"MegatonTheBrassLantern","MegatonPlayerHouse"}){
  assert(cells.count(edid));fo3interior::Snapshot s;assert(fo3interior::Load(path,cells.at(edid),{100,200,300},s));
  assert(s.cell.authored&&s.total==11&&s.lights.size()==11&&s.unsupported==0&&s.unresolvedParents==0);
  assert(s.cell.ambient[0]==47.0f/255&&s.cell.ambient[1]==70.0f/255&&s.cell.fogNear==100&&s.cell.fogFar==1500);
  Fo3ImageSpace image;assert(fo3imagespace::ParseImageSpacePayload(s.imagePayload,image));assert(image.editorId=="ShackInterior01");
  assert(std::abs(image.cinematicSaturation-0.9f)<1e-6&&std::abs(image.cinematicBrightness-1.1f)<1e-6&&std::abs(image.bloomAlphaInterior-0.8f)<1e-6);
  float lo=INFINITY,hi=0;for(const auto& l:s.lights){lo=std::min(lo,l.radius*70);hi=std::max(hi,l.radius*70);assert(l.fade==1&&l.flags==0);}
  std::cout<<edid<<" cell="<<std::hex<<s.cell.formId<<" XCIM="<<s.cell.imageSpace<<std::dec<<" lights="<<s.lights.size()<<" radius="<<lo<<".."<<hi<<" fade=1 flags=0 IMGS="<<image.editorId<<"\n";
 }
 assert(cells.count("SuperDuperMart"));
 {
  fo3interior::Snapshot s;assert(fo3interior::Load(path,cells.at("SuperDuperMart"),{100,200,300},s));
  assert(s.cell.authored&&s.cell.lightingTemplate==0x0006532Eu&&s.cell.inherit==0x9Fu);
  assert(s.total==47&&s.lights.size()==47&&s.unresolvedParents==0);
  assert(std::abs(s.cell.ambient[0]-29.0f/255.0f)<1e-6&&
         std::abs(s.cell.ambient[1]-31.0f/255.0f)<1e-6&&
         std::abs(s.cell.ambient[2]-43.0f/255.0f)<1e-6);
  Fo3ImageSpace image;assert(fo3imagespace::ParseImageSpacePayload(s.imagePayload,image));
  assert(image.editorId=="OfficeDefaultImageSpace");
  assert(std::abs(image.hdrSkinDimmer-1.0f)<1e-6&&std::abs(image.bloomBlurRadius-0.03f)<1e-6);
  assert(std::abs(image.cinematicSaturation-0.75f)<1e-6&&
         std::abs(image.cinematicContrastAvgLum-0.1f)<1e-6&&
         std::abs(image.cinematicContrast-1.1f)<1e-6&&
         std::abs(image.cinematicBrightness-1.1f)<1e-6&&
         std::abs(image.cinematicTint[0]-46.0f/255.0f)<1e-5&&
         std::abs(image.cinematicTint[1]-146.0f/255.0f)<1e-5&&
         std::abs(image.cinematicTint[2]-96.0f/255.0f)<1e-5&&
         std::abs(image.cinematicTintValue-0.3f)<1e-6);
  std::cout<<"SuperDuperMart cell="<<std::hex<<s.cell.formId<<" LTMP="<<s.cell.lightingTemplate
           <<" XCIM="<<s.cell.imageSpace<<std::dec<<" lights="<<s.lights.size()
           <<" IMGS="<<image.editorId<<" brightness="<<image.cinematicBrightness<<"\n";
 }
}
int main(int argc,char** argv){fixtures();if(argc>1)original(argv[1]);std::cout<<"Interior lighting checks passed\n";}
