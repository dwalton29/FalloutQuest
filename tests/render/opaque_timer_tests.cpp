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
    assert(reads==1 && timer.gpu.Mean()==45000 && timer.phaseGpu[fqopaque::NativeLod].Mean()==45000);
    fqopaque::eye=1;
    timer.Begin(7,fqopaque::DetailedWorld);timer.End();
    timer.Poll(9);assert(reads==1); // Even ready queries must wait for later frames.
    timer.Poll(10);
    assert(reads==2 && timer.eyeGpu[1][fqopaque::DetailedWorld].Mean()==45000);
    assert(timer.eyeGpu[0][fqopaque::DetailedWorld].count==0);
    assert(timer.eyeGpu[0][fqopaque::NativeLod].count==1);
    timer.Begin(11);timer.End();driverDisjoint=1;timer.Poll(15);
    assert(reads==2 && timer.gpu.count==0 && timer.disjoints==1);
    assert(timer.eyeGpu[1][fqopaque::DetailedWorld].count==0 && timer.phaseGpu[0].count==0);
    driverDisjoint=0;ready=false;
    for(unsigned i=0;i<timer.slots.size();++i) {timer.Begin(16);timer.End();}
    timer.Begin(16);assert(timer.dropped==1 && begins==timer.slots.size()+3 && ends==timer.slots.size()+3);
    timer.Shutdown();assert(deletes==32 && !timer.supported);
    // A different query owner may consume GL_GPU_DISJOINT_EXT first.
    // This owner must still discard its pending result when polling later.
    fqopaque::GpuTimer other;
    other.initialized=other.supported=true;
    other.begin=Begin;other.end=End;other.available=Available;other.result=Result;other.del=Delete;
    for(unsigned i=0;i<other.slots.size();++i)other.slots[i].id=i+50;
    other.observedDisjointEpoch=fqopaque::gpuDisjointEpoch;
    other.Begin(20);other.End();
    ++fqopaque::gpuDisjointEpoch;ready=true;
    const auto readsBefore=reads;other.Poll(24);
    assert(reads==readsBefore&&other.disjoints==1&&other.gpu.count==0);
    other.Shutdown();
    // Full-eye captures own the GL query. Phase CPU scopes remain active,
    // but must neither nest nor end that owner's elapsed query.
    fqopaque::externalGpuScope=true;
    {fqopaque::PhaseScope phase(fqopaque::Npc,25);}
    fqopaque::externalGpuScope=false;
    fqopaque::Average average;for(int i=1;i<=240;++i) average.Add(i);
    assert(average.count==120 && average.Mean()==180.5);
    std::cout << "Nonblocking timer/disjoint/ring regressions passed\n";
}
