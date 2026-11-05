#pragma once
#include "fo3-player-state.h"
#include <fstream>
#include <sstream>
#include <cctype>
#include <dirent.h>

namespace fo3newgame {
inline std::string Trim(std::string s) {
  const auto a=s.find_first_not_of(" \t\r\n");
  return a==std::string::npos?std::string{}:s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);
}
inline std::string Lower(std::string s){for(auto& c:s)c=char(std::tolower(static_cast<unsigned char>(c)));return s;}
// The installation selects the bootstrap quest; the engine embeds no quest ID.
inline bool ReadQuest(std::istream& input,uint32_t& quest,std::string& error){
  quest=0;std::string line,section;bool seen=false;
  while(std::getline(input,line)){
    if(line.compare(0,3,"\xEF\xBB\xBF")==0)line.erase(0,3);
    line=Trim(line.substr(0,line.find(';')));
    if(line.empty())continue;
    if(line.front()=='['&&line.back()==']'){section=Lower(Trim(line.substr(1,line.size()-2)));continue;}
    const auto eq=line.find('=');if(section!="general"||eq==std::string::npos||Lower(Trim(line.substr(0,eq)))!="schargenquest")continue;
    const auto value=Trim(line.substr(eq+1));
    if(seen||value.empty()||value.size()>8||value.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos){error="Invalid or duplicate General/SCharGenQuest";return false;}
    seen=true;quest=uint32_t(std::stoul(value,nullptr,16));
  }
  if(!seen||!quest){error="Missing General/SCharGenQuest";return false;}error.clear();return true;
}
inline bool FindQuest(const std::string& dataRoot,uint32_t& quest,std::string& source,std::string& error){
  auto root=dataRoot;while(!root.empty()&&root.back()=='/')root.pop_back();
  const auto slash=root.find_last_of('/');
  const std::vector<std::string> roots={root,slash==std::string::npos?root:root.substr(0,slash)};
  for(const char* name:{"fallout.ini","fallout_default.ini"})for(const auto& directory:roots){
    DIR* dir=opendir(directory.c_str());if(!dir)continue;std::string path;
    while(auto* entry=readdir(dir))if(Lower(entry->d_name)==name){path=directory+'/'+entry->d_name;break;}
    closedir(dir);if(path.empty())continue;
    std::ifstream input(path);if(!input){error="Could not read "+path;return false;}
    source=path;return ReadQuest(input,quest,error);
  }
  error="New Game needs Fallout.ini or Fallout_default.ini beside Data (or inside Data), with General/SCharGenQuest.";return false;
}
} // namespace fo3newgame
