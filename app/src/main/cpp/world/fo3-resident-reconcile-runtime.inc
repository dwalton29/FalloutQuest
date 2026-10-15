// Interior-only reconciliation of work parked before the reference epoch
// changed. Never consume a result into live geometry/collision/actors here.
void DiscardFo3ResidentParkedCellState(Q1900CellStateQ19& state) noexcept {
    if(state.q1960TextureUpload.id)glDeleteTextures(1,&state.q1960TextureUpload.id);
    state.q1960TextureUpload={};
    QActorDeleteSkin(state.q1960ShapeUpload.gpu);Q1900DeleteGpuShape(state.q1960ShapeUpload.gpu);
    state.q1960ShapeUpload={};
    Q74DeleteGpuObjects(state.stagedGpu);state.cpu.clear();state.textures.clear();
}
bool ReconcileFo3ResidentParkedWork(fo3scene::ResidentExchangeStatus status) {
    if(gExteriorWorldspaceQ1890||!Fo3ResidentAssetLaneBlocked()||gPendingStreamQ1900.active||
        !CaptureFo3ResidentAssetActivity().Quiescent())return false;
    if(status==fo3scene::ResidentExchangeStatus::Rejected)return true; // Original epoch/results remain valid.
    if(status!=fo3scene::ResidentExchangeStatus::Committed&&status!=fo3scene::ResidentExchangeStatus::RolledBack)return false;
    // Allocate the only new queue before disposing any old one. Already
    // registered actors are filtered by the ordinary arrival scheduler later.
    auto npcPending=gQ230NpcArrival.pending;
    npcPending.insert(gQ230NpcArrival.requested.begin(),gQ230NpcArrival.requested.end());
    if(!DiscardFo3TerrainParkedWorkQ2000())return false;
    Q230ResetNpcArrivals();gQ230NpcArrival.pending.swap(npcPending);
    for(auto& entry:gWorldStreaming.cells)DiscardFo3ResidentParkedCellState(entry.second);
    gWorldStreaming.cells.clear();
    for(auto& worker:gWorldStreaming.workers)worker.reset();
    if(gWorldStreaming.collisionTask){
        DiscardFo3CollisionSnapshotQ1930(gWorldStreaming.collisionTask->snapshotToken);
        gWorldStreaming.collisionTask.reset();
    }
    for(auto& worker:gQ2013LodWorkersQ19){
        if(worker&&worker->q2023AuthoredAsset&&worker->q2023AssetIndex<gQ2023LodAssets.size())
            gQ2023LodAssets[worker->q2023AssetIndex].pending=false;
        worker.reset();
    }
    for(auto& upload:gQ2013LodUploadsQ19){
        if(upload.state)DiscardFo3ResidentParkedCellState(*upload.state);
        if(upload.q2023AuthoredAsset&&upload.q2023AssetIndex<gQ2023LodAssets.size())
            gQ2023LodAssets[upload.q2023AssetIndex].pending=false;
    }
    gQ2013LodUploadsQ19.clear();gFo3LegacyMetadataWorkers.clear();
    ++gWorldStreaming.contextSerial;gWorldStreaming.contextReady=false;
    gWorldStreaming.publication=gFo3ScenePublication.Stamp();
    // This queue already owns retired geometry; no live scene slots are touched.
    Q1910DrainDeferredGpuDeletesQ19(true);
    return true;
}
