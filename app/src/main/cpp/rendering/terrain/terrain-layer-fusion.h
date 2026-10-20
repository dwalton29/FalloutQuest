#pragma once
#include "rendering/exact-indexed-mesh.h"
#include <algorithm>
#include <iterator>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace fo3terrain {
struct PaintMaterial {
    std::string texturePath, normalPath;
    bool specularEnabled = false;
};
struct FusionStats { size_t triangles=0, paintTriangles=0, groups=0, fallback=0; };

// Exact topology/attribute admission. Only complete base+paint stacks are
// folded, so remaining legacy paint never overlaps a partially fused stack.
// Lighting is evaluated per original material by the shader, not baked here.
template<class Batches, class TextureReady>
FusionStats FuseLayers(Batches& batches, TextureReady ready) {
    using Key=std::array<uint32_t,9>;
    struct Ref { size_t batch=0, triangle=0; };
    struct Stack { Ref base; bool ambiguous=false; std::vector<Ref> paint; };
    std::unordered_map<Key,Stack,fqmesh::VertexHash<9>> stacks;
    auto count=[](const auto& b){return b.indices.empty()?b.vertices.size()/12u:b.indices.size();};
    auto vertex=[](const auto& b,size_t corner){
        return b.vertices.data()+12u*(b.indices.empty()?corner:b.indices[corner]);
    };
    auto keyFor=[&](const auto& b,size_t triangle){
        std::array<std::array<uint32_t,3>,3> points{};
        for(size_t c=0;c<3;++c)std::memcpy(points[c].data(),vertex(b,triangle*3u+c),12u);
        std::sort(points.begin(),points.end());
        Key key{};
        for(size_t c=0;c<3;++c)std::memcpy(key.data()+c*3u,points[c].data(),12u);
        return key;
    };
    const size_t original=batches.size();
    std::vector<std::vector<uint8_t>> keep(original);
    for(size_t i=0;i<original;++i){
        const auto& b=batches[i];
        if(b.vertexFloats!=12u)continue;
        const size_t corners=count(b);
        if(corners%3u)continue;
        keep[i].assign(corners/3u,1u);
        if(b.alphaLayer)continue;
        for(size_t t=0;t<corners/3u;++t){
            const auto result=stacks.emplace(keyFor(b,t),Stack{Ref{i,t},false,{}});
            if(!result.second)result.first->second.ambiguous=true;
        }
    }
    // Vector traversal is exactly the authored batch and triangle draw order.
    for(size_t i=0;i<original;++i){
        const auto& b=batches[i];
        if(!b.alphaLayer||b.vertexFloats!=12u||!ready(b))continue;
        for(size_t t=0;t<keep[i].size();++t){
            auto stack=stacks.find(keyFor(b,t));
            if(stack!=stacks.end())stack->second.paint.push_back({i,t});
        }
    }
    FusionStats stats;
    // Ordered material tuples group spatially adjacent triangles by CELL and
    // original base batch. No hash iteration order enters triangle ordering.
    std::map<std::vector<size_t>,size_t> groups;
    std::vector<std::vector<size_t>> groupLayers;
    Batches fused;
    for(size_t i=0;i<original;++i){
        const auto& base=batches[i];
        if(base.alphaLayer||base.vertexFloats!=12u)continue;
        for(size_t t=0;t<keep[i].size();++t){
            auto found=stacks.find(keyFor(base,t));
            if(found==stacks.end())continue;
            const Stack& stack=found->second;
            if(stack.paint.empty())continue;
            bool valid=!stack.ambiguous&&stack.paint.size()<=3u;
            for(size_t p=1;p<stack.paint.size();++p)
                if(stack.paint[p-1].batch==stack.paint[p].batch)valid=false;
            for(size_t c=0;c<3u&&valid;++c){
                const float* b=vertex(base,t*3u+c);
                valid=b[8]==1.0f;
                for(const Ref& ref:stack.paint){
                    const float* p=vertex(batches[ref.batch],ref.triangle*3u+c);
                    // UV, normal and VCLR interpolation must also match.
                    for(size_t a=0;a<12u;++a)if(a!=8u&&std::memcmp(b+a,p+a,4u)!=0)valid=false;
                }
            }
            if(!valid){++stats.fallback;continue;}
            std::vector<size_t> tuple{i};
            for(const Ref& ref:stack.paint)tuple.push_back(ref.batch);
            auto group=groups.find(tuple);
            if(group==groups.end()){
                // Sparse paint coverage must not create a draw for every
                // subset of the same materials. Merge compatible ordered
                // tuples, padding missing paint alphas with zero.
                size_t compatible=fused.size();
                for(size_t g=0;g<groupLayers.size();++g){
                    if(groupLayers[g][0]!=i)continue;
                    std::vector<size_t> merged{i};
                    std::set_union(groupLayers[g].begin()+1,groupLayers[g].end(),
                                   tuple.begin()+1,tuple.end(),std::back_inserter(merged));
                    if(merged.size()>4u)continue;
                    auto& output=fused[g];const auto previous=groupLayers[g];
                    for(size_t v=0;v<output.vertices.size();v+=15u){
                        const std::array<float,3> alpha{output.vertices[v+12],output.vertices[v+13],output.vertices[v+14]};
                        std::fill_n(output.vertices.begin()+v+12,3,0.0f);
                        for(size_t p=1;p<previous.size();++p){
                            const auto at=std::find(merged.begin()+1,merged.end(),previous[p]);
                            output.vertices[v+11u+size_t(at-merged.begin())]=alpha[p-1];
                        }
                    }
                    groupLayers[g]=std::move(merged);output.paintMaterials.clear();
                    for(size_t p=1;p<groupLayers[g].size();++p){
                        const auto& material=batches[groupLayers[g][p]];
                        output.paintMaterials.push_back({material.texturePath,material.normalPath,material.specularEnabled});
                    }
                    compatible=g;break;
                }
                if(compatible<fused.size()){
                    groups.emplace(tuple,compatible);group=groups.find(tuple);
                }else{
                    const size_t index=fused.size();groups.emplace(tuple,index);groupLayers.push_back(tuple);
                    typename Batches::value_type batch;
                    batch.gridX=base.gridX;batch.gridY=base.gridY;
                    batch.texturePath=base.texturePath;batch.normalPath=base.normalPath;
                    batch.specularEnabled=base.specularEnabled;
                    batch.alphaLayer=false;batch.layerOrder=-1;batch.vertexFloats=15u;
                    for(const Ref& ref:stack.paint){
                        const auto& p=batches[ref.batch];
                        batch.paintMaterials.push_back({p.texturePath,p.normalPath,p.specularEnabled});
                    }
                    fused.push_back(std::move(batch));group=groups.find(tuple);
                }
            }
            auto& output=fused[group->second];
            for(size_t c=0;c<3u;++c){
                const float* b=vertex(base,t*3u+c);
                output.vertices.insert(output.vertices.end(),b,b+12u);
                for(size_t layer=0;layer<3u;++layer){
                    float alpha=0;
                    if(layer+1u<groupLayers[group->second].size()){
                        const size_t material=groupLayers[group->second][layer+1u];
                        for(const Ref& ref:stack.paint)if(ref.batch==material)
                            alpha=vertex(batches[ref.batch],ref.triangle*3u+c)[8];
                    }
                    output.vertices.push_back(alpha);
                }
            }
            keep[i][t]=0u;
            for(const Ref& ref:stack.paint)keep[ref.batch][ref.triangle]=0u;
            ++stats.triangles;stats.paintTriangles+=stack.paint.size();
        }
    }
    // Material combinations can fragment a cell. Retain the original geometry
    // if consolidation would increase its number of nonempty draw batches.
    size_t before=0,after=fused.size();
    for(size_t i=0;i<original;++i){
        if(!batches[i].vertices.empty())++before;
        if(keep[i].empty())after+=!batches[i].vertices.empty();
        else after+=std::find(keep[i].begin(),keep[i].end(),uint8_t(1))!=keep[i].end();
    }
    if(after>before){stats.fallback+=stats.triangles;stats.triangles=stats.paintTriangles=0;return stats;}
    for(size_t i=0;i<original;++i){
        auto& b=batches[i];if(keep[i].empty())continue;
        const bool indexed=!b.indices.empty();size_t write=0;
        for(size_t t=0;t<keep[i].size();++t){
            if(!keep[i][t])continue;
            const size_t first=t*3u;
            if(indexed)for(size_t c=0;c<3u;++c)b.indices[write+c]=b.indices[first+c];
            else if(write!=first)std::memmove(b.vertices.data()+write*12u,b.vertices.data()+first*12u,36u*sizeof(float));
            write+=3u;
        }
        if(indexed)b.indices.resize(write);else b.vertices.resize(write*12u);
        if(!write){b.vertices.clear();b.indices.clear();}
    }
    stats.groups=fused.size();
    for(auto& b:fused)batches.push_back(std::move(b));
    return stats;
}
} // namespace fo3terrain
