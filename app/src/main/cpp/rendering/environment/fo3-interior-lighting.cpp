#include "fo3-interior-lighting.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace fo3interior {
namespace {
bool Is(const char* a,const char* b) { return std::memcmp(a,b,4)==0; }
void Colour(const uint8_t* b,std::array<float,3>& c) {
    for(size_t i=0;i<3;++i)c[i]=static_cast<float>(b[i])/255.0f;
}
bool Lighting(const uint8_t* b,uint32_t n,Cell& c) {
    // Original FO3 XCLL and LGTM DATA are 40 bytes (not TES5's layout).
    if(n!=40)return false;
    Colour(b,c.ambient);Colour(b+4,c.directional);Colour(b+8,c.fog);
    c.fogNear=fo3esm::ReadF32(b+12);c.fogFar=fo3esm::ReadF32(b+16);
    c.rotationXY=static_cast<int32_t>(fo3esm::ReadU32(b+20));
    c.rotationZ=static_cast<int32_t>(fo3esm::ReadU32(b+24));
    c.directionalFade=fo3esm::ReadF32(b+28);c.fogClip=fo3esm::ReadF32(b+32);
    c.fogPower=fo3esm::ReadF32(b+36);
    return std::isfinite(c.fogNear)&&std::isfinite(c.fogFar)&&
        std::isfinite(c.directionalFade)&&std::isfinite(c.fogClip)&&std::isfinite(c.fogPower);
}
}
Cell ParseCell(uint32_t id,const std::vector<uint8_t>& payload) {
    Cell c;c.formId=id;c.found=true;
    fo3esm::WalkSubrecords(payload,[&](const char* t,const uint8_t* b,uint32_t n){
        if(Is(t,"EDID"))c.editorId=fo3esm::ZString(b,n);
        else if(Is(t,"DATA")&&n)c.flags=b[0];
        else if(Is(t,"XCLL"))c.authored=Lighting(b,n,c);
        else if(Is(t,"XCIM")&&n==4)c.imageSpace=fo3esm::ReadU32(b);
        else if(Is(t,"LTMP")&&n==4)c.lightingTemplate=fo3esm::ReadU32(b);
        else if(Is(t,"LNAM")&&n==4)c.inherit=fo3esm::ReadU32(b);
    });return c;
}
Reference ParseReference(uint32_t id,uint32_t flags,const std::vector<uint8_t>& payload) {
    Reference r;r.formId=id;r.flags=flags;
    fo3esm::WalkSubrecords(payload,[&](const char* t,const uint8_t* b,uint32_t n){
        if(Is(t,"NAME")&&n==4)r.base=fo3esm::ReadU32(b);
        else if(Is(t,"DATA")&&n==24) {
            r.transform=true;
            for(size_t i=0;i<3;++i){r.position[i]=fo3esm::ReadF32(b+4*i);r.rotation[i]=fo3esm::ReadF32(b+12+4*i);
                r.transform=r.transform&&std::isfinite(r.position[i])&&std::isfinite(r.rotation[i]);}
        }else if(Is(t,"XESP")&&n>=5){r.parent=fo3esm::ReadU32(b);r.parentFlags=b[4];}
    });return r;
}
Light ParseLight(uint32_t id,const std::vector<uint8_t>& payload) {
    Light l;l.base=id;
    fo3esm::WalkSubrecords(payload,[&](const char* t,const uint8_t* b,uint32_t n){
        if(Is(t,"EDID"))l.editorId=fo3esm::ZString(b,n);
        else if(Is(t,"DATA")&&n==32){l.radius=static_cast<float>(fo3esm::ReadU32(b+4));Colour(b+8,l.colour);
            l.flags=fo3esm::ReadU32(b+12);l.falloff=fo3esm::ReadF32(b+16);l.fov=fo3esm::ReadF32(b+20);}
        else if(Is(t,"FNAM")&&n==4)l.fade=fo3esm::ReadF32(b);
    });return l;
}
std::array<float,3> ToScene(const std::array<float,3>& p,const std::array<float,3>& o) {
    return {(p[0]-o[0])/70.0f,-1.55f+(p[2]-o[2])/70.0f,-(p[1]-o[1])/70.0f};
}
float Attenuation(float d,float r) {
    if(r<=0)return 0;
    return std::clamp(1.0f-(d*d)/(r*r),0.0f,1.0f);
}
bool Load(const std::string& path,uint32_t cell,const std::array<float,3>& origin,Snapshot& out) {
    out={};out.cell.formId=cell;
    struct Close { void operator()(FILE* f) const { std::fclose(f); } };
    std::unique_ptr<FILE,Close> file(std::fopen(path.c_str(),"rb"));
    if(!file)return false;
    const auto size=fo3esm::FileSize(file.get());if(size<24)return false;
    struct Group { uint64_t end;uint32_t label,type; };
    std::vector<Group> groups;
    std::unordered_map<uint32_t,fo3esm::RecordLocation> index;
    std::vector<uint32_t> children;
    while(true) {
        const auto offset=ftello(file.get());if(offset<0||offset+24>size)break;
        while(!groups.empty()&&static_cast<uint64_t>(offset)>=groups.back().end)groups.pop_back();
        uint8_t h[24];if(!fo3esm::ReadExact(file.get(),h,24))return false;
        uint32_t n=fo3esm::ReadU32(h+4),flags=fo3esm::ReadU32(h+8),id=fo3esm::ReadU32(h+12);
        if(Is(reinterpret_cast<char*>(h),"GRUP")) {
            if(n<24||offset+n>size)return false;
            groups.push_back({static_cast<uint64_t>(offset)+n,flags,id});continue;
        }
        const auto end=offset+24+n;if(end>size)return false;
        std::string type(reinterpret_cast<char*>(h),4);
        if(type=="CELL"||type=="LGTM"||type=="IMGS"||type=="LIGH"||type=="REFR"||type=="ACHR")
            index[id]={static_cast<uint64_t>(offset)+24,n,flags,type};
        if(type=="REFR")for(auto it=groups.rbegin();it!=groups.rend();++it) {
            if(it->type==6){if(it->label==cell)children.push_back(id);break;}
        }
        if(fseeko(file.get(),end,SEEK_SET)!=0)return false;
    }
    auto read=[&](uint32_t id,const char* type,std::vector<uint8_t>& b){
        auto it=index.find(id);return it!=index.end()&&it->second.type==type&&fo3esm::ReadPayload(file.get(),it->second,b);
    };
    std::vector<uint8_t> payload;
    if(!read(cell,"CELL",payload))return false;
    out.cell=ParseCell(cell,payload);
    if((out.cell.flags&1)==0)return false;
    if(out.cell.lightingTemplate && out.cell.inherit) {
        Cell t;bool have=false;
        if(read(out.cell.lightingTemplate,"LGTM",payload))
            fo3esm::WalkSubrecords(payload,[&](const char* tag,const uint8_t* b,uint32_t n){
                if(Is(tag,"DATA"))have=Lighting(b,n,t);
            });
        if(have) {
            const uint32_t mask=out.cell.inherit;
            if((mask&0x1ffu)==0x1ffu)out.cell.authored=true;
            if(mask&1)out.cell.ambient=t.ambient;
            if(mask&2)out.cell.directional=t.directional;
            if(mask&4)out.cell.fog=t.fog;
            if(mask&8)out.cell.fogNear=t.fogNear;
            if(mask&16)out.cell.fogFar=t.fogFar;
            if(mask&32){out.cell.rotationXY=t.rotationXY;out.cell.rotationZ=t.rotationZ;}
            if(mask&64)out.cell.directionalFade=t.directionalFade;
            if(mask&128)out.cell.fogClip=t.fogClip;
            if(mask&256)out.cell.fogPower=t.fogPower;
        } else ++out.unsupported;
    }
    if(out.cell.flags&0x80)++out.unsupported; // Behave Like Exterior: explicit diagnostic.
    if(out.cell.imageSpace)read(out.cell.imageSpace,"IMGS",out.imagePayload);
    std::unordered_map<uint32_t,Reference> references;
    std::unordered_map<uint32_t,bool> memo;
    std::unordered_set<uint32_t> visiting, unknown;
    std::function<bool(uint32_t)> enabled=[&](uint32_t id) {
        auto m=memo.find(id);if(m!=memo.end())return m->second;
        if(!visiting.insert(id).second){unknown.insert(id);++out.unresolvedParents;return false;}
        auto it=references.find(id);
        if(it==references.end()) {
            auto ix=index.find(id);std::vector<uint8_t> b;
            if(ix==index.end()||!fo3esm::ReadPayload(file.get(),ix->second,b)){
                visiting.erase(id);unknown.insert(id);++out.unresolvedParents;return false;
            }
            it=references.emplace(id,ParseReference(id,ix->second.flags,b)).first;
        }
        Reference r=it->second;
        bool e=(r.flags&(0x800u|0x20u))==0;
        if(r.parent){e=enabled(r.parent);if(unknown.count(r.parent)){unknown.insert(id);e=false;}else if(r.parentFlags&1)e=!e;}
        visiting.erase(id);memo[id]=e;return e;
    };
    std::unordered_map<uint32_t,Light> bases;
    for(uint32_t id:children) {
        if(!read(id,"REFR",payload))return false;
        Reference r=ParseReference(id,index.at(id).flags,payload);references[id]=r;
        auto base=bases.find(r.base);
        if(base==bases.end()) {
            if(!read(r.base,"LIGH",payload))continue;
            base=bases.emplace(r.base,ParseLight(r.base,payload)).first;
        }
        ++out.total;
        if(!enabled(id)||(base->second.flags&0x20u)||!r.transform){++out.disabled;continue;}
        Light l=base->second;l.ref=id;l.position=ToScene(r.position,origin);l.rotation=r.rotation;
        if(!std::isfinite(l.fade)||l.fade<0||l.radius<=0){++out.unsupported;continue;}
        l.radius/=70.0f;
        const float sign=(l.flags&4u)?-1.0f:1.0f;
        for(float& c:l.colour)c*=sign*l.fade; // D3D light constants use normalized editor bytes.
        // Spotlight, flicker/pulse motion, carryable/time-limited behaviour remain explicit.
        if(l.flags&(0x200u|0x400u|0x8u|0x40u|0x80u|0x100u|0x2u))++out.unsupported;
        out.lights.push_back(std::move(l));
    }
    out.referencesRead=true;return true;
}
Selection Select(const std::vector<Light>& lights,const std::array<float,3>& mn,const std::array<float,3>& mx) {
    Selection out;std::array<float,Budget> scores{};
    for(size_t i=0;i<lights.size();++i) {
        const auto& l=lights[i];
        if(l.flags&0x200u)continue; // Spot sources retained, not silently turned into point lights.
        float d2=0;
        for(size_t c=0;c<3;++c){float d=l.position[c]-std::clamp(l.position[c],mn[c],mx[c]);d2+=d*d;}
        if(l.radius<=0||d2>=l.radius*l.radius)continue;
        float strength=0;for(float c:l.colour)strength=std::max(strength,std::abs(c));
        float score=strength*(1-d2/(l.radius*l.radius));int at=0;
        while(at<out.count&&scores[at]>=score)++at;
        if(at==static_cast<int>(Budget))continue;
        int last=std::min(out.count,static_cast<int>(Budget)-1);
        for(int j=last;j>at;--j){scores[j]=scores[j-1];out.indices[j]=out.indices[j-1];}
        scores[at]=score;out.indices[at]=i;out.count=std::min(out.count+1,static_cast<int>(Budget));
    }return out;
}
}
