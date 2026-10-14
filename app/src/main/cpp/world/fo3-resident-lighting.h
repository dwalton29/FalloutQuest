#pragma once
#include "rendering/environment/fo3-interior-lighting.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <unordered_map>

namespace fo3scene {
inline bool SameResidentLight(const fo3interior::Light& a,const fo3interior::Light& b) {
    return a.ref==b.ref&&a.base==b.base&&a.flags==b.flags&&a.editorId==b.editorId&&
        a.position==b.position&&a.rotation==b.rotation&&a.colour==b.colour&&
        a.radius==b.radius&&a.fade==b.fade&&a.falloff==b.falloff&&a.fov==b.fov;
}
inline bool SameResidentCellLighting(const fo3interior::Cell& a,const fo3interior::Cell& b) {
    return a.formId==b.formId&&a.imageSpace==b.imageSpace&&a.lightingTemplate==b.lightingTemplate&&
        a.inherit==b.inherit&&a.flags==b.flags&&a.found==b.found&&a.authored==b.authored&&
        a.editorId==b.editorId&&a.ambient==b.ambient&&a.directional==b.directional&&a.fog==b.fog&&
        a.fogNear==b.fogNear&&a.fogFar==b.fogFar&&a.directionalFade==b.directionalFade&&
        a.fogClip==b.fogClip&&a.fogPower==b.fogPower&&a.rotationXY==b.rotationXY&&a.rotationZ==b.rotationZ;
}
inline bool SameResidentLighting(const fo3interior::Snapshot& a,const fo3interior::Snapshot& b) {
    if(!SameResidentCellLighting(a.cell,b.cell)||a.imagePayload!=b.imagePayload||a.total!=b.total||
        a.disabled!=b.disabled||a.unsupported!=b.unsupported||a.unresolvedParents!=b.unresolvedParents||
        a.referencesRead!=b.referencesRead||a.lights.size()!=b.lights.size())return false;
    for(size_t i=0;i<a.lights.size();++i)if(!SameResidentLight(a.lights[i],b.lights[i]))return false;
    return true;
}
inline bool ValidResidentInteriorLighting(const fo3interior::Snapshot& s) {
    const auto& c=s.cell;
    if(!c.formId||!c.found||!(c.flags&1)||(c.flags&0x80)||!s.referencesRead||
        s.unsupported||s.unresolvedParents)return false;
    for(const auto* colour:{&c.ambient,&c.directional,&c.fog})for(float v:*colour)if(!std::isfinite(v))return false;
    for(float v:{c.fogNear,c.fogFar,c.directionalFade,c.fogClip,c.fogPower})if(!std::isfinite(v))return false;
    for(const auto& l:s.lights) {
        if(!l.ref||!l.base||l.radius<=0||l.fade<0||(l.flags&(0x200u|0x400u|0x8u|0x40u|0x80u|0x100u|0x2u)))return false;
        for(const auto* values:{&l.position,&l.rotation,&l.colour})for(float v:*values)if(!std::isfinite(v))return false;
        for(float v:{l.radius,l.fade,l.falloff,l.fov})if(!std::isfinite(v))return false;
    }
    return true;
}
struct ResidentInteriorLighting {fo3interior::Snapshot before,candidate;};
// Root-enable changes may only add/remove original point lights in the same
// authored environment. Full scene/template/image-space changes are separate.
inline bool ValidateResidentLightDelta(const fo3interior::Snapshot& before,
        const fo3interior::Snapshot& after,const std::vector<uint32_t>& affected,std::string& error) {
    error.clear();
    if(!ValidResidentInteriorLighting(before)||!ValidResidentInteriorLighting(after)||
        !SameResidentCellLighting(before.cell,after.cell)||before.imagePayload!=after.imagePayload||
        before.total!=after.total){error="unsupported or changed interior lighting environment";return false;}
    for(size_t i=0;i<affected.size();++i)if(!affected[i]||(i&&affected[i]<=affected[i-1])){
        error="invalid affected light reference list";return false;}
    std::unordered_map<uint32_t,const fo3interior::Light*> old,next;
    for(const auto& l:before.lights)if(!old.emplace(l.ref,&l).second){error="duplicate published light";return false;}
    for(const auto& l:after.lights)if(!next.emplace(l.ref,&l).second){error="duplicate candidate light";return false;}
    for(const auto& l:old) {
        const auto n=next.find(l.first);
        if(n!=next.end()) {
            if(!SameResidentLight(*l.second,*n->second)){error="authored light metadata changed";return false;}
        }else if(!std::binary_search(affected.begin(),affected.end(),l.first)){
            error="unlisted removed light";return false;}
    }
    for(const auto& l:next)if(!old.count(l.first)&&!std::binary_search(affected.begin(),affected.end(),l.first)){
        error="unlisted incoming light";return false;}
    return true;
}
inline bool PrepareResidentInteriorLighting(const std::string& path,uint32_t cell,
        const std::array<float,3>& origin,const fo3world::ReferenceStateSnapshot& published,
        const fo3world::ReferenceStateSnapshot& candidate,const std::vector<uint32_t>& affected,
        ResidentInteriorLighting& out,const std::atomic<bool>& cancelled,std::string& error) {
    error.clear();
    for(float v:origin)if(!std::isfinite(v)){error="invalid light origin";return false;}
    if(cancelled.load(std::memory_order_acquire)){error="light preparation cancelled";return false;}
    ResidentInteriorLighting next;
    if(!fo3interior::Load(path,cell,origin,next.before,&published,true)||
        cancelled.load(std::memory_order_acquire)||
        !fo3interior::Load(path,cell,origin,next.candidate,&candidate,true)){
        error="interior light decode failed or cancelled";return false;}
    if(!ValidateResidentLightDelta(next.before,next.candidate,affected,error))return false;
    if(cancelled.load(std::memory_order_acquire)){error="light preparation cancelled";return false;}
    out=std::move(next);return true;
}
} // namespace fo3scene
