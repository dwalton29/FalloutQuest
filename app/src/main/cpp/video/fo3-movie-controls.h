#pragma once
#include <cstdint>
namespace fo3movie {
struct Controls {
    uint64_t serial=0;bool neutral=false,aDown=false,bDown=false;
    bool Step(uint64_t request,bool a,bool b,bool focused,bool failed,bool interruptible){
        if(serial!=request){*this={};serial=request;aDown=a;bDown=b;return false;}
        const bool pressA=a&&!aDown,pressB=b&&!bDown;aDown=a;bDown=b;
        if(!focused){neutral=false;return false;}
        if(!a&&!b)neutral=true;
        return neutral&&((failed&&pressA)||(!failed&&interruptible&&pressB));
    }
};
}
