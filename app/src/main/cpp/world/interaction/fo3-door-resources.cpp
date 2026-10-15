#include "fo3-door-resources.h"
#include "data/fo3-esm-reader.h"
#include "world/fo3-resource-exchange.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <unordered_set>

namespace {
using Plan=fo3scene::ResourceExchange<Fo3DoorResources>;
fo3scene::VisualPublication& Publication(){static fo3scene::VisualPublication value;return value;}
std::unique_ptr<Plan>& Pending(){static std::unique_ptr<Plan> value;return value;}
bool Is(const char* a,const char* b){return std::memcmp(a,b,4)==0;}
bool SameTeleport(const Fo3DoorTeleport& a,const Fo3DoorTeleport& b){
    return a.valid==b.valid&&a.destinationDoorRefFormId==b.destinationDoorRefFormId&&
        a.destinationCellFormId==b.destinationCellFormId&&a.flags==b.flags&&a.x==b.x&&a.y==b.y&&
        a.z==b.z&&a.rx==b.rx&&a.ry==b.ry&&a.rz==b.rz;
}
bool SameAnchor(const Fo3DoorAnchor& a,const Fo3DoorAnchor& b){return a.sourceDoorRef==b.sourceDoorRef&&
    a.sourceX==b.sourceX&&a.sourceY==b.sourceY&&a.sourceZ==b.sourceZ&&SameTeleport(a.teleport,b.teleport);}
struct Record {fo3esm::RecordLocation location;uint32_t cell=0,world=0;};
struct Ref {uint32_t base=0,parent=0;uint8_t parentFlags=0;bool position=false,xtel=false;
    std::array<float,3> point{};Fo3DoorTeleport teleport;};
struct Scope {Fo3DoorCellCache cache;std::vector<uint32_t> refs;
    std::unordered_map<uint32_t,Fo3DoorPromptMetadata> metadata;
    std::unordered_map<uint32_t,std::string> prompts;};
// Complete framing is checked independently of the permissive legacy walker.
bool Complete(const std::vector<uint8_t>& b){
    size_t p=0;uint32_t extended=0;
    while(p<b.size()){
        if(b.size()-p<6)return false;
        const auto* tag=b.data()+p;const auto n=fo3esm::ReadU16(tag+4);p+=6;
        if(Is(reinterpret_cast<const char*>(tag),"XXXX")){
            if(extended||n!=4||b.size()-p<4)return false;
            extended=fo3esm::ReadU32(b.data()+p);p+=4;if(!extended)return false;continue;
        }
        const uint32_t count=extended?extended:n;extended=0;
        if(count>b.size()-p)return false;
        p+=count;
    }
    return !extended;
}
class Reader {
public:
    bool Open(const std::string& path,const std::atomic<bool>& cancel){
        file.reset(std::fopen(path.c_str(),"rb"));if(!file)return false;
        const auto size=fo3esm::FileSize(file.get());if(size<24)return false;
        struct Frame{uint64_t end;uint32_t label,type;};std::vector<Frame> groups;
        while(true){
            if(cancel.load(std::memory_order_acquire))return false;
            const auto p=ftello(file.get());if(p<0)return false;if(p==size)return true;
            if(p+24>size)return false;
            while(!groups.empty()&&uint64_t(p)>=groups.back().end)groups.pop_back();
            uint8_t h[24];if(!fo3esm::ReadExact(file.get(),h,24))return false;
            const uint32_t n=fo3esm::ReadU32(h+4),flags=fo3esm::ReadU32(h+8),id=fo3esm::ReadU32(h+12);
            if(Is(reinterpret_cast<char*>(h),"GRUP")){
                if(n<24||uint64_t(p)+n>uint64_t(size)||(!groups.empty()&&uint64_t(p)+n>groups.back().end))return false;
                groups.push_back({uint64_t(p)+n,flags,id});continue;
            }
            const uint64_t end=uint64_t(p)+24+n;
            if(end>uint64_t(size)||(!groups.empty()&&end>groups.back().end))return false;
            uint32_t cell=0,world=0;
            for(auto g=groups.rbegin();g!=groups.rend();++g){
                if(!cell&&(g->type==6||g->type==8||g->type==9||g->type==10))cell=g->label;
                if(!world&&g->type==1)world=g->label;
            }
            const std::string type(reinterpret_cast<char*>(h),4);
            if(type=="REFR"||type=="ACHR"||type=="CELL"||type=="WRLD"||type=="DOOR"){
                if(!records.emplace(id,Record{{uint64_t(p)+24,n,flags,type},cell,world}).second)return false;
            }
            if(fseeko(file.get(),end,SEEK_SET))return false;
        }
    }
    bool Payload(uint32_t id,std::vector<uint8_t>& out){auto r=records.find(id);return r!=records.end()&&
        fo3esm::ReadPayload(file.get(),r->second.location,out)&&Complete(out);}
    bool Reference(uint32_t id,Ref& out){
        const auto old=refs.find(id);if(old!=refs.end()){out=old->second;return true;}
        const auto r=records.find(id);
        if(r==records.end()||(r->second.location.type!="REFR"&&r->second.location.type!="ACHR"))return false;
        std::vector<uint8_t> b;if(!Payload(id,b))return false;
        Ref next;bool valid=true;
        fo3esm::WalkSubrecords(b,[&](const char* tag,const uint8_t* p,uint32_t n){
            if(Is(tag,"NAME")){if(n!=4){valid=false;return;}next.base=fo3esm::ReadU32(p);}
            if(Is(tag,"XESP")){if(n!=8){valid=false;return;}next.parent=fo3esm::ReadU32(p);next.parentFlags=p[4];if(!next.parent||(p[4]&~3u))valid=false;}
            if(Is(tag,"DATA")){if(n!=24){valid=false;return;}next.position=true;
                for(int i=0;i<6;++i){const float f=fo3esm::ReadF32(p+i*4);if(!std::isfinite(f))valid=false;if(i<3)next.point[i]=f;}}
            if(Is(tag,"XTEL")){if(next.xtel||(n!=28&&n!=32)){valid=false;return;}next.xtel=true;
                next.teleport.destinationDoorRefFormId=fo3esm::ReadU32(p);
                float* fields[]{&next.teleport.x,&next.teleport.y,&next.teleport.z,&next.teleport.rx,&next.teleport.ry,&next.teleport.rz};
                for(int i=0;i<6;++i){*fields[i]=fo3esm::ReadF32(p+4+i*4);if(!std::isfinite(*fields[i]))valid=false;}
                if(n==32)next.teleport.flags=fo3esm::ReadU32(p+28);
                if(!next.teleport.destinationDoorRefFormId)valid=false;
            }
        });
        if(!valid||!next.base)return false;
        refs.emplace(id,next);out=next;return true;
    }
    bool Enabled(uint32_t id,bool& value,std::unordered_set<uint32_t>& visiting){
        auto m=memo.find(id);if(m!=memo.end()){value=m->second;return true;}
        if(visiting.size()>=128||!visiting.insert(id).second)return false;
        Ref ref;const auto rec=records.find(id);
        if(rec==records.end()||(rec->second.location.flags&0x20)||!Reference(id,ref))return false;
        if(ref.parent){if(!Enabled(ref.parent,value,visiting))return false;if(ref.parentFlags&1)value=!value;}
        else value=!(rec->second.location.flags&0x800);
        visiting.erase(id);memo[id]=value;return true;
    }
    bool Name(uint32_t id,std::string& out){
        std::vector<uint8_t> b;if(!Payload(id,b))return false;out.clear();
        fo3esm::WalkSubrecords(b,[&](const char* tag,const uint8_t* p,uint32_t n){if(Is(tag,"FULL"))out=fo3esm::ZString(p,n);});return true;
    }
    bool Build(uint32_t cell,const fo3world::ReferenceStateSnapshot& state,Scope& out,const std::atomic<bool>& cancel){
        auto source=records.find(cell);if(source==records.end()||source->second.location.type!="CELL")return false;
        out.cache.attempted=true;
        std::vector<uint32_t> children;
        for(const auto& entry:records)if(entry.second.cell==cell&&entry.second.location.type=="REFR")children.push_back(entry.first);
        std::sort(children.begin(),children.end());
        for(uint32_t id:children){
            if(cancel.load(std::memory_order_acquire))return false;
            Ref ref;if(!Reference(id,ref))return false;
            const auto base=records.find(ref.base);
            // Non-door bases are deliberately not indexed. Only sources with
            // authored XTEL must prove their original DOOR base and destination.
            if(base==records.end()||base->second.location.type!="DOOR") {if(ref.xtel)return false;continue;}
            out.refs.push_back(id);
            if(records.at(id).location.flags&0x20)continue;
            bool authored=false;std::unordered_set<uint32_t> visiting;
            if(!Enabled(id,authored,visiting))return false;
            const auto projected=state.states.find(id);
            if(projected!=state.states.end()&&projected->second==fo3world::ReferenceEnableState::Unavailable)return false;
            if(!state.Enabled(id,authored)||!ref.xtel)continue;
            if(!ref.position||(base->second.location.flags&0x20))return false;
            Ref destination;if(!Reference(ref.teleport.destinationDoorRefFormId,destination))return false;
            const auto target=records.find(ref.teleport.destinationDoorRefFormId);
            const auto targetBase=records.find(destination.base);
            if(target==records.end()||!target->second.cell||(target->second.location.flags&0x20)||
                targetBase==records.end()||targetBase->second.location.type!="DOOR"||
                (targetBase->second.location.flags&0x20))return false;
            const auto targetCell=records.find(target->second.cell);
            if(targetCell==records.end()||targetCell->second.location.type!="CELL"||
                (targetCell->second.location.flags&0x20))return false;
            auto teleport=ref.teleport;teleport.destinationCellFormId=target->second.cell;teleport.valid=true;
            std::string door,dest;if(!Name(ref.base,door)||!Name(teleport.destinationCellFormId,dest))return false;
            if(dest.empty()){auto c=records.find(teleport.destinationCellFormId);
                if(c!=records.end()&&c->second.world&&!Name(c->second.world,dest))return false;}
            std::string prompt;if(!FormatFo3DoorPrompt(door,dest,prompt))return false;
            out.cache.anchors.push_back({id,ref.point[0],ref.point[1],ref.point[2],teleport});
            out.metadata.emplace(id,Fo3DoorPromptMetadata{ref.base,teleport});out.prompts.emplace(id,std::move(prompt));
        }
        return true;
    }
private:
    struct Close{void operator()(FILE* f)const{std::fclose(f);}};
    std::unique_ptr<FILE,Close> file;
    std::unordered_map<uint32_t,Record> records;
    std::unordered_map<uint32_t,Ref> refs;
    std::unordered_map<uint32_t,bool> memo;
};
bool SameAnchors(const std::vector<Fo3DoorAnchor>& a,const std::vector<Fo3DoorAnchor>& b){
    if(a.size()!=b.size())return false;
    std::unordered_map<uint32_t,const Fo3DoorAnchor*> indexed;
    for(const auto& x:a)if(!indexed.emplace(x.sourceDoorRef,&x).second)return false;
    for(const auto& x:b){auto found=indexed.find(x.sourceDoorRef);if(found==indexed.end()||!SameAnchor(*found->second,x))return false;indexed.erase(found);}
    return true;
}
}
Fo3DoorResources& Fo3LiveDoorResources(){static Fo3DoorResources value;return value;}
uint64_t GetFo3DoorResourceRevision(){return Publication().Revision();}
void MarkFo3DoorResourcesChanged(){Publication().Changed();}
Fo3DoorResources CaptureFo3DoorResources(){return Fo3LiveDoorResources();}
bool FormatFo3DoorPrompt(const std::string& door,const std::string& destination,std::string& out){
    if(door.empty()&&destination.empty())return false;
    std::string next="Open";if(!door.empty())next+=" "+door;if(!destination.empty())next+=" to "+destination;out=std::move(next);return true;
}
bool PrepareFo3ResidentDoorResources(const std::string& path,uint32_t cell,
        const fo3world::ReferenceStateSnapshot& published,const fo3world::ReferenceStateSnapshot& candidate,
        const std::vector<uint32_t>& affected,const Fo3DoorResources& captured,Fo3DoorResources& out,
        const std::atomic<bool>& cancelled,std::string& error){
    error.clear();for(size_t i=0;i<affected.size();++i)if(!affected[i]||(i&&affected[i]<=affected[i-1])){error="invalid affected door list";return false;}
    Reader reader;Scope before,after;
    if(!reader.Open(path,cancelled)||!reader.Build(cell,published,before,cancelled)||!reader.Build(cell,candidate,after,cancelled)){
        error="authored load-door preparation failed or cancelled";return false;}
    const auto old=captured.cells.find(cell);
    if(old!=captured.cells.end()&&old->second.attempted&&!SameAnchors(old->second.anchors,before.cache.anchors)){
        error="published load-door anchor cache mismatch";return false;}
    for(const auto& entry:before.metadata){
        const auto metadata=captured.metadata.find(entry.first);
        const auto prompt=captured.prompts.find(entry.first);
        if((metadata!=captured.metadata.end()&&(metadata->second.sourceBase!=entry.second.sourceBase||
                !SameTeleport(metadata->second.teleport,entry.second.teleport)))||
            (prompt!=captured.prompts.end()&&prompt->second!=before.prompts.at(entry.first))){
            error="published load-door prompt cache mismatch";return false;
        }
    }
    auto covered=[&](uint32_t ref){return std::binary_search(affected.begin(),affected.end(),ref);};
    for(const auto& x:before.metadata){auto next=after.metadata.find(x.first);
        if(next==after.metadata.end()){if(!covered(x.first)){error="unlisted removed load door";return false;}}
        else if(x.second.sourceBase!=next->second.sourceBase||!SameTeleport(x.second.teleport,next->second.teleport)||
            before.prompts.at(x.first)!=after.prompts.at(x.first)){error="authored load-door metadata changed";return false;}}
    for(const auto& x:after.metadata)if(!before.metadata.count(x.first)&&!covered(x.first)){error="unlisted incoming load door";return false;}
    Fo3DoorResources next=captured;
    next.cells[cell]=std::move(after.cache);
    for(uint32_t id:before.refs){next.metadata.erase(id);next.prompts.erase(id);}
    for(auto& x:after.metadata)next.metadata[x.first]=std::move(x.second);
    for(auto& x:after.prompts)next.prompts[x.first]=std::move(x.second);
    if(cancelled.load(std::memory_order_acquire)){error="load-door preparation cancelled";return false;}
    out=std::move(next);return true;
}
bool PrepareFo3ResidentDoorPlan(uint64_t revision,Fo3DoorResources& candidate){
    if(Pending()||!revision||revision!=GetFo3DoorResourceRevision())return false;
    if(&candidate==&Fo3LiveDoorResources())return false;
    // Copy before allocation: failure leaves both caller and live ownership intact.
    Plan::Payload payload{candidate};
    auto plan=Plan::Prepare(Publication(),std::tie(Fo3LiveDoorResources()),payload);
    if(!plan)return false;
    candidate=Fo3DoorResources{};
    Pending()=std::move(plan);return true;
}
bool CanCommitFo3ResidentDoorPlan(){return Pending()&&Pending()->CanCommit(Publication(),std::tie(Fo3LiveDoorResources()));}
bool CanRollbackFo3ResidentDoorPlan(){return Pending()&&Pending()->CanRollback(Publication(),std::tie(Fo3LiveDoorResources()));}
bool CanFinalizeFo3ResidentDoorPlan(bool committed){return Pending()&&Pending()->CanFinalize(Publication(),std::tie(Fo3LiveDoorResources()),committed);}
bool CommitFo3ResidentDoorPlan(){return Pending()&&Pending()->Commit(Publication(),std::tie(Fo3LiveDoorResources()));}
bool RollbackFo3ResidentDoorPlan(){return Pending()&&Pending()->Rollback(Publication(),std::tie(Fo3LiveDoorResources()));}
void DiscardFo3ResidentDoorPlan(){Pending().reset();}
bool HasFo3ResidentDoorPlan(){return bool(Pending());}
bool Fo3ResidentDoorPlanApplied(){return Pending()&&Pending()->Applied();}

