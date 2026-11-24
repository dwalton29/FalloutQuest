// Included beside the native LAND texture caches. GPU handles only live for
// resident cells. Small filtered source tiles follow the resolved texture cache.
fo3terrain::composite::Sources compositeSources;
struct TerrainComposite {
    GLuint maps[2]{};
    float minX,maxX,minZ,maxZ,blend=0;
};
std::unordered_map<uint64_t,TerrainComposite> terrainComposites;
GLint terrainCompositeRegion=-1;
void RemoveTerrainComposite(uint64_t key) {
    auto it=terrainComposites.find(key);
    if(it==terrainComposites.end())return;
    glDeleteTextures(2,it->second.maps);
    terrainComposites.erase(it);
}
void PublishTerrainComposite(uint64_t key,const fo3terrain::composite::Surface& src) {
    if(!src.valid||!fqrender::nativeOnly||!fqquality::WorldMaterials())return;
    TerrainComposite cell{{},src.minX,src.maxX,src.minZ,src.maxZ,0};
    glGenTextures(2,cell.maps);
    const auto upload=[&](GLuint id,const auto& pixels){
        return id&&fqvk::Texture(id,fo3terrain::composite::Size,fo3terrain::composite::Size,
            reinterpret_cast<const uint8_t*>(pixels.data()),false,true);
    };
    if(!upload(cell.maps[0],src.colour)||!upload(cell.maps[1],src.normal)){glDeleteTextures(2,cell.maps);return;}
    RemoveTerrainComposite(key);
    terrainComposites.emplace(key,cell);
}
void PrepareColdComposites(const std::vector<Q711CpuBatch>& batches) {
    if(!fqrender::nativeOnly||!fqquality::WorldMaterials())return;
    std::unordered_map<uint64_t,std::vector<const Q711CpuBatch*>> groups;
    for(const auto& b:batches)groups[Q711GridKey(b.gridX,b.gridY)].push_back(&b);
    for(const auto& [key,group]:groups){
        const bool ready=std::all_of(group.begin(),group.end(),[](const auto* b){
            return b->vertices.empty()||q1910TerrainTextureCache.count(b->texturePath);
        });
        if(ready)PublishTerrainComposite(key,fo3terrain::composite::Bake(group,compositeSources));
    }
}
void UpdateCompositeBlend() {
    for(auto& entry:terrainComposites){
        auto& cell=entry.second;
        const float x=gFo3EyePosition[0],z=gFo3EyePosition[2];
        const float dx=std::max({cell.minX-x,0.f,x-cell.maxX}),dz=std::max({cell.minZ-z,0.f,z-cell.maxZ});
        const float t=std::clamp((std::hypot(dx,dz)-24.f)/16.f,0.f,1.f);
        cell.blend=t*t*(3.f-2.f*t);
    }
}
const TerrainComposite* CompositeFor(const Q711TerrainBatch& batch) {
    const auto it=terrainComposites.find(Q711GridKey(batch.gridX,batch.gridY));
    return it!=terrainComposites.end()&&it->second.blend>0?&it->second:nullptr;
}
void DrawTerrainComposite(const Q711TerrainBatch& batch,const TerrainComposite& cell) {
    glUniform4f(terrainCompositeRegion,float(batch.gridX),float(batch.gridY),cell.blend,1);
    glUniform1i(q76bUseTextureLocation,1);
    glUniform1i(q1060TerrainUseNormalLocation,1);
    glUniform1f(q1060TerrainSpecularLocation,1);
    glUniform1i(q238PaintCountLocation,0);
    glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,cell.maps[1]);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,cell.maps[0]);
    glBindVertexArray(batch.vao);
    Q237DrawTerrainBatch(batch);
    glUniform4f(terrainCompositeRegion,0,0,0,0);
}
