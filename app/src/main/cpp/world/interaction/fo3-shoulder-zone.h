#pragma once
#include <array>
#include <cmath>
#include <cstdint>

namespace fo3shoulder {
using Point = std::array<float,3>;
// VR adaptation in metres: local axes are right/up/rear in the inferred torso.
// The first headset pass used a small shoulder-centred ellipsoid and proved too
// brittle in real use: a naturally raised hand often sits 25-40 cm from the
// upper-arm pivot. Treat this as a forgiving backpack-mouth volume behind the
// shoulder instead. Release still commits only a currently held eligible REFR,
// so a generous volume is safer and more natural than precision targeting.
struct Zone {
    Point shoulder{}, right{1,0,0}, rear{0,0,1};
    bool ready=false;
    Point Coordinates(Point hand) const {
        Point d{hand[0]-shoulder[0],hand[1]-shoulder[1],hand[2]-shoulder[2]};
        return {d[0]*right[0]+d[2]*right[2],d[1],d[0]*rear[0]+d[2]*rear[2]};
    }
    bool Contains(Point hand,bool entered) const {
        if (!ready) return false;
        for(float v:hand) if(!std::isfinite(v)) return false;
        const auto p=Coordinates(hand);
        const float shell=entered ? .05f : 0.0f;
        // Broad rounded volume centred behind/slightly above the authored
        // shoulder. The rear plane is the important false-positive guard:
        // ordinary chest/face movement can never inventory an item.
        const float x=(p[0]-.04f)/(.27f+shell);
        const float y=(p[1]-.03f)/(.30f+shell);
        const float z=(p[2]-.17f)/(.28f+shell);
        return p[2]>=0.015f-shell*0.35f &&
               x*x+y*y+z*z<=1.0f;
    }
};
struct Gesture {
    uint32_t reference=0;
    bool inside=false,armed=false;
    double enteredAt=0;
    void Reset() {*this={};}
    bool Update(int hand,uint32_t ref,bool tracked,bool eligible,float grip,
                const Zone& zone,Point palm,double now) {
        if(hand!=1 || !ref || !tracked || !eligible || !std::isfinite(grip) || !std::isfinite(now)) {
            Reset(); return false;
        }
        if(reference!=ref) {Reset();reference=ref;}
        const bool next=zone.Contains(palm,inside);
        if(!next) {inside=armed=false;return false;}
        if(!inside) {inside=true;enteredAt=now;}
        // The object is already in a latched physical grab, so dwelling for
        // 80 ms adds friction without adding useful intent. One tracked frame
        // inside while grip is still held is enough to arm; release commits.
        if(grip>.25f) armed=true;
        return grip<=.25f && armed;
    }
};
} // namespace fo3shoulder
