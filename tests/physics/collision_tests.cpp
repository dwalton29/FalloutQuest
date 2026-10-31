// Match renderer order: value-only resident resources must not import the
// transition collision-policy macro before the original parser definition.
#include "world/fo3-resident-resources.h"
#ifdef LoadFo3NifCollisionShapesQ6F
#error Resident resource DTOs must not define collision policy macros
#endif
#include <chrono>
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <cstdlib>
#include <new>
#include "../../app/src/main/cpp/physics/fo3-collision-runtime.cpp"
#include "physics/fo3-collision-token.h"
#include "world/fo3-resident-preparation-lane.h"
// Mutation publication must consist only of swaps: simulate allocation failure
// during both forward publication and rollback in the real collision runtime.
static bool forbidAllocations = false;
void* operator new(std::size_t size) {
    if (forbidAllocations) throw std::bad_alloc();
    if (void* p = std::malloc(size ? size : 1u)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

// Test the real parser, assembly, publication, resolver and occlusion paths.
// Only platform GL, LAND and asset IO are replaced; no Bethesda fixtures ship.
static std::map<std::string,std::vector<uint8_t>> assets;
static std::string assetRoot;
static std::atomic<bool>* cancelPrivateAsset=nullptr;
bool LoadFalloutMeshFile(const std::string& model,std::vector<uint8_t>& bytes,std::string* resolved) {
    if(cancelPrivateAsset){cancelPrivateAsset->store(true,std::memory_order_release);cancelPrivateAsset=nullptr;}
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
static bool terrainAvailable=false;
static float terrainOffset=4.25f;
bool SampleFo3TerrainFootstepMaterial(float,float,uint32_t*) {return false;}
bool SampleFo3TerrainGroundQ77(float,float,float* out) {
    if(!terrainAvailable)return false;
    *out=terrainOffset;return true;
}
bool IsFo3TerrainGroundingActiveQ77(){return false;}
void SynchronizeFo3TerrainPhysics(float,float){}
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
// Capture on the render thread before a worker sees the immutable input. Tests
// can override the revision/collection input to exercise rejection paths.
static bool prepareMutation(const std::vector<Fo3WorldPlacement>& ps,
        float x,float y,float z,float forward,float floor,float units,
        const std::unordered_set<uint32_t>& collected,uint64_t revision,
        uint64_t* token,bool exterior,uint32_t cell) {
    auto state=CaptureFo3CollisionMutationState();
    state.collected=collected;state.revision=revision;
    return PrepareFo3CollisionMutationSnapshot(ps,x,y,z,forward,floor,units,state,token,exterior,cell);
}
static void publish(uint64_t t){assert(PublishFo3CollisionSnapshotQ1930(t,nullptr));assert(IsFo3PlayerCollisionReadyQ6G());}
static void emptyQueries(bool exterior) {
    assert(IsFo3PlayerCollisionReadyQ6G()&&gWorldTriangles.empty());
    const bool safeSpawn=gSafeSpawnResolved;
    float x=-1,y=-1,z=-1,ground=-19,nx=1,ny=1,nz=1;
    uint32_t contacts=99,candidates=99;
    // No synthetic ground or arrival rescue, even before a safe spawn resolved.
    assert(ResolveFo3PlayerMotionQ6G(0,0,2,3,1.5f,&x,&z,&y));
    assert(x==2&&z==3&&y==1.5f&&gSafeSpawnResolved==safeSpawn);
    assert(!SampleFo3ActorGroundQ240(0,0,1.5f,&ground)&&ground==-19);
    assert(!HasFo3InteractionOccluder(0,1,0,0,-1,0,2,0));
    assert(ResolveFo3DynamicBoxQ225(0,0,1,0,2,-3,4,.1f,.1f,.1f,
        1,0,0,0,1,0,0,0,1,.2f,&x,&y,&z,&nx,&ny,&nz,&contacts,&candidates));
    assert(x==2&&y==-3&&z==4&&nx==0&&ny==0&&nz==0&&contacts==0&&candidates==0);
    // LAND still grounds exterior players/actors. An interior cannot inherit it.
    terrainAvailable=true;
    assert(ResolveFo3PlayerMotionQ6G(0,0,2,3,1.5f,&x,&z,&y));
    assert(x==2&&z==3&&y==(exterior?terrainOffset:1.5f));
    assert(SampleFo3ActorGroundQ240(0,0,1.5f,&ground)==exterior);
    if(exterior)assert(ground==gCollisionFloorY+terrainOffset);
    else assert(ground==-19);
    assert(gSafeSpawnResolved==safeSpawn);
    terrainAvailable=false;
}
#include "resident_collision_tests.inc"

// Q226: the spatially indexed actor-ground resolver must return precisely the
// same original collision surface as the old full-array walk, including ties,
// upper Megaton platforms, oversized meshes, cell boundaries and stale epochs.
static void IndexedActorGroundParityQ226() {
    assert(gWorldTriangles.empty() && !gPlayerCollisionReady);
    auto flat=[](float x,float z,float y,float radius) {
        CollisionTriangle tri{};
        tri.a={x-radius,y,z-radius};
        tri.b={x+radius,y,z-radius};
        tri.c={x,y,z+radius};
        tri.normal={0,1,0};
        tri.minX=x-radius;tri.maxX=x+radius;
        tri.minY=tri.maxY=y;
        tri.minZ=z-radius;tri.maxZ=z+radius;
        return tri;
    };
    gWorldTriangles.push_back(flat(0,0,0,1.1f));
    gWorldTriangles.push_back(flat(0,0,2,1.1f));
    gWorldTriangles.push_back(flat(0,0,-3,1.1f));
    gWorldTriangles.push_back(flat(0,0,-1,22.f)); // broadphase oversized
    // Distant geometry must not force NPC grounding to scan the whole world.
    for(int i=0;i<1700;++i)
        gWorldTriangles.push_back(flat(12.f+float(i)*2.f,0,0,.75f));
    Q225RebuildDynamicGrid();
    assert(!gQ225DynamicGrid.empty()&&!gInteractionLargeTriangles.empty());
    const auto legacy=[&](float x,float z,float reference,float& out) {
        float best=std::numeric_limits<float>::max();
        bool found=false;
        for(const auto& tri:gWorldTriangles) {
            if(std::fabs(tri.normal.y)<WalkableNormalThresholdQ714(tri))continue;
            if(x<tri.minX-.02f||x>tri.maxX+.02f||z<tri.minZ-.02f||z>tri.maxZ+.02f)continue;
            float u=0,v=0,w=0;
            if(!BarycentricXZ(tri,x,z,u,v,w))continue;
            const float y=tri.a.y*u+tri.b.y*v+tri.c.y*w;
            const float dist=std::fabs(y-reference);
            if(dist>SAFE_SPAWN_VERTICAL_SEARCH)continue;
            if(!found||dist<best) {found=true;best=dist;out=y;}
        }
        return found;
    };
    const auto parity=[&]() {
        for(float x:{-1.5f,-.75f,-.001f,0.f,.75f,1.5f,12.f,12.7f,31.5f,127.f})
            for(float z:{-1.5f,-.1f,0.f,.1f,1.5f})
                for(float y:{-8.f,-2.f,-1.f,0.f,1.f,2.f,6.f,64.f}) {
                    float actual=-345.f,expected=-345.f;
                    const bool found=FindGroundWide(x,z,y,actual);
                    const bool before=legacy(x,z,y,expected);
                    assert(found==before);
                    assert(actual==expected);
                }
    };
    // Footsteps select the actual supporting layer and retain stairs metadata.
    gPlayerCollisionReady=true;gCollisionFloorY=0;
    gWorldTriangles[0].havokMaterialQ714=9;gWorldTriangles[0].stairsQ714=true;
    gWorldTriangles[1].havokMaterialQ714=5;
    uint32_t material=0;
    assert(SampleFo3PlayerFootstepSurface(0,0,0,&material)&&material==(9u|64u));
    assert(SampleFo3PlayerFootstepSurface(0,0,2,&material)&&material==5u);
    assert(!SampleFo3PlayerFootstepSurface(0,0,1,&material));
    assert(!SampleFo3PlayerFootstepSurface(0,0,.09f,&material));
    assert(!SampleFo3PlayerFootstepSurface(0,0,0,nullptr));
    gPlayerCollisionReady=false;
    parity();
    float ground=0;
    assert(FindGroundWide(0,0,1.f,ground)&&ground==0.f); // lower-index tie
    assert(FindGroundWide(0,0,2.f,ground)&&ground==2.f);
    // Collision publication epoch change must never consult stale buckets.
    AdvanceFo3CollisionPublicationRevision();
    parity();
    Q225RebuildDynamicGrid();
    parity();
    // Mutated collision geometry is rebuilt on the same publication path.
    gWorldTriangles.push_back(flat(-4.f,0,4.f,.5f));
    AdvanceFo3CollisionPublicationRevision();
    parity();
    Q225RebuildDynamicGrid();
    parity();
    gWorldTriangles.clear();
    Q225RebuildDynamicGrid();
    assert(gWorldTriangles.empty());
    std::cout<<"Q226 indexed actor ground preserves complete authored layer selection passed\n";
}

static void FallingAndLandingQ263(){
    fo3rigid::Live().ClearBodies();
    gWorldTriangles.clear();gSurfaceSourcesQ722.clear();terrainAvailable=false;
    auto floor=[&](float xmin,float xmax,float y){
        CollisionTriangle t;
        assert(BuildTriangle({xmin,y,-4},{xmax,y,4},{xmax,y,-4},t));gWorldTriangles.push_back(t);
        assert(BuildTriangle({xmin,y,-4},{xmin,y,4},{xmax,y,4},t));gWorldTriangles.push_back(t);
    };
    floor(-2,0,0);floor(-4,4,-3);
    gPlayerCollisionReady=true;gSafeSpawnResolved=true;gCollisionFloorY=0;gExteriorAllBhksQ78A=false;
    InvalidateDerivedCollisionCachesQ17();AdvanceFo3CollisionPublicationRevision();Q225RebuildDynamicGrid();
    float x=-.5f,z=0,y=0,nx,nz,ny;
    assert(ResolveFo3PlayerMotionWithGravity(x,z,1,0,y,1.f/72,&nx,&nz,&ny));
    assert(ny<0&&ny>-.01f);x=nx;z=nz;y=ny;
    for(int frame=0;frame<180;++frame){
        assert(ResolveFo3PlayerMotionWithGravity(x,z,1,0,y,1.f/72,&nx,&nz,&ny));
        assert(ny>=-3.01f);x=nx;z=nz;y=ny;
        if(frame==15)assert(y<-.1f&&y>-1.f);
    }
    assert(y>=-3.005f&&y< -2.97f);
    std::cout<<"Player gravity: accelerated ledge descent and continuous capsule landing passed\n";
}

#include "interior_stair_tests.inc"
#include "walkway_tests.inc"

int main(int argc,char**argv) {
    if(argc==4&&std::string(argv[1])=="--walkway-scene"){OriginalPlacedWalkways(argv[2],argv[3]);return 0;}
    if(argc==3&&std::string(argv[1])=="--walkways"){OriginalMegatonWalkways(argv[2]);return 0;}
    if(argc==4&&std::string(argv[1])=="--stairs"){OriginalInteriorStairs(argv[2],argv[3]);return 0;}
    IndexedActorGroundParityQ226();
    const std::string nonMegaton="Dungeons\\Office\\Room\\Wall.NIF",megaton="Architecture\\Megaton\\interior\\Wall.NIF",item="Clutter\\Bottle.NIF",trigger="Effects\\Trigger.NIF";
    assets[nonMegaton]=packedNif();assets[megaton]=packedNif();assets[item]=packedNif();assets[trigger]=packedNif(12);
    for(auto layer:{11,12,15,16})assert(!Fo3HavokLayerBlocksPlayerQ714(layer));
    assert(Fo3HavokLayerBlocksPlayerQ714(1));
    auto a=placement(1,nonMegaton),b=placement(2,megaton),d=placement(3,item,"MISC"),t=placement(4,trigger);
    auto selected=SelectFo3AuthoredCollisionPlacements({a,a,b,d,t});
    assert(selected.size()==4 && selected[0].modelPath==nonMegaton);
    auto token=prepare(selected,false,100);
    assert(!ExchangeFo3CollisionMutationSnapshot(token)); // ordinary token is not reversible
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
    assert(PublishFo3CollisionSnapshotQ1930(token,nullptr));assert(IsFo3PlayerCollisionReadyQ6G());
    assert(!gExteriorAllBhksQ78A && gWorldTriangles.empty()); // collected ref reapplied
    emptyQueries(false);
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

    assets["Door.nif"]=packedNif(1,true);
    auto door=placement(10,"Door.nif","DOOR");
    publish(prepare({b,door},false,107));
    auto countRef=[&](uint32_t ref) {
        size_t count=0;
        for(const auto& tri:gWorldTriangles) {
            const auto source=gSurfaceSourcesQ722.find(tri.surfaceKeyQ714);
            if(source!=gSurfaceSourcesQ722.end()&&source->second.refFormId==ref) ++count;
        }
        return count;
    };
    assert(countRef(10)==2);
    assert(SetFo3DoorCollisionTransformQ2400(10,nullptr,false));
    assert(countRef(10)==0);
    const float turn90[16]{0,0,-1,0, 0,1,0,0, 1,0,0,0, 0,0,0,1};
    assert(SetFo3DoorCollisionTransformQ2400(10,turn90,true));
    assert(countRef(10)==2);
    bool rotatedDoor=false;
    for(const auto& tri:gWorldTriangles) {
        const auto source=gSurfaceSourcesQ722.find(tri.surfaceKeyQ714);
        if(source!=gSurfaceSourcesQ722.end()&&source->second.refFormId==10)
            rotatedDoor=rotatedDoor||(tri.maxZ < -0.10f && tri.minZ < -0.10f);
    }
    assert(rotatedDoor);
    const float identity[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    assert(SetFo3DoorCollisionTransformQ2400(10,identity,true));
    assert(countRef(10)==2);

    float dy,nx,ny,nz;uint32_t contacts,candidates;
    assert(ResolveFo3DynamicBoxQ225(3,0,.2f,0,0,.05f,0,.1f,.1f,.1f,
        1,0,0,0,1,0,0,0,1,.18f,&x,&dy,&z,&nx,&ny,&nz,&contacts,&candidates));
    assert(dy>0 && contacts>0); // loose-object physics still contacts authored floor
    // Reproduce a thin pistol crossing more than its vertical half-extent in a
    // frame. The resolver must keep it above the authored floor.
    contacts=candidates=0;nx=ny=nz=0;
    assert(ResolveFo3DynamicBoxQ225(3,0,.20f,0,0,-.30f,0,.08f,.025f,.16f,
        1,0,0,0,1,0,0,0,1,.20f,&x,&dy,&z,&nx,&ny,&nz,&contacts,&candidates));
    assert(dy>0.020f && contacts>0 && ny>0.25f);
    // Failed/discarded replacements must retain the currently published world.
    auto oldTriangles=gWorldTriangles.size();
    token=prepare({a},false,106);DiscardFo3CollisionSnapshotQ1930(token);
    assert(!PublishFo3CollisionSnapshotQ1930(token,nullptr));
    assert(gWorldTriangles.size()==oldTriangles && IsFo3PlayerCollisionReadyQ6G());
    // Synchronous legacy entry points are generic too.
    ConfigureFo3CollisionPolicyQ710({a,d});SetNextFo3CollisionExteriorModeQ1931(false);
    assert(InitializeFo3CollisionOverlay({a,d},0,0,0,0,0,70));assert(gWorldTriangles.size()==2);
    // Reversible in-place collision publication. This uses real prepared bhk
    // geometry, broadphases, door transforms, motion and interaction queries.
    const auto registeredBefore = gQ1930PreparedSnapshots.size();
    SetFo3CollectedCollisionRefs({});
    publish(prepare({b,door},false,107));
    assert(SetFo3DoorCollisionTransformQ2400(10,turn90,true));
    gSafeSpawnResolved=true;
    const auto oldData=gWorldTriangles.data();
    const auto oldGrid=gQ225DynamicGrid;
    const auto oldTriangle=gWorldTriangles.back();
    const auto oldMapRevision=gPipLocalMapRevision;
    const auto oldCollisionRevision=GetFo3CollisionPublicationRevision();
    uint64_t mutation=0;
    assert(prepareMutation({b},0,0,0,0,0,70,{},
        oldCollisionRevision,&mutation,false,107));
    assert(GetFo3CollisionPublicationRevision()==oldCollisionRevision);
    assert(gWorldTriangles.data()==oldData && countRef(10)==2);
    // Legacy consuming publication must not consume a reversible handle.
    assert(!PublishFo3CollisionSnapshotQ1930(mutation,nullptr));
    forbidAllocations=true;
    const bool exchanged=ExchangeFo3CollisionMutationSnapshot(mutation);
    forbidAllocations=false;
    assert(exchanged && countRef(10)==0 && countRef(2)==2);
    assert(gSafeSpawnResolved && !gHkManifoldValidQ800 && gHkManifoldQ800.empty());
    assert(gPipLocalMapRevision>oldMapRevision && GetFo3CollisionPublicationRevision()>oldCollisionRevision);
    assert(!HasFo3InteractionOccluder(0,1,0,1,0,0,2,0));
    contacts=candidates=0;
    assert(ResolveFo3DynamicBoxQ225(3,0,.2f,0,0,-.3f,0,.1f,.1f,.1f,
        1,0,0,0,1,0,0,0,1,.18f,&x,&dy,&z,&nx,&ny,&nz,&contacts,&candidates));
    assert(contacts>0 && dy>0);
    forbidAllocations=true;
    const bool rolledBack=ExchangeFo3CollisionMutationSnapshot(mutation);
    forbidAllocations=false;
    assert(rolledBack && countRef(10)==2 && gWorldTriangles.data()==oldData);
    assert(gQ225DynamicGrid==oldGrid && gSafeSpawnResolved);
    assert(gWorldTriangles.back().a.x==oldTriangle.a.x &&
           gWorldTriangles.back().a.z==oldTriangle.a.z); // authored rotated door restored exactly
    DiscardFo3CollisionSnapshotQ1930(mutation);
    assert(!ExchangeFo3CollisionMutationSnapshot(mutation));

    // Successful finalization retains the new world and releases the old one.
    assert(prepareMutation({b},0,0,0,0,0,70,{},
        GetFo3CollisionPublicationRevision(),&mutation,false,107));
    assert(ExchangeFo3CollisionMutationSnapshot(mutation));
    const auto newData=gWorldTriangles.data();
    DiscardFo3CollisionSnapshotQ1930(mutation);
    assert(gWorldTriangles.data()==newData && countRef(10)==0);

    // Re-enable uses unchanged original cache data; it does not undo collection
    // or fabricate replacement geometry. Repeated exchanges do not duplicate it.
    assert(prepareMutation({b,door},0,0,0,0,0,70,{},
        GetFo3CollisionPublicationRevision(),&mutation,false,107));
    assert(ExchangeFo3CollisionMutationSnapshot(mutation) && countRef(10)==2);
    assert(gWorldTriangles.back().a.x==oldTriangle.a.x && gWorldTriangles.back().a.z==oldTriangle.a.z);
    DiscardFo3CollisionSnapshotQ1930(mutation);
    // A door made nonsolid during its authored animation must stay nonsolid
    // when an unrelated reference mutation rebuilds the collision world.
    assert(SetFo3DoorCollisionTransformQ2400(10,nullptr,false));
    assert(prepareMutation({b,door},0,0,0,0,0,70,{},
        GetFo3CollisionPublicationRevision(),&mutation,false,107));
    assert(ExchangeFo3CollisionMutationSnapshot(mutation) && countRef(10)==0);
    assert(ExchangeFo3CollisionMutationSnapshot(mutation) && countRef(10)==0);
    DiscardFo3CollisionSnapshotQ1930(mutation);
    assert(SetFo3DoorCollisionTransformQ2400(10,identity,true));
    SetFo3CollectedCollisionRefs({10});
    assert(countRef(10)==0);
    assert(prepareMutation({b,door},0,0,0,0,0,70,{10},
        GetFo3CollisionPublicationRevision(),&mutation,false,107));
    assert(ExchangeFo3CollisionMutationSnapshot(mutation) && countRef(10)==0);
    assert(ExchangeFo3CollisionMutationSnapshot(mutation) && countRef(10)==0);
    DiscardFo3CollisionSnapshotQ1930(mutation);

    // Collection changes invalidate pending candidates even if they remove no
    // active triangle. A changed live door also prevents stale rollback.
    assert(prepareMutation({b},0,0,0,0,0,70,{10},
        GetFo3CollisionPublicationRevision(),&mutation,false,107));
    const auto unchangedData=gWorldTriangles.data();
    SetFo3CollectedCollisionRefs({10,9999});
    assert(!ExchangeFo3CollisionMutationSnapshot(mutation) && gWorldTriangles.data()==unchangedData);
    DiscardFo3CollisionSnapshotQ1930(mutation);
    SetFo3CollectedCollisionRefs({});
    publish(prepare({b,door},false,108));
    assert(prepareMutation({b},0,0,0,0,0,70,{},
        GetFo3CollisionPublicationRevision(),&mutation,false,108));
    assert(ExchangeFo3CollisionMutationSnapshot(mutation));
    assert(SetFo3DoorCollisionTransformQ2400(10,identity,true));
    const auto doorChangedData=gWorldTriangles.data();
    assert(!ExchangeFo3CollisionMutationSnapshot(mutation) && gWorldTriangles.data()==doorChangedData);
    DiscardFo3CollisionSnapshotQ1930(mutation);

    // A later scene publication invalidates both unapplied and rollback tokens.
    assert(prepareMutation({b},0,0,0,0,0,70,{},
        GetFo3CollisionPublicationRevision(),&mutation,false,108));
    assert(ExchangeFo3CollisionMutationSnapshot(mutation));
    publish(prepare({a},false,109));
    const auto newerSceneData=gWorldTriangles.data();
    assert(!ExchangeFo3CollisionMutationSnapshot(mutation) && gWorldTriangles.data()==newerSceneData);
    DiscardFo3CollisionSnapshotQ1930(mutation);

    // Exterior exchange has prebuilt weld/shape/player and loose-object grids.
    publish(prepare({a,b},true,110));
    const auto exteriorData=gWorldTriangles.data();
    const auto exteriorGrid=gQ950Grid.size();
    assert(prepareMutation({b},0,0,0,0,0,70,{},
        GetFo3CollisionPublicationRevision(),&mutation,true,110));
    forbidAllocations=true;
    const bool exteriorExchanged=ExchangeFo3CollisionMutationSnapshot(mutation);
    forbidAllocations=false;
    assert(exteriorExchanged && gQ950Ready && gHkShapesReadyQ900 && gHkWeldAdjacencyReadyQ801);
    assert(gQ950Data==gWorldTriangles.data() && gHkShapeTriangleDataQ900==gWorldTriangles.data() &&
           gHkWeldTriangleDataQ801==gWorldTriangles.data());
    assert(gQ950TriangleCount==gWorldTriangles.size() && gQ225DynamicStamp.size()==gWorldTriangles.size());
    assert(HasFo3InteractionOccluder(0,1,0,0,-1,0,2,0));
    forbidAllocations=true;
    const bool exteriorRolledBack=ExchangeFo3CollisionMutationSnapshot(mutation);
    forbidAllocations=false;
    assert(exteriorRolledBack && gWorldTriangles.data()==exteriorData && gQ950Grid.size()==exteriorGrid);
    DiscardFo3CollisionSnapshotQ1930(mutation);

    // Empty worlds are complete publications, not failed/unloaded controllers.
    // Cover empty placement selection, non-solid-only, collected-only and an
    // animation-nonsolid door. Derived structures and query caches exchange too.
    for(bool exterior:{false,true}) {
        for(int filtered=0;filtered<(exterior?3:4);++filtered) {
            SetFo3CollectedCollisionRefs({});
            publish(prepare({a,b,door,t},exterior,111));
            if(filtered==2)SetFo3CollectedCollisionRefs({2});
            if(filtered==3)assert(SetFo3DoorCollisionTransformQ2400(10,nullptr,false));
            gSafeSpawnResolved=(filtered%2)==1;
            const auto beforeData=gWorldTriangles.data();
            const auto beforeSize=gWorldTriangles.size();
            const auto beforeMap=gPipLocalMapRevision;
            const auto beforeRevision=GetFo3CollisionPublicationRevision();
            const std::vector<Fo3WorldPlacement> chosen=filtered==0?std::vector<Fo3WorldPlacement>{}:
                filtered==1?std::vector<Fo3WorldPlacement>{t}:
                filtered==2?std::vector<Fo3WorldPlacement>{b}:std::vector<Fo3WorldPlacement>{door};
            auto captured=CaptureFo3CollisionMutationState();
            assert(PrepareFo3CollisionMutationSnapshot(chosen,0,0,0,0,0,70,captured,&mutation,exterior,111));
            assert(gWorldTriangles.data()==beforeData&&GetFo3CollisionPublicationRevision()==beforeRevision);
            forbidAllocations=true;
            const bool emptied=ExchangeFo3CollisionMutationSnapshot(mutation);
            forbidAllocations=false;
            assert(emptied&&gWorldTriangles.empty()&&gPipLocalMapRevision>beforeMap);
            assert(gQ225DynamicGrid.empty()&&gQ225DynamicStamp.empty()&&gInteractionLargeTriangles.empty());
            if(exterior)assert(gQ950Ready&&gQ950Grid.empty()&&gHkShapesQ900.empty()&&gHkWeldNeighboursQ801.empty());
            forbidAllocations=true;
            emptyQueries(exterior);
            const bool restored=ExchangeFo3CollisionMutationSnapshot(mutation);
            forbidAllocations=false;
            assert(restored&&gWorldTriangles.data()==beforeData&&gWorldTriangles.size()==beforeSize);
            assert(GetFo3CollisionPublicationRevision()>beforeRevision&&gPipLocalMapRevision>beforeMap+1);
            DiscardFo3CollisionSnapshotQ1930(mutation);
        }
        // An empty live world can prepare/re-enable geometry and roll back to
        // empty without allocations. Another exchange invalidates older tokens.
        SetFo3CollectedCollisionRefs({});publish(prepare({b},exterior,112));
        auto captured=CaptureFo3CollisionMutationState();
        assert(PrepareFo3CollisionMutationSnapshot({},0,0,0,0,0,70,captured,&mutation,exterior,112));
        assert(ExchangeFo3CollisionMutationSnapshot(mutation));
        uint64_t reenable=0;captured=CaptureFo3CollisionMutationState();
        assert(PrepareFo3CollisionMutationSnapshot({b},0,0,0,0,0,70,captured,&reenable,exterior,112));
        forbidAllocations=true;
        const bool enabled=ExchangeFo3CollisionMutationSnapshot(reenable);
        const bool emptyAgain=ExchangeFo3CollisionMutationSnapshot(reenable);
        const bool stale=ExchangeFo3CollisionMutationSnapshot(mutation);
        forbidAllocations=false;
        assert(enabled&&emptyAgain&&!stale&&gWorldTriangles.empty());
        emptyQueries(exterior);
        assert(ExchangeFo3CollisionMutationSnapshot(reenable)&&gWorldTriangles.size()==2);
        DiscardFo3CollisionSnapshotQ1930(reenable);DiscardFo3CollisionSnapshotQ1930(mutation);
    }
    SetFo3CollectedCollisionRefs({});publish(prepare({a,b},true,110));
    const auto preflightData=gWorldTriangles.data();
    // Ordinary CELL load tokens retain their existing empty-input rejection.
    uint64_t ordinaryEmpty=123;
    assert(!PrepareFo3CollisionSnapshotQ1930({},0,0,0,0,0,70,&ordinaryEmpty,true,110)&&ordinaryEmpty==0);
    size_t nonSolidTriangles=99;
    assert(PrimeFo3CollisionPlacementCacheQ1820(t,0,0,0,0,0,70,&nonSolidTriangles));
    assert(nonSolidTriangles==0);
    assert(!PrepareFo3CollisionSnapshotQ1930({t},0,0,0,0,0,70,&ordinaryEmpty,true,110)&&ordinaryEmpty==0);
    // Bad preflight never touches the published world or yields a usable token.
    mutation=123;
    assert(!prepareMutation({placement(9998,nonMegaton)},0,0,0,0,0,70,{},
        GetFo3CollisionPublicationRevision(),&mutation,true,110)); // not prewarmed
    assert(!prepareMutation({},1,0,0,0,0,70,{},
        GetFo3CollisionPublicationRevision(),&mutation,true,110)); // even empty needs context
    assert(!prepareMutation({},0,0,0,0,0,0,{},
        GetFo3CollisionPublicationRevision(),&mutation,true,110));
    assert(!prepareMutation({b},1,0,0,0,0,70,{},
        GetFo3CollisionPublicationRevision(),&mutation,true,110)); // different transform context
    assert(!prepareMutation({b},0,0,0,0,0,70,{},0,&mutation,true,110));
    assert(gWorldTriangles.data()==preflightData && gQ1930PreparedSnapshots.size()==registeredBefore);
    assert(!ExchangeFo3CollisionMutationSnapshot(0));

    // A prewarmed cache for a different origin is not the current live scene.
    ResetFo3CollisionSceneCache();
    size_t primedTriangles=0;
    assert(PrimeFo3CollisionPlacementCacheQ1820(b,100,0,0,0,0,70,&primedTriangles));
    assert(prepareMutation({b},100,0,0,0,0,70,{},
        GetFo3CollisionPublicationRevision(),&mutation,true,110));
    assert(!ExchangeFo3CollisionMutationSnapshot(mutation) && gWorldTriangles.data()==preflightData);
    DiscardFo3CollisionSnapshotQ1930(mutation);
    // Exterior cap is a preflight failure, never a partially published mutation.
    large.clear();
    for(unsigned i=0;i<1025;++i)large.push_back(placement(20000+i,nonMegaton,"STAT",i*2000.f));
    ResetFo3CollisionSceneCache();
    for(const auto& p:large)assert(PrimeFo3CollisionPlacementCacheQ1820(p,0,0,0,0,0,70,&primedTriangles));
    assert(!prepareMutation(large,0,0,0,0,0,70,{},
        GetFo3CollisionPublicationRevision(),&mutation,true,110) && mutation==0);
    assert(gWorldTriangles.data()==preflightData && gQ1930PreparedSnapshots.size()==registeredBefore);
    if(argc==3 || argc==4) {
        assetRoot=argv[1];std::ifstream manifest(argv[2]);std::vector<Fo3WorldPlacement> ps;Fo3WorldPlacement p;
        while(manifest>>p.refFormId>>p.baseRecordType>>p.x>>p.y>>p.z>>p.rx>>p.ry>>p.rz>>p.scale){manifest.get();std::getline(manifest,p.modelPath);ps.push_back(p);}
        auto start=std::chrono::steady_clock::now();publish(prepare(ps,false,argc==4 ? std::stoul(argv[3],nullptr,16) : 0x17f37));
        auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
        std::cout<<"ORIGINAL CELL input="<<ps.size()<<" static="<<gPlacementCount<<" triangles="<<gWorldTriangles.size()<<" ready="<<IsFo3PlayerCollisionReadyQ6G()<<" hostMs="<<ms<<"\n";
        assert(!gWorldTriangles.empty() && gPlacementCount>0);
        uint64_t privateToken=0;std::atomic<bool> privateCancel{false};
        const bool originalPrivate=PrepareFo3ResidentCollisionSnapshot(ps,0,0,0,0,0,70,
            CaptureFo3CollisionMutationState(),privateCancel,&privateToken,false,0);
        std::cout<<"Original private whole-CELL candidate complete="<<originalPrivate<<" token="<<privateToken<<"\n";
        Fo3CollisionSnapshotOwner fullPrivateOwner(privateToken);
        std::vector<Fo3WorldPlacement> available;size_t missing=0;
        for(const auto& original:ps){std::vector<uint8_t> source;
            if(LoadFalloutMeshFile(original.modelPath,source,nullptr))available.push_back(original);else ++missing;}
        assert(!available.empty());
        assert(PrepareFo3ResidentCollisionSnapshot(available,0,0,0,0,0,70,
            CaptureFo3CollisionMutationState(),privateCancel,&privateToken,false,0));
        {Fo3CollisionSnapshotOwner owner(privateToken);
            assert(CanExchangeFo3CollisionMutationSnapshot(privateToken));
            const auto before=gWorldTriangles.data();const auto triangles=gWorldTriangles.size();
            forbidAllocations=true;
            const bool forward=ExchangeFo3CollisionMutationSnapshot(privateToken);
            const auto candidateTriangles=gWorldTriangles.size();
            const bool backward=ExchangeFo3CollisionMutationSnapshot(privateToken);
            forbidAllocations=false;
            assert(forward&&backward&&gWorldTriangles.data()==before&&gWorldTriangles.size()==triangles);
            std::cout<<"Original available private candidate refs="<<available.size()<<" missingSources="<<missing
                     <<" triangles="<<candidateTriangles<<" exchange/rollback passed\n";
        }
        const auto originalData=gWorldTriangles.data();
        const uint32_t target=gSurfaceSourcesQ722.begin()->second.refFormId;
        const auto targetTriangles=countRef(target);
        assert(targetTriangles>0);
        auto withoutTarget=ps;
        withoutTarget.erase(std::remove_if(withoutTarget.begin(),withoutTarget.end(),
            [target](const Fo3WorldPlacement& p){return p.refFormId==target;}),withoutTarget.end());
        assert(prepareMutation(withoutTarget,0,0,0,0,0,70,{},
            GetFo3CollisionPublicationRevision(),&mutation,false,0));
        assert(countRef(target)==targetTriangles && gWorldTriangles.data()==originalData);
        forbidAllocations=true;
        const bool originalExchanged=ExchangeFo3CollisionMutationSnapshot(mutation);
        forbidAllocations=false;
        assert(originalExchanged && countRef(target)==0);
        forbidAllocations=true;
        const bool originalRolledBack=ExchangeFo3CollisionMutationSnapshot(mutation);
        forbidAllocations=false;
        assert(originalRolledBack && countRef(target)==targetTriangles && gWorldTriangles.data()==originalData);
        DiscardFo3CollisionSnapshotQ1930(mutation);
        const auto wholeCellTriangles=gWorldTriangles.size();
        auto wholeCellState=CaptureFo3CollisionMutationState();
        assert(PrepareFo3CollisionMutationSnapshot({},0,0,0,0,0,70,wholeCellState,&mutation,false,0));
        forbidAllocations=true;
        const bool wholeCellEmptied=ExchangeFo3CollisionMutationSnapshot(mutation);
        emptyQueries(false);
        const bool wholeCellRestored=ExchangeFo3CollisionMutationSnapshot(mutation);
        forbidAllocations=false;
        assert(wholeCellEmptied&&wholeCellRestored&&gWorldTriangles.data()==originalData&&
            gWorldTriangles.size()==wholeCellTriangles);
        DiscardFo3CollisionSnapshotQ1930(mutation);
        std::cout<<"Original CELL empty-world exchange/query/rollback passed for "<<wholeCellTriangles<<" triangles\n";
        std::cout<<"Original CELL authored reference "<<std::hex<<target<<std::dec
                 <<" removed/restored triangles="<<targetTriangles<<" without allocation\n";
    }
    privateCollisionTests(a,b,d,door);
    // Independent authored collision targets: opening one leaf must preserve
    // the gatehouse walls, unrelated references and original metadata.
    gPlayerCollisionReady=true;gWorldTriangles.clear();gSurfaceSourcesQ722.clear();gQ1990CollisionPlacementCache.clear();gDoorCollisionPoses.clear();
    CollisionTriangle leaf,wall;
    assert(BuildTriangle({0,0,0},{0,1,0},{0,0,1},leaf));
    assert(BuildTriangle({5,0,0},{5,1,0},{5,0,1},wall));
    leaf.animationTargetBlock=11;wall.animationTargetBlock=22;leaf.surfaceKeyQ714=111;wall.surfaceKeyQ714=222;
    gSurfaceSourcesQ722.emplace(111,CollisionSurfaceSourceQ722{900,"test","test"});
    gSurfaceSourcesQ722.emplace(222,CollisionSurfaceSourceQ722{900,"test","test"});
    leaf.meshKeyQ801=11;wall.meshKeyQ801=22;
    gQ1990CollisionPlacementCache[900].triangles={leaf,wall};gWorldTriangles={leaf,wall};
    // A large unrelated resident world must retain storage, weld links, shape
    // indices and static broadphase cells during recurring gate poses.
    for(uint32_t i=0;i<4000;++i){CollisionTriangle t;
        assert(BuildTriangle({float(i)+100,0,0},{float(i)+100,1,0},{float(i)+100,0,1},t));
        t.surfaceKeyQ714=1000+i;t.meshKeyQ801=1000+i;
        gSurfaceSourcesQ722[t.surfaceKeyQ714]={1000+i,"static","static"};gWorldTriangles.push_back(t);
    }
    InvalidateDerivedCollisionCachesQ17();Q225RebuildDynamicGrid();EnsureQ950SpatialIndex();
    const auto* worldStorage=gWorldTriangles.data();const auto* shapeStorage=gHkShapesQ900.data();
    const auto* weldStorage=gHkWeldNeighboursQ801.data();
    Fo3DoorCollisionPose animated;auto delta=animated.transform;delta[12]=3;animated.nodeTransforms.emplace(11,delta);
    assert(SetFo3ScriptCollisionPose(900,animated));assert(gWorldTriangles.size()==4002);
    assert(gWorldTriangles[0].a.x==3&&gWorldTriangles[1].a.x==5&&gWorldTriangles[0].animationTargetBlock==11);
    const auto animatedRevision=GetFo3CollisionPublicationRevision();
    animated.nodeTransforms.emplace(99,delta); // No collider owns this animated node.
    assert(SetFo3ScriptCollisionPose(900,animated)&&GetFo3CollisionPublicationRevision()==animatedRevision);
    assert(gQ1990CollisionPlacementCache[900].triangles[0].a.x==0); // Bind cache stays immutable.
    for(int step=0;step<40;++step){animated.nodeTransforms[11][12]=float(step)-20;
        assert(SetFo3ScriptCollisionPose(900,animated));
        assert(gWorldTriangles.data()==worldStorage&&gHkShapesQ900.data()==shapeStorage&&gHkWeldNeighboursQ801.data()==weldStorage);
        assert(gQ950Ready&&gHkShapesReadyQ900&&gHkWeldAdjacencyReadyQ801);
        assert(gWorldTriangles[2].a.x==100&&gWorldTriangles.back().a.x==4099);
    }
    auto grid=gQ225DynamicGrid;auto movingLarge=gInteractionLargeTriangles;
    auto objects=gQ950Objects;auto shapeBounds=gHkShapesQ900;
    auto spatial=gQ950Grid;auto largeObjects=gQ950LargeObjects;
    InvalidateDerivedCollisionCachesQ17();Q225RebuildDynamicGrid();EnsureQ950SpatialIndex();
    auto sameValues=[](auto a,auto b){std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());return a==b;};
    assert(grid.size()==gQ225DynamicGrid.size()&&sameValues(movingLarge,gInteractionLargeTriangles));
    for(const auto& c:grid)assert(sameValues(c.second,gQ225DynamicGrid.at(c.first)));
    assert(objects.size()==gQ950Objects.size()&&spatial.size()==gQ950Grid.size()&&sameValues(largeObjects,gQ950LargeObjects));
    for(const auto& c:spatial){const auto& actual=gQ950Grid.at(c.first);assert(sameValues(c.second.objects,actual.objects)&&sameValues(c.second.legacyTriangles,actual.legacyTriangles));}
    for(size_t i=0;i<objects.size();++i){const auto& a=objects[i];const auto& b=gQ950Objects[i];
        assert(a.minX==b.minX&&a.maxX==b.maxX&&a.minY==b.minY&&a.maxY==b.maxY&&a.minZ==b.minZ&&a.maxZ==b.maxZ&&a.walkableTriangles==b.walkableTriangles);}
    for(size_t i=0;i<shapeBounds.size();++i){const auto& a=shapeBounds[i];const auto& b=gHkShapesQ900[i];
        assert(a.minX==b.minX&&a.maxX==b.maxX&&a.walkableTriangles==b.walkableTriangles);}
    std::cout<<"40 moving BHK poses preserve 4000 static meshes; incremental player/prop grids match a full rebuild\n";
    std::cout<<"Per-node scripted collision preserves static structure passed\n";
    // A settled animated gate survives rolling snapshot publication. Once its
    // reference leaves the physical window, further visual animation must not
    // append distant triangles or invalidate the static world every 100 ms.
    publish(prepare({b,door},true,0));
    Fo3DoorCollisionPose openGate;openGate.transform[12]=4;
    assert(SetFo3ScriptCollisionPose(10,openGate));
    auto checkGatePose=[&](float translation){
        const auto index=gAnimatedReferenceTriangles.find(10);assert(index!=gAnimatedReferenceTriangles.end());
        const auto& original=gQ1990CollisionPlacementCache.at(10).triangles.front();
        assert(gWorldTriangles[index->second.front()].a.x==original.a.x+translation);
    };
    publish(prepare({b,door},true,0));EnsureAnimatedReferenceIndex();checkGatePose(4);
    uint64_t departingWindow=0;
    assert(PrepareFo3CollisionSnapshotQ1930({b},0,0,0,0,0,70,&departingWindow,true,0));
    publish(departingWindow);EnsureQ950SpatialIndex();
    assert(countRef(10)==0&&!gCollisionScopeRefs.count(10));
    const auto outsideRevision=GetFo3CollisionPublicationRevision();
    const auto* outsideStorage=gWorldTriangles.data();
    for(int step=0;step<100;++step){openGate.transform[12]=float(step);
        assert(SetFo3ScriptCollisionPose(10,openGate));
        assert(countRef(10)==0&&gWorldTriangles.data()==outsideStorage&&gQ950Ready&&gHkShapesReadyQ900);
    }
    assert(GetFo3CollisionPublicationRevision()==outsideRevision);
    gQ1990CollisionPlacementCache.erase(10); // The far BHK cache may be evicted before re-entry.
    assert(PrepareFo3CollisionSnapshotQ1930({b},0,0,0,0,0,70,&departingWindow,true,0));
    publish(departingWindow);assert(gDoorCollisionPoses.at(10).transform[12]==99);
    publish(prepare({b,door},true,0));EnsureAnimatedReferenceIndex();checkGatePose(99);
    publish(prepare({b,door},false,111));assert(gDoorCollisionPoses.empty());
    std::cout<<"Rolling gate pose retention, distant animation isolation and re-entry passed\n";
    ShutdownFo3CollisionOverlay();
    assert(!IsFo3PlayerCollisionReadyQ6G());
    assert(!ResolveFo3PlayerMotionQ6G(0,0,1,2,3,&x,&z,&y));
    float ground=99;assert(!SampleFo3ActorGroundQ240(0,0,0,&ground)&&ground==99);
    nx=ny=nz=99;contacts=candidates=99;
    assert(!ResolveFo3DynamicBoxQ225(0,0,1,0,2,-3,4,.1f,.1f,.1f,
        1,0,0,0,1,0,0,0,1,.2f,&x,&y,&z,&nx,&ny,&nz,&contacts,&candidates));
    assert(nx==99&&ny==99&&nz==99&&contacts==99&&candidates==99);
    assert(gQ1930PreparedSnapshots.size()==registeredBefore);
    std::cout<<"Authored collision and empty-world publication tests passed\n";
    FallingAndLandingQ263();
    InteriorStepRegression();

}
