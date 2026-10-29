#include "rendering/tiled-occlusion.h"
#include <cassert>
#include <iostream>
#include <random>
using namespace fo3occ;
const float camera[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
Polygon Quad(float left,float right,float bottom,float top,float z){
    const Point p[]{{left,bottom,z},{right,bottom,z},{right,top,z},{left,top,z}};
    Polygon out;assert(ProjectPolygon(camera,p,4,false,false,out));return out;
}
bool Hidden(const TiledBuffer& b,Rect r){uint32_t tests=0;return b.Hidden(r,tests,100000);}
int main(){
    TiledBuffer buffer;
    const Rect target{-.3f,.3f,-.2f,.2f,.5f};
    auto left=Quad(-.9f,.013f,-.9f,.9f,-.5f),right=Quad(.013f,.9f,-.9f,.9f,-.4f);
    assert(!left.Covers(target)&&!right.Covers(target));
    buffer.Add(left);assert(!Hidden(buffer,target));buffer.Add(right);assert(Hidden(buffer,target));
    assert(!Hidden(buffer,{-.3f,.3f,-.2f,.2f,-.6f}));
    buffer.Clear();buffer.Add(Quad(-.9f,-.001f,-.9f,.9f,-.5f));buffer.Add(Quad(.001f,.9f,-.9f,.9f,-.5f));
    assert(!Hidden(buffer,target)); // A genuine gap is never filled.
    buffer.Clear();buffer.Add(left);buffer.Add(Quad(.013f,.9f,-.9f,.9f,.8f));assert(!Hidden(buffer,target));
    buffer.Clear();buffer.Add(left);buffer.Add(Quad(-.9f,.9f,-.9f,.9f,.8f));assert(!Hidden(buffer,target)); // Partial shallow data cannot become a full shallow tile.
    buffer.Clear();buffer.Add(Quad(-.9f,.9f,-.9f,.9f,-.5f),[]{return false;});assert(!Hidden(buffer,target));
    buffer.Clear();buffer.Add(Quad(-.9f,.9f,-.9f,.9f,-.5f));uint32_t tests=0;assert(!buffer.Hidden(target,tests,1));
    // Two authored triangles share a diagonal; their union covers the tile seam.
    buffer.Clear();Point a[]{{-.9f,-.9f,-.5f},{.9f,-.9f,-.5f},{.9f,.9f,-.5f}},b[]{{-.9f,-.9f,-.5f},{.9f,.9f,-.5f},{-.9f,.9f,-.5f}};
    Polygon pa,pb;assert(ProjectPolygon(camera,a,3,false,false,pa));assert(ProjectPolygon(camera,b,3,false,false,pb));buffer.Add(pa);buffer.Add(pb);assert(Hidden(buffer,target));
    std::mt19937 random(284);std::uniform_real_distribution<float> coordinate(-.95f,.95f),size(.04f,.6f);
    size_t hidden=0;
    for(int scene=0;scene<80;++scene){
        buffer.Clear();std::vector<Polygon> polygons;
        for(int i=0;i<40;++i){const float x=coordinate(random),y=coordinate(random),w=size(random),h=size(random);const float angle=coordinate(random)*3.14f,c=std::cos(angle),s=std::sin(angle);
            Point vertices[4]{{x,y,0},{x+w*c,y+w*s,0},{x+w*c-h*s,y+w*s+h*c,0},{x-h*s,y+h*c,0}};
            for(auto& v:vertices)v.z=-.6f+.02f*(i%20);
            Polygon p;assert(ProjectPolygon(camera,vertices,i%3==0?3:4,false,false,p));polygons.push_back(p);buffer.Add(p);}
        for(int query=0;query<100;++query){const float x=coordinate(random),y=coordinate(random);Rect r{x,x+.04f,y,y+.04f,.5f};if(!Hidden(buffer,r))continue;++hidden;
            // Independent dense point oracle against the original half-planes,
            // including the expanded query boundary and depth separation.
            for(int iy=0;iy<=20;++iy)for(int ix=0;ix<=20;++ix){const double px=r.left-.004+(r.right-r.left+.008)*ix/20,py=r.bottom-.004+(r.top-r.bottom+.008)*iy/20;bool covered=false;
                for(const auto& p:polygons){bool inside=r.nearZ>p.farZ+.00001f;for(unsigned e=0;e<p.count;++e)inside&=p.nx[e]*px+p.ny[e]*py+p.c[e]>=-1e-7;covered|=inside;}assert(covered);}
        }
    }
    assert(hidden>100);
    std::cout<<"Tiled occlusion: adjacent triangles/quads, real cracks, mixed depths, partial/full regression, budget fail-open and randomized union coverage passed; hidden="<<hidden<<"\n";
}
