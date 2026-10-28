#pragma once
#include <cstring>
namespace fqspacewarp {
enum class Mode { Off,Synthesis,Validate,Visualize };
inline Mode SelectMode(const char* property){
    if(!property||!property[0]||!std::strcmp(property,"on")||!std::strcmp(property,"auto"))return Mode::Synthesis;
    if(!std::strcmp(property,"validate")||!std::strcmp(property,"1"))return Mode::Validate;
    if(!std::strcmp(property,"visualize"))return Mode::Visualize;
    return Mode::Off;
}
}
