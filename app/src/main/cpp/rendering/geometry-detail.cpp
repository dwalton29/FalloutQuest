#include "geometry-detail.h"
#include "../../../../../third_party/meshoptimizer/meshoptimizer.h"
#include <numeric>
#include <map>
#include <unordered_map>

namespace fqdetail {
namespace {
// Keep one actual source face in every island. Edge collapse can otherwise
// eliminate thin disconnected trim inside a single NiTriShape.
void ProtectIslands(const float* v,size_t stride,const std::vector<uint32_t>& indices,
                    std::vector<unsigned char>& locked) {
    std::map<std::array<float,3>,uint32_t> positions;
    std::vector<uint32_t> positionId(locked.size(),UINT32_MAX);
    for(auto i:indices){
        if(positionId[i]!=UINT32_MAX)continue;
        const float* p=v+i*stride;
        positionId[i]=positions.emplace(std::array<float,3>{p[0],p[1],p[2]},i).first->second;
    }
    // A split UV or normal on an edge still belongs to the same physical
    // piece. A point contact alone does not join two pieces.
    std::vector<uint32_t> parent(indices.size()/3);
    std::iota(parent.begin(),parent.end(),0u);
    auto root=[&](uint32_t i) {
        while(parent[i]!=i){parent[i]=parent[parent[i]];i=parent[i];}
        return i;
    };
    std::unordered_map<uint64_t,uint32_t> edges;
    for(size_t i=0;i<indices.size();i+=3)for(unsigned j=0;j<3;++j){
        const uint32_t a=positionId[indices[i+j]],b=positionId[indices[i+(j+1)%3]];
        if(a==b)continue;
        const uint64_t key=(uint64_t(std::min(a,b))<<32)|std::max(a,b);
        auto edge=edges.emplace(key,uint32_t(i/3));
        if(!edge.second)parent[root(edge.first->second)]=root(uint32_t(i/3));
    }
    std::vector<double> largest(parent.size(),0);
    std::vector<size_t> face(parent.size());
    for(size_t i=0;i<indices.size();i+=3){
        const float* a=v+indices[i]*stride;
        const float* b=v+indices[i+1]*stride;
        const float* c=v+indices[i+2]*stride;
        double ab[3],ac[3],area=0;
        for(unsigned j=0;j<3;++j){ab[j]=double(b[j])-a[j];ac[j]=double(c[j])-a[j];}
        for(unsigned j=0;j<3;++j){
            const double n=ab[(j+1)%3]*ac[(j+2)%3]-ab[(j+2)%3]*ac[(j+1)%3];
            area+=n*n;
        }
        const auto island=root(uint32_t(i/3));
        if(area>largest[island]){largest[island]=area;face[island]=i;}
    }
    for(size_t i=0;i<largest.size();++i)if(largest[i]>0)
        for(size_t j=face[i];j<face[i]+3;++j)locked[indices[j]]=1;
}

// Meshoptimizer may stop at a non-manifold or split-attribute edge. Collapse
// that physical edge consistently across its render vertices. Never move an
// island anchor or invert a surviving triangle.
size_t CollapseSceneryEdges(const float* v,size_t stride,const std::vector<unsigned char>& locked,
                            std::vector<uint32_t>& indices,size_t target,size_t used) {
    using Point=std::array<float,3>;
    struct Node {Point p;bool fixed=false;std::vector<uint32_t> wedges,faces;};
    std::vector<Node> nodes;
    std::map<Point,uint32_t> ids;
    std::vector<uint32_t> node(locked.size(),UINT32_MAX),remap(locked.size());
    std::iota(remap.begin(),remap.end(),0u);
    for(size_t i=0;i<indices.size();++i){
        const uint32_t id=indices[i];
        if(node[id]==UINT32_MAX){
            const float* p=v+id*stride;
            const Point xyz{p[0],p[1],p[2]};
            auto slot=ids.emplace(xyz,uint32_t(nodes.size()));
            if(slot.second)nodes.push_back({xyz,false,{},{}});
            node[id]=slot.first->second;
            auto& n=nodes[node[id]];n.fixed|=locked[id]!=0;n.wedges.push_back(id);
        }
        nodes[node[id]].faces.push_back(uint32_t(i/3));
    }
    struct Edge {float cost;uint32_t a,b;};
    std::vector<Edge> edges;
    for(size_t i=0;i<indices.size();i+=3)for(unsigned k=0;k<3;++k){
        auto a=node[indices[i+k]],b=node[indices[i+(k+1)%3]];
        if(a==b||(nodes[a].fixed&&nodes[b].fixed))continue;
        if(a>b)std::swap(a,b);
        float length=0;for(unsigned j=0;j<3;++j){float d=nodes[a].p[j]-nodes[b].p[j];length+=d*d;}
        edges.push_back({length,a,b});
    }
    std::sort(edges.begin(),edges.end(),[](const Edge& l,const Edge& r){
        if(l.cost!=r.cost)return l.cost<r.cost;
        return l.a==r.a?l.b<r.b:l.a<r.a;
    });
    std::vector<unsigned char> busy(nodes.size());
    auto normal=[](const Point& p,const Point& q,const Point& r){
        Point n{};
        for(unsigned j=0;j<3;++j){auto u=(j+1)%3,w=(j+2)%3;n[j]=(q[u]-p[u])*(r[w]-p[w])-(q[w]-p[w])*(r[u]-p[u]);}
        return n;
    };
    for(auto edge:edges){
        auto a=edge.a,b=edge.b;
        if(busy[a]||busy[b])continue;
        if(nodes[a].fixed||(!nodes[b].fixed&&nodes[b].wedges.size()<nodes[a].wedges.size()))std::swap(a,b);
        if(nodes[a].wedges.size()>used-target && !nodes[b].fixed)std::swap(a,b);
        if(nodes[a].fixed)continue;
        bool valid=true;
        for(auto f:nodes[a].faces){
            std::array<uint32_t,3> t{node[indices[3*f]],node[indices[3*f+1]],node[indices[3*f+2]]};
            if(busy[t[0]]||busy[t[1]]||busy[t[2]]){valid=false;break;}
            if(std::find(t.begin(),t.end(),b)!=t.end())continue;
            const auto before=normal(nodes[t[0]].p,nodes[t[1]].p,nodes[t[2]].p);
            for(auto& id:t)if(id==a)id=b;
            const auto after=normal(nodes[t[0]].p,nodes[t[1]].p,nodes[t[2]].p);
            if(std::inner_product(before.begin(),before.end(),after.begin(),0.f)<=0.f){valid=false;break;}
        }
        if(!valid)continue;
        for(auto src:nodes[a].wedges){
            auto best=nodes[b].wedges.front();float error=INFINITY;
            for(auto dst:nodes[b].wedges){
                const auto* s=v+src*stride;const auto* d=v+dst*stride;
                float dist=0;
                for(size_t j=3;j<stride;++j){float e=s[j]-d[j];dist+=e*e*((j==12||j==13)?16.f:1.f);}
                if(dist<error){error=dist;best=dst;}
            }
            remap[src]=best;
        }
        busy[a]=busy[b]=1;
        if(nodes[a].wedges.size()>=used-target){used=target;break;}
        used-=nodes[a].wedges.size();
    }
    size_t out=0;
    for(size_t i=0;i<indices.size();i+=3){
        const uint32_t a=remap[indices[i]],b=remap[indices[i+1]],c=remap[indices[i+2]];
        if(node[a]==node[b]||node[a]==node[c]||node[b]==node[c])continue;
        indices[out++]=a;indices[out++]=b;indices[out++]=c;
    }
    indices.resize(out);
    return out;
}

size_t Referenced(const std::vector<uint32_t>& indices,std::vector<unsigned char>& seen) {
    std::fill(seen.begin(),seen.end(),0);
    size_t count=0;
    for(auto i:indices)if(!seen[i]){seen[i]=1;++count;}
    return count;
}
}

Levels Build(Family family,const float* vertices,size_t count,size_t stride,
             const std::vector<uint32_t>& sourceCorners,bool expanded,
             const std::vector<uint16_t>* bones,const std::vector<float>* weights) {
    Levels levels;
    levels.family=family;
    const size_t corners=sourceCorners.size();
    if(!vertices||corners<3||corners%3||!count||count>UINT32_MAX||
       corners>UINT32_MAX/3||(expanded&&count!=corners)||
       (stride!=18 && (family!=Family::Terrain || (stride!=12 && stride!=15))))return levels;
    const size_t unique=1ull+*std::max_element(sourceCorners.begin(),sourceCorners.end());
    if((!expanded&&unique>count)||unique>std::max(count,corners)*4)return levels;
    std::vector<uint32_t> first;
    std::vector<float> compact;
    if(expanded){
        first.assign(unique,UINT32_MAX);
        compact.resize(unique*stride);
        for(size_t i=0;i<corners;++i){
            const unsigned v=sourceCorners[i];
            if(first[v]!=UINT32_MAX)continue;
            first[v]=static_cast<uint32_t>(i);
            std::copy_n(vertices+i*stride,stride,compact.data()+v*stride);
        }
        vertices=compact.data();
    }
    for(size_t i=0;i<unique*stride;++i)if(!std::isfinite(vertices[i]))return levels;
    std::vector<unsigned char> locked(unique);
    if(family!=Family::Terrain)ProtectIslands(vertices,stride,sourceCorners,locked);
    if(bones||weights){
        if(!bones||!weights||bones->size()<unique*4||weights->size()<unique*4)return levels;
        // Protect edges spanning unrelated bones. Vertices within a blended
        // joint keep their original weights and palette on the existing GPU stream.
        for(size_t i=0;i<corners;i+=3)for(int k=0;k<3;++k){
            const unsigned a=sourceCorners[i+k],b=sourceCorners[i+(k+1)%3];
            bool related=false;
            for(unsigned j=0;j<4&&!related;++j)if((*weights)[4*a+j]>0)
                for(unsigned k=0;k<4;++k)if((*weights)[4*b+k]>0&&(*bones)[4*a+j]==(*bones)[4*b+k]){related=true;break;}
            if(!related)locked[a]=locked[b]=1;
        }
    }
    // Position is in metres. Constrain normals, mapping, tint and paint alongside
    // shape. Keep LAND and actor borders fixed at all levels; static edges
    // may simplify across attribute discontinuities.
    std::array<float,15> attributes{};
    for(size_t j=3;j<stride;++j)attributes[j-3]=.1f;
    if(stride==18)for(int j=6;j<12;++j)attributes[j-3]=.015f;
    // meshoptimizer normalizes positions before weighting attributes. Convert
    // their weights to metre-equivalents so a large facade and a small prop
    // get the same UV, normal and tint treatment.
    const float extent=meshopt_simplifyScale(vertices,unique,stride*sizeof(float));
    if(!std::isfinite(extent)||extent<=0.f)return levels;
    for(float& weight:attributes)weight/=extent;
    // Count referenced stream vertices, which can differ substantially from
    // index count on hard-edged props. Carry earlier collapses into later tiers.
    const unsigned keepPercent[]{90,60,30};
    const unsigned options=family==Family::Scenery?unsigned(meshopt_SimplifyPermissive):unsigned(meshopt_SimplifyLockBorder);
    std::vector<unsigned char> seen(unique);
    auto current=sourceCorners;
    size_t used=Referenced(current,seen);
    const size_t original=used;
    levels.sourceVertices=uint32_t(used);
    std::vector<uint32_t> candidate;
    for(unsigned tier=0;tier<3;++tier){
        const size_t target=std::max<size_t>(3,(original*uint64_t(keepPercent[tier])+50)/100);
        const size_t before=current.size();
        while(used>target && current.size()>3){
            // The library accepts a triangle budget. Recount actual vertices
            // after each pass; interior edges usually remove two triangles.
            const size_t delta=std::min(current.size()/3-1,(used-target+1)/2);
            auto simplify=[&](size_t remove,std::vector<uint32_t>& out){
                out.resize(current.size());
                const size_t size=meshopt_simplifyWithAttributes(out.data(),current.data(),current.size(),
                vertices,unique,stride*sizeof(float),vertices+3,stride*sizeof(float),attributes.data(),stride-3,
                    locked.data(),current.size()-3*remove,1e15f,options,nullptr);
                out.resize(size);
            };
            simplify(delta,candidate);
            const bool stalled=candidate.size()<3||candidate.size()>=current.size();
            if(stalled){
                if(family!=Family::Scenery)break;
                candidate=current;
                const auto result=CollapseSceneryEdges(vertices,stride,locked,candidate,target,used);
                if(result<3||result>=current.size())break;
            }
            size_t next=Referenced(candidate,seen);
            // A physical collapse can retire several seam vertices at once.
            // Refine an overshooting library request before accepting its mesh.
            if(!stalled&&next+1<target&&delta>1){
                size_t lo=1,hi=delta;
                std::vector<uint32_t> trial;
                while(lo<hi&&next<target){
                    const size_t mid=lo+(hi-lo)/2;
                    simplify(mid,trial);
                    const size_t n=Referenced(trial,seen);
                    if(trial.empty()||n>target)lo=mid+1;
                    else {hi=mid;if(n>next){next=n;candidate.swap(trial);}}
                }
            }
            current.swap(candidate);
            if(next>=used){used=next;break;}
            used=next;
        }
        levels.tierVertices[tier]=uint32_t(used);
        if(current.size()==before){
            if(tier)levels.ranges[tier]=levels.ranges[tier-1];
            continue;
        }
        levels.ranges[tier]={uint32_t(levels.indices.size()),uint32_t(current.size())};
        for(auto i:current)levels.indices.push_back(expanded?first[i]:i);
    }
    return levels;
}
}
