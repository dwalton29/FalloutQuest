#pragma once
#include <array>
#include <cmath>
#include <cstdint>

namespace fo3shoulder {
using Point = std::array<float,3>;
// VR adaptation in metres: local axes are right/up/rear in the inferred torso.
// A small exit shell prevents edge chatter; the rear plane excludes the face.
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
        const float shell=entered ? .03f : 0;
        const float x=p[0]/(.18f+shell),y=p[1]/(.20f+shell),z=(p[2]-.09f)/(.18f+shell);
        return p[2]>=-.025f && x*x+y*y+z*z<=1;
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
        if(grip>.25f && now-enteredAt>=.08) armed=true;
        // Release commits only an already armed gesture. Passing through with
        // an open grip cannot arm it; exiting/untracked clears all intent.
        return grip<=.25f && armed;
    }
};
} // namespace fo3shoulder
