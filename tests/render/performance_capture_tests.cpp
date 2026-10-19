#include "rendering/performance-capture.h"
#include <cassert>
#include <fstream>
#include <string>
int main() {
    using namespace fqperf;
    static Session s;
    s.BeginFrame();s.FinishFrame();assert(s.size==0);
    s.Start(223);
    for(unsigned i=0;i<Capacity+10;++i) {
        s.BeginFrame();s.current.us[Work]=1000+i;s.current.sceneSamples=i?2:1;s.FinishFrame();
    }
    assert(s.size==Capacity);
    auto serial=s.frames[0].serial;
    s.Gpu(serial,0,100);assert(s.frames[0].gpuEyesUs<0);
    s.Gpu(serial,1,150);assert(s.frames[0].gpuEyesUs==250);
    s.Gpu(serial,9,100);assert(s.frames[0].gpuEyesUs==250);
    s.Gpu(serial-1,1,200);assert(s.size==Capacity); // stale completion ignored
    s.frames[0].gpuExpected[0][GpuTerrain]=2;
    s.frames[0].gpuExpected[1][GpuReflection]=2; // only one completion: export missing
    s.GpuStage(serial,0,GpuTerrain,100);
    s.GpuStage(serial,0,GpuTerrain,50);
    s.GpuStage(serial,1,GpuReflection,200);
    assert(s.frames[0].gpuPassUs[0][GpuTerrain]==150);
    assert(s.frames[0].gpuPassUs[1][GpuReflection]==200);
    assert(s.frames[0].gpuPassUs[1][GpuTerrain]==-1);
    s.GpuStage(serial,2,GpuTerrain,999);
    s.GpuStage(serial-1,0,GpuTerrain,999);
    assert(s.frames[0].gpuPassUs[0][GpuTerrain]==150);
    assert(s.Write("/tmp/fq-perf-test.csv"));
    std::ifstream in("/tmp/fq-perf-test.csv");std::string line;
    unsigned lines=0;while(std::getline(in,line)){
        if(lines==0)assert(line.find("format=232")!=std::string::npos);
        if(lines==1)assert(line.find(",sceneSamples,submittedVertices")!=std::string::npos);
        if(lines==2)assert(line.find(",1,0,-1.000")!=std::string::npos);
        if(lines==3)assert(line.find(",2,0,-1.000")!=std::string::npos);
        ++lines;
    }
    assert(lines==Capacity+2);
    s.Start(224);assert(s.size==0);
    s.Gpu(serial,0,999);assert(s.size==0); // previous session queries cannot populate this one
    capture.Start(1);capture.BeginFrame();
    {Scope scope(Sight);}
    assert(capture.current.us[Sight]>=0);
    capture.FinishFrame();assert(capture.size==1);
}
