#pragma once
#include "../geometry-detail.h"
#include "fo3-terrain-bounds.h"
#include <map>
#include <numeric>

namespace fo3terrain {
// Overlays use depth from the base LAND. Partition by the exact stack of draws
// covering each triangle and collapse one topology for all materials in it.
// Output indices address each material's original UV/colour/paint stream.
template<class Batch>
void PrepareCellDetail(const std::vector<Batch*>& batches) {
    if(batches.size()==1){
        auto& batch=*batches.front();
        const size_t count=batch.vertices.size()/batch.vertexFloats;
        batch.detailBounds=BuildBounds(batch.vertices.data(),batch.vertices.size(),batch.vertexFloats);
        if(!batch.indices.empty()){
            batch.geometryLevels=fqdetail::Build(fqdetail::Family::Terrain,batch.vertices.data(),
                count,batch.vertexFloats,batch.indices,false);
        }else{
            std::vector<uint32_t> order(count);
            std::iota(order.begin(),order.end(),0u);
            batch.geometryLevels=fqdetail::Build(fqdetail::Family::Terrain,batch.vertices.data(),
                count,batch.vertexFloats,order,false);
        }
        return;
    }
    using Point=std::array<float,3>;
    using Face=std::array<Point,3>;
    struct Copy {size_t batch;std::array<uint32_t,3> vertices;bool reversed;};
    std::map<Face,std::vector<Copy>> faces;
    Bounds bounds;
    for(size_t b=0;b<batches.size();++b){
        auto& batch=*batches[b];
        const size_t stride=batch.vertexFloats;
        const size_t count=batch.indices.empty()?batch.vertices.size()/stride:batch.indices.size();
        for(size_t i=0;i<count;i+=3){
            Copy copy{b,{},false};
            Face points{};
            for(size_t j=0;j<3;++j){
                const auto index=batch.indices.empty()?uint32_t(i+j):batch.indices[i+j];
                const float* v=batch.vertices.data()+index*stride;
                points[j]={v[0],v[1],v[2]};copy.vertices[j]=index;
            }
            // Use a consistent X/Z winding for the shared chart. Restore each
            // original draw's own winding on output.
            const auto a=points[0],c=points[1],d=points[2];
            copy.reversed=(c[0]-a[0])*(d[2]-a[2])<(c[2]-a[2])*(d[0]-a[0]);
            if(copy.reversed){std::swap(points[1],points[2]);std::swap(copy.vertices[1],copy.vertices[2]);}
            const auto pivot=std::min_element(points.begin(),points.end())-points.begin();
            std::rotate(points.begin(),points.begin()+pivot,points.end());
            std::rotate(copy.vertices.begin(),copy.vertices.begin()+pivot,copy.vertices.end());
            faces[points].push_back(copy);
        }
        const Bounds part=BuildBounds(batch.vertices.data(),batch.vertices.size(),stride);
        if(!part.boundsValid)continue;
        if(!bounds.boundsValid)bounds=part;
        else {
            bounds.minX=std::min(bounds.minX,part.minX);bounds.maxX=std::max(bounds.maxX,part.maxX);
            bounds.minY=std::min(bounds.minY,part.minY);bounds.maxY=std::max(bounds.maxY,part.maxY);
            bounds.minZ=std::min(bounds.minZ,part.minZ);bounds.maxZ=std::max(bounds.maxZ,part.maxZ);
        }
    }

    using Stack=std::vector<std::pair<size_t,bool>>;
    std::map<Stack,std::vector<const std::vector<Copy>*>> patches;
    for(auto& entry:faces){
        Stack stack;
        for(const auto& copy:entry.second)stack.emplace_back(copy.batch,copy.reversed);
        patches[stack].push_back(&entry.second);
    }

    using Tiers=std::array<std::vector<uint32_t>,3>;
    std::vector<Tiers> output(batches.size());
    for(const auto& patch:patches){
        const auto& stack=patch.first;
        std::map<std::vector<uint32_t>,uint32_t> ids;
        std::vector<std::vector<uint32_t>> streams;
        std::vector<float> vertices;
        std::vector<uint32_t> corners;
        for(const auto* copies:patch.second)for(size_t k=0;k<3;++k){
            std::vector<uint32_t> key;
            for(const auto& copy:*copies)key.push_back(copy.vertices[k]);
            auto insert=ids.emplace(key,uint32_t(ids.size()));
            corners.push_back(insert.first->second);
            if(!insert.second)continue;
            const auto& first=*batches[stack[0].first];
            const float* src=first.vertices.data()+key[0]*first.vertexFloats;
            vertices.insert(vertices.end(),src,src+12);
            for(size_t layer=0;layer<3;++layer){
                float weight=first.vertexFloats==15?src[12+layer]:0;
                if(layer+1<stack.size()){
                    const auto& paint=*batches[stack[layer+1].first];
                    weight=paint.vertices[key[layer+1]*paint.vertexFloats+8];
                }
                vertices.push_back(weight);
            }
            streams.push_back(std::move(key));
        }
        const auto levels=fqdetail::Build(fqdetail::Family::Terrain,vertices.data(),streams.size(),15,corners,false);
        for(size_t tier=0;tier<3;++tier){
            const auto range=levels.ranges[tier];
            const auto* indices=range.count?levels.indices.data()+range.first:corners.data();
            const size_t count=range.count?range.count:corners.size();
            for(size_t layer=0;layer<stack.size();++layer){
                auto& out=output[stack[layer].first][tier];
                const bool reverse=stack[layer].second;
                for(size_t i=0;i<count;i+=3){
                    out.push_back(streams[indices[i]][layer]);
                    out.push_back(streams[indices[i+(reverse?2:1)]][layer]);
                    out.push_back(streams[indices[i+(reverse?1:2)]][layer]);
                }
            }
        }
    }
    for(size_t b=0;b<batches.size();++b){
        auto& batch=*batches[b];batch.detailBounds=bounds;
        auto& levels=batch.geometryLevels;levels.family=fqdetail::Family::Terrain;
        const size_t original=batch.indices.empty()?batch.vertices.size()/batch.vertexFloats:batch.indices.size();
        std::vector<unsigned char> seen(batch.vertices.size()/batch.vertexFloats);
        auto referenced=[&](const std::vector<uint32_t>& indices){
            std::fill(seen.begin(),seen.end(),0);
            uint32_t used=0;
            for(auto id:indices)if(!seen[id]){seen[id]=1;++used;}
            return used;
        };
        levels.sourceVertices=batch.indices.empty()?uint32_t(original):referenced(batch.indices);
        for(size_t tier=0;tier<3;++tier){
            const auto& out=output[b][tier];
            levels.tierVertices[tier]=out.empty()?levels.sourceVertices:referenced(out);
            if(out.empty()||out.size()>=original)continue;
            if(tier&&out==output[b][tier-1]){levels.ranges[tier]=levels.ranges[tier-1];continue;}
            levels.ranges[tier]={uint32_t(levels.indices.size()),uint32_t(out.size())};
            levels.indices.insert(levels.indices.end(),out.begin(),out.end());
        }
    }
}
}
