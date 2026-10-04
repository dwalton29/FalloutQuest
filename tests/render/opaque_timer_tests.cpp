#include "rendering/opaque-telemetry.h"
#include <cassert>
#include <iostream>

namespace {
unsigned reads=0, begins=0, ends=0, deletes=0;
bool ready=false;
void Begin(GLenum,GLuint) {++begins;}
void End(GLenum) {++ends;}
void Available(GLuint,GLenum,GLuint* out) {*out=ready ? 1 : 0;}
void Result(GLuint,GLenum,GLuint64* out) {assert(ready);++reads;*out=45000000;}
void Delete(GLsizei,const GLuint*) {++deletes;}
}
int main() {
    fqopaque::GpuTimer timer;
    timer.initialized=timer.supported=true;
    timer.begin=Begin;timer.end=End;timer.available=Available;timer.result=Result;timer.del=Delete;
    for(unsigned i=0;i<timer.slots.size();++i) timer.slots[i].id=i+1;
    timer.Begin(1);timer.End();timer.Poll(5);
    assert(reads==0 && timer.slots[0].pending);
    ready=true;timer.Poll(6);
    assert(reads==1 && timer.gpu.Mean()==45000);
    timer.Begin(7);timer.End();driverDisjoint=1;timer.Poll(11);
    assert(reads==1 && timer.gpu.count==0 && timer.disjoints==1);
    driverDisjoint=0;ready=false;
    for(unsigned i=0;i<8;++i) {timer.Begin(12);timer.End();}
    timer.Begin(12);assert(timer.dropped==1 && begins==10 && ends==10);
    timer.Shutdown();assert(deletes==8 && !timer.supported);
    fqopaque::Average average;for(int i=1;i<=240;++i) average.Add(i);
    assert(average.count==120 && average.Mean()==180.5);
    std::cout << "Nonblocking timer/disjoint/ring regressions passed\n";
}
