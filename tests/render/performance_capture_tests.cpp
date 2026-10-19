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
    assert(s.Write("/tmp/fq-perf-test.csv"));
    std::ifstream in("/tmp/fq-perf-test.csv");std::string line;
    unsigned lines=0;while(std::getline(in,line)){
        if(lines==0)assert(line.find("format=231")!=std::string::npos);
        if(lines==1)assert(line.find(",sceneSamples,submittedVertices")!=std::string::npos);
        if(lines==2)assert(line.substr(line.size()-4)==",1,0");
        if(lines==3)assert(line.substr(line.size()-4)==",2,0");
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
