bool DiscardFo3TerrainParkedWorkQ2000() {
    if(!IsFo3TerrainStreamingCpuQuiescentQ2000())return false;
    if(gQ2000WorkerThread.joinable())gQ2000WorkerThread.join();
    ++gQ2000Serial;gQ2000Worker.reset();
    if(gQ2000TextureUpload.texture)glDeleteTextures(1,&gQ2000TextureUpload.texture);
    gQ2000TextureUpload={};
    if(gQ2000VboUpload.vbo)glDeleteBuffers(1,&gQ2000VboUpload.vbo);
    gQ2000VboUpload={};
    for(auto& batch:gQ2000CurrentCellBatches)Q2000DeleteBatchGeometry(batch);
    gQ2000CurrentCellBatches.clear();Q2000DrainDeferredGeometry(true);
    gQ2000TargetActive=false;gQ2000TargetKeys.clear();
    gQ2000PendingCells.clear();gQ2000PendingTextures.clear();
    gQ2000TextureCursor=gQ2000CellCursor=gQ2000BatchCursor=0;
    return true;
}
