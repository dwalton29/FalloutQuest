#pragma once
#include <cstdint>
#include <string>
namespace fo3movie {
struct Request {uint64_t serial=0;uint32_t owner=0;uint8_t flags=14;std::string path;};
inline bool ValidPath(const std::string& path){
    if(path.empty()||path.size()>240||path.front()=='/'||path.front()=='\\')return false;
    std::string component;
    for(unsigned char c:path){
        if(c<32||c>126||c==':'||c=='"')return false;
        if(c=='/'||c=='\\'){if(component.empty()||component=="."||component=="..")return false;component.clear();}else component.push_back(char(c));
    }
    if(component.empty()||component=="."||component=="..")return false;
    return path.size()>=4&&(path.substr(path.size()-4)==".bik"||path.substr(path.size()-4)==".BIK");
}
}
