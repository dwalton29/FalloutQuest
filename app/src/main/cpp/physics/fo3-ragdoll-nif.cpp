#include "fo3-ragdoll-data.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
namespace fo3rag { namespace {
struct Reader {
 const std::vector<uint8_t>& bytes;size_t at=0;
 void Need(size_t n){if(n>bytes.size()-at)throw "truncated skeleton";}
 uint8_t U8(){Need(1);return bytes[at++];}
 uint16_t U16(){uint16_t v=U8();return v|uint16_t(U8())<<8;}
 uint32_t U32(){uint32_t v=U16();return v|uint32_t(U16())<<16;}
 void Skip(size_t n){Need(n);at+=n;}
 std::string Text(size_t n){Need(n);std::string s(bytes.begin()+at,bytes.begin()+at+n);at+=n;return s;}
};
uint32_t U(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
float F(const uint8_t* p){auto u=U(p);float f;std::memcpy(&f,&u,4);if(!std::isfinite(f))throw "nonfinite skeleton property";return f;}
V Vec(const uint8_t* p,float scale=1){return {F(p)*scale,F(p+4)*scale,F(p+8)*scale};}
M Frame(const uint8_t* p,bool hinge){
 M m{};m[15]=1;
 // Havok's hinge axis is Z in Bullet, while the ragdoll twist is X.
 for(int axis=0;axis<3;++axis){const int slot=hinge?(axis+1)%3:axis;auto v=Vec(p+slot*16);float length=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);if(length<.9f||length>1.1f)throw "invalid joint axis";for(int k=0;k<3;++k)m[axis*4+k]=v[k]/length;}
 for(int a=0;a<3;++a)for(int b=a+1;b<3;++b){float dot=0;for(int k=0;k<3;++k)dot+=m[a*4+k]*m[b*4+k];if(std::abs(dot)>.005f)throw "nonorthogonal joint frame";}
 const float determinant=m[0]*(m[5]*m[10]-m[9]*m[6])-m[4]*(m[1]*m[10]-m[9]*m[2])+m[8]*(m[1]*m[6]-m[5]*m[2]);if(determinant<.99f)throw "reflected joint frame";
 auto v=Vec(p+48,7);for(int k=0;k<3;++k)m[12+k]=v[k];return m;
}
}
bool Read(const std::vector<uint8_t>& bytes,Definition& out,std::string& error){
 out={};error.clear();try{
 if(bytes.size()>4*1024*1024)throw "oversize skeleton";
 Reader r{bytes};std::string line;while(r.at<bytes.size()&&line.size()<128){auto c=r.U8();line+=char(c);if(c=='\n')break;}
 if(line.find("Gamebryo File Format, Version 20.2.0.7")!=0||r.U32()!=0x14020007||r.U8()!=1||r.U32()!=11)throw "unsupported skeleton version";
 const auto count=r.U32();if(!count||count>10000||r.U32()!=34)throw "unsupported Bethesda skeleton version";
 for(int k=0;k<3;++k){auto n=r.U8();r.Skip(n);}
 std::vector<std::string> types;auto nt=r.U16();if(nt>1024)throw "invalid block types";
 for(unsigned i=0;i<nt;++i){auto n=r.U32();types.push_back(r.Text(n));}
 std::vector<uint16_t> tags(count);for(auto& t:tags){t=r.U16()&0x7fff;if(t>=types.size())throw "invalid block type";}
 std::vector<uint32_t> sizes(count);for(auto& n:sizes)n=r.U32();
 auto ns=r.U32();r.U32();if(ns>10000)throw "invalid string count";
 std::vector<std::string> strings;for(unsigned i=0;i<ns;++i){auto n=r.U32();strings.push_back(r.Text(n));}
 auto ng=r.U32();if(ng>count)throw "invalid group count";r.Skip(ng*4);
 std::vector<const uint8_t*> blocks;for(auto n:sizes){r.Need(n);blocks.push_back(bytes.data()+r.at);r.Skip(n);}
 const auto roots=r.U32();if(roots>count)throw "invalid skeleton roots";for(uint32_t i=0;i<roots;++i)if(r.U32()>=count)throw "invalid skeleton root";if(r.at!=bytes.size())throw "trailing skeleton data";
 Definition result;std::unordered_map<uint32_t,uint16_t> bodyIds;std::unordered_set<std::string> names;
 for(uint32_t i=0;i<count;++i)if(types[tags[i]]=="bhkBlendCollisionObject"){
  if(sizes[i]!=18)throw "invalid blended collision object";
  const auto target=U(blocks[i]),body=U(blocks[i]+6);
  if(target>=count||body>=count||(types[tags[target]]!="NiNode"||sizes[target]<4)||types[tags[body]]!="bhkRigidBody"||sizes[body]<236)throw "unsupported ragdoll body";
  const auto* d=blocks[body];const auto shape=U(d);if(shape>=count||types[tags[shape]]!="bhkCapsuleShape"||sizes[shape]!=48)throw "unsupported ragdoll shape";
  const auto name=U(blocks[target]);if(name>=strings.size()||strings[name].empty()||!names.insert(strings[name]).second||result.bodies.size()>=64)throw "invalid body bone";
  const auto* s=blocks[shape];Body b;b.bone=strings[name];b.a=Vec(s+16,7);b.b=Vec(s+32,7);b.radius=F(s+4)*7;
  if(b.radius<=0||std::abs(F(s+28)*7-b.radius)>.001f||std::abs(F(s+44)*7-b.radius)>.001f)throw "unsupported tapered capsule";
  b.center=Vec(d+164,7);b.mass=F(d+180);b.linearDamping=F(d+184);b.angularDamping=F(d+188);b.friction=F(d+192);b.restitution=F(d+196);
  if(b.mass<=0||b.mass>10000)throw "invalid body mass";
  for(int a=0;a<3;++a)for(int k=0;k<3;++k)b.inertia[a*3+k]=F(d+116+a*16+k*4)*49;
  bodyIds.emplace(body,uint16_t(result.bodies.size()));result.bodies.push_back(std::move(b));
 }
 if(result.bodies.size()<2)throw "no articulated capsule bodies";
 for(uint32_t i=0;i<count;++i){const auto& type=types[tags[i]];
  if(type!="bhkRagdollConstraint"&&type!="bhkMalleableConstraint"&&type!="bhkLimitedHingeConstraint")continue;
  if(sizes[i]<16||U(blocks[i])!=2)throw "invalid constraint owners";
  auto a=bodyIds.find(U(blocks[i]+4)),b=bodyIds.find(U(blocks[i]+8));if(a==bodyIds.end()||b==bodyIds.end()||a==b)throw "constraint references unavailable body";
  size_t start=16;uint32_t kind=type=="bhkLimitedHingeConstraint"?2:7;
  if(type=="bhkMalleableConstraint"){if(sizes[i]<36)throw "invalid malleable constraint";kind=U(blocks[i]+16);start=36;}
  if(kind!=2&&kind!=7)throw "unsupported joint type";
  const auto parameters=kind==2?141u:153u;const auto end=start+parameters;
  if(sizes[i]!=end+(type=="bhkMalleableConstraint"?4u:0u)||blocks[i][end-1]!=0)throw "unsupported joint motor";
  const auto* d=blocks[i]+start;Joint j;j.a=a->second;j.b=b->second;j.hinge=kind==2;j.frameA=Frame(d,j.hinge);j.frameB=Frame(d+64,j.hinge);
  if(j.hinge){j.minimum={0,0,F(d+128)};j.maximum={0,0,F(d+132)};j.friction=F(d+136);}
  else{const float cone=F(d+128);j.minimum={F(d+140),F(d+132),-cone};j.maximum={F(d+144),F(d+136),cone};j.friction=F(d+148);}
  if(type=="bhkMalleableConstraint")j.strength=F(blocks[i]+end);
  for(int k=0;k<3;++k)if(j.minimum[k]>j.maximum[k])throw "invalid joint limits";
  result.joints.push_back(j);
 }
 if(result.joints.size()+1!=result.bodies.size())throw "incomplete ragdoll graph";
 std::vector<bool> connected(result.bodies.size());connected[0]=true;
 for(size_t n=0;n<connected.size();++n)for(const auto& j:result.joints)if(connected[j.a]||connected[j.b])connected[j.a]=connected[j.b]=true;
 if(std::find(connected.begin(),connected.end(),false)!=connected.end())throw "disconnected ragdoll";
 out=std::move(result);return true;
 }catch(const char* reason){error=reason;return false;}
}
}
