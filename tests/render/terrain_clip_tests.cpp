#include "rendering/terrain/fo3-terrain-bounds.h"
#include <array>
#include <cassert>
#include <limits>
#include <random>
#include <vector>
using Point=std::array<double,4>;
static Point Transform(const float* m,double x,double y,double z){
    Point p{};for(int r=0;r<4;++r)p[r]=m[r]*x+m[r+4]*y+m[r+8]*z+m[r+12];return p;
}
// Independent Sutherland-Hodgman clipping, including intersections of edges
// whose endpoints are all outside. No AABB/frustum helper used by this oracle.
static bool VisibleTriangle(std::vector<Point> points){
    for(int axis=0;axis<3;++axis)for(int sign:{-1,1}){
        std::vector<Point> next;
        if(points.empty())return false;
        auto a=points.back();double da=a[3]+sign*a[axis];
        for(auto b:points){
            double db=b[3]+sign*b[axis];
            if((da>=0)!=(db>=0)){
                const double t=da/(da-db);Point p{};
                for(int k=0;k<4;++k)p[k]=a[k]+t*(b[k]-a[k]);
                next.push_back(p);
            }
            if(db>=0)next.push_back(b);
            a=b;da=db;
        }
        points=std::move(next);
    }
    return !points.empty();
}
int main(){
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    assert(fo3terrain::MayIntersectClip({},identity)); // unknown streamed bounds must draw
    std::vector<float> stream(36,0);
    stream[0]=100;stream[1]=20;stream[2]=-30;
    stream[12]=200;stream[13]=25;stream[14]=-40;
    stream[24]=150;stream[25]=30;stream[26]=-35;
    auto b=fo3terrain::BuildBounds(stream.data(),stream.size());
    assert(b.boundsValid&&b.minX==100&&b.maxX==200&&b.minY==20&&b.maxZ==-30);
    assert(!fo3terrain::MayIntersectClip(b,identity));
    float translated[16];std::copy(identity,identity+16,translated);
    translated[12]=-150;translated[13]=-25;translated[14]=35;
    assert(fo3terrain::MayIntersectClip(b,translated));
    stream[12]=std::numeric_limits<float>::quiet_NaN();
    assert(!fo3terrain::BuildBounds(stream.data(),stream.size()).boundsValid);
    const fo3terrain::Bounds lightBounds{0,1,0,1,0,1,true};
    const float outside[]{3,0,0,1},touching[]{2,0,0,1},inside[]{.5f,.5f,.5f,.1f};
    assert(!fo3terrain::MayIntersectLight(lightBounds,outside));
    assert(fo3terrain::MayIntersectLight(lightBounds,touching));
    assert(fo3terrain::MayIntersectLight(lightBounds,inside));
    assert(fo3terrain::MayIntersectLight({},outside));
    const float invalid[]{std::numeric_limits<float>::quiet_NaN(),0,0,1};
    assert(fo3terrain::MayIntersectLight(lightBounds,invalid));
    const float lights[]{3,0,0,1, .5f,.5f,.5f,.1f, 2,0,0,1};
    const float colours[]{1,2,3,4, 5,6,7,8, -1,-2,-3,-4};
    float keptPositions[12]{},keptColours[12]{};
    assert(fo3terrain::CullLights(lightBounds,3,lights,colours,keptPositions,keptColours)==2);
    assert(std::equal(lights+4,lights+12,keptPositions));
    assert(std::equal(colours+4,colours+12,keptColours)); // preserve order and negative light colour
    // Actual OpenXR GLES projection convention: asymmetric FOV, near .04m,
    // far 125000/70m. Translate/rotate and mirror the view across a water plane.
    std::mt19937 random(231);std::uniform_real_distribution<float> d(-1000,1000),u(-1,1);
    size_t rejected=0,visible=0;
    for(int i=0;i<20000;++i){
        const float n=.04f,f=125000.f/70;
        float p[16]={1.1f,0,0,0,0,1.25f,0,0,.12f,-.08f,-(f+n)/(f-n),-1,0,0,-2*f*n/(f-n),0};
        // Matrix columns transformed without changing the projection convention.
        const float angle=u(random)*3.14f,c=std::cos(angle),s=std::sin(angle);
        const float mirror=(i%2)?-1.f:1.f;
        float m[16]{};
        for(int r=0;r<4;++r){
            m[r]=p[r]*c+p[r+8]*s;m[r+4]=p[r+4]*mirror;
            m[r+8]=-p[r]*s+p[r+8]*c;
            m[r+12]=p[r+12]+p[r+4]*(i%2?14.f:0.f);
        }
        const float x=d(random),y=d(random),z=d(random),extent=std::abs(u(random))*100;
        std::vector<float> vertices(36,0);
        std::vector<Point> clip;
        for(int k=0;k<3;++k){
            const float px=x+u(random)*extent,py=y+u(random)*extent,pz=z+u(random)*extent;
            vertices[k*12]=px;vertices[k*12+1]=py;vertices[k*12+2]=pz;
            clip.push_back(Transform(m,px,py,pz));
        }
        auto bounds=fo3terrain::BuildBounds(vertices.data(),vertices.size());
        const bool draw=fo3terrain::MayIntersectClip(bounds,m);
        const bool raster=VisibleTriangle(clip);
        assert(draw||!raster); // zero false-negative raster contributions
        rejected+=!draw;visible+=raster;
    }
    assert(rejected>5000&&visible>500); // exercise rejection and intersection
}
