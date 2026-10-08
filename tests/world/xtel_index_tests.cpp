#include "data/fo3-xtel-index.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>

namespace {
using Bytes=std::vector<uint8_t>;
void U32(Bytes& out,size_t offset,uint32_t v) {
    assert(offset+4<=out.size());
    for(unsigned i=0;i<4;++i)out[offset+i]=uint8_t(v>>(8u*i));
}
void F32(Bytes& out,size_t offset,float v) {
    uint32_t bits=0;std::memcpy(&bits,&v,sizeof(v));U32(out,offset,bits);
}
void Append(Bytes& a,const Bytes& b){a.insert(a.end(),b.begin(),b.end());}
Bytes Record(const char* name,uint32_t id,const Bytes& body={},uint32_t flags=0) {
    Bytes result(24u,0);
    for(int i=0;i<4;++i)result[size_t(i)]=uint8_t(name[i]);
    U32(result,4,uint32_t(body.size()));U32(result,8,flags);U32(result,12,id);
    Append(result,body);return result;
}
Bytes Group(uint32_t label,uint32_t type,const Bytes& body) {
    Bytes result(24u,0);
    const char* tag="GRUP";for(int i=0;i<4;++i)result[size_t(i)]=uint8_t(tag[i]);
    U32(result,4,uint32_t(24u+body.size()));U32(result,8,label);U32(result,12,type);
    Append(result,body);return result;
}
Bytes Sub(const char* name,const Bytes& body) {
    assert(body.size()<=65535);
    Bytes result(6u,0);
    for(int i=0;i<4;++i)result[size_t(i)]=uint8_t(name[i]);
    result[4]=uint8_t(body.size());result[5]=uint8_t(body.size()>>8);
    Append(result,body);return result;
}
Bytes Word(uint32_t x){Bytes b(4u);U32(b,0,x);return b;}
Bytes DoorRef(uint32_t base,uint32_t target,bool optionalFlags,bool malformed=false) {
    Bytes payload=Sub("NAME",Word(base));
    Bytes xtel(optionalFlags?32u:28u,0);
    U32(xtel,0,target);
    for(unsigned i=0;i<3;++i) {
        F32(xtel,4+4*i,float(i+11));
        F32(xtel,16+4*i,float(i+1)*.125f);
    }
    if(optionalFlags)U32(xtel,28,0xA0000001u);
    if(malformed)xtel.pop_back();
    Append(payload,Sub("XTEL",xtel));return payload;
}
std::string Write(const Bytes& bytes) {
    char path[]="/tmp/fq-xtel-XXXXXX";
    int fd=mkstemp(path);assert(fd>=0);close(fd);
    std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
    assert(out.good());out.close();return path;
}
Bytes Master(bool malformed=false) {
    Bytes result=Record("TES4",0);
    Append(result,Record("DOOR",0x1000));
    Append(result,Record("ACTI",0x5000));
    // Source interior 0x200; target exterior 0x300 in worldspace 0x700.
    Append(result,Record("CELL",0x200));
    Bytes room;
    Append(room,Record("REFR",0x101,DoorRef(0x1000,0x102,false)));
    // A second link with a source missing its destination ref; preserve
    // diagnostics without ever inventing a destination CELL.
    Append(room,Record("REFR",0x103,DoorRef(0x1000,0x999,false)));
    // Another same-cell link whose base is ACTI: never map as a load door.
    Append(room,Record("REFR",0x104,DoorRef(0x5000,0x102,false)));
    Append(result,Group(0x200,6,room));
    Bytes exterior;
    Append(exterior,Record("CELL",0x300));
    Bytes exteriorRefs;
    Append(exteriorRefs,Record("REFR",0x102,DoorRef(0x1000,0x101,true,malformed),0x800u));
    Append(exterior,Group(0x300,6,exteriorRefs));
    Append(result,Group(0x700,1,exterior));
    return result;
}
void Synthetic() {
    const auto path=Write(Master());
    fo3xtel::Index index;std::string error;
    assert(index.Build(path,error)&&error.empty());std::remove(path.c_str());
    assert(index.Size()==2&&index.Stats().authoredXtels==4);
    assert(index.Stats().resolved==2&&index.Stats().missingDestination==1);
    assert(index.Stats().nonDoor==1&&index.Stats().reciprocal==2);
    const auto* door=index.Find(0x101);assert(door);
    assert(door->sourceCell==0x200&&door->sourceWorld==0);
    assert(door->destinationCell==0x300&&door->destinationWorld==0x700);
    assert(door->arrival[0]==11.f&&door->rotation[0]==.125f);
    assert(door->teleportFlags==0&&door->reciprocal);
    const auto* reverse=index.Find(0x102);assert(reverse);
    assert(reverse->teleportFlags==0xA0000001u&&reverse->sourceRecordFlags==0x800u);
    assert(!index.Find(0x103)&&!index.Find(0x104));
    assert(index.Outgoing(0x200)&&index.Outgoing(0x200)->size()==1);
    std::vector<uint32_t> pathRefs;
    assert(index.CellRoute(0x200,0x300,pathRefs)&&pathRefs==std::vector<uint32_t>{0x101});
    assert(index.CellRoute(0x300,0x200,pathRefs)&&pathRefs==std::vector<uint32_t>{0x102});
    assert(index.CellRoute(0x200,0x200,pathRefs)&&pathRefs.empty());
    assert(!index.CellRoute(0x200,0x999,pathRefs)&&pathRefs.empty());
    const auto bad=Write(Master(true));
    assert(!index.Build(bad,error)&&!error.empty());
    assert(index.Find(0x101)); // Failed scan cannot publish a partial map.
    std::remove(bad.c_str());
    std::cout<<"Synthetic directed XTEL links, owner cells/worlds, flags, unresolved and transactional failure passed\n";
}
void Original(const std::string& esm) {
    fo3xtel::Index index;std::string error;
    assert(index.Build(esm,error)&&error.empty());
    const auto& s=index.Stats();
    // Verified on original unmodified Fallout3.esm. World ownership is derived
    // from CELL/WRLD GRUP ancestry; no hand-authored inter-cell destinations.
    assert(s.authoredXtels==1118&&s.resolved==1118);
    assert(s.missingDestination==0&&s.nonDoor==0&&s.reciprocal==1118);
    assert(s.crossCell==1114&&s.crossWorld==778);
    const auto* gate=index.Find(0x3a73);
    assert(gate&&gate->sourceCell==0x3a34&&gate->sourceWorld==0);
    assert(gate->destinationRef==0x3a1c&&gate->destinationCell==0xa96&&gate->destinationWorld==0xa74);
    assert(gate->reciprocal);
    const auto* shop=index.Find(0x3a1d);
    assert(shop&&shop->sourceCell==0xa96&&shop->destinationRef==0x3a43&&shop->destinationCell==0x3a2a);
    std::vector<uint32_t> route;
    assert(index.CellRoute(0x3a34,0x3a2a,route)&&route.size()==2);
    assert(route[0]==0x3a73&&route[1]==0x3a1d);
    std::cout<<"Original Fallout3.esm XTEL edges="<<s.authoredXtels
             <<" directed cross-cell="<<s.crossCell<<" cross-world="<<s.crossWorld
             <<" reciprocal="<<s.reciprocal
             <<" Megaton Room->Exterior->Shop verified\n";
}
}
int main(int argc,char** argv) {
    Synthetic();
    if(argc>1)Original(argv[1]);
    return 0;
}
