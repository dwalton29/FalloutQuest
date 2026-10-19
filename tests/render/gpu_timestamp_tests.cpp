#include "rendering/gpu-timestamp-timer.h"
#include <cassert>
#include <map>
#include <iostream>
namespace {
std::map<GLuint,GLuint64> values;
std::map<GLuint,bool> ready;
unsigned reads=0,issued=0,deletes=0,samples=0;
uint64_t clockNs=1000000000;
void Counter(GLuint id,GLenum target){assert(target==GL_TIMESTAMP_EXT);values[id]=clockNs;ready[id]=false;++issued;}
void Available(GLuint id,GLenum,GLuint* out){*out=ready[id];}
void Result(GLuint id,GLenum,GLuint64* out){assert(ready[id]);*out=values[id];++reads;}
void Delete(GLsizei count,const GLuint*){deletes+=count;}
void Sample(uint64_t f,unsigned e,unsigned p,double us){assert(f==1&&e==1&&p==3&&us==1000);++samples;}
void Setup(fqgpu::TimestampTimer& timer){
 timer.initialized=timer.supported=true;timer.counterBits=64;timer.counter=Counter;
 timer.available=Available;timer.result=Result;timer.del=Delete;
 timer.epoch=fqopaque::gpuDisjointEpoch;
 for(unsigned i=0;i<timer.slots.size();++i){timer.slots[i].id[0]=2*i+1;timer.slots[i].id[1]=2*i+2;}
}
}
int main(){
 double us=0;
 assert(fqgpu::Elapsed(1000,2000,64,us)&&us==1);
 assert(fqgpu::Elapsed((1ull<<30)-1000,1000,30,us)&&us==2);
 assert(!fqgpu::Elapsed(2000,1000,64,us));
 assert(!fqgpu::Elapsed(0,~uint64_t{0}-1000,64,us));
 assert(!fqgpu::Elapsed(0,1ull<<32,30,us));
 assert(!fqgpu::Elapsed(0,2000000000,64,us));
 assert(!fqgpu::Elapsed(0,1000,0,us));
 fqgpu::TimestampTimer unsupported;unsupported.Initialize();assert(!unsupported.supported);
 fqgpu::TimestampTimer timer;Setup(timer);timer.sample=Sample;
 auto outer=timer.Begin(1,1,3);clockNs+=100000;
 auto inner=timer.Begin(1,1,3);assert(inner!=outer);
 clockNs+=1000000;timer.End(inner);clockNs+=100000;timer.End(outer);
 assert(issued==4);timer.Poll(5);assert(reads==0);
 ready[timer.slots[inner].id[0]]=true;timer.Poll(5);assert(reads==0); // both endpoints required
 ready[timer.slots[inner].id[1]]=true;timer.Poll(3);assert(reads==0); // delayed
 timer.Poll(5);assert(reads==2&&samples==1&&timer.valid==1);
 timer.sample=nullptr;
 for(auto& r:ready)r.second=true;
 ++fqopaque::gpuDisjointEpoch;timer.Poll(6);assert(reads==2&&timer.disjoints==1);
 const auto bad=timer.Begin(7,0,0);timer.End(bad);
 values[timer.slots[bad].id[1]]=values[timer.slots[bad].id[0]]-1;
 for(auto& r:ready)r.second=true;
 timer.Poll(11);assert(timer.invalid==1&&timer.valid==1);
 for(unsigned i=0;i<timer.slots.size();++i){int q=timer.Begin(12,0,0);assert(q>=0);timer.End(q);}
 assert(timer.Begin(12,0,0)==-1&&timer.dropped==1);
 timer.Shutdown();assert(deletes==192&&!timer.supported);
 std::cout<<"Timestamp nesting, asynchronous reads, disjoint, wrap, invalid and capacity passed\n";
}
