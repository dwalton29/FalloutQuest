#pragma once
#include <string>
#include <cctype>

// Editor trigger NIFs describe script volumes, not world-visible objects.
// Keep their REFR residency independently of mesh admission.
inline bool Fo3IsTriggerHelperModel(std::string path) {
    for(char& c:path){if(c=='/')c='\\';c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));}
    return path.rfind("triggers\\",0)==0;
}
