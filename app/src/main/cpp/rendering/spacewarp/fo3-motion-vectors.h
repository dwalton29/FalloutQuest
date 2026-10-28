#pragma once
#include "fo3-frame-history.h"
#include <algorithm>
namespace fqspacewarp {
using Vector4=std::array<float,4>;
inline Vector4 Transform(const Matrix& m,const Vector4& p){Vector4 out{};for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)out[row]+=m[col*4+row]*p[col];return out;}
inline bool Inverse(const Matrix& m,Matrix& out){
  if(!Finite(m))return false;
  double a[4][8]{};for(unsigned r=0;r<4;++r){for(unsigned c=0;c<4;++c)a[r][c]=m[c*4+r];a[r][r+4]=1;}
  for(unsigned c=0;c<4;++c){unsigned pivot=c;for(unsigned r=c+1;r<4;++r)if(std::abs(a[r][c])>std::abs(a[pivot][c]))pivot=r;
    if(std::abs(a[pivot][c])<1e-12)return false;
    for(unsigned j=0;j<8;++j)std::swap(a[c][j],a[pivot][j]);
    const double scale=a[c][c];for(double& v:a[c])v/=scale;
    for(unsigned r=0;r<4;++r)if(r!=c){const double factor=a[r][c];for(unsigned j=0;j<8;++j)a[r][j]-=factor*a[c][j];}}
  Matrix candidate{};for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)candidate[c*4+r]=float(a[r][c+4]);
  if(!Finite(candidate))return false;
  out=candidate;return true;
}
// GL depth is [0,1]; its source clip coordinates are [-1,1]. Scale is an
// explicit backend/API contract (including EXT's depth-range/Y convention).
inline bool StaticMotion(const Matrix& inverseCurrent,const Matrix& previous,
                         float u,float v,float depth,const std::array<float,3>& scale,
                         std::array<float,3>& velocity){
  velocity={};if(!std::isfinite(u)||!std::isfinite(v)||!std::isfinite(depth)||depth<0||depth>=1)return false;
  const Vector4 current{u*2-1,v*2-1,depth*2-1,1};
  const auto world=Transform(inverseCurrent,current);const auto prior=Transform(previous,world);
  if(!std::isfinite(prior[3])||prior[3]<=1e-6f)return false;
  for(unsigned i=0;i<3;++i){velocity[i]=(current[i]-prior[i]/prior[3])*scale[i];if(!std::isfinite(velocity[i])){velocity={};return false;}}
  return true;
}
inline constexpr const char* StaticVertex=R"GLSL(#version 300 es
precision highp float;
out vec2 uv;
void main(){vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));uv=p;gl_Position=vec4(p*2.0-1.0,0.0,1.0);}
)GLSL";
inline constexpr const char* StaticFragment=R"GLSL(#version 300 es
precision highp float;
precision highp sampler2D;
in vec2 uv;
uniform sampler2D uDepth;
uniform mat4 uInverseCurrentViewProjection;
uniform mat4 uPreviousViewProjection;
uniform vec3 uMotionScale;
layout(location=0) out vec4 velocity;
void main(){
 float depth=texture(uDepth,uv).r;gl_FragDepth=depth;
 vec4 current=vec4(uv*2.0-1.0,depth*2.0-1.0,1.0);
 vec4 previous=uPreviousViewProjection*(uInverseCurrentViewProjection*current);
 velocity=vec4(0.0);
 if(depth<1.0&&previous.w>0.000001)velocity.xyz=(current.xyz-previous.xyz/previous.w)*uMotionScale;
}
)GLSL";
inline constexpr const char* DebugFragment=R"GLSL(#version 300 es
precision highp float;
in vec2 uv;
uniform sampler2D uMotion;
layout(location=0) out vec4 colour;
void main(){colour=vec4(vec3(.5)+texture(uMotion,uv).xyz*4.0,1.0);}
)GLSL";
} // namespace fqspacewarp
