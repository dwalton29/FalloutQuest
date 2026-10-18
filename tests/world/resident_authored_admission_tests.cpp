#include "world/fo3-resident-authored-admission.h"
#include "data/fo3-esm-reader.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <unistd.h>
#include <zlib.h>
using Bytes=std::vector<uint8_t>;
void u32(Bytes& b,size_t p,uint32_t v){if(b.size()<p+4)b.resize(p+4);std::memcpy(b.data()+p,&v,4);}
Bytes word(uint32_t v){Bytes b(4);u32(b,0,v);return b;}
Bytes text(const std::string& s){Bytes b(s.begin(),s.end());b.push_back(0);return b;}
void append(Bytes& b,const Bytes& v){b.insert(b.end(),v.begin(),v.end());}
void sub(Bytes& b,const char* t,const Bytes& v){b.insert(b.end(),t,t+4);b.push_back(v.size()&255);b.push_back(v.size()>>8);append(b,v);}
Bytes record(const char* t,uint32_t id,const Bytes& payload,uint32_t flags=0){
    Bytes b(24);std::memcpy(b.data(),t,4);u32(b,4,payload.size());u32(b,8,flags);u32(b,12,id);append(b,payload);return b;
}
Bytes group(uint32_t id,uint32_t type,const Bytes& payload){
    Bytes b(24);std::memcpy(b.data(),"GRUP",4);u32(b,4,payload.size()+24);u32(b,8,id);u32(b,12,type);append(b,payload);return b;
}
struct Options{
    const char* baseType="STAT";const char* refType="REFR";const char* extra=nullptr;
    uint32_t refFlags=0,baseFlags=0,owner=10,world=0;int malformed=0;
    bool nav=false,pack=false,light=false,duplicate=false,missing=false,script=false,interior=true;
    bool baseTarget=false,lightModel=false,deletedNav=false,marker=false;uint32_t targetSize=12;
    const char* incoming=nullptr;const char* incomingType="REFR";
    uint32_t incomingTarget=30,incomingCell=11,incomingWorld=0,incomingFlags=0;
    int incomingSize=-1;bool duplicateLink=false,compressedIncoming=false,extendedIncoming=false;
};
Bytes fixture(Options o={}){
    Bytes b,cell,base,ref,children;
    sub(cell,"DATA",Bytes{uint8_t(o.interior?1:0)});
    append(b,record("CELL",10,cell));
    sub(base,"EDID",text(o.marker?"XMarker":"Fixture"));
    if(!o.light||o.lightModel)sub(base,"MODL",text(o.marker?"MarkerX.nif":"fixture.nif"));
    if(o.script)sub(base,"SCRI",word(777));
    if(!o.missing)append(b,record(o.light?"LIGH":o.baseType,20,base,o.baseFlags));
    sub(ref,"NAME",word(20));Bytes transform(24);
    if(o.malformed==1){float v=std::numeric_limits<float>::infinity();std::memcpy(transform.data(),&v,4);}
    sub(ref,"DATA",transform);
    if(o.extra)sub(ref,o.extra,word(888));
    if(o.malformed==2)sub(ref,"NAME",word(20));
    if(o.malformed==3)ref.push_back(1);
    if(o.malformed==4){Bytes ext;u32(ext,0,100);sub(ref,"XXXX",ext);sub(ref,"EDID",Bytes{0});}
    if(o.malformed==5)sub(ref,"XSCL",word(0));
    if(o.malformed==6)sub(ref,"XESP",Bytes(8));
    if(o.malformed==7)sub(ref,"XSCL",Bytes(3));
    append(children,record(o.refType,30,ref,o.refFlags));
    if(o.duplicate)append(children,record(o.refType,30,ref));
    auto scope=group(o.owner,6,group(o.owner,9,children));
    if(o.world)scope=group(o.world,1,scope);
    append(b,scope);
    if(o.incoming){
        Bytes incoming;sub(incoming,"NAME",word(20));sub(incoming,"DATA",Bytes(24));
        const size_t size=o.incomingSize>=0?size_t(o.incomingSize):
            !std::strcmp(o.incoming,"XESP")?8:!std::strcmp(o.incoming,"XTEL")?32:4;
        Bytes data(size);if(size>=4)u32(data,0,o.incomingTarget);
        if(o.extendedIncoming){sub(incoming,"XXXX",word(data.size()));sub(incoming,o.incoming,{});append(incoming,data);}
        else sub(incoming,o.incoming,data);
        if(o.duplicateLink)sub(incoming,o.incoming,data);
        if(o.compressedIncoming){
            uLongf n=compressBound(incoming.size());Bytes compressed(n+4);u32(compressed,0,incoming.size());
            assert(compress2(compressed.data()+4,&n,incoming.data(),incoming.size(),Z_BEST_SPEED)==Z_OK);
            compressed.resize(n+4);incoming=std::move(compressed);o.incomingFlags|=0x40000u;
        }
        auto dependency=record(o.incomingType,60,incoming,o.incomingFlags);
        auto foreign=group(o.incomingCell,6,dependency);
        if(o.incomingWorld)foreign=group(o.incomingWorld,1,foreign);
        append(b,foreign);
    }
    if(o.nav){Bytes data(12),p;u32(data,0,10);sub(p,"DATA",data);
        // Ownership must come from DATA, not this deliberately unrelated group.
        append(b,group(999,6,record("NAVM",40,p,o.deletedNav?0x20u:0)));}
    if(o.pack){Bytes data(o.targetSize),p;u32(data,4,o.baseTarget?20:30);sub(p,"PTDT",data);append(b,record("PACK",50,p));}
    if(o.malformed==8)b.push_back(1);
    if(o.malformed==9)u32(b,4,0xffffffffu);
    return b;
}
const std::string path="/tmp/falloutquest-resident-authored-admission-"+std::to_string(getpid())+".esm";
void write(const Bytes& b){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());}
void scriptDependencies(){
    std::atomic<bool> cancel{false};std::string error;
    fo3scene::ResidentAuthoredAdmission out;out.cell=999;out.affected={888};
    auto master=[&](const char* type,const char* tag,const Bytes& value,int encoding=0,uint32_t flags=0){
        Bytes p;
        if(encoding==2){sub(p,"XXXX",word(value.size()));sub(p,tag,{});append(p,value);}
        else sub(p,tag,value);
        if(encoding==1){uLongf n=compressBound(p.size());Bytes compressed(n+4);u32(compressed,0,p.size());
            assert(compress2(compressed.data()+4,&n,p.data(),p.size(),Z_BEST_SPEED)==Z_OK);
            compressed.resize(n+4);p=std::move(compressed);flags|=0x40000u;}
        auto b=fixture();append(b,group(12,1,group(11,6,record(type,60,p,flags))));write(b);
    };
    auto rejects=[&](const std::string& field){
        assert(!fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));
        assert(error.find(field)!=std::string::npos&&error.find("60")!=std::string::npos);
        assert(out.cell==999&&out.affected==std::vector<uint32_t>{888});
    };
    auto admits=[&](){fo3scene::ResidentAuthoredAdmission accepted;
        const bool valid=fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},accepted,cancel,error);
        if(!valid)std::cerr<<"Expected admission: "<<error<<'\n';
        assert(valid);
        assert(accepted.Matches(10,{30})&&error.empty());};
    for(const char* type:{"SCPT","QUST","INFO","PACK","IDLE","PERK","TERM","MGEF","REFR","ACHR","ACRE","UNKN"})
      for(uint32_t target:{20u,30u})for(int encoding:{0,1,2}){
        master(type,"SCRO",word(target),encoding);rejects("SCRO");
        assert(error.find(target==30?"(reference)":"(base)")!=std::string::npos);
      }
    // Only the exact original SCPT executing the transaction may own its own
    // directly affected reference's SCRO. A different script, an INFO/QUST,
    // and a reference to an affected BASE must remain rejected.
    for(int encoding:{0,1,2}){
        master("SCPT","SCRO",word(30),encoding);
        fo3scene::ResidentAuthoredAdmission admitted;
        assert(fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},admitted,cancel,error,{60}));
        assert(admitted.Matches(10,{30}));
        assert(!fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},admitted,cancel,error,{61}));
        assert(error.find("unowned incoming script/condition dependency: SCRO")!=std::string::npos);
        master("SCPT","SCRO",word(20),encoding);
        assert(!fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},admitted,cancel,error,{60}));
        assert(error.find("(base)")!=std::string::npos);
        master("INFO","SCRO",word(30),encoding);
        assert(!fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},admitted,cancel,error,{60}));
        assert(error.find("unowned incoming script/condition dependency: SCRO")!=std::string::npos);
    }
    for(uint32_t size:{20u,24u,28u})for(uint32_t offset:{12u,16u,24u}){
        if(offset+4>size)continue;
        for(uint32_t target:{20u,30u})for(int encoding:{0,1,2}){
            Bytes c(size);u32(c,offset,target);u32(c,8,65535); // Unknown arity is not an exemption.
            master("INFO","CTDA",c,encoding);rejects("CTDA +"+std::to_string(offset));
        }
    }
    Bytes c(28);c[0]=4;u32(c,4,30);master("QUST","CTDA",c);rejects("CTDA comparison-global");
    c[0]=0;master("QUST","CTDA",c);admits(); // A literal comparison is not a global FormID.
    for(const char* tag:{"SCRO","SCRV","CTDA"}){
        master("UNKN",tag,Bytes(3));rejects(tag);
        master("UNKN",tag,Bytes(3),0,0x20);admits();
    }
    for(uint32_t size:{0u,19u,21u,25u,29u,32u}){master("PACK","CTDA",Bytes(size));rejects("CTDA");}
    for(int encoding:{0,1,2}){
        master("SCPT","SCRO",word(99),encoding);admits();
        for(uint32_t local:{20u,30u}){master("SCPT","SCRV",word(local),encoding);admits();}
        master("INFO","CTDA",Bytes(28),encoding);admits();
        for(uint32_t member:{20u,30u}){
            master("FLST","LNAM",word(member),encoding);rejects("FLST LNAM");
            assert(error.find(member==30?"(reference)":"(base)")!=std::string::npos);
        }
        for(uint32_t member:{0u,60u,99u}){master("FLST","LNAM",word(member),encoding);admits();}
    }
    for(uint32_t size:{0u,1u,3u,5u,8u}){
        master("FLST","LNAM",Bytes(size));rejects("FLST LNAM");
        master("FLST","LNAM",Bytes(size),0,0x20);admits();
    }
    // LNAM has other meanings outside FLST; it is not a global member tag.
    for(const char* type:{"INFO","SCPT","UNKN"}){
        master(type,"LNAM",word(30));admits();
    }
    for(bool cycle:{false,true})for(bool leafFirst:{false,true}){
        auto b=fixture();Bytes outer,inner,source;
        sub(outer,"LNAM",word(61));sub(inner,"LNAM",word(30));
        if(cycle)sub(inner,"LNAM",word(60));
        sub(source,"SCRO",word(60));append(b,record("SCPT",80,source));
        append(b,record("FLST",leafFirst?61:60,leafFirst?inner:outer));
        append(b,record("FLST",leafFirst?60:61,leafFirst?outer:inner));write(b);
        assert(!fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));
        assert(error.find("FLST LNAM from FLST 61 to 30")!=std::string::npos);
        assert(out.Matches(999,{888}));
        // Remove the affected member but retain the cycle and script consumer.
        inner.clear();sub(inner,"LNAM",word(60));b=fixture();
        append(b,record("FLST",60,outer));append(b,record("FLST",61,inner));
        append(b,record("SCPT",80,source));write(b);admits();
    }
    {auto b=fixture();Bytes p;sub(p,"LNAM",word(99));sub(p,"LNAM",word(99));
        append(b,record("FLST",60,p));write(b);admits();}
    {auto b=fixture();Bytes p;sub(p,"EDID",text("EmptyFixtureList"));
        append(b,record("FLST",60,p));write(b);admits();}
    auto b=fixture();Bytes p;sub(p,"SCRO",word(99));sub(p,"SCRO",word(99));append(b,record("SCPT",60,p));write(b);admits();
    b=fixture();p={1,2,3};append(b,record("UNKN",60,p));write(b);rejects("UNKN");
    b=fixture();append(b,record("SCPT",60,{1,2,3,4},0x40000));write(b);rejects("SCPT");
    std::cout<<"Incoming SCRO/CTDA operands cover all record classes, foreign scopes and raw encodings; SCRV stays local\n";
}
void original(const std::string& master){
    // Resolve fixtures from Bethesda's records; no invented original form IDs.
    FILE* f=std::fopen(master.c_str(),"rb");assert(f);const auto size=fo3esm::FileSize(f);
    uint32_t cell=0;std::unordered_map<uint32_t,std::string> types;
    std::unordered_set<uint32_t> linkedTargets,interiorCells,scriptTargets;
    while(ftello(f)<size){
        uint8_t h[24];assert(fo3esm::ReadExact(f,h,24));const auto n=fo3esm::ReadU32(h+4);
        if(!std::memcmp(h,"GRUP",4))continue;
        const auto id=fo3esm::ReadU32(h+12);const std::string type(reinterpret_cast<char*>(h),4);
        if(type=="STAT"||type=="LIGH"||type=="DOOR")types[id]=type;
        if(type=="CELL"){
            Bytes b;assert(fo3esm::ReadPayloadCurrent(f,n,fo3esm::ReadU32(h+8),b));
            fo3esm::WalkSubrecords(b,[&](const char* t,const uint8_t* p,uint32_t count){
                if(!std::memcmp(t,"EDID",4)&&fo3esm::ZString(p,count)=="MegatonTheBrassLantern")cell=id;
                if(!std::memcmp(t,"DATA",4)&&count&&(p[0]&1))interiorCells.insert(id);
            });
        }else if((type=="REFR"||type=="ACHR"||type=="ACRE")&&!(fo3esm::ReadU32(h+8)&0x20u)){
            Bytes b;assert(fo3esm::ReadPayloadCurrent(f,n,fo3esm::ReadU32(h+8),b));
            fo3esm::WalkSubrecords(b,[&](const char* t,const uint8_t* p,uint32_t count){
                if(!std::memcmp(t,"XLKR",4)&&count==4)linkedTargets.insert(fo3esm::ReadU32(p));
            });
        }else if(!(fo3esm::ReadU32(h+8)&0x20u)){
            Bytes b;assert(fo3esm::ReadPayloadCurrent(f,n,fo3esm::ReadU32(h+8),b));
            fo3esm::WalkSubrecords(b,[&](const char* t,const uint8_t* p,uint32_t count){
                if(!std::memcmp(t,"SCRO",4)&&count==4)scriptTargets.insert(fo3esm::ReadU32(p));
            });
        }else assert(!fseeko(f,n,SEEK_CUR));
    }
    assert(cell);assert(!fseeko(f,0,SEEK_SET));
    struct Frame{uint64_t end;uint32_t label,type;};std::vector<Frame> stack;
    std::unordered_map<uint32_t,uint32_t> refs;
    std::vector<std::pair<uint32_t,uint32_t>> incomingFixtures;
    std::vector<std::pair<uint32_t,uint32_t>> scriptFixtures;
    while(ftello(f)<size){
        const auto start=ftello(f);while(!stack.empty()&&uint64_t(start)>=stack.back().end)stack.pop_back();
        uint8_t h[24];assert(fo3esm::ReadExact(f,h,24));const auto n=fo3esm::ReadU32(h+4);
        if(!std::memcmp(h,"GRUP",4)){
            stack.push_back({uint64_t(start)+n,fo3esm::ReadU32(h+8),fo3esm::ReadU32(h+12)});continue;
        }
        uint32_t owner=0;for(auto g=stack.rbegin();g!=stack.rend();++g)
            if(g->type==6||g->type==8||g->type==9||g->type==10){owner=g->label;break;}
        const auto id=fo3esm::ReadU32(h+12),flags=fo3esm::ReadU32(h+8);
        if(!std::memcmp(h,"REFR",4)&&(owner==cell||linkedTargets.count(id)||interiorCells.count(owner))){
            Bytes b;assert(fo3esm::ReadPayloadCurrent(f,n,fo3esm::ReadU32(h+8),b));
            uint32_t base=0;bool simple=true;
            fo3esm::WalkSubrecords(b,[&](const char* t,const uint8_t* p,uint32_t count){
                if(!std::memcmp(t,"NAME",4)&&count==4)base=fo3esm::ReadU32(p);
                const std::string tag(t,4);
                if(tag!="NAME"&&tag!="DATA"&&tag!="EDID"&&tag!="XSCL"&&tag!="XESP"&&
                    tag!="XOWN"&&tag!="XRNK"&&tag!="XGLB")simple=false;
            });
            if(owner==cell)refs[id]=base;
            const auto type=types.find(base);
            if(simple&&!(flags&(0x20u|0x8000u))&&interiorCells.count(owner)&&linkedTargets.count(id)&&
                type!=types.end()&&(type->second=="STAT"||type->second=="LIGH"))
                incomingFixtures.emplace_back(owner,id);
            if(simple&&!(flags&(0x20u|0x8000u))&&interiorCells.count(owner)&&
                (scriptTargets.count(id)||scriptTargets.count(base))&&type!=types.end()&&
                (type->second=="STAT"||type->second=="LIGH"))scriptFixtures.emplace_back(owner,id);
        }else assert(!fseeko(f,n,SEEK_CUR));
    }
    std::fclose(f);
    bool light=false,door=false,nav=false;std::atomic<bool> cancel{false};
    std::unordered_map<std::string,bool> logged;
    fo3scene::ResidentAuthoredAdmission out;std::string error;
    for(const auto& ref:refs){
        const auto type=types.find(ref.second);if(type==types.end())continue;
        if((type->second=="LIGH"&&light)||(type->second=="DOOR"&&door)||(type->second=="STAT"&&nav))continue;
        const bool admitted=fo3scene::PrepareResidentAuthoredAdmission(master,cell,{ref.first},out,cancel,error);
        if(!admitted&&!logged[type->second]){
            std::cout<<"Original "<<type->second<<" guard: "<<error<<std::endl;logged[type->second]=true;
        }
        if(type->second=="LIGH"&&admitted)light=true;
        if(type->second=="DOOR"){assert(!admitted);door=true;}
        if(type->second=="STAT"&&error=="geometry mutation requires navigation reconciliation")nav=true;
    }
    if(!light||!door||!nav)std::cerr<<"Original coverage light="<<light<<" door="<<door<<" nav="<<nav<<" last="<<error<<"\n";
    assert(light&&door&&nav);
    bool incoming=false;
    for(const auto& fixture:incomingFixtures){
        const auto previous=out;
        assert(!fo3scene::PrepareResidentAuthoredAdmission(master,fixture.first,{fixture.second},out,cancel,error));
        assert(out.cell==previous.cell&&out.affected==previous.affected);
        if(error.find("unowned incoming authored link: XLKR")!=std::string::npos){incoming=true;break;}
    }
    assert(incoming);
    std::cout<<"Original incoming XLKR target resolved from authored records and rejected\n";
    bool script=false;
    for(const auto& fixture:scriptFixtures){
        const auto previous=out;
        assert(!fo3scene::PrepareResidentAuthoredAdmission(master,fixture.first,{fixture.second},out,cancel,error));
        assert(out.cell==previous.cell&&out.affected==previous.affected);
        if(error.find("unowned incoming script/condition dependency: SCRO")!=std::string::npos){
            script=true;std::cout<<"Original incoming dependency: "<<error<<'\n';break;
        }
    }
    if(!script)std::cerr<<"No original SCRO fixture reached its guard; candidates="<<scriptFixtures.size()<<" last="<<error<<'\n';
    assert(script);
    std::cout<<"Original Brass Lantern: light guard admitted, load door and navmeshed geometry rejected\n";
}
int main(int argc,char** argv){
    scriptDependencies();
    std::atomic<bool> cancel{false};std::string error;
    fo3scene::ResidentAuthoredAdmission out;out.cell=999;out.affected={888};
    auto rejected=[&](Options o){write(fixture(o));
        assert(!fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));
        assert(!error.empty()&&out.cell==999&&out.affected==std::vector<uint32_t>{888});};
    for(const char* type:{"NPC_","CREA","DOOR","FURN","IDLM","CONT","MISC","SCOL","UNKN"}){
        Options o;o.baseType=type;rejected(o);
    }
    for(const char* type:{"ACHR","ACRE","UNKN"}){Options o;o.refType=type;rejected(o);}
    for(const char* tag:{"XTEL","XEMI","XPRM","XRMR","XLKR","UNKN"}){Options o;o.extra=tag;rejected(o);}
    for(uint32_t flags:{0x20u,0x8000u}){
        Options o;o.refFlags=flags;rejected(o);o.refFlags=0;o.baseFlags=flags;rejected(o);
    }
    for(int i=1;i<=9;++i){Options o;o.malformed=i;rejected(o);}
    {Options o;o.owner=11;rejected(o);}
    {Options o;o.world=11;rejected(o);}
    {Options o;o.duplicate=true;rejected(o);}
    {Options o;o.missing=true;rejected(o);}
    {Options o;o.script=true;rejected(o);}
    {Options o;o.interior=false;rejected(o);}
    {Options o;o.nav=true;rejected(o);}
    {Options o;o.pack=true;rejected(o);}
    {Options o;o.pack=true;o.targetSize=16;rejected(o);}
    {Options o;o.pack=true;o.baseTarget=true;rejected(o);}
    {Options o;o.pack=true;o.targetSize=13;rejected(o);}
    {Options o;o.light=true;o.lightModel=true;o.nav=true;rejected(o);}
    // Incoming dependencies anywhere in the master, not only resident meshes.
    for(const char* kind:{"REFR","ACHR","ACRE"})for(const char* tag:{"XLKR","XTEL","XEMI","XESP"}){
        Options o;o.incoming=tag;o.incomingType=kind;rejected(o);
        assert(error.find(tag)!=std::string::npos&&error.find("60")!=std::string::npos);
        o.incomingCell=10;rejected(o); // Same CELL is not sufficient ownership.
        o.incomingCell=11;o.incomingWorld=12;rejected(o);
        o.incomingWorld=0;o.incomingSize=1;rejected(o);
        o.incomingSize=-1;o.duplicateLink=true;rejected(o);
        o.duplicateLink=false;o.compressedIncoming=true;rejected(o);
        o.compressedIncoming=false;o.extendedIncoming=true;rejected(o);
    }
    write(fixture());cancel=true;
    assert(!fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));cancel=false;
    for(const auto& ids:std::vector<std::vector<uint32_t>>{{},{0},{30,30},{31,30},{31}}){
        assert(!fo3scene::PrepareResidentAuthoredAdmission(path,10,ids,out,cancel,error));
        assert(out.cell==999);
    }
    assert(fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));
    assert(error.empty()&&out.Matches(10,{30})&&!out.Matches(11,{30})&&!out.Matches(10,{31}));
    for(const char* tag:{"XLKR","XTEL","XEMI","XESP"}){
        Options o;o.incoming=tag;o.incomingTarget=99;write(fixture(o));
        assert(fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));
        o.compressedIncoming=true;write(fixture(o));
        assert(fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));
        o.compressedIncoming=false;o.extendedIncoming=true;write(fixture(o));
        assert(fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));
        o.incomingTarget=30;o.incomingFlags=0x20;o.incomingSize=1;write(fixture(o));
        assert(fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));
    }
    {Options o;o.incoming="XESP";o.incomingCell=10;write(fixture(o));
        assert(fo3scene::PrepareResidentAuthoredAdmission(path,10,{30,60},out,cancel,error));
        assert(out.Matches(10,{30,60}));
        o.incomingCell=11;write(fixture(o));
        assert(!fo3scene::PrepareResidentAuthoredAdmission(path,10,{30,60},out,cancel,error));
        assert(out.Matches(10,{30,60}));
        write(fixture());assert(fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));
    }
    {Options o;o.light=true;o.nav=true;write(fixture(o));
        assert(fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));}
    // Original XMarker is a nonvisual STAT; the navmesh is unchanged because
    // its enable state does not add renderable geometry. Ordinary STATs reject.
    {Options o;o.marker=true;o.nav=true;write(fixture(o));
        assert(fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));}
    // Shared XMarker BASE appears in other conditions as a numeric argument:
    // it is not the changed reference. A CTDA hit on REFR must still reject.
    {Options o;o.marker=true;o.nav=true;
        Bytes content=fixture(o),ctda(20);u32(ctda,16,20);
        Bytes condition;sub(condition,"CTDA",ctda);
        append(content,record("INFO",70,condition));write(content);
        assert(fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));
        u32(ctda,16,30);condition.clear();sub(condition,"CTDA",ctda);
        content=fixture(o);append(content,record("INFO",70,condition));write(content);
        assert(!fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));
        assert(error.find("CTDA +16")!=std::string::npos);
    }
    {Options o;o.nav=true;o.deletedNav=true;write(fixture(o));
        assert(fo3scene::PrepareResidentAuthoredAdmission(path,10,{30},out,cancel,error));}
    // Complete logical set: a valid visual cannot hide an unresolvable no-mesh ID.
    assert(!fo3scene::PrepareResidentAuthoredAdmission(path,10,{30,99},out,cancel,error));
    assert(out.Matches(10,{30}));
    if(argc>1)original(argv[1]);
    std::remove(path.c_str());std::cout<<"Resident authored admission tests passed\n";
}
