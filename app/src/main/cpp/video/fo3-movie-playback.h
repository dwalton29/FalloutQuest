#pragma once
#include "fo3-bink-decoder.h"
namespace fo3movie {
class Playback {
    struct Impl;std::unique_ptr<Impl> impl_;
public:
    enum class State {Opening,Playing,Finished,Failed};
    Playback();~Playback();
    void Start(const std::string& path);
    void Stop();
    void Tick(double now,bool focused,float volume);
    State Status()const;
    std::string Error()const;
    std::shared_ptr<const Decoded> Frame()const;
};
std::string ResolvePath(const std::string& dataRoot,const std::string& relative);
}
