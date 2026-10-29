// Included inside Renderer. Vulkan objects stay on the render thread; the file
// writer owns byte copies only. Cache failure is optional, never a render failure.
bool pipelineCacheDirty=false,warmingPipelines=false;
std::string pipelineCachePath;
std::thread pipelineWriter;
std::atomic<bool> pipelineWriterDone{true};
uint64_t lastPipelineSaveUs=0;
static constexpr size_t PipelineCacheLimit=64u*1024u*1024u,RecipeLimit=4096;

static uint64_t PipelineChecksum(const uint8_t* data,size_t size){
    uint64_t hash=1469598103934665603ull;
    for(size_t i=0;i<size;++i)hash=(hash^data[i])*1099511628211ull;
    return hash;
}
static std::vector<uint8_t> ReadPipelineFile(const std::string& path,size_t limit){
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file)return {};
    const auto size=file.tellg();if(size<=0||uint64_t(size)>limit)return {};
    std::vector<uint8_t> bytes(static_cast<size_t>(size));file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(bytes.data()),size))return {};
    return bytes;
}
static bool WritePipelineFile(const std::string& path,const std::vector<uint8_t>& bytes){
    const auto temporary=path+".tmp";
    {std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
     if(!file||!file.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size())))return false;
     file.flush();if(!file)return false;}
    return std::rename(temporary.c_str(),path.c_str())==0;
}
void CreatePipelineCache(){
    uint64_t fingerprint=1469598103934665603ull;
    const auto hash=[&](const void* data,size_t bytes){const auto* p=static_cast<const uint8_t*>(data);for(size_t i=0;i<bytes;++i)fingerprint=(fingerprint^p[i])*1099511628211ull;};
    // Explicit recipe schema version plus shaders, driver and cache UUID.
    const uint32_t schema=1;hash(&schema,sizeof(schema));
    for(const auto& program:shaders::programs){hash(program.vertex,program.vertexBytes);hash(program.fragment,program.fragmentBytes);}
    for(const auto& fragment:shaders::solidFragments)hash(fragment.code,fragment.bytes);
    hash(&properties.vendorID,4);hash(&properties.deviceID,4);hash(&properties.driverVersion,4);
    hash(properties.pipelineCacheUUID,VK_UUID_SIZE);
    if(!outputDirectory.empty())pipelineCachePath=outputDirectory+"/vulkan-pipelines-"+std::to_string(fingerprint);
    auto bytes=pipelineCachePath.empty()?std::vector<uint8_t>{}:ReadPipelineFile(pipelineCachePath+".bin",PipelineCacheLimit+16);
    if(!bytes.empty()){
        const auto valid=[&](){
            if(bytes.size()<16)return false;
            uint64_t magic=0,checksum=0;std::memcpy(&magic,bytes.data(),8);std::memcpy(&checksum,bytes.data()+8,8);
            return magic==0x31504351464bull&&checksum==PipelineChecksum(bytes.data()+16,bytes.size()-16);
        };
        if(!valid())bytes.clear();else bytes.erase(bytes.begin(),bytes.begin()+16);
    }
    if(!bytes.empty()){
        // Validate the standard header before passing optional disk bytes to a driver.
        std::array<uint32_t,4> header{};
        if(bytes.size()>=32)std::memcpy(header.data(),bytes.data(),16);
        if(bytes.size()<32||header[0]<32||header[0]>bytes.size()||header[1]!=VK_PIPELINE_CACHE_HEADER_VERSION_ONE||
           header[2]!=properties.vendorID||header[3]!=properties.deviceID||std::memcmp(bytes.data()+16,properties.pipelineCacheUUID,VK_UUID_SIZE))bytes.clear();
    }
    VkPipelineCacheCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};info.initialDataSize=bytes.size();info.pInitialData=bytes.data();
    auto result=vkCreatePipelineCache(device,&info,nullptr,&pipelineCache);
    if(result!=VK_SUCCESS&&!bytes.empty()){info.initialDataSize=0;info.pInitialData=nullptr;bytes.clear();result=vkCreatePipelineCache(device,&info,nullptr,&pipelineCache);}
    if(result!=VK_SUCCESS){pipelineCache={};VKLOG("FQVK PIPELINE CACHE unavailable=%d",int(result));}
    else VKLOG("FQVK PIPELINE CACHE loadedBytes=%zu schema=%u",bytes.size(),schema);
}
bool PrewarmCompatible(const PipelineKey& key)const{
    if(key.ui&&key.ui!=9&&key.ui!=10&&key.ui!=11)return false;
    if(key.lighting>2||key.paintCount>3||!key.stride||key.stride>64)return false;
    if(key.fused&&(!key.terrain||key.stride!=15||!key.paintCount))return false;
    if(int(bool(key.ui))+int(key.sky)+int(key.water)+int(key.terrain)>1)return false;
    if(key.noDiscard&&(key.ui||key.sky||key.water))return false;
    if(key.cullFace!=GL_FRONT&&key.cullFace!=GL_BACK&&key.cullFace!=GL_FRONT_AND_BACK)return false;
    if(key.depthFunc<GL_NEVER||key.depthFunc>GL_ALWAYS)return false;
    const auto equation=[](uint32_t e){return e==GL_FUNC_ADD||e==GL_FUNC_SUBTRACT||e==GL_FUNC_REVERSE_SUBTRACT||e==GL_MIN||e==GL_MAX;};
    const auto blend=[](uint32_t b){return b==GL_ZERO||b==GL_ONE||(b>=GL_SRC_COLOR&&b<=GL_SRC_ALPHA_SATURATE);};
    if(!equation(key.equation)||!equation(key.equationA)||!blend(key.src)||!blend(key.dst)||!blend(key.srcA)||!blend(key.dstA))return false;
    if(key.offscreen&&!uiPass)return false;
    if(key.swapchain&&!xrOverlayPass)return false;
    if(key.modal&&!(key.swapchain?modalXrPass:modalPass))return false;
    if(key.split&&(!splitPass||!splitModules[0]))return false;
    if(key.foveated&&!(key.split?densitySplit.clear:densityMain.clear))return false;
    return true;
}
void PrewarmPipelines(bool seed=true){
    if(!device||begun||pipelineCachePath.empty())return;
    using Words=decltype(PipelineKey{}.Words());
    auto bytes=ReadPipelineFile(pipelineCachePath+".keys",sizeof(Words)*RecipeLimit);
    if(bytes.size()%sizeof(Words))bytes.clear();
    const auto started=NowUs();size_t warmed=0;warmingPipelines=true;
    for(size_t offset=0;offset<bytes.size();offset+=sizeof(Words)){
        Words words{};std::memcpy(words.data(),bytes.data()+offset,sizeof(words));
        const auto key=PipelineKey::FromWords(words);
        if(key.Words()!=words||!PrewarmCompatible(key)||pipelines.count(key))continue;
        try{Pipeline(key);++warmed;}catch(const std::exception& e){VKLOG("FQVK PIPELINE WARM skipped=%s",e.what());}
    }
    if(seed)for(bool split:{false,true}){
        if(split&&(!splitPass||!compositionChain))continue;
        for(bool noLocal:{false,true}){
            PipelineKey base;base.noLocal=noLocal;base.noDiscard=true;base.split=split;
            base.foveated=foveationRequested&&bool(split?densitySplit.clear:densityMain.clear);
            for(int material=0;material<5;++material){
                auto key=base;key.terrain=true;key.stride=material<2?12:15;
                key.fused=material>=2;key.paintCount=key.fused?uint32_t(material-1):0;
                if(material==1){key.write=false;key.blend=true;key.src=key.srcA=GL_SRC_ALPHA;key.dst=key.dstA=GL_ONE_MINUS_SRC_ALPHA;}
                if(!pipelines.count(key))try{Pipeline(key);++warmed;}
                catch(const std::exception& e){VKLOG("FQVK PIPELINE WARM seedSkipped=%s",e.what());}
            }
            for(int layout=0;layout<3;++layout){
                auto key=base;key.stride=18;key.lighting=2;key.cull=true;key.noEnvironment=true;
                key.instanced=layout==1;key.skinned=layout==2;
                if(!pipelines.count(key))try{Pipeline(key);++warmed;}
                catch(const std::exception& e){VKLOG("FQVK PIPELINE WARM seedSkipped=%s",e.what());}
            }
        }
    }
    warmingPipelines=false;
    VKLOG("FQVK PIPELINE WARM pipelines=%zu elapsedUs=%llu phase=startup",warmed,(unsigned long long)(NowUs()-started));
}
void SavePipelineCache(bool finish=false)noexcept{
    if(pipelineWriter.joinable()){
        if(!finish&&!pipelineWriterDone.load(std::memory_order_acquire))return;
        pipelineWriter.join();
    }
    if(!device||!pipelineCache||pipelineCachePath.empty()||!pipelineCacheDirty)return;
    if(!finish&&NowUs()-lastPipelineSaveUs<10000000)return;
    try{
        const auto started=NowUs();size_t size=0;
        if(vkGetPipelineCacheData(device,pipelineCache,&size,nullptr)!=VK_SUCCESS||!size||size>PipelineCacheLimit)return;
        std::vector<uint8_t> bytes(size);
        if(vkGetPipelineCacheData(device,pipelineCache,&size,bytes.data())!=VK_SUCCESS)return;
        bytes.resize(size);
        std::vector<uint8_t> recipes;
        using Words=decltype(PipelineKey{}.Words());
        for(const auto& entry:pipelines){if(recipes.size()>=sizeof(Words)*RecipeLimit)break;const auto words=entry.first.Words();const auto* data=reinterpret_cast<const uint8_t*>(words.data());recipes.insert(recipes.end(),data,data+sizeof(words));}
        const auto path=pipelineCachePath;pipelineWriterDone.store(false,std::memory_order_release);
        pipelineWriter=std::thread([this,path,bytes=std::move(bytes),recipes=std::move(recipes)]()mutable{
            try{
                const uint64_t magic=0x31504351464bull,checksum=PipelineChecksum(bytes.data(),bytes.size());
                bytes.insert(bytes.begin(),16,0);std::memcpy(bytes.data(),&magic,8);std::memcpy(bytes.data()+8,&checksum,8);
                const bool ok=WritePipelineFile(path+".bin",bytes)&&WritePipelineFile(path+".keys",recipes);
                VKLOG("FQVK PIPELINE CACHE saved=%d bytes=%zu recipes=%zu",ok,bytes.size(),recipes.size()/sizeof(Words));
            }catch(const std::exception& e){VKLOG("FQVK PIPELINE CACHE writerSkipped=%s",e.what());}
            pipelineWriterDone.store(true,std::memory_order_release);
        });
        pipelineCacheDirty=false;lastPipelineSaveUs=NowUs();
        VKLOG("FQVK PIPELINE CACHE snapshotUs=%llu",(unsigned long long)(NowUs()-started));
        if(finish)pipelineWriter.join();
    }catch(const std::exception& e){pipelineWriterDone.store(true,std::memory_order_release);VKLOG("FQVK PIPELINE CACHE saveSkipped=%s",e.what());}
}
