#pragma once
#include "npc/fo3-actor-animation.h"
#include "player/fo3-player-state.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace fo3actorfx {
// The non-player actor emitter visits collision-owning NiNodes in scene order.
// Each node receives one draw from the DEBR weight table at effect start.
template<class Emit>
void VisitAddons(const fo3anim::Skeleton& skeleton,const fo3anim::Pose& pose,
                 const fo3player::CorpsePose* corpse,const fo3anim::Matrix& scene,
                 const fo3pipdata::EffectShader& shader,const std::vector<fo3pipdata::DebrisModel>& models,
                 const fo3player::ShaderPlayback& play,Emit emit){
    uint32_t total=0;
    for(const auto& model:models){
        if(model.percentage>127||model.percentage>UINT32_MAX-total)return;
        total+=model.percentage;
    }
    if(!total||!std::isfinite(shader.addonFadeIn)||!std::isfinite(shader.addonScaleIn)||
       !std::isfinite(shader.addonStartScale))return;
    std::mt19937 rng(play.variantSeed);
    const float age=play.elapsed,startScale=std::max(.01f,shader.addonStartScale);
    const float alpha=shader.addonFadeIn>0?std::clamp(age/shader.addonFadeIn,0.f,1.f):1.f;
    const float fraction=shader.addonScaleIn>0?std::clamp(startScale+age/shader.addonScaleIn,0.f,1.f):1.f;
    const float scale=age==0?startScale:startScale+(1.f-startScale)*fraction;
    for(size_t i=0;i<skeleton.bones.size();++i){
        const auto& bone=skeleton.bones[i];
        if(!bone.collisionOwner)continue;
        uint32_t draw;
        do{draw=rng();}while(draw<(-total)%total);
        draw%=total;
        const fo3pipdata::DebrisModel* selected=nullptr;
        for(const auto& model:models){
            if(draw<model.percentage){selected=&model;break;}
            draw-=model.percentage;
        }
        const fo3anim::Matrix* global=i<pose.global.size()?&pose.global[i]:nullptr;
        if(corpse){
            for(const auto& body:corpse->bones)if(body.name==bone.name){global=&body.global;break;}
        }
        if(!global||!selected||selected->path.empty())continue;
        auto placement=fo3anim::Multiply(scene,*global);
        for(int col=0;col<3;++col)for(int row=0;row<3;++row)placement[col*4+row]*=scale;
        emit(i,*selected,placement,alpha);
    }
}
}
