#include "fo3-resident-authored-admission.h"
#include "data/fo3-esm-reader.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace fo3scene {
namespace {
bool Is(const char* a,const char* b){return std::memcmp(a,b,4)==0;}
bool Complete(const std::vector<uint8_t>& b){
    size_t p=0;uint32_t extended=0;
    while(p<b.size()){
        if(b.size()-p<6)return false;
        const auto* tag=b.data()+p;const auto n=fo3esm::ReadU16(tag+4);p+=6;
        if(Is(reinterpret_cast<const char*>(tag),"XXXX")){
            if(extended||n!=4||b.size()-p<4)return false;
            extended=fo3esm::ReadU32(b.data()+p);p+=4;if(!extended)return false;
        }else{
            const uint32_t count=extended?extended:n;extended=0;
            if(count>b.size()-p)return false;
            p+=count;
        }
    }
    return !extended;
}
struct Record{uint32_t id=0,cell=0,world=0;fo3esm::RecordLocation location;};
struct Close{void operator()(FILE* f)const{std::fclose(f);}};
// Two bounded streaming passes: never build a whole-master reference catalogue.
template<class Visit> bool Scan(FILE* f,const std::atomic<bool>& cancel,Visit visit){
    if(fseeko(f,0,SEEK_SET))return false;
    const auto size=fo3esm::FileSize(f);if(size<24)return false;
    struct Frame{uint64_t end;uint32_t label,type;};std::vector<Frame> stack;
    uint64_t p=0;
    while(p<uint64_t(size)){
        if(cancel.load(std::memory_order_acquire))return false;
        while(!stack.empty()&&p==stack.back().end)stack.pop_back();
        const uint64_t limit=stack.empty()?uint64_t(size):stack.back().end;
        if(p>limit||limit-p<24)return false;
        uint8_t h[24];if(!fo3esm::ReadExact(f,h,24))return false;
        const uint32_t n=fo3esm::ReadU32(h+4);
        if(Is(reinterpret_cast<const char*>(h),"GRUP")){
            if(n<24||n>limit-p||stack.size()>=128)return false;
            stack.push_back({p+n,fo3esm::ReadU32(h+8),fo3esm::ReadU32(h+12)});
            p+=24;continue;
        }
        if(n>limit-p-24)return false;
        Record r;r.id=fo3esm::ReadU32(h+12);
        r.location={p+24,n,fo3esm::ReadU32(h+8),std::string(reinterpret_cast<char*>(h),4)};
        for(auto g=stack.rbegin();g!=stack.rend();++g){
            if(!r.cell&&(g->type==6||g->type==8||g->type==9||g->type==10))r.cell=g->label;
            if(!r.world&&g->type==1)r.world=g->label;
        }
        if(!visit(r))return false;
        p+=24+n;if(fseeko(f,p,SEEK_SET))return false;
    }
    return p==uint64_t(size);
}
bool Payload(FILE* f,const Record& r,std::vector<uint8_t>& b){
    return fo3esm::ReadPayload(f,r.location,b)&&Complete(b);
}
}
bool ResidentAuthoredAdmission::Matches(uint32_t expectedCell,const std::vector<uint32_t>& ids) const noexcept {
    // The producer requires sorted unique IDs, as emitted by the logical plan.
    return cell&&cell==expectedCell&&affected==ids;
}
bool PrepareResidentAuthoredAdmission(const std::string& master,uint32_t cell,
    const std::vector<uint32_t>& affected,ResidentAuthoredAdmission& out,
    const std::atomic<bool>& cancel,std::string& error){
    error.clear();
    auto fail=[&](const char* why){error=why;return false;};
    if(!cell||affected.empty()||!affected.front()||!std::is_sorted(affected.begin(),affected.end())||
        std::adjacent_find(affected.begin(),affected.end())!=affected.end())
        return fail("invalid authored admission identity");
    std::unique_ptr<FILE,Close> f(std::fopen(master.c_str(),"rb"));
    if(!f)return fail("authored master unavailable");
    std::unordered_set<uint32_t> wanted(affected.begin(),affected.end());
    std::unordered_map<uint32_t,uint32_t> bases;
    std::unordered_set<uint32_t> found;
    std::unordered_set<uint32_t> packageValues;
    bool interior=false,seenCell=false,navmesh=false,package=false;
    std::string scanning;
    std::vector<uint8_t> b;
    const bool first=Scan(f.get(),cancel,[&](const Record& r){
        scanning=r.location.type+" "+std::to_string(r.id);
        bool loaded=false;
        auto payload=[&](){
            if(!loaded)loaded=Payload(f.get(),r,b);
            return loaded;
        };
        const bool reference=r.location.type=="REFR"||r.location.type=="ACHR"||r.location.type=="ACRE";
        if(reference&&!(r.location.flags&0x20u)){
            // Reverse edges can originate outside the changed CELL or from an
            // actor with no resident mesh. Never index just selected placements.
            if(!payload())return false;
            bool valid=true;std::unordered_set<std::string> links;
            fo3esm::WalkSubrecords(b,[&](const char* t,const uint8_t* p,uint32_t n){
                const bool parent=Is(t,"XESP"),linked=Is(t,"XLKR");
                const bool teleport=Is(t,"XTEL"),emittance=Is(t,"XEMI");
                if(!parent&&!linked&&!teleport&&!emittance)return;
                if(!links.insert(std::string(t,4)).second||
                    (parent?n!=8:teleport?(n!=28&&n!=32):n!=4)){valid=false;return;}
                const auto target=fo3esm::ReadU32(p);
                if(!wanted.count(target))return;
                // XESP children must all be in the supplied logical change
                // set. Conservative: independent child overrides do not relax
                // this guard. Other incoming links remain wholly unadmitted.
                if(parent&&wanted.count(r.id))return;
                valid=false;
                error=std::string(parent?"incomplete affected enable-parent closure: ":
                    "unowned incoming authored link: ")+std::string(t,4)+" from "+scanning+
                    " to "+std::to_string(target);
            });
            if(!valid)return false;
        }
        if(r.id==cell){
            if(seenCell||r.location.type!="CELL"||r.world||(r.location.flags&0x20)||!payload())return false;
            seenCell=true;bool data=false,valid=true;
            fo3esm::WalkSubrecords(b,[&](const char* t,const uint8_t* p,uint32_t n){
                if(Is(t,"DATA")){if(data||n<1)valid=false;else{data=true;interior=(p[0]&1)!=0;}}
                if(Is(t,"XCLC"))valid=false;
            });
            if(!valid||!data||!interior)return false;
        }
        if(wanted.count(r.id)){
            if(!found.insert(r.id).second||r.location.type!="REFR"||r.cell!=cell||r.world||
                (r.location.flags&(0x20u|0x8000u))||!payload())return false;
            bool valid=true,position=false;uint32_t base=0;std::unordered_set<std::string> tags;
            fo3esm::WalkSubrecords(b,[&](const char* t,const uint8_t* p,uint32_t n){
                if(!tags.insert(std::string(t,4)).second){valid=false;return;}
                if(Is(t,"NAME")){if(n!=4)valid=false;else base=fo3esm::ReadU32(p);}
                else if(Is(t,"DATA")){if(n!=24)valid=false;else{position=true;
                    for(int i=0;i<6;++i)if(!std::isfinite(fo3esm::ReadF32(p+4*i)))valid=false;}}
                else if(Is(t,"XSCL")){if(n!=4||!std::isfinite(fo3esm::ReadF32(p))||fo3esm::ReadF32(p)<=0)valid=false;}
                else if(Is(t,"XESP")){if(n!=8||!fo3esm::ReadU32(p)||(p[4]&~3u))valid=false;}
                else if(Is(t,"XOWN")||Is(t,"XRNK")||Is(t,"XGLB")){if(n!=4)valid=false;}
                else if(!Is(t,"EDID"))valid=false; // Includes XTEL, rooms, portals, emittance and unknown attachments.
            });
            if(!valid||!position||!base||base==cell||wanted.count(base))return false;
            bases.emplace(r.id,base);
        }
        if(r.location.type=="NAVM"&&!(r.location.flags&0x20u)){
            if(!payload())return false;
            bool data=false,valid=true;
            fo3esm::WalkSubrecords(b,[&](const char* t,const uint8_t* p,uint32_t n){
                if(Is(t,"DATA")){if(data||n<12)valid=false;else{data=true;if(fo3esm::ReadU32(p)==cell)navmesh=true;}}
            });
            if(!valid||!data)return false;
        }
        if(r.location.type=="PACK"&&!(r.location.flags&0x20u)){
            if(!payload())return false;
            bool valid=true;
            fo3esm::WalkSubrecords(b,[&](const char* t,const uint8_t* p,uint32_t n){
                if(Is(t,"PLDT")||Is(t,"PLD2")||Is(t,"PTDT")||Is(t,"PTD2")){
                    // Original master has 12-byte locations and 16-byte targets.
                    // Both share the existing package loader's type/value/radius prefix.
                    const bool target=Is(t,"PTDT")||Is(t,"PTD2");
                    if(n!=12&&!(target&&n==16))valid=false;
                    else {const auto value=fo3esm::ReadU32(p+4);packageValues.insert(value);
                        if(wanted.count(value))package=true;}
                }
            });
            if(!valid)return false;
        }
        return true;
    });
    if(!first){
        if(cancel.load())error="authored admission cancelled";
        else if(error.empty())error="unsupported or malformed authored reference/dependency near "+scanning;
        return false;
    }
    if(!seenCell||!interior||found.size()!=wanted.size())return fail("authored reference or interior CELL missing");
    if(package)return fail("affected reference has package navigation dependency");
    std::unordered_set<uint32_t> baseIds;for(const auto& r:bases)baseIds.insert(r.second);
    for(uint32_t value:packageValues)if(baseIds.count(value))
        return fail("affected base has package navigation dependency");
    std::unordered_set<uint32_t> seenBases;bool geometry=false;
    const bool second=Scan(f.get(),cancel,[&](const Record& r){
        scanning=r.location.type+" "+std::to_string(r.id);
        const bool base=baseIds.count(r.id)!=0;
        if(base&&(!seenBases.insert(r.id).second||(r.location.type!="STAT"&&r.location.type!="LIGH")||
            (r.location.flags&(0x20u|0x8000u))))return false;
        if(r.location.flags&0x20u)return true;
        // No record-type allowlist: dependencies also live in nonresident
        // quests, dialogue, packages, effects and otherwise unknown records.
        if(!Payload(f.get(),r,b))return false;
        bool dependencies=true;
        auto target=[&](uint32_t id,const char* field){
            if(!wanted.count(id)&&!baseIds.count(id))return;
            dependencies=false;
            if(error.empty())error="unowned incoming script/condition dependency: "+std::string(field)+
                " from "+scanning+" to "+std::to_string(id)+(wanted.count(id)?" (reference)":" (base)");
        };
        auto malformed=[&](const char* field){
            dependencies=false;
            if(error.empty())error="malformed script/condition dependency: "+std::string(field)+" in "+scanning;
        };
        fo3esm::WalkSubrecords(b,[&](const char* t,const uint8_t* p,uint32_t n){
            if(Is(t,"SCRO")||Is(t,"SCRV")){
                if(n!=4){malformed(Is(t,"SCRO")?"SCRO":"SCRV");return;}
                // SCRV stores a local-variable index, not an authored FormID.
                if(Is(t,"SCRO"))target(fo3esm::ReadU32(p),"SCRO");
            }else if(Is(t,"CTDA")){
                if(n!=20&&n!=24&&n!=28){malformed("CTDA");return;}
                // Conservative operand matching, not a function-arity table:
                // numeric/unconsumed operands can intentionally false-positive.
                target(fo3esm::ReadU32(p+12),"CTDA +12");
                target(fo3esm::ReadU32(p+16),"CTDA +16");
                if(n==28)target(fo3esm::ReadU32(p+24),"CTDA +24");
                if(p[0]&4)target(fo3esm::ReadU32(p+4),"CTDA comparison-global");
            }else if(r.location.type=="FLST"&&Is(t,"LNAM")){
                if(n!=4){malformed("FLST LNAM");return;}
                // Cut potential list indirection at its leaf membership, even
                // for dormant lists. Nested/cyclic graphs need no traversal:
                // every chain to an affected ID has a direct member edge.
                target(fo3esm::ReadU32(p),"FLST LNAM");
            }
        });
        if(!dependencies)return false;
        if(!base)return true;
        bool valid=true,model=false;std::unordered_set<std::string> tags;
        fo3esm::WalkSubrecords(b,[&](const char* t,const uint8_t* p,uint32_t n){
            if(!tags.insert(std::string(t,4)).second)valid=false;
            if(Is(t,"MODL")){if(!n||p[n-1]!=0||fo3esm::ZString(p,n).empty())valid=false;else model=true;}
            else if(Is(t,"SNAM")){if(n!=4||fo3esm::ReadU32(p))valid=false;}
            else if(!Is(t,"EDID")&&!Is(t,"OBND")&&!Is(t,"MODT")&&!Is(t,"MODS")&&
                !(r.location.type=="LIGH"&&(Is(t,"DATA")||Is(t,"FNAM")||Is(t,"FULL")||Is(t,"ICON")||Is(t,"MICO"))))valid=false;
        });
        // A model-bearing light can also change collision/grounding geometry.
        geometry=geometry||model;
        if(r.location.type=="STAT"&&!model)valid=false;
        return valid;
    });
    if(!second){
        if(cancel.load())error="authored admission cancelled";
        else if(error.empty())error="unsupported or malformed authored base/dependency near "+scanning;
        return false;
    }
    if(seenBases.size()!=baseIds.size())return fail("authored base missing");
    if(navmesh&&geometry)return fail("geometry mutation requires navigation reconciliation");
    if(cancel.load(std::memory_order_acquire))return fail("authored admission cancelled");
    ResidentAuthoredAdmission next;next.cell=cell;next.affected=affected;out=std::move(next);return true;
}
}
