#pragma once
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>

namespace fo3loadingmenu {
// Defaults are copied from the supplied Fallout.ini and loading_menu.xml.
struct Definition {
    std::string overlayPath="Interface\\Loading\\LoadingAnim01.NIF";
    std::string compassPath="Interface\\Circular Loading\\loading01.nif";
    std::string compassAnimation="Idle";
    int red=199,green=255,blue=165;
    float fadeSeconds=0.75f;
};
inline std::string Trim(std::string s) {
    auto space=[](unsigned char c){return std::isspace(c)!=0;};
    s.erase(s.begin(),std::find_if_not(s.begin(),s.end(),space));
    s.erase(std::find_if_not(s.rbegin(),s.rend(),space).base(),s.end());return s;
}
inline std::string Lower(std::string s) {
    for(char& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
inline std::string Tag(const std::string& text,const std::string& name) {
    const std::string open="<"+name+">",close="</"+name+">";
    const auto a=text.find(open);if(a==std::string::npos)return {};
    const auto start=a+open.size(),end=text.find(close,start);
    return end==std::string::npos?std::string{}:Trim(text.substr(start,end-start));
}
inline std::string Nif(const std::string& xml,const std::string& name) {
    const auto start=xml.find("<nif name=\""+name+"\">");if(start==std::string::npos)return {};
    const auto end=xml.find("</nif>",start);return end==std::string::npos?std::string{}:xml.substr(start,end-start);
}
inline void ReadXml(const std::string& xml,Definition& out) {
    const auto overlay=Nif(xml,"loading_nif"),compass=Nif(xml,"loading_pinwheel");
    const auto op=Tag(overlay,"filename"),cp=Tag(compass,"filename"),animation=Tag(compass,"animation");
    if(!op.empty())out.overlayPath=op;
    if(!cp.empty())out.compassPath=cp;
    if(!animation.empty())out.compassAnimation=animation;
    const auto fade=Tag(xml,"menufade");char* end=nullptr;
    const float seconds=std::strtof(fade.c_str(),&end);
    if(!fade.empty()&&end&&*end==0&&seconds>0&&seconds<=10)out.fadeSeconds=seconds;
}
inline void ReadIni(const std::string& ini,Definition& out) {
    std::istringstream input(ini);std::string line,section;
    while(std::getline(input,line)) {
        line=Trim(line.substr(0,line.find(';')));
        if(line.empty())continue;
        if(line.front()=='['&&line.back()==']'){section=Lower(Trim(line.substr(1,line.size()-2)));continue;}
        if(section!="interface")continue;
        const auto eq=line.find('=');if(eq==std::string::npos)continue;
        const auto key=Lower(Trim(line.substr(0,eq))),value=Trim(line.substr(eq+1));
        char* end=nullptr;const long n=std::strtol(value.c_str(),&end,10);
        if(value.empty()||!end||*end||n<0||n>255)continue;
        if(key=="isystemcolormainmenured")out.red=static_cast<int>(n);
        else if(key=="isystemcolormainmenugreen")out.green=static_cast<int>(n);
        else if(key=="isystemcolormainmenublue")out.blue=static_cast<int>(n);
    }
}
} // namespace fo3loadingmenu
