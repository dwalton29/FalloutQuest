#pragma once
#include <array>
#include <cstddef>
#include <chrono>
#include <cstdint>
namespace fqframehitch {
using Clock=std::chrono::steady_clock;
inline uint64_t Micros(Clock::time_point from){return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-from).count());}
struct Span {const char* name="unknown";uint64_t us=0;};
template<class Log> class Trace {
    Log log;Clock::time_point start=Clock::now(),previous=start;
    std::array<Span,16> spans{};size_t count=0;uint64_t frame;
public:
    Trace(uint64_t serial,Log logger):log(logger),frame(serial){}
    void Mark(const char* name){const auto now=Clock::now();if(count<spans.size())spans[count++]={name,uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(now-previous).count())};previous=now;}
    ~Trace(){Mark("tail");const auto total=Micros(start);if(total>=50000)log(frame,total,spans.data(),count);}
};
template<class Log> class Operation {
    const char* name;Log log;Clock::time_point start=Clock::now();uint64_t threshold;
public:
    Operation(const char* label,Log logger,uint64_t limit=8000):name(label),log(logger),threshold(limit){}
    ~Operation(){const auto us=Micros(start);if(us>=threshold)log(name,us);}
};
}
