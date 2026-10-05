#pragma once
#include "fo3-audio-catalog.h"
#include <dirent.h>
#include <strings.h>
#include <string>
#include <vector>

namespace fo3audio {
// Case-insensitive lookup of original Windows paths on Android/Linux. Accept
// both the original Data layout and an extracted Sound/Music sibling of Data.
inline std::string LooseAt(std::string base, const std::string& relative) {
    if(!SafePath(relative))return {};
    if(base.empty() || base.back()!='/')base+='/';
    size_t at=0;
    while(at<relative.size()) {
        const auto end=relative.find('/',at);
        const auto part=relative.substr(at,end-at);
        DIR* dir=opendir(base.c_str());if(!dir)return {};
        std::string found;
        while(auto* entry=readdir(dir))if(strcasecmp(entry->d_name,part.c_str())==0){found=entry->d_name;break;}
        closedir(dir);if(found.empty())return {};
        base+=found;if(end==std::string::npos)break;
        base+='/';at=end+1;
    }
    return base;
}
inline std::vector<std::string> AudioRoots(std::string data) {
    while(!data.empty() && data.back()=='/')data.pop_back();
    std::vector<std::string> roots{data};
    const auto slash=data.rfind('/');
    if(slash!=std::string::npos)roots.push_back(data.substr(0,slash));
    return roots;
}
inline std::string FindAudioFile(const std::string& data,const std::string& relative) {
    for(const auto& root:AudioRoots(data)) {
        auto path=LooseAt(root,relative);if(!path.empty())return path;
    }
    return {};
}
inline std::vector<std::string> SoundArchives(const std::string& data) {
    std::vector<std::string> result;
    for(const auto& root:AudioRoots(data))
        for(const char* name:{"Fallout - Sound.bsa","sound.bsa","sounds.bsa","Fallout - Voices.bsa","voices.bsa"}) {
            auto path=LooseAt(root,name);if(!path.empty())result.push_back(path);
        }
    return result;
}
} // namespace fo3audio
