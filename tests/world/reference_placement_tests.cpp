#include "data/fo3-install-paths.h"
#include "world/fo3-transition.h"
#include "world/fo3-worldspace-runtime.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <iostream>
#include <unistd.h>
#define Q71_LOGI(...) std::fprintf(stderr,__VA_ARGS__)
#define Q71_LOGE(...) std::fprintf(stderr,__VA_ARGS__)
#include "world/fo3-cell-interior.inc"
namespace {
using Bytes=std::vector<uint8_t>;
void put(Bytes& b,size_t p,uint32_t v){if(b.size()<p+4)b.resize(p+4);for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(v>>(8*i));}
Bytes word(uint32_t v){Bytes b(4);put(b,0,v);return b;}
Bytes str(const char* s){return Bytes(s,s+std::strlen(s)+1);}
void sub(Bytes& b,const char* tag,const Bytes& v){b.insert(b.end(),tag,tag+4);b.push_back(uint8_t(v.size()));b.push_back(uint8_t(v.size()>>8));b.insert(b.end(),v.begin(),v.end());}
void add(Bytes& a,const Bytes& b){a.insert(a.end(),b.begin(),b.end());}
Bytes rec(const char* tag,uint32_t id,const Bytes& data,uint32_t flags=0){Bytes b(24);std::memcpy(b.data(),tag,4);put(b,4,uint32_t(data.size()));put(b,8,flags);put(b,12,id);add(b,data);return b;}
Bytes group(uint32_t id,uint32_t kind,const Bytes& data){Bytes b(24);std::memcpy(b.data(),"GRUP",4);put(b,4,uint32_t(data.size()+24));put(b,8,id);put(b,12,kind);add(b,data);return b;}
Bytes ref(uint32_t parent=0){Bytes b;sub(b,"NAME",word(300));Bytes pose(24);const float p[6]={100,100,10,0,0,0};std::memcpy(pose.data(),p,sizeof(p));sub(b,"DATA",pose);if(parent){Bytes x(8);put(x,0,parent);sub(b,"XESP",x);}return b;}
const Fo3WorldPlacement* find(const std::vector<Fo3WorldPlacement>& p,uint32_t id){for(const auto& r:p)if(r.refFormId==id)return &r;return nullptr;}
void synthetic(const std::string& path){
 Bytes base;sub(base,"EDID",str("OriginalFormatFixture"));sub(base,"MODL",str("architecture\\fixture.nif"));
 Bytes refs;add(refs,rec("REFR",400,ref()));add(refs,rec("REFR",401,ref(),0x800));add(refs,rec("REFR",402,ref(999)));add(refs,rec("REFR",403,ref(),0x20));
 Bytes cell;sub(cell,"EDID",str("FixturePersistent"));Bytes children;add(children,rec("CELL",200,cell));add(children,group(200,6,group(200,8,refs)));
 Bytes grid;sub(grid,"EDID",str("FixtureGrid"));sub(grid,"XCLC",Bytes(8));add(children,rec("CELL",201,grid));
 Bytes world;sub(world,"EDID",str("FixtureWorld"));Bytes esm=rec("TES4",0,{});add(esm,rec("STAT",300,base));add(esm,rec("WRLD",100,world));add(esm,group(100,1,children));
 {std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(esm.data()),esm.size());assert(f);}
 std::vector<Fo3WorldPlacement> before,after;
 assert(LoadFo3WorldspacePlacements({100,200,100,100,0},before));
 assert(find(before,400)&&!find(before,401)&&!find(before,403));
 fo3world::ReferenceStateSnapshot state;state.states[400]=fo3world::ReferenceEnableState::Disabled;state.states[401]=fo3world::ReferenceEnableState::Enabled;state.states[402]=fo3world::ReferenceEnableState::Unavailable;state.states[403]=fo3world::ReferenceEnableState::Enabled;
 assert(LoadFo3WorldspacePlacements({100,200,100,100,0},after,&state));
 assert(!find(after,400)&&find(after,401)&&!find(after,402)&&!find(after,403));
 assert(find(after,401)->x==100&&find(after,401)->y==100&&find(after,401)->z==10);
 const auto collision=SelectFo3AuthoredCollisionPlacements(after);assert(find(collision,401)&&!find(collision,400));
 // Concurrent CELL jobs keep distinct immutable snapshots over the same index.
 const auto first=std::make_shared<const fo3world::ReferenceStateSnapshot>(state);
 state.states[400]=fo3world::ReferenceEnableState::Enabled;
 state.states[401]=fo3world::ReferenceEnableState::Disabled;
 const auto second=std::make_shared<const fo3world::ReferenceStateSnapshot>(state);
 auto job=[](std::shared_ptr<const fo3world::ReferenceStateSnapshot> snapshot){std::vector<Fo3WorldPlacement> result;
   assert(LoadFo3WorldspacePlacements({100,200,100,100,0},result,snapshot.get()));return result;};
 auto a=std::async(std::launch::async,job,first);auto b=std::async(std::launch::async,job,second);
 const auto firstResult=a.get(),secondResult=b.get();
 assert(!find(firstResult,400)&&find(firstResult,401));assert(find(secondResult,400)&&!find(secondResult,401));
 state=*first;

 // Interior selection uses the identical snapshot before model resolution.
 assert(LoadFo3InteriorCellPlacementsQ1700(200,after,&state));
 assert(find(after,401)&&!find(after,400)&&!find(after,402)&&!find(after,403));
 assert(LoadFo3WorldspacePlacements({100,200,100,100,0},after));assert(find(after,400)&&!find(after,401));
}
void original(){
 constexpr uint32_t refId=0x1e99f;std::vector<Fo3WorldPlacement> before,after;
 const Fo3WorldspaceSelection selection{0x3c,0x2db4,-62729.95703125f,-66686.6640625f,0};
 assert(LoadFo3WorldspacePlacements(selection,before)&&!find(before,refId));
 fo3world::ReferenceStateSnapshot state;state.states[refId]=fo3world::ReferenceEnableState::Enabled;
 assert(LoadFo3WorldspacePlacements(selection,after,&state));const auto* p=find(after,refId);assert(p);
 assert(p->baseFormId==0x1e9aa&&p->modelPath.size()>4&&p->x==selection.gameX&&p->y==selection.gameY&&p->z==17706.421875f);
 assert(find(SelectFo3AuthoredCollisionPlacements(after),refId));
 state.states[refId]=fo3world::ReferenceEnableState::Disabled;
 assert(LoadFo3WorldspacePlacements(selection,after,&state)&&!find(after,refId));
 std::cout<<"Original MS11 detonator model/pose selection and collision input honor saved snapshots\n";
}
}
int main(int argc,char** argv){
 const auto directory="/tmp/fq-reference-placement-"+std::to_string(getpid());std::filesystem::create_directory(directory);
 assert(setenv("FALLOUTQUEST_DATA_ROOT",directory.c_str(),1)==0);
 const auto path=directory+"/Fallout3.esm";
 if(argc>1){std::filesystem::create_symlink(std::filesystem::absolute(argv[1]),path);original();}else synthetic(path);
 std::filesystem::remove(path);std::filesystem::remove(directory);
 std::cout<<"Reference placement snapshots passed\n";
}
