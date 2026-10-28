#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <vector>
namespace fo3terminal {
using Point=std::array<float,3>;
inline Point Add(Point a,Point b){for(int i=0;i<3;++i)a[i]+=b[i];return a;}
inline Point Sub(Point a,Point b){for(int i=0;i<3;++i)a[i]-=b[i];return a;}
inline Point Scale(Point a,float f){for(float& v:a)v*=f;return a;}
inline float Dot(Point a,Point b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline Point Cross(Point a,Point b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
inline Point Normal(Point a){float l=std::sqrt(Dot(a,a));return l>1e-7f?Scale(a,1/l):Point{};}
struct Surface {
  std::vector<Point> points;std::vector<uint32_t> triangles;
  Point right{},up{},normal{},origin{};
  float left=0,bottom=0,width=0,height=0;
  bool Ready() const{return width>.03f&&height>.03f&&!triangles.empty();}
  // Select the largest connected surface. Original screen materials also contain
  // keyboard lamps and side labels; those must never become the display area.
  bool Build(const std::vector<Point>& source,const std::vector<uint32_t>& indices,const std::vector<Point>& normals,Point upHint={0,1,0},Point fallbackUp={0,0,-1}){
    *this={};if(source.empty()||normals.size()!=source.size()||indices.size()%3)return false;
    std::vector<size_t> parent(source.size());std::iota(parent.begin(),parent.end(),0);
    auto root=[&](size_t n){while(parent[n]!=n){parent[n]=parent[parent[n]];n=parent[n];}return n;};
    for(size_t i=0;i<indices.size();i+=3){if(indices[i]>=source.size()||indices[i+1]>=source.size()||indices[i+2]>=source.size())return false;
      parent[root(indices[i+1])]=root(indices[i]);parent[root(indices[i+2])]=root(indices[i]);}
    std::vector<float> areas(source.size(),0);
    for(size_t i=0;i<indices.size();i+=3){auto cross=Cross(Sub(source[indices[i+1]],source[indices[i]]),Sub(source[indices[i+2]],source[indices[i]]));areas[root(indices[i])]+=std::sqrt(Dot(cross,cross));}
    auto best=size_t(std::max_element(areas.begin(),areas.end())-areas.begin());if(areas[best]<=0)return false;
    points=source;
    for(size_t i=0;i<indices.size();i+=3)if(root(indices[i])==best)for(int c=0;c<3;++c)triangles.push_back(indices[i+c]);
    std::vector<bool> used(source.size());for(auto i:triangles)used[i]=true;
    int n=0;for(size_t i=0;i<source.size();++i)if(used[i]){normal=Add(normal,normals[i]);origin=Add(origin,source[i]);++n;}
    normal=Normal(normal);origin=Scale(origin,1.f/n);
    // Follow the authored placement even for rotated or horizontal consoles.
    auto horizontal=Cross(upHint,normal);
    if(Dot(horizontal,horizontal)<.0001f)horizontal=Cross(fallbackUp,normal);
    right=Normal(horizontal);up=Normal(Cross(normal,right));
    float r=-1e9f,t=-1e9f;left=bottom=1e9f;
    for(size_t i=0;i<source.size();++i)if(used[i]){auto d=Sub(source[i],origin);float x=Dot(d,right),y=Dot(d,up);left=std::min(left,x);r=std::max(r,x);bottom=std::min(bottom,y);t=std::max(t,y);}
    width=r-left;height=t-bottom;return Ready();
  }
  Point At(float u,float v,float offset=.0015f) const {
    const float x=left+u*width,y=bottom+(1-v)*height; // screen v grows down
    for(size_t i=0;i<triangles.size();i+=3){
      const auto a=points[triangles[i]],b=points[triangles[i+1]],c=points[triangles[i+2]];
      auto p=Sub(a,origin),ab=Sub(b,a),ac=Sub(c,a);float ax=Dot(p,right),ay=Dot(p,up);
      float bx=Dot(ab,right),by=Dot(ab,up),cx=Dot(ac,right),cy=Dot(ac,up),det=bx*cy-by*cx;
      if(std::fabs(det)<1e-10f)continue;
      float s=((x-ax)*cy-(y-ay)*cx)/det,t=(bx*(y-ay)-by*(x-ax))/det;
      if(s>=-.0001f&&t>=-.0001f&&s+t<=1.0001f)return Add(Add(a,Add(Scale(ab,s),Scale(ac,t))),Scale(normal,offset));
    }
    return Add(Add(origin,Add(Scale(right,x),Scale(up,y))),Scale(normal,offset));
  }
};
}
