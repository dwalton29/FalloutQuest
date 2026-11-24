#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <numeric>
#include <vector>

namespace fo3terrain {
// Continue past patch boundaries in a single heightfield. A contraction moves
// every draw sharing the source point to the same existing destination point.
// Cell borders and holes stay fixed. The 2D winding check prevents LAND folds.
template<class Batch>
void ReduceSharedEdges(const std::vector<Batch*>& batches,
                       std::vector<std::array<std::vector<uint32_t>,3>>& output,
                       unsigned tier,size_t target) {
    using Point=std::array<float,3>;
    using Edge=std::array<uint32_t,2>;
    struct Triangle {
        uint32_t batch,section;
        std::array<uint32_t,3> vertex,point;
        bool live=true;
    };
    std::map<Point,uint32_t> ids;
    std::vector<Point> position;
    std::vector<Triangle> triangles;
    std::vector<std::vector<unsigned>> uses(batches.size());
    std::vector<uint32_t> parent;
    auto root=[&](uint32_t i){
        while(parent[i]!=i){parent[i]=parent[parent[i]];i=parent[i];}
        return i;
    };
    size_t used=0;
    for(uint32_t b=0;b<batches.size();++b){
        auto& batch=*batches[b];const auto& idx=output[b][tier];
        uses[b].resize(batch.vertices.size()/batch.vertexFloats);
        std::map<Edge,uint32_t> faces;
        for(size_t i=0;i<idx.size();i+=3){
            const auto id=uint32_t(triangles.size());
            Triangle tri{b,id,{},{},true};parent.push_back(id);
            for(unsigned j=0;j<3;++j){
                const auto v=tri.vertex[j]=idx[i+j];
                if(!uses[b][v]++)++used;
                const float* p=batch.vertices.data()+v*batch.vertexFloats;
                const Point xyz{p[0],p[1],p[2]};
                auto slot=ids.emplace(xyz,uint32_t(position.size()));
                if(slot.second)position.push_back(xyz);
                tri.point[j]=slot.first->second;
            }
            for(unsigned j=0;j<3;++j){
                auto a=tri.point[j],d=tri.point[(j+1)%3];
                auto edge=faces.emplace(Edge{std::min(a,d),std::max(a,d)},id);
                if(!edge.second)parent[root(id)]=root(edge.first->second);
            }
            triangles.push_back(tri);
        }
    }
    if(used<=target)return;
    std::vector<unsigned> sectionFaces(triangles.size());
    for(auto& tri:triangles){tri.section=root(tri.section);++sectionFaces[tri.section];}
    struct Node{
        std::vector<unsigned> faces;
        std::vector<Edge> wedges; // batch, render vertex
        bool fixed=false,busy=false;
    };
    struct Opposite{uint32_t first=UINT32_MAX,second=UINT32_MAX;bool invalid=false;};
    struct Candidate{uint32_t a,b;float error;};
    auto area=[&](const std::array<uint32_t,3>& p){
        const auto &a=position[p[0]],&b=position[p[1]],&c=position[p[2]];
        return double(b[0]-a[0])*(c[2]-a[2])-double(c[0]-a[0])*(b[2]-a[2]);
    };
    while(used>target){
        const size_t before=used;
        std::vector<Node> nodes(position.size());
        std::map<Edge,Opposite> edges;
        for(unsigned f=0;f<triangles.size();++f){
            const auto& tri=triangles[f];if(!tri.live)continue;
            for(unsigned j=0;j<3;++j){
                auto a=tri.point[j],b=tri.point[(j+1)%3],c=tri.point[(j+2)%3];
                auto& n=nodes[a];n.faces.push_back(f);
                Edge wedge{tri.batch,tri.vertex[j]};
                if(std::find(n.wedges.begin(),n.wedges.end(),wedge)==n.wedges.end())n.wedges.push_back(wedge);
                auto& opp=edges[{std::min(a,b),std::max(a,b)}];
                if(opp.first==UINT32_MAX)opp.first=c;
                else if(opp.first!=c){
                    if(opp.second==UINT32_MAX)opp.second=c;
                    else if(opp.second!=c)opp.invalid=true;
                }
            }
        }
        for(const auto& entry:edges)if(entry.second.second==UINT32_MAX||entry.second.invalid)
            for(auto p:entry.first)nodes[p].fixed=true;
        std::vector<Candidate> candidates;
        candidates.reserve(edges.size()*2);
        // Prefer short edges on smooth slopes and similar paint. Height
        // deviation from the local tangent plane gives cliffs a higher cost.
        for(const auto& entry:edges)for(unsigned dir=0;dir<2;++dir){
            const auto a=entry.first[dir],b=entry.first[1-dir];
            if(nodes[a].fixed)continue;
            const auto &p=position[a],&q=position[b];
            const float dx=q[0]-p[0],dy=q[1]-p[1],dz=q[2]-p[2];
            float error=dx*dx+dy*dy+dz*dz;
            bool supported=true;
            for(auto src:nodes[a].wedges){
                const auto& batch=*batches[src[0]];
                const float* s=batch.vertices.data()+src[1]*batch.vertexFloats;
                const float normal=dx*s[3]+dy*s[4]+dz*s[5];
                float diff=INFINITY;
                for(auto dst:nodes[b].wedges)if(dst[0]==src[0]){
                    const float* d=batch.vertices.data()+dst[1]*batch.vertexFloats;
                    float dist=0;
                    for(size_t k=8;k<batch.vertexFloats;++k){float v=s[k]-d[k];dist+=v*v;}
                    diff=std::min(diff,dist);
                }
                if(!std::isfinite(diff)){supported=false;break;}
                error+=4*normal*normal+4*diff;
            }
            if(supported)candidates.push_back({a,b,error});
        }
        std::sort(candidates.begin(),candidates.end(),[](const Candidate& l,const Candidate& r){
            if(l.error!=r.error)return l.error<r.error;
            return l.a==r.a?l.b<r.b:l.a<r.a;
        });
        for(auto move:candidates){
            const auto a=move.a,b=move.b;
            if(used<=target)break;
            if(nodes[a].busy||nodes[b].busy)continue;
            bool valid=true;
            std::map<uint32_t,unsigned> removed;
            for(auto f:nodes[a].faces){
                const auto& tri=triangles[f];
                if(!tri.live||nodes[tri.point[0]].busy||nodes[tri.point[1]].busy||nodes[tri.point[2]].busy){valid=false;break;}
                if(std::find(tri.point.begin(),tri.point.end(),b)!=tri.point.end()){
                    if(++removed[tri.section]>=sectionFaces[tri.section]){valid=false;break;}
                }else{
                    auto next=tri.point;for(auto& v:next)if(v==a)v=b;
                    const double after=area(next),prior=area(tri.point);
                    // The LAND lattice is transformed to floats in metres.
                    // Reject nearly collinear survivors as well as reversals.
                    double span=0;
                    for(unsigned j=0;j<3;++j){
                        const auto &p=position[next[j]],&q=position[next[(j+1)%3]];
                        span=std::max(span,std::abs(double(p[0])-q[0])+std::abs(double(p[2])-q[2]));
                    }
                    if(after*prior<=0||std::abs(after)<1e-5*span*span){valid=false;break;}
                }
            }
            if(!valid)continue;
            for(auto f:nodes[a].faces){
                auto& tri=triangles[f];
                if(std::find(tri.point.begin(),tri.point.end(),b)!=tri.point.end()){
                    for(auto v:tri.vertex)if(!--uses[tri.batch][v])--used;
                    --sectionFaces[tri.section];tri.live=false;continue;
                }
                const auto& batch=*batches[tri.batch];
                for(unsigned j=0;j<3;++j)if(tri.point[j]==a){
                    const auto original=tri.vertex[j];
                    const float* s=batch.vertices.data()+original*batch.vertexFloats;
                    uint32_t best=0;float error=INFINITY;
                    for(auto dst:nodes[b].wedges)if(dst[0]==tri.batch){
                        const float* d=batch.vertices.data()+dst[1]*batch.vertexFloats;
                        float diff=0;
                        for(size_t k=3;k<batch.vertexFloats;++k){float v=s[k]-d[k];diff+=v*v;}
                        if(diff<error){error=diff;best=dst[1];}
                    }
                    if(!--uses[tri.batch][original])--used;
                    if(!uses[tri.batch][best]++)++used;
                    tri.point[j]=b;tri.vertex[j]=best;
                }
            }
            nodes[a].busy=nodes[b].busy=true;
        }
        if(used>=before)break;
    }
    for(auto& levels:output)levels[tier].clear();
    for(const auto& tri:triangles)if(tri.live){
        auto& indices=output[tri.batch][tier];indices.insert(indices.end(),tri.vertex.begin(),tri.vertex.end());
    }
}
}