bool QueryFo3DoorAnchorResources(const std::vector<Fo3DoorAnchor>& anchors,
        float sceneCenterX,float sceneCenterY,float sceneFloorZ,float unitsPerMetre,
        float floorY,float sceneForward,float originX,float originY,float originZ,
        float dirX,float dirY,float dirZ,Fo3DoorAimQ1700* outAim) {
    if(outAim)*outAim={};
    for(float value:{sceneCenterX,sceneCenterY,sceneFloorZ,unitsPerMetre,floorY,sceneForward,
            originX,originY,originZ,dirX,dirY,dirZ})if(!std::isfinite(value))return false;
    if(unitsPerMetre<=1e-4f)return false;
    const float length=std::sqrt(dirX*dirX+dirY*dirY+dirZ*dirZ);
    if(!std::isfinite(length)||length<1e-5f)return false;
    dirX/=length;dirY/=length;dirZ/=length;
    // Preserve Q16.3's authored focus geometry exactly; only cache construction
    // changed. Rendered DOOR AABBs still win before this fallback is queried.
    constexpr float kMaxDistance = 4.0f;
    constexpr float kAnchorRadius = 1.35f;
    constexpr float kRadiusSq = kAnchorRadius * kAnchorRadius;

    const Fo3DoorAnchor* best = nullptr;
    float bestScore = 1e30f;
    float bestT = 0.0f;
    for (const Fo3DoorAnchor& anchor : anchors) {
        const float px = (anchor.sourceX - sceneCenterX) / unitsPerMetre;
        const float py = floorY + (anchor.sourceZ - sceneFloorZ) / unitsPerMetre;
        const float pz = sceneForward - (anchor.sourceY - sceneCenterY) / unitsPerMetre;

        const float vx = px - originX;
        const float vy = py - originY;
        const float vz = pz - originZ;
        const float t = vx * dirX + vy * dirY + vz * dirZ;
        if (t < 0.05f || t > kMaxDistance) continue;

        const float distanceSq = vx * vx + vy * vy + vz * vz;
        const float perpendicularSq = std::max(0.0f, distanceSq - t * t);
        if (perpendicularSq > kRadiusSq) continue;

        const float score = perpendicularSq * 3.0f + t * 0.05f;
        if (score >= bestScore) continue;
        bestScore = score;
        bestT = t;
        best = &anchor;
    }

    if (!best) return false;
    if (outAim) {
        outAim->valid = true;
        outAim->sourceDoorRef = best->sourceDoorRef;
        outAim->destinationDoorRef = best->teleport.destinationDoorRefFormId;
        outAim->distance = bestT;
        outAim->x = best->teleport.x;
        outAim->y = best->teleport.y;
        outAim->z = best->teleport.z;
        outAim->rx = best->teleport.rx;
        outAim->ry = best->teleport.ry;
        outAim->rz = best->teleport.rz;
    }
    return true;
}
