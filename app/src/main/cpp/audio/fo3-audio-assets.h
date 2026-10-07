#pragma once
#include "fo3-audio-catalog.h"
#include <dirent.h>
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include "data/fo3-asset-store.h"
#include <strings.h>
#include <string>
#include <vector>

namespace fo3audio {
// Case-insensitive lookup of original Windows paths on Android/Linux. Accept
// both the original Data layout and an extracted Sound/Music sibling of Data.
inline std::string LooseAt(std::string base, const std::string& relative) {
    std::string normalized=relative;
    std::replace(normalized.begin(),normalized.end(),'\\','/');
    if(!SafePath(normalized))return {};
    if(base.empty() || base.back()!='/')base+='/';
    std::vector<std::string> candidates{base};
    size_t at=0;
    while(at<normalized.size()) {
        const auto end=normalized.find('/',at);
        const auto part=normalized.substr(at,end-at);
        std::vector<std::string> next;
        for(const auto& directory:candidates) {
            DIR* dir=opendir(directory.c_str());if(!dir)continue;
            std::vector<std::string> names;
            while(auto* entry=readdir(dir))if(strcasecmp(entry->d_name,part.c_str())==0)names.emplace_back(entry->d_name);
            closedir(dir);
            std::sort(names.begin(),names.end(),[&](const auto& a,const auto& b){
                if((a==part)!=(b==part))return a==part;
                return a<b;
            });
            for(const auto& name:names) {
                if(next.size()>=256)return {}; // Bound ambiguous extracted trees.
                next.push_back(directory+name+(end==std::string::npos?"":"/"));
            }
        }
        if(next.empty())return {};
        candidates=std::move(next);if(end==std::string::npos)break;
        at=end+1;
    }
    // Extracted installs may contain both Sound and sound. A matching first
    // component is insufficient: select a complete original voice/LIP path.
    return candidates.front();
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
        for(const char* name:{"Fallout - Sound.bsa","Fallout - Sounds.bsa","sound.bsa","sounds.bsa",
                              "Fallout - Voices.bsa","voices.bsa","Fallout - MenuVoices.bsa"}) {
            auto path=LooseAt(root,name);if(!path.empty())result.push_back(path);
        }
    return result;
}

inline bool PlayableAudio(const std::string& path) {
    const auto dot=path.rfind('.');if(dot==std::string::npos)return false;
    auto extension=path.substr(dot);
    for(auto& c:extension)c=char(std::tolower(static_cast<unsigned char>(c)));
    return extension==".wav"||extension==".mp3"||extension==".ogg";
}
struct VoiceLookup {
    std::string path, prefix;
    size_t candidates=0;
    bool ambiguous=false;
};
// A loose directory overrides individual files, not the entire archive folder.
// Keep all sources, normalizing the archive's Windows separators before dedup.
class VoiceResolver {
    std::unordered_map<std::string,std::vector<std::string>> directories;
public:
    VoiceLookup Resolve(const std::string& data,const std::vector<std::string>& archives,
                        const std::string& request) {
        VoiceLookup result;
        if(request.rfind("@voice:",0)!=0)return result;
        const auto colon=request.find(':',7);if(colon==std::string::npos)return result;
        auto voice=request.substr(7,colon-7),suffix=request.substr(colon+1);
        if(voice.empty()||voice.find_first_of("/\\:")!=std::string::npos||voice=="."||voice==".."||suffix.empty())return result;
        for(auto& c:voice)c=char(std::tolower(static_cast<unsigned char>(c)));
        for(auto& c:suffix)c=char(std::tolower(static_cast<unsigned char>(c)));
        result.prefix="sound/voice/fallout3.esm/"+voice+"/";
        const auto key=data+"\n"+result.prefix;
        auto found=directories.find(key);
        if(found==directories.end()) {
            std::vector<std::string> paths;
            for(const auto& root:AudioRoots(data)) {
                const auto folder=LooseAt(root,result.prefix);
                if(DIR* dir=folder.empty()?nullptr:opendir(folder.c_str())) {
                    while(auto* entry=readdir(dir))if(PlayableAudio(entry->d_name))paths.push_back(result.prefix+entry->d_name);
                    closedir(dir);
                }
            }
            for(const auto& archive:archives) {
                std::vector<fo3assets::BsaFileInfo> files;
                if(fo3assets::GetBsaArchive(archive)->List(result.prefix,files))for(const auto& file:files)
                    if(PlayableAudio(file.path))paths.push_back(file.path);
            }
            for(auto& path:paths) {
                path=fo3assets::NormalizeArchivePath(path);
                std::replace(path.begin(),path.end(),'\\','/');
            }
            std::sort(paths.begin(),paths.end());paths.erase(std::unique(paths.begin(),paths.end()),paths.end());
            found=directories.emplace(key,std::move(paths)).first;
        }
        result.candidates=found->second.size();
        for(const auto& path:found->second) {
            const auto stem=path.substr(0,path.rfind('.'));
            if(stem.size()<suffix.size()||stem.compare(stem.size()-suffix.size(),suffix.size(),suffix)!=0)continue;
            if(!result.path.empty()) {result.ambiguous=true;result.path.clear();return result;}
            result.path=path;
        }
        return result;
    }
};
} // namespace fo3audio
