#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace fo3movie {
struct Decoded {
    enum class Kind {Video,Audio};Kind kind=Kind::Video;
    double pts=0,duration=0;
    int width=0,height=0,sampleRate=0,channels=0;
    std::vector<uint8_t> rgba;
    std::vector<float> samples;
};
class Decoder {
    struct Impl;std::unique_ptr<Impl> impl_;
public:
    Decoder();~Decoder();
    Decoder(const Decoder&)=delete;Decoder& operator=(const Decoder&)=delete;
    bool Open(const std::string& path,std::string& error);
    // False + empty error is clean EOF; decoder failures never look like EOF.
    bool Next(Decoded& out,std::string& error);
};
}
