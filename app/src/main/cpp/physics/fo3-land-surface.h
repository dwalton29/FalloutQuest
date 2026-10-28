#pragma once
namespace fo3land {
// Same p00-p11 diagonal as both cold and streamed LAND rendering.
inline float Height(float h00,float h10,float h01,float h11,float x,float y){
    return x>=y ? h00+(h10-h00)*x+(h11-h10)*y
                : h00+(h11-h01)*x+(h01-h00)*y;
}
}
