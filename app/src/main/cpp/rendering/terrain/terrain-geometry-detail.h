#pragma once
#include "../geometry-detail.h"
#include "fo3-terrain-bounds.h"
#include "terrain-shared-edge-detail.h"

namespace fo3terrain {
// Each material retains its original attribute stream. Select a single
// topology per cell and contract shared vertices across all overlapping draws.
template<class Batch>
void PrepareCellDetail(const std::vector<Batch*>& batches) {
    using Tiers=std::array<std::vector<uint32_t>,3>;
    std::vector<Tiers> output(batches.size());
    Bounds bounds;
    size_t source=0;
    for(size_t b=0;b<batches.size();++b){
        auto& batch=*batches[b];
        const auto n=batch.vertices.size()/batch.vertexFloats;
        auto& original=output[b][0];
        if(batch.indices.empty()){
            original.resize(n);
            std::iota(original.begin(),original.end(),0u);
        }else original=batch.indices;
        std::vector<unsigned char> seen(n);
        auto& levels=batch.geometryLevels;levels={};levels.family=fqdetail::Family::Terrain;
        for(auto index:original)if(!seen[index]){seen[index]=1;++levels.sourceVertices;}
        source+=levels.sourceVertices;
        const Bounds part=BuildBounds(batch.vertices.data(),batch.vertices.size(),batch.vertexFloats);
        if(!part.boundsValid)continue;
        if(!bounds.boundsValid)bounds=part;
        else{
            bounds.minX=std::min(bounds.minX,part.minX);bounds.maxX=std::max(bounds.maxX,part.maxX);
            bounds.minY=std::min(bounds.minY,part.minY);bounds.maxY=std::max(bounds.maxY,part.maxY);
            bounds.minZ=std::min(bounds.minZ,part.minZ);bounds.maxZ=std::max(bounds.maxZ,part.maxZ);
        }
    }
    constexpr unsigned keep[]{90,60,30};
    for(unsigned tier=0;tier<3;++tier){
        if(tier)for(auto& batch:output)batch[tier]=batch[tier-1];
        ReduceSharedEdges(batches,output,tier,(source*keep[tier]+50)/100);
    }
    for(size_t b=0;b<batches.size();++b){
        auto& batch=*batches[b];batch.detailBounds=bounds;
        auto& levels=batch.geometryLevels;
        std::vector<unsigned char> seen(batch.vertices.size()/batch.vertexFloats);
        for(unsigned tier=0;tier<3;++tier){
            const auto& idx=output[b][tier];
            std::fill(seen.begin(),seen.end(),0);
            for(auto v:idx)if(!seen[v]){seen[v]=1;++levels.tierVertices[tier];}
            if(tier&&idx==output[b][tier-1]){levels.ranges[tier]=levels.ranges[tier-1];continue;}
            // A shared edge may move a vertex without changing triangle count
            // in this draw. Publish the new indices whenever topology changes.
            if(!tier){
                bool same=idx==batch.indices;
                if(batch.indices.empty()&&idx.size()==seen.size()){
                    same=true;
                    for(size_t i=0;i<idx.size();++i)if(idx[i]!=i){same=false;break;}
                }
                if(same)continue;
            }
            levels.ranges[tier]={uint32_t(levels.indices.size()),uint32_t(idx.size())};
            levels.indices.insert(levels.indices.end(),idx.begin(),idx.end());
        }
    }
}
}
