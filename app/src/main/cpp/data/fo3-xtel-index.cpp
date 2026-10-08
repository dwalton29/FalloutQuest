#include "fo3-xtel-index.h"
#include "fo3-esm-reader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <queue>
#include <unordered_set>
#include <utility>

namespace fo3xtel {
namespace {
struct Group {uint64_t end=0;uint32_t label=0,type=0;};
struct RawRef {uint32_t base=0,cell=0,recordFlags=0;};
struct RawXtel {
    uint32_t destination=0,flags=0;
    std::array<float,3> arrival{},rotation{};
};
struct RawData {
    std::unordered_map<uint32_t,RawRef> refs;
    std::unordered_map<uint32_t,uint32_t> cellWorlds;
    std::unordered_set<uint32_t> doorBases;
    std::unordered_map<uint32_t,RawXtel> xtels;
    Statistics stats;
};
uint32_t Owner(const std::vector<Group>& stack) {
    for(auto it=stack.rbegin();it!=stack.rend();++it)
        if(it->type==6u||it->type==8u||it->type==9u||it->type==10u)return it->label;
    return 0;
}
uint32_t World(const std::vector<Group>& stack) {
    for(auto it=stack.rbegin();it!=stack.rend();++it)if(it->type==1u)return it->label;
    return 0;
}
bool DecodeReference(const std::vector<uint8_t>& bytes,RawRef& ref,
                     RawXtel& xtel,bool& hasXtel,bool& malformed) {
    hasXtel=false;malformed=false;
    bool sawXtel=false;
    size_t pos=0;
    uint32_t extended=0;
    while(pos+6u<=bytes.size()) {
        const uint8_t* tag=bytes.data()+pos;
        const uint16_t stored=fo3esm::ReadU16(tag+4u);
        pos+=6u;
        if(std::memcmp(tag,"XXXX",4u)==0) {
            if(stored!=4u||pos+4u>bytes.size()||extended){malformed=true;return false;}
            extended=fo3esm::ReadU32(bytes.data()+pos);pos+=4u;
            continue;
        }
        const uint32_t length=extended?extended:stored;extended=0;
        if(length>bytes.size()-pos){malformed=true;return false;}
        const uint8_t* data=bytes.data()+pos;
        if(std::memcmp(tag,"NAME",4u)==0) {
            if(length!=4u){malformed=true;return false;}
            ref.base=fo3esm::ReadU32(data);
        } else if(std::memcmp(tag,"XTEL",4u)==0) {
            if(sawXtel||!(length==28u||length==32u)){malformed=true;return false;}
            sawXtel=true;
            xtel.destination=fo3esm::ReadU32(data);
            for(unsigned i=0;i<3;++i) {
                xtel.arrival[i]=fo3esm::ReadF32(data+4u+4u*i);
                xtel.rotation[i]=fo3esm::ReadF32(data+16u+4u*i);
                if(!std::isfinite(xtel.arrival[i])||!std::isfinite(xtel.rotation[i])){
                    malformed=true;return false;
                }
            }
            xtel.flags=length==32u?fo3esm::ReadU32(data+28u):0;
            hasXtel=xtel.destination!=0;
            if(!hasXtel){malformed=true;return false;}
        }
        pos+=length;
    }
    if(pos!=bytes.size()||extended) {malformed=true;return false;}
    return true;
}
bool Scan(const std::string& path,RawData& data,std::string& error) {
    FILE* file=std::fopen(path.c_str(),"rb");
    if(!file){error="cannot open Fallout3.esm";return false;}
    const int64_t length=fo3esm::FileSize(file);
    if(length<24){error="invalid ESM length";std::fclose(file);return false;}
    std::vector<Group> stack;
    bool ok=true;
    while(true) {
        const off_t cursor=ftello(file);
        if(cursor<0){error="ESM seek failure";ok=false;break;}
        const uint64_t offset=static_cast<uint64_t>(cursor);
        while(!stack.empty()&&offset>=stack.back().end)stack.pop_back();
        if(offset==uint64_t(length))break;
        if(offset+fo3esm::HEADER_SIZE>uint64_t(length)){
            error="truncated ESM header";ok=false;break;
        }
        uint8_t header[fo3esm::HEADER_SIZE]{};
        if(!fo3esm::ReadExact(file,header,sizeof(header))){error="unreadable ESM header";ok=false;break;}
        const uint32_t size=fo3esm::ReadU32(header+4u);
        if(std::memcmp(header,"GRUP",4u)==0) {
            const uint64_t end=offset+size;
            if(size<fo3esm::HEADER_SIZE||end>uint64_t(length)||
               (!stack.empty()&&end>stack.back().end)){
                error="invalid group bounds";ok=false;break;
            }
            stack.push_back({end,fo3esm::ReadU32(header+8u),fo3esm::ReadU32(header+12u)});
            continue;
        }
        const uint64_t end=offset+fo3esm::HEADER_SIZE+size;
        if(end>uint64_t(length)||(!stack.empty()&&end>stack.back().end)){
            error="record overruns group";ok=false;break;
        }
        ++data.stats.records;
        const uint32_t form=fo3esm::ReadU32(header+12u);
        const uint32_t flags=fo3esm::ReadU32(header+8u);
        if(std::memcmp(header,"DOOR",4u)==0) {
            data.doorBases.insert(form);
        } else if(std::memcmp(header,"CELL",4u)==0) {
            data.cellWorlds[form]=World(stack);
        } else if(std::memcmp(header,"REFR",4u)==0) {
            RawRef ref;ref.cell=Owner(stack);ref.recordFlags=flags;
            if(ref.cell==0){error="reference with no owning CELL";ok=false;break;}
            std::vector<uint8_t> bytes;
            if(!fo3esm::ReadPayloadCurrent(file,size,flags,bytes)){
                error="unreadable REFR payload";ok=false;break;
            }
            RawXtel xtel;bool hasXtel=false,malformed=false;
            if(!DecodeReference(bytes,ref,xtel,hasXtel,malformed)) {
                ++data.stats.malformed;error="malformed REFR subrecords";ok=false;break;
            }
            if(!data.refs.emplace(form,ref).second) {
                error="duplicate REFR FormID";ok=false;break;
            }
            if(hasXtel)data.xtels.emplace(form,xtel);
        }
        if(fseeko(file,static_cast<off_t>(end),SEEK_SET)!=0){
            error="ESM record seek failed";ok=false;break;
        }
    }
    std::fclose(file);
    data.stats.referenceOwners=data.refs.size();
    data.stats.authoredXtels=data.xtels.size();
    return ok;
}
} // namespace

bool Index::Build(const std::string& esmPath,std::string& error) {
    error.clear();
    RawData raw;
    if(!Scan(esmPath,raw,error))return false;
    Index built;
    built.stats_=raw.stats;
    for(const auto& entry:raw.xtels) {
        const auto source=raw.refs.find(entry.first);
        const auto destination=raw.refs.find(entry.second.destination);
        if(source==raw.refs.end()||destination==raw.refs.end()||
           !raw.cellWorlds.count(source->second.cell)||
           !raw.cellWorlds.count(destination->second.cell)) {
            ++built.stats_.missingDestination;continue;
        }
        if(!raw.doorBases.count(source->second.base)||
           !raw.doorBases.count(destination->second.base)) {
            ++built.stats_.nonDoor;continue;
        }
        DoorLink link;
        link.sourceRef=entry.first;link.sourceBase=source->second.base;
        link.sourceCell=source->second.cell;
        link.sourceWorld=raw.cellWorlds.at(link.sourceCell);
        link.sourceRecordFlags=source->second.recordFlags;
        link.destinationRef=entry.second.destination;
        link.destinationBase=destination->second.base;
        link.destinationCell=destination->second.cell;
        link.destinationWorld=raw.cellWorlds.at(link.destinationCell);
        link.destinationRecordFlags=destination->second.recordFlags;
        link.arrival=entry.second.arrival;
        link.rotation=entry.second.rotation;
        link.teleportFlags=entry.second.flags;
        built.links_.emplace(link.sourceRef,link);
        built.byCell_[link.sourceCell].push_back(link.sourceRef);
        ++built.stats_.resolved;
        if(link.sourceCell!=link.destinationCell)++built.stats_.crossCell;
        if(link.sourceWorld!=link.destinationWorld)++built.stats_.crossWorld;
    }
    for(auto& entry:built.links_) {
        DoorLink& link=entry.second;
        const auto destination=built.links_.find(link.destinationRef);
        link.reciprocal=destination!=built.links_.end() &&
            destination->second.destinationRef==link.sourceRef;
        if(link.reciprocal)++built.stats_.reciprocal;
    }
    for(auto& entry:built.byCell_)std::sort(entry.second.begin(),entry.second.end());
    *this=std::move(built);
    return true;
}
const DoorLink* Index::Find(uint32_t id) const {
    const auto it=links_.find(id);
    return it==links_.end()?nullptr:&it->second;
}
const std::vector<uint32_t>* Index::Outgoing(uint32_t cell) const {
    const auto it=byCell_.find(cell);
    return it==byCell_.end()?nullptr:&it->second;
}
bool Index::CellRoute(uint32_t from,uint32_t to,std::vector<uint32_t>& doors) const {
    doors.clear();
    if(!from||!to)return false;
    if(from==to)return true;
    struct Step{uint32_t prior=0,door=0;};
    std::unordered_map<uint32_t,Step> came;
    std::queue<uint32_t> open;
    came.emplace(from,Step{});
    open.push(from);
    while(!open.empty()&&!came.count(to)) {
        const uint32_t cell=open.front();open.pop();
        const auto* options=Outgoing(cell);if(!options)continue;
        for(uint32_t ref:*options) {
            const auto* edge=Find(ref);
            if(!edge||!edge->destinationCell||came.count(edge->destinationCell))continue;
            came.emplace(edge->destinationCell,Step{cell,ref});
            open.push(edge->destinationCell);
        }
    }
    if(!came.count(to))return false;
    for(uint32_t cur=to;cur!=from;) {
        const Step step=came.at(cur);
        if(!step.door){doors.clear();return false;}
        doors.push_back(step.door);cur=step.prior;
    }
    std::reverse(doors.begin(),doors.end());
    return true;
}
} // namespace fo3xtel
