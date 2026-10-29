#include <filesystem>
void PipelineCacheReuse(){
    const auto path=std::filesystem::temp_directory_path()/("fq-pipeline-cache-"+std::to_string(fqvk::Renderer::NowUs()));
    std::filesystem::create_directories(path);
    const auto xr=reinterpret_cast<XrInstance>(uintptr_t(1));
    assert(fqvk::InitializeWorldProbe(xr,1,64,64,path.c_str()));
    auto& cold=*fqvk::renderer;assert(cold.pipelineCache);
    std::vector<fqvk::PipelineKey> keys;
    fqvk::PipelineKey world;world.stride=18;world.lighting=2;world.noLocal=true;world.noDiscard=true;keys.push_back(world);
    auto terrain=world;terrain.terrain=true;terrain.stride=12;terrain.lighting=0;keys.push_back(terrain);
    for(uint32_t paints=1;paints<=3;++paints){auto fused=terrain;fused.fused=true;fused.stride=15;fused.paintCount=paints;keys.push_back(fused);}
    for(const auto& key:keys){assert(cold.PrewarmCompatible(key));assert(fqvk::PipelineKey::FromWords(key.Words())==key);cold.Pipeline(key);}
    auto redundant=world;redundant.clockwise=true;redundant.cullFace=GL_FRONT;
    redundant.src=GL_DST_ALPHA;redundant.equation=GL_FUNC_SUBTRACT;
    const auto count=cold.pipelines.size();assert(cold.Pipeline(redundant)==cold.Pipeline(world));assert(cold.pipelines.size()==count);
    cold.SavePipelineCache(true);const auto cachePath=cold.pipelineCachePath;
    assert(std::filesystem::file_size(cachePath+".bin")>=32);
    assert(std::filesystem::file_size(cachePath+".keys")==keys.size()*sizeof(keys[0].Words()));
    fqvk::ShutdownWorldProbe();
    assert(fqvk::InitializeWorldProbe(xr,1,64,64,path.c_str()));auto& warm=*fqvk::renderer;
    fqvk::PrewarmPipelines();assert(warm.pipelines.size()>=keys.size());
    for(const auto& key:keys)assert(warm.pipelines.count(key));
    // Invalid flags normalize differently and cannot become an executable recipe.
    auto malformed=world.Words();malformed[1]=123;
    assert(fqvk::PipelineKey::FromWords(malformed).Words()!=malformed);
    warm.pipelineCacheDirty=false;fqvk::ShutdownWorldProbe();
    // Corrupt and truncated cache data is optional: discard it and create cold.
    {std::ofstream file(cachePath+".bin",std::ios::binary|std::ios::trunc);file<<"bad cache";}
    {std::ofstream file(cachePath+".keys",std::ios::binary|std::ios::trunc);file<<"bad recipe";}
    assert(fqvk::InitializeWorldProbe(xr,1,64,64,path.c_str()));fqvk::renderer->PrewarmPipelines(false);assert(fqvk::renderer->pipelines.empty());
    fqvk::ShutdownWorldProbe();std::filesystem::remove_all(path);
    std::cout<<"Native pipeline cache: device/shader-keyed save/reload, startup recipe warmup, exact pipeline state and corrupt cache fallback passed\n";
}
void StereoWaterVisibility(){
    const auto xr=reinterpret_cast<XrInstance>(uintptr_t(1));assert(fqvk::InitializeWorldProbe(xr,1,64,64));
    float left[16]{},right[16]{},eye[3]{};left[0]=left[5]=left[10]=left[15]=1;std::copy_n(left,16,right);right[12]=-20;
    assert(fqvk::BeginWorldProbe(left,right,eye,eye));
    const float leftOnly[]{0,0,0},rightOnly[]{20,0,0},outside[]{50,0,0},nearBoundary[]{7,0,0};
    assert(fqvk::StereoBoundsVisible(leftOnly,1,3));assert(fqvk::StereoBoundsVisible(rightOnly,1,3));
    assert(!fqvk::StereoBoundsVisible(outside,1,3));assert(fqvk::StereoBoundsVisible(nearBoundary,1,3));
    assert(!fqvk::renderer->snapshotReady);fqvk::EndWorldProbe();fqvk::ShutdownWorldProbe();
    std::cout<<"Water visibility: either-eye contribution, conservative boundary and visibility queries leave snapshot untouched passed\n";
}
