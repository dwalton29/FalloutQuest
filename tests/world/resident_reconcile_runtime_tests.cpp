// Compile the actual native cleanup bridges with tracked private resource slots.
// Published scene/actor/LAND registries are deliberately absent: touching them
// from these bridges would fail this host compilation.
#include "fo3-resident-exchange.h"
#include "fo3-resident-preparation-lane.h"
#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <map>
#include <new>
#include <unordered_set>
#include <vector>
static bool failAllocation=false;
void* operator new(std::size_t n){if(failAllocation)throw std::bad_alloc();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
using GLuint=unsigned;
std::array<int,256> deleted{};
void glDeleteTextures(int n,const GLuint* ids){for(int i=0;i<n;++i)if(ids[i])++deleted.at(ids[i]);}
void glDeleteBuffers(int n,const GLuint* ids){glDeleteTextures(n,ids);}
struct Gpu {GLuint vbo=0,vao=0;};
void Q1900DeleteGpuShape(Gpu& gpu){glDeleteBuffers(1,&gpu.vbo);glDeleteBuffers(1,&gpu.vao);gpu={};}
void QActorDeleteSkin(Gpu&){}
void Q74DeleteGpuObjects(std::vector<Gpu>& slots){for(auto& slot:slots)Q1900DeleteGpuShape(slot);slots.clear();}
bool terrainReady=true;
bool IsFo3TerrainStreamingCpuQuiescentQ2000(){return terrainReady;}
struct Thread {
    bool active=true;int joined=0;
    bool joinable()const{return active;}
    void join(){assert(terrainReady);active=false;++joined;}
} gQ2000WorkerThread;
uint64_t gQ2000Serial=10;
std::shared_ptr<int> gQ2000Worker;
struct {GLuint texture=0;} gQ2000TextureUpload;
struct {GLuint vbo=0;} gQ2000VboUpload;
std::vector<Gpu> gQ2000CurrentCellBatches,retiredTerrain;
void Q2000DeleteBatchGeometry(Gpu& batch){Q1900DeleteGpuShape(batch);}
void Q2000DrainDeferredGeometry(bool all){assert(all);Q74DeleteGpuObjects(retiredTerrain);}
bool gQ2000TargetActive=true;
std::vector<int> gQ2000TargetKeys,gQ2000PendingCells,gQ2000PendingTextures;
size_t gQ2000TextureCursor=0,gQ2000CellCursor=0,gQ2000BatchCursor=0;
#include "rendering/terrain/fo3-terrain-parked-work.inc"

struct Q1900CellStateQ19 {
    struct {GLuint id=0;} q1960TextureUpload;
    struct {Gpu gpu;} q1960ShapeUpload;
    std::vector<Gpu> stagedGpu;
    std::vector<int> cpu,textures;
};
uint32_t gExteriorWorldspaceQ1890=0;
bool blocked=true;
bool Fo3ResidentAssetLaneBlocked(){return blocked;}
fo3scene::ResidentAssetActivity activity;
fo3scene::ResidentAssetActivity CaptureFo3ResidentAssetActivity(){return activity;}
struct {bool active=false;} gPendingStreamQ1900;
struct {std::unordered_set<uint32_t> pending,requested;std::vector<Gpu> privateGpu;} gQ230NpcArrival;
void Q230ResetNpcArrivals(){Q74DeleteGpuObjects(gQ230NpcArrival.privateGpu);gQ230NpcArrival.pending.clear();gQ230NpcArrival.requested.clear();}
struct Collision {uint64_t snapshotToken=0;};
struct {
    std::map<int,Q1900CellStateQ19> cells;
    std::array<std::shared_ptr<int>,2> workers;
    std::unique_ptr<Collision> collisionTask;
    uint64_t contextSerial=10,publication=3;
    bool contextReady=true;
    std::vector<Gpu> retired;
} gWorldStreaming;
int collisionDiscarded=0;
void DiscardFo3CollisionSnapshotQ1930(uint64_t token){assert(token==44);++collisionDiscarded;}
struct LodWorker {bool q2023AuthoredAsset=true;size_t q2023AssetIndex=0;};
std::array<std::shared_ptr<LodWorker>,2> gQ2013LodWorkersQ19;
struct LodUpload {bool q2023AuthoredAsset=true;size_t q2023AssetIndex=1;std::unique_ptr<Q1900CellStateQ19> state;};
std::vector<LodUpload> gQ2013LodUploadsQ19;
struct Asset {bool pending=true,loaded=true;};
std::array<Asset,2> gQ2023LodAssets;
std::vector<std::weak_ptr<int>> gFo3LegacyMetadataWorkers;
struct {uint64_t Stamp()const{return 8;}} gFo3ScenePublication;
void Q1910DrainDeferredGpuDeletesQ19(bool all){assert(all);Q74DeleteGpuObjects(gWorldStreaming.retired);}
#include "world/fo3-resident-reconcile-runtime.inc"

static void fixture() {
    deleted={};collisionDiscarded=0;terrainReady=true;blocked=true;activity={};gExteriorWorldspaceQ1890=0;gPendingStreamQ1900.active=false;
    gQ2000WorkerThread={};gQ2000Worker=std::make_shared<int>(1);gQ2000Serial=10;
    gQ2000TextureUpload.texture=1;gQ2000VboUpload.vbo=2;
    gQ2000CurrentCellBatches={{3,4}};retiredTerrain={{5,6}};
    gQ2000TargetActive=true;gQ2000TargetKeys={1};gQ2000PendingCells={1};gQ2000PendingTextures={1};
    gQ2000TextureCursor=gQ2000CellCursor=gQ2000BatchCursor=3;
    gQ230NpcArrival.pending={10,20};gQ230NpcArrival.requested={20,30};gQ230NpcArrival.privateGpu={{7,8}};
    gWorldStreaming.cells.clear();auto& cell=gWorldStreaming.cells[1];
    cell.q1960TextureUpload.id=9;cell.q1960ShapeUpload.gpu={10,11};cell.stagedGpu={{12,13}};cell.cpu={1};cell.textures={1};
    for(auto& worker:gWorldStreaming.workers)worker=std::make_shared<int>(1);
    gWorldStreaming.collisionTask=std::make_unique<Collision>();gWorldStreaming.collisionTask->snapshotToken=44;
    gWorldStreaming.contextSerial=10;gWorldStreaming.contextReady=true;gWorldStreaming.publication=3;gWorldStreaming.retired={{14,15}};
    gQ2013LodWorkersQ19[0]=std::make_shared<LodWorker>();gQ2013LodWorkersQ19[1].reset();
    gQ2013LodUploadsQ19.clear();LodUpload upload;upload.state=std::make_unique<Q1900CellStateQ19>();
    upload.state->stagedGpu={{16,17}};gQ2013LodUploadsQ19.push_back(std::move(upload));
    gQ2023LodAssets={};gFo3LegacyMetadataWorkers={gWorldStreaming.workers[0]};
}
static void untouched() {
    for(int n:deleted)assert(n==0);
    assert(gWorldStreaming.contextSerial==10&&gWorldStreaming.contextReady&&gWorldStreaming.cells.size()==1);
    assert(gQ2000Serial==10&&gQ2000WorkerThread.joined==0&&gQ230NpcArrival.requested.count(30));
    assert(!collisionDiscarded&&gQ2013LodUploadsQ19.size()==1);
}
int main(){
    using Status=fo3scene::ResidentExchangeStatus;
    for(int guard=0;guard<10;++guard){
        fixture();
        if(guard==0)gExteriorWorldspaceQ1890=1;
        if(guard==1)blocked=false;
        if(guard==2)gPendingStreamQ1900.active=true;
        if(guard>=3&&guard<=8){
            switch(guard){case 3:activity.cells=true;break;case 4:activity.collision=true;break;
                case 5:activity.lod=true;break;case 6:activity.terrain=true;break;case 7:activity.npc=true;break;case 8:activity.legacy=true;break;}
        }
        if(guard==9)terrainReady=false;
        assert(!ReconcileFo3ResidentParkedWork(Status::Committed));untouched();
    }
    fixture();assert(ReconcileFo3ResidentParkedWork(Status::Rejected));untouched();
    assert(!ReconcileFo3ResidentParkedWork(Status::RecoveryFailed));untouched();
    fixture();failAllocation=true;bool threw=false;
    try{ReconcileFo3ResidentParkedWork(Status::Committed);}catch(const std::bad_alloc&){threw=true;}
    failAllocation=false;assert(threw);untouched();
    for(auto status:{Status::Committed,Status::RolledBack}){
        fixture();assert(ReconcileFo3ResidentParkedWork(status));
        for(int id=1;id<=17;++id)assert(deleted[id]==1);
        assert(gQ230NpcArrival.pending==std::unordered_set<uint32_t>({10,20,30})&&gQ230NpcArrival.requested.empty());
        assert(gWorldStreaming.cells.empty()&&!gWorldStreaming.collisionTask&&collisionDiscarded==1);
        assert(!gWorldStreaming.workers[0]&&!gWorldStreaming.workers[1]&&!gQ2013LodWorkersQ19[0]);
        assert(gQ2013LodUploadsQ19.empty()&&gFo3LegacyMetadataWorkers.empty());
        assert(!gQ2023LodAssets[0].pending&&!gQ2023LodAssets[1].pending&&gQ2023LodAssets[0].loaded&&gQ2023LodAssets[1].loaded);
        assert(!gWorldStreaming.contextReady&&gWorldStreaming.contextSerial==11&&gWorldStreaming.publication==8);
        assert(gQ2000WorkerThread.joined==1&&!gQ2000Worker&&!gQ2000TargetActive&&gQ2000PendingCells.empty());
        assert(gQ2000TextureCursor==0&&gQ2000CellCursor==0&&gQ2000BatchCursor==0);
        assert(ReconcileFo3ResidentParkedWork(status));
        for(int id=1;id<=17;++id)assert(deleted[id]==1);
    }
    std::cout<<"actual interior reconciliation/terrain bridges reclaim private queues, preserve valid rejection and requeue NPC arrivals\n";
}
