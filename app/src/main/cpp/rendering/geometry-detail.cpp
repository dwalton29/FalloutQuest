#include "geometry-detail.h"
#include "../../../../../third_party/meshoptimizer/meshoptimizer.h"
#include <limits>

namespace fqdetail {
Levels Build(Family family,const float* vertices,size_t count,size_t stride,
             const std::vector<uint32_t>& sourceCorners,bool expanded,
             const std::vector<uint16_t>* bones,const std::vector<float>* weights) {
    Levels levels;
    levels.family=family;
    const size_t corners=sourceCorners.size();
    if(!vertices||corners<192||corners%3||!count||count>UINT32_MAX||
       corners>UINT32_MAX/3||(expanded&&count!=corners)||
       (family==Family::Terrain?(stride!=12&&stride!=15):stride!=18))return levels;
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
    std::vector<unsigned char> locked;
    if(bones||weights){
        if(!bones||!weights||bones->size()<unique*4||weights->size()<unique*4)return levels;
        // Preserve deformation boundaries. Only edges inside regions with matching
        // bone influences are free to collapse; the surviving vertices keep their weights.
        locked.resize(unique);
        for(size_t i=0;i<corners;i+=3)for(int k=0;k<3;++k){
            const unsigned a=sourceCorners[i+k],b=sourceCorners[i+(k+1)%3];
            for(int s=0;s<4;++s){
                const float wa=(*weights)[4*a+s],wb=(*weights)[4*b+s];
                if((wa!=0||wb!=0)&&((*bones)[4*a+s]!=(*bones)[4*b+s]||std::abs(wa-wb)>.015f)){
                    locked[a]=locked[b]=1;break;
                }
            }
        }
    }
    // Position is in metres. Constrain normals, mapping, tint and paint alongside
    // shape. Keep LAND and actor borders fixed at all levels; static edges
    // may simplify within the tier error tolerance.
    std::array<float,15> attributes{};
    for(size_t j=3;j<stride;++j)attributes[j-3]=.1f;
    if(stride==18)for(int j=6;j<12;++j)attributes[j-3]=.015f;
    // meshoptimizer normalizes positions before weighting attributes. Convert
    // their weights to metre-equivalents so a large facade and a small prop
    // get the same UV, normal and tint treatment.
    const float extent=meshopt_simplifyScale(vertices,unique,stride*sizeof(float));
    if(!std::isfinite(extent)||extent<=0.f)return levels;
    for(float& weight:attributes)weight/=extent;
    const float ratios[]{.90f,.55f,.25f},errors[]{.001f,.018f,.09f};
    std::vector<uint32_t> simplified(corners);
    size_t previous=corners;
    for(unsigned tier=0;tier<3;++tier){
        const size_t wanted=std::max<size_t>(3,3*size_t((corners/3)*ratios[tier]));
        const size_t size=meshopt_simplifyWithAttributes(simplified.data(),sourceCorners.data(),corners,
            vertices,unique,stride*sizeof(float),vertices+3,stride*sizeof(float),attributes.data(),stride-3,
            locked.empty()?nullptr:locked.data(),wanted,errors[tier],
            ((family==Family::Scenery)?0:meshopt_SimplifyLockBorder)|meshopt_SimplifyErrorAbsolute,nullptr);
        if(size<3||size+previous/32>=previous)continue;
        levels.ranges[tier]={uint32_t(levels.indices.size()),uint32_t(size)};
        for(size_t j=0;j<size;++j)levels.indices.push_back(expanded?first[simplified[j]]:simplified[j]);
        previous=size;
    }
    return levels;
}
}
