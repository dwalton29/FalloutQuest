#pragma once
#include <cstdint>
namespace fo3vr {
struct Tracking {bool valid=false,tracked=false;};
inline Tracking LocationTracking(uint64_t flags,uint64_t validMask,uint64_t trackedMask){
 return {(flags&validMask)==validMask,(flags&trackedMask)==trackedMask};
}
inline bool PipboyAvailable(bool ready,bool loading,bool focused,Tracking head,
                            Tracking grip,Tracking aim){
 const Tracking left=grip.valid?grip:aim;
 return ready&&!loading&&focused&&head.valid&&head.tracked&&left.valid&&left.tracked;
}
}
