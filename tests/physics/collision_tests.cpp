#include <chrono>
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include "../../app/src/main/cpp/physics/fo3-collision-runtime.cpp"
// Test the real parser, assembly, publication, resolver and occlusion paths.
// Only platform GL, LAND and asset IO are replaced; no Bethesda fixtures ship.
static std::map<std::string,std::vector<uint8_t>> assets;
static std::string assetRoot;
bool LoadFalloutMeshFile(const std::string& model,std::vector<uint8_t>& bytes,std::string* resolved) {
    auto it=assets.find(model);
    if(it!=assets.end()) bytes=it->second;
    else {
        std::string p=model; for(char& c:p){if(c=='\\')c='/';c=std::tolower(static_cast<unsigned char>(c));}
        std::ifstream f(assetRoot+"/"+p,std::ios::binary);
        bytes.assign(std::istreambuf_iterator<char>(f),{});
    }
    if(resolved)*resolved=model;
    return !bytes.empty();
}
void PumpFo3AndroidEventsQ1860() {}
bool ConsumeFo3PlayerResetQ74() {return false;}
bool SampleFo3TerrainGroundQ77(float,float,float*) {return false;}
bool IsFo3TerrainGroundingActiveQ77(){return false;}
const std::vector<Fo3TerrainCellQ76>& GetFo3TerrainQ76(){static std::vector<Fo3TerrainCellQ76> cells;return cells;}
static void u32(std::vector<uint8_t>& b,uint32_t v){for(int i=0;i<4;++i)b.push_back(v>>(8*i));}
static void u16(std::vector<uint8_t>& b,uint16_t v){b.push_back(v);b.push_back(v>>8);}
static void f32(std::vector<uint8_t>& b,float v){uint32_t n;std::memcpy(&n,&v,4);u32(b,n);}
static void str(std::vector<uint8_t>& b,const std::string& v){u32(b,v.size());b.insert(b.end(),v.begin(),v.end());}
static std::vector<uint8_t> packedNif(uint8_t layer=1,bool wall=false) {
    // Collision-only NIF: visual support is deliberately absent.
    std::vector<uint8_t> co;u32(co,0xffffffff);u16(co,1);u32(co,1);
    std::vector<uint8_t> body;u32(body,2);body.insert(body.end(),{layer,0,0,0});
    std::vector<uint8_t> shape(52);u32(shape,3);
    std::vector<uint8_t> data;u32(data,2);
    for(auto v:{0,1,2,0,0,2,3,0})u16(data,v);
    u32(data,4);data.push_back(0);
    const std::vector<float> vertices = wall
        ? std::vector<float>{10,-10,0,10,10,0,10,10,30,10,-10,30}
        : std::vector<float>{-10,-10,0,10,-10,0,10,10,0,-10,10,0};
    for(float v:vertices)f32(data,v);
    u16(data,1);data.insert(data.end(),{layer,0,0,0});u32(data,4);u32(data,0);
    std::vector<std::vector<uint8_t>> blocks{co,body,shape,data};
    std::string line="Gamebryo File Format, Version 20.2.0.7\n";std::vector<uint8_t>b(line.begin(),line.end());
    u32(b,0x14020007);b.push_back(1);u32(b,11);u32(b,4);u32(b,34);b.insert(b.end(),{0,0,0});u16(b,4);
    for(auto t:{"bhkCollisionObject","bhkRigidBody","bhkPackedNiTriStripsShape","hkPackedNiTriStripsData"})str(b,t);
    for(unsigned i=0;i<4;++i)u16(b,i);
    for(auto& block:blocks)u32(b,block.size());
    u32(b,0);u32(b,0);u32(b,0);for(auto& block:blocks)b.insert(b.end(),block.begin(),block.end());return b;
}
static Fo3WorldPlacement placement(uint32_t ref,const std::string& model,const std::string& type="STAT",float x=0) {
    Fo3WorldPlacement p;p.refFormId=ref;p.modelPath=model;p.baseRecordType=type;p.x=x;return p;
}
static uint64_t prepare(const std::vector<Fo3WorldPlacement>& ps,bool exterior,uint32_t cell) {
    ResetFo3CollisionSceneCache();ConfigureFo3CollisionPolicyQ710(ps);
    for(auto& p:ps){size_t tris;assert(PrimeFo3CollisionPlacementCacheQ1820(p,0,0,0,0,0,70,&tris));}
    uint64_t token=0;assert(PrepareFo3CollisionSnapshotQ1930(ps,0,0,0,0,0,70,&token,exterior,cell));return token;
}
static void publish(uint64_t t){assert(PublishFo3CollisionSnapshotQ1930(t,nullptr));assert(IsFo3PlayerCollisionReadyQ6G());}
int main(int argc,char**argv) {
    const std::string nonMegaton="Dungeons\\Office\\Room\\Wall.NIF",megaton="Architecture\\Megaton\\interior\\Wall.NIF",item="Clutter\\Bottle.NIF",trigger="Effects\\Trigger.NIF";
    assets[nonMegaton]=packedNif();assets[megaton]=packedNif();assets[item]=packedNif();assets[trigger]=packedNif(12);
    for(auto layer:{11,12,15,16})assert(!Fo3HavokLayerBlocksPlayerQ714(layer));
    assert(Fo3HavokLayerBlocksPlayerQ714(1));
    auto a=placement(1,nonMegaton),b=placement(2,megaton),d=placement(3,item,"MISC"),t=placement(4,trigger);
    auto selected=SelectFo3AuthoredCollisionPlacements({a,a,b,d,t});
    assert(selected.size()==4 && selected[0].modelPath==nonMegaton);
    auto token=prepare(selected,false,100);
    assert(!IsFo3PlayerCollisionReadyQ6G());publish(token);
    assert(!gExteriorAllBhksQ78A && gWorldTriangles.size()==4 && gPlacementCount==2);
    assert(gQ1820NoCollisionModels.count(item)==0); // policy must not poison asset cache
    float x,z,y;assert(ResolveFo3PlayerMotionQ6G(0,0,0,0,0,&x,&z,&y));assert(std::fabs(y)<.01f);
    assert(HasFo3InteractionOccluder(0,1,0,0,-1,0,2,0));
    SetFo3CollectedCollisionRefs({1});assert(gWorldTriangles.size()==2);
    // Preparing replacement retains the old triangles until publish.
    token=prepare({placement(5,item,"STAT",2000)},true,101);
    assert(gWorldTriangles.size()==2 && !gExteriorAllBhksQ78A);publish(token);
    assert(gExteriorAllBhksQ78A && gWorldTriangles.size()==2);
    assert(gSurfaceSourcesQ722.begin()->second.refFormId==5);
    token=prepare({a},false,102);assert(gExteriorAllBhksQ78A);
    assert(PublishFo3CollisionSnapshotQ1930(token,nullptr));assert(!IsFo3PlayerCollisionReadyQ6G());
    assert(!gExteriorAllBhksQ78A && gWorldTriangles.empty()); // collected ref reapplied
    SetFo3CollectedCollisionRefs({});token=prepare({b},false,103);publish(token);
    assert(gWorldTriangles.size()==2 && gSurfaceSourcesQ722.begin()->second.refFormId==2);
    ConfigureFo3CollisionPolicyQ710({d,placement(6,item,"STAT")});assert(ShouldLoadFo3StaticCollisionModelQ710(item));
    ConfigureFo3CollisionPolicyQ710({placement(6,item,"WEAP")});assert(!ShouldLoadFo3StaticCollisionModelQ710(item));
    // A CELL with >256 valid authored placements must not silently truncate.
    std::vector<Fo3WorldPlacement> large;for(unsigned i=0;i<300;++i)large.push_back(placement(1000+i,nonMegaton,"STAT",i*2000.f));
    publish(prepare(large,false,104));assert(gPlacementCount==300 && gWorldTriangles.size()==600);
    // Exercise floor contact and an authored structural wall through the unchanged
    // interior player resolver. Wall at x=1m, capsule radius=.26m.
    assets["Wall.nif"]=packedNif(1,true);
    publish(prepare({b,placement(9,"Wall.nif")},false,105));
    assert(ResolveFo3PlayerMotionQ6G(.65f,0,.65f,0,0,&x,&z,&y));
    assert(ResolveFo3PlayerMotionQ6G(.65f,0,.95f,0,y,&x,&z,&y));
    assert(x<.8f && std::fabs(y)<.01f);
    assert(HasFo3InteractionOccluder(0,1,0,1,0,0,2,0));
    float dy,nx,ny,nz;uint32_t contacts,candidates;
    assert(ResolveFo3DynamicBoxQ225(3,0,.2f,0,0,.05f,0,.1f,.1f,.1f,
        1,0,0,0,1,0,0,0,1,.18f,&x,&dy,&z,&nx,&ny,&nz,&contacts,&candidates));
    assert(dy>0 && contacts>0); // loose-object physics still contacts authored floor
    // Failed/discarded replacements must retain the currently published world.
    auto oldTriangles=gWorldTriangles.size();
    token=prepare({a},false,106);DiscardFo3CollisionSnapshotQ1930(token);
    assert(!PublishFo3CollisionSnapshotQ1930(token,nullptr));
    assert(gWorldTriangles.size()==oldTriangles && IsFo3PlayerCollisionReadyQ6G());
    // Synchronous legacy entry points are generic too.
    ConfigureFo3CollisionPolicyQ710({a,d});SetNextFo3CollisionExteriorModeQ1931(false);
    assert(InitializeFo3CollisionOverlay({a,d},0,0,0,0,0,70));assert(gWorldTriangles.size()==2);
    if(argc==3 || argc==4) {
        assetRoot=argv[1];std::ifstream manifest(argv[2]);std::vector<Fo3WorldPlacement> ps;Fo3WorldPlacement p;
        while(manifest>>p.refFormId>>p.baseRecordType>>p.x>>p.y>>p.z>>p.rx>>p.ry>>p.rz>>p.scale){manifest.get();std::getline(manifest,p.modelPath);ps.push_back(p);}
        auto start=std::chrono::steady_clock::now();publish(prepare(ps,false,argc==4 ? std::stoul(argv[3],nullptr,16) : 0x17f37));
        auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
        std::cout<<"ORIGINAL CELL input="<<ps.size()<<" static="<<gPlacementCount<<" triangles="<<gWorldTriangles.size()<<" ready="<<IsFo3PlayerCollisionReadyQ6G()<<" hostMs="<<ms<<"\n";
        assert(!gWorldTriangles.empty() && gPlacementCount>0);
    }
    std::cout<<"Authored collision tests passed\n";
}
