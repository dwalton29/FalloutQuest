#include "ui/fo3-system-colour.h"
#include "fo3-vulkan-renderer.h"
#include "fo3-vulkan-bridge.h"
#include "fo3-vulkan-shaders.h"
#include "rendering/spacewarp/fo3-motion-vectors.h"
#include "fo3-vulkan-foveation.h"
#include <GLES3/gl3.h> // Legacy numeric material enums only; no GLES calls.
#include <algorithm>
#include <array>
#include <cstring>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <cstdio>
#include <thread>
#include <atomic>
#include "rendering/terrain/fo3-terrain-bounds.h"
#include <cmath>
#include <chrono>
#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>
#ifdef __ANDROID__
#include <android/log.h>
#define VKLOG(...) __android_log_print(ANDROID_LOG_INFO,"FalloutQuest",__VA_ARGS__)
#else
#include <cstdio>
#define VKLOG(...) do { std::fprintf(stderr,__VA_ARGS__);std::fputc('\n',stderr); } while(0)
#endif

namespace fqvk {
namespace {
void Check(VkResult r,const char* what) {
    if(r!=VK_SUCCESS)throw std::runtime_error(std::string(what)+": VkResult="+std::to_string(r));
}
void CheckXr(XrResult r,const char* what) {
    if(XR_FAILED(r))throw std::runtime_error(std::string(what)+": XrResult="+std::to_string(r));
}
template<class T> T Proc(XrInstance xr,const char* name) {
    PFN_xrVoidFunction fn=nullptr;CheckXr(xrGetInstanceProcAddr(xr,name,&fn),name);
    if(!fn)throw std::runtime_error(name);
    return reinterpret_cast<T>(fn);
}
struct Buffer {VkBuffer buffer{};VkDeviceMemory memory{};void* mapped{};VkDeviceSize size{},allocationBytes{},capacity{};};
struct Image {VkImage image{};VkImageView view{};VkDeviceMemory memory{};uint32_t levels=1,width=0,height=0;bool palette=false,clamp=false,repeatT=false,repeatS=false;};
struct PostTarget {Image image;VkFramebuffer framebuffer{},overlay{};uint32_t width=0,height=0;bool initialized=false;};
struct UiTarget {PostTarget target;VkImageView sampled{};};
struct MeshData {Buffer vertices,indices,skin;uint32_t count=0,stride=0;};
struct ProgramData {
    bool terrain=false,fused=false,sky=false,water=false;int ui=0;
    std::vector<uint8_t> bytes;
    struct Location {int location;shaders::Field field;};
    std::vector<Location> fields;
    std::vector<int> samplers;
    std::vector<int> units;
    struct Binding { uint32_t offset=0,components=0,count=0;int unit=-1; };
    std::unordered_map<int,Binding> bindings;bool bindingsReady=false;
    uint32_t lightingOffset=UINT32_MAX,localOffset=UINT32_MAX,environmentOffset=UINT32_MAX,paintOffset=UINT32_MAX,skinOffset=UINT32_MAX,roundOffset=UINT32_MAX;
    std::array<uint32_t,3> discardOffsets{{UINT32_MAX,UINT32_MAX,UINT32_MAX}};
};
void ConfigureDiscardGuards(ProgramData& p,const shaders::Program& shader) {
    for(size_t i=0;i<shader.fieldCount;++i){const auto& f=shader.fields[i];if(!std::strcmp(f.name,"uNoLighting"))p.lightingOffset=f.offset;if(!std::strcmp(f.name,"uLocalLightCount"))p.localOffset=f.offset;if(!std::strcmp(f.name,"uEnvironmentPassQ2050"))p.environmentOffset=f.offset;if(!std::strcmp(f.name,"uPaintCount"))p.paintOffset=f.offset;if(!std::strcmp(f.name,"uActorSkinMode"))p.skinOffset=f.offset;if(!std::strcmp(f.name,"uRoundMode"))p.roundOffset=f.offset;}
    const char* guards[]{"uWaterReflectionClipEnabledQ2090","uNativeLodClipEnabledQ1810","uAlphaTest"};
    for(size_t i=0;i<shader.fieldCount;++i)for(size_t j=0;j<3;++j)
        if(!std::strcmp(shader.fields[i].name,guards[j]))p.discardOffsets[j]=shader.fields[i].offset;
}
struct PipelineKey {
    uint32_t ui=0;bool strip=false,offscreen=false,swapchain=false,noDiscard=false,modal=false;
    uint32_t paintCount=0,lighting=0;bool noLocal=false,noEnvironment=false,foveated=false;
    bool split=false,offset=false,water=false,sky=false,skinned=false,instanced=false,terrain=false,fused=false,depth=true,write=true,blend=false,cull=false,clockwise=false;
    uint32_t cullFace=GL_BACK,equation=GL_FUNC_ADD,equationA=GL_FUNC_ADD,stride=0,depthFunc=GL_LEQUAL,src=GL_ONE,dst=GL_ZERO,srcA=GL_ONE,dstA=GL_ZERO;
    std::array<uint32_t,33> Words()const {return {{uint32_t(ui),uint32_t(strip),uint32_t(offscreen),uint32_t(swapchain),uint32_t(noDiscard),uint32_t(modal),uint32_t(paintCount),uint32_t(lighting),uint32_t(noLocal),uint32_t(noEnvironment),uint32_t(foveated),uint32_t(split),uint32_t(offset),uint32_t(water),uint32_t(sky),uint32_t(skinned),uint32_t(instanced),uint32_t(terrain),uint32_t(fused),uint32_t(depth),uint32_t(write),uint32_t(blend),uint32_t(cull),uint32_t(clockwise),uint32_t(cullFace),uint32_t(equation),uint32_t(equationA),uint32_t(stride),uint32_t(depthFunc),uint32_t(src),uint32_t(dst),uint32_t(srcA),uint32_t(dstA)}};}
    static PipelineKey FromWords(const std::array<uint32_t,33>& words){PipelineKey key;key.ui=words[0];key.strip=words[1];key.offscreen=words[2];key.swapchain=words[3];key.noDiscard=words[4];key.modal=words[5];key.paintCount=words[6];key.lighting=words[7];key.noLocal=words[8];key.noEnvironment=words[9];key.foveated=words[10];key.split=words[11];key.offset=words[12];key.water=words[13];key.sky=words[14];key.skinned=words[15];key.instanced=words[16];key.terrain=words[17];key.fused=words[18];key.depth=words[19];key.write=words[20];key.blend=words[21];key.cull=words[22];key.clockwise=words[23];key.cullFace=words[24];key.equation=words[25];key.equationA=words[26];key.stride=words[27];key.depthFunc=words[28];key.src=words[29];key.dst=words[30];key.srcA=words[31];key.dstA=words[32];return key;}
    void Normalize(){
        if(!cull){clockwise=false;cullFace=GL_BACK;}
        if(!blend){src=srcA=GL_ONE;dst=dstA=GL_ZERO;equation=equationA=GL_FUNC_ADD;}
        if(!depth){write=false;depthFunc=GL_ALWAYS;}
    }
    bool operator==(const PipelineKey& k)const {
        return paintCount==k.paintCount&&lighting==k.lighting&&noLocal==k.noLocal&&noEnvironment==k.noEnvironment&&foveated==k.foveated&&split==k.split&&modal==k.modal&&noDiscard==k.noDiscard&&swapchain==k.swapchain&&offscreen==k.offscreen&&strip==k.strip&&ui==k.ui&&water==k.water&&offset==k.offset&&cullFace==k.cullFace&&equation==k.equation&&equationA==k.equationA&&sky==k.sky && skinned==k.skinned && instanced==k.instanced && terrain==k.terrain && fused==k.fused && depth==k.depth && write==k.write && blend==k.blend &&
            cull==k.cull && clockwise==k.clockwise && stride==k.stride && depthFunc==k.depthFunc &&
            src==k.src && dst==k.dst && srcA==k.srcA && dstA==k.dstA;
    }
};
struct PipelineHash {size_t operator()(const PipelineKey& k)const {
    size_t h=uint32_t(k.swapchain)*8191+uint32_t(k.offscreen)*4093+uint32_t(k.strip)*2017+k.ui*1009+uint32_t(k.water)*997+k.stride+uint32_t(k.instanced)*31+uint32_t(k.skinned)+uint32_t(k.sky)*101;for(auto v:{uint32_t(k.terrain),uint32_t(k.fused),uint32_t(k.depth),uint32_t(k.write),uint32_t(k.blend),
        k.paintCount,k.lighting,uint32_t(k.noLocal),uint32_t(k.noEnvironment),uint32_t(k.foveated),uint32_t(k.split),uint32_t(k.modal),uint32_t(k.noDiscard),uint32_t(k.cull),uint32_t(k.clockwise),uint32_t(k.offset),k.cullFace,k.equation,k.equationA,k.depthFunc,k.src,k.dst,k.srcA,k.dstA}) { h=(h*16777619u)^v; }
    return h;
}};
VkBlendFactor Blend(uint32_t x) {
    switch(x) {
        case GL_SRC_ALPHA_SATURATE:return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        case GL_ZERO:return VK_BLEND_FACTOR_ZERO;case GL_ONE:return VK_BLEND_FACTOR_ONE;
        case GL_SRC_ALPHA:return VK_BLEND_FACTOR_SRC_ALPHA;case GL_ONE_MINUS_SRC_ALPHA:return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case GL_DST_ALPHA:return VK_BLEND_FACTOR_DST_ALPHA;case GL_ONE_MINUS_DST_ALPHA:return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case GL_SRC_COLOR:return VK_BLEND_FACTOR_SRC_COLOR;case GL_ONE_MINUS_SRC_COLOR:return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case GL_DST_COLOR:return VK_BLEND_FACTOR_DST_COLOR;case GL_ONE_MINUS_DST_COLOR:return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        default:throw std::runtime_error("unsupported blend factor");
    }
}
VkBlendOp BlendOperation(uint32_t op) {
    switch(op) {
        case GL_FUNC_ADD:return VK_BLEND_OP_ADD;case GL_FUNC_SUBTRACT:return VK_BLEND_OP_SUBTRACT;
        case GL_FUNC_REVERSE_SUBTRACT:return VK_BLEND_OP_REVERSE_SUBTRACT;
        case GL_MIN:return VK_BLEND_OP_MIN;case GL_MAX:return VK_BLEND_OP_MAX;
        default:throw std::runtime_error("unsupported blend operation");
    }
}
VkCompareOp Compare(uint32_t x) {
    switch(x) {
        case GL_LESS:return VK_COMPARE_OP_LESS;case GL_LEQUAL:return VK_COMPARE_OP_LESS_OR_EQUAL;
        case GL_EQUAL:return VK_COMPARE_OP_EQUAL;case GL_ALWAYS:return VK_COMPARE_OP_ALWAYS;
        case GL_NEVER:return VK_COMPARE_OP_NEVER;case GL_GREATER:return VK_COMPARE_OP_GREATER;
        case GL_GEQUAL:return VK_COMPARE_OP_GREATER_OR_EQUAL;case GL_NOTEQUAL:return VK_COMPARE_OP_NOT_EQUAL;
        default:throw std::runtime_error("unsupported depth function");
    }
}
class Renderer {
public:
    VkInstance instance{};VkPhysicalDevice physical{};VkDevice device{};VkQueue queue{};uint32_t family=0;
    VkPhysicalDeviceProperties properties{};VkPhysicalDeviceMemoryProperties memory{};
    VkCommandPool pool{},uploads{};VkCommandBuffer command{};VkFence fence{};
    VkRenderPass pass{},resumePass{};VkFramebuffer framebuffer{};Image color,depth,white,cube,sceneColor,sceneDepth,modalDepth;
    VkRenderPass modalPass{},modalXrPass{};VkFramebuffer modalFramebuffer{};
    std::vector<VkFramebuffer> modalXrFramebuffers;VkFormat xrColorFormat=VK_FORMAT_UNDEFINED;
    bool snapshotReady=false,snapshotInitialized=false;
    VkSampler sampler{},nearest{},clamped{},verticalRepeat{},horizontalRepeat{};VkDescriptorSetLayout sets[14]{};VkPipelineLayout layouts[14]{};
    VkPipelineCache pipelineCache{};
    VkShaderModule modules[13][2]{};VkDescriptorPool descriptors{};
    VkShaderModule solidModules[3]{};bool genericShaders=false;
    uint64_t foveatedDraws=0,specializedDraws=0;
    uint64_t solidDraws=0,guardedDraws=0,worldDraws=0,landDraws=0,fusedLandDraws=0;
    VkQueryPool timestamps{};bool timestampSupported=false;uint32_t timestampBits=0;
    enum GpuPass {Scene,SceneResume,Ui,Adapt,Bloom,Film,Output,Snapshot,DepthCopy,Overlay,Motion,Composition,PassCount};
    static constexpr uint32_t QueryCount=4096;
    struct GpuSpan {GpuPass kind;uint32_t begin,end;};
    bool passTiming=false;uint32_t queryCursor=4,timingDropped=0;int activeSpan=-1;
    std::vector<GpuSpan> gpuSpans;
    std::array<double,PassCount> passMs{};
    uint64_t submissionSerial=0;
    uint64_t uniformCalls=0,uniformUnchanged=0;
    uint64_t queueCpuUs=0,drawCpuUs=0,reuseWaitUs=0,finishWaitUs=0,xrWaitUs=0,recordStarted=0,recordSpanUs=0;
    uint64_t uploadPrepareUs=0,lastUploadPrepareUs=0;
    struct UploadCpu {
        Renderer& owner;uint64_t started;
        explicit UploadCpu(Renderer& r):owner(r),started(r.passTiming?NowUs():0){}
        ~UploadCpu(){if(started)owner.uploadPrepareUs+=NowUs()-started;}
    };
    static uint64_t NowUs(){return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
    void EndTiming() {
        if(passOpen)throw std::runtime_error("GPU timer boundary inside render pass");
        if(activeSpan<0)return;
        vkCmdWriteTimestamp(command,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,timestamps,gpuSpans[size_t(activeSpan)].end);activeSpan=-1;
    }
    void Phase(GpuPass kind) {
        if(passOpen)throw std::runtime_error("GPU timer boundary inside render pass");
        if(!passTiming||!timestamps)return;
        if(activeSpan>=0&&gpuSpans[size_t(activeSpan)].kind==kind)return;
        EndTiming();
        // Each multiview timestamp reserves two query slots, including when a
        // boundary is outside a render pass. Never read unwritten companion slots.
        if(queryCursor+4>QueryCount){++timingDropped;return;}
        gpuSpans.push_back({kind,queryCursor,queryCursor+2});queryCursor+=4;activeSpan=int(gpuSpans.size()-1);
        vkCmdWriteTimestamp(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,timestamps,gpuSpans.back().begin);
    }
    void BeginPass(GpuPass kind,const VkRenderPassBeginInfo& info) {
        Phase(kind);vkCmdBeginRenderPass(command,&info,VK_SUBPASS_CONTENTS_INLINE);passOpen=true;
    }
    void EndPass() {
        vkCmdEndRenderPass(command);passOpen=false;EndTiming();
    }
    void ReadTimings(VkQueryPool queries,const std::vector<GpuSpan>& spans,uint32_t& dropped) {
        passMs.fill(0);
        for(const auto& span:spans) {
            uint64_t begin=0,end=0;
            if(vkGetQueryPoolResults(device,queries,span.begin,1,8,&begin,8,VK_QUERY_RESULT_64_BIT)!=VK_SUCCESS||
               vkGetQueryPoolResults(device,queries,span.end,1,8,&end,8,VK_QUERY_RESULT_64_BIT)!=VK_SUCCESS){++dropped;continue;}
            const uint64_t mask=timestampBits==64?~uint64_t{0}:(uint64_t{1}<<timestampBits)-1;
            passMs[span.kind]+=double((end-begin)&mask)*properties.limits.timestampPeriod/1e6;
        }
    }
    Buffer arena,stereo;VkDeviceSize cursor=0;uint32_t width=0,height=0;
    std::unordered_map<uint32_t,MeshData> meshes;
    std::unordered_map<uint32_t,Image> textures;
    std::unordered_map<uint32_t,ProgramData> programs;
    std::unordered_map<PipelineKey,VkPipeline,PipelineHash> pipelines;
    VkRenderPass postPass{},adaptPass{};
    VkPipeline postPipelines[5]{};VkFramebuffer postOverlayFramebuffer{};
    PostTarget adapted[2],bloom[3],presented;
    template<class T>struct Retired {T resource;uint64_t frame,upload;};
    std::unordered_map<uint32_t,UiTarget> uiTargets;std::vector<Retired<UiTarget>> retiredUi;
    UiTarget* activeUi=nullptr;bool offscreen=false,modalActive=false;uint32_t targetWidth=0,targetHeight=0;VkRenderPass uiPass{};
    int adaptIndex=0;bool exposureFirst=true,postDone=false,passOpen=false;
    std::vector<Retired<Buffer>> retiredBuffers;std::vector<Retired<Image>> retiredImages;
    uint64_t draws=0,vertices=0,skipped=0,frames=0;std::array<uint64_t,6> skipReasons{};uint32_t firstSkippedProgram=0,visualDiagnostics=0;bool begun=false;
    bool failed=false;
    // The OpenXR runtime owns these images. Only our views/framebuffers are destroyed.
    XrInstance xrInstance{};XrSession xrSession{};XrSwapchain xrSwapchain{};
    std::vector<PostTarget> xrTargets;
    XrSwapchain xrDepthSwapchain{};
    std::vector<XrSwapchainImageVulkan2KHR> xrDepthImages;
    XrCompositionLayerDepthInfoKHR xrDepthInfo[2]{{XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR},{XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR}};
    VkRenderPass xrPass{},xrOverlayPass{};VkPipeline xrPipeline{},xrFilmPipeline{};
    bool legacyPresentation=false,directOutput=false,colorAcquired=false,xrSrgb=false;
    uint32_t acquiredColor=0;
    uint64_t presentationCopies=0,directFilmFrames=0;
    bool gameplay=false;
    std::string outputDirectory;
    bool dumped=false,loadingFrame=false;
    struct DescriptorKey {
        int kind=0;uint32_t uniformBytes=0;std::vector<VkDescriptorImageInfo> images;
        bool operator==(const DescriptorKey& other)const {
            if(kind!=other.kind||uniformBytes!=other.uniformBytes||images.size()!=other.images.size())return false;
            for(size_t i=0;i<images.size();++i)if(images[i].sampler!=other.images[i].sampler||images[i].imageView!=other.images[i].imageView||images[i].imageLayout!=other.images[i].imageLayout)return false;
            return true;
        }
    };
    struct DescriptorHash {size_t operator()(const DescriptorKey& key)const {
        size_t hash=size_t(key.kind)*16777619u+key.uniformBytes;
        for(const auto& image:key.images){hash=(hash*16777619u)^std::hash<VkImageView>{}(image.imageView);hash=(hash*16777619u)^std::hash<VkSampler>{}(image.sampler);hash=(hash*16777619u)^size_t(image.imageLayout);}
        return hash;
    }};
    std::unordered_map<DescriptorKey,VkDescriptorSet,DescriptorHash> descriptorCache;
    static constexpr uint32_t DescriptorCapacity=16384,FrameDescriptorReserve=8192;
    uint64_t descriptorAllocations=0,descriptorHits=0,descriptorResets=0;
    uint64_t viewRevision=0,descriptorRevision=0;
    void PrepareDescriptors() {
        // Called after the frame fence. No cached set is reset while in flight.
        const size_t reserve=FrameDescriptorReserve+(motionPass?fqspacewarp::History::MaxDrawables+128:0);
        if(descriptorRevision!=viewRevision||descriptorCache.size()>DescriptorCapacity-reserve) {
            Check(vkResetDescriptorPool(device,descriptors,0),"recycle descriptors");
            descriptorCache.clear();descriptorRevision=viewRevision;++descriptorResets;
        }
    }
    VkDescriptorSet Descriptor(int kind,uint32_t uniformBytes,const std::vector<VkDescriptorImageInfo>& images) {
        DescriptorKey key{kind,uniformBytes,images};
        const auto cached=descriptorCache.find(key);
        if(cached!=descriptorCache.end()){++descriptorHits;return cached->second;}
        VkDescriptorSet set{};VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};ai.descriptorPool=descriptors;ai.descriptorSetCount=1;ai.pSetLayouts=&sets[kind];
        Check(vkAllocateDescriptorSets(device,&ai,&set),"allocate cached descriptor");
        const std::array<VkDescriptorBufferInfo,2> buffers{{{arena.buffer,0,uniformBytes},{stereo.buffer,0,432}}};
        std::vector<VkWriteDescriptorSet> writes(images.size()+2);
        for(size_t i=0;i<writes.size();++i){auto& w=writes[i];w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;w.dstSet=set;w.dstBinding=uint32_t(i);w.descriptorCount=1;
            w.descriptorType=i==0?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:i==1?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            if(i<2)w.pBufferInfo=&buffers[i];else w.pImageInfo=&images[i-2];}
        vkUpdateDescriptorSets(device,uint32_t(writes.size()),writes.data(),0,nullptr);
        descriptorCache.emplace(std::move(key),set);++descriptorAllocations;return set;
    }
    struct FrameSample {
        uint64_t solid=0,guarded=0,world=0,land=0,fusedLand=0,foveated=0,specialized=0;bool compact=false;
        uint64_t serial=0,frame=0,copies=0,record=0,queueCpu=0,drawCpu=0,reuse=0,finish=0,xrWait=0,uploadCpu=0;
        uint64_t drawCount=0,vertexCount=0,skipCount=0;std::array<uint64_t,6> reasons{};uint32_t firstSkip=0,diagnostic=0,dropped=0;
        bool offscreen=false,profile=false,supported=false,direct=false,logFrame=false,buffered=false;
        std::vector<GpuSpan> spans;
    };
    struct FrameSlot {
        VkCommandPool pool{};VkCommandBuffer command{};VkFence fence{};VkQueryPool queries{};
        Buffer arena,stereo;VkDescriptorPool descriptors{};
        std::unordered_map<DescriptorKey,VkDescriptorSet,DescriptorHash> cache;
        uint64_t descriptorRevision=0,serial=0;bool pending=false;FrameSample sample;
    };
    std::array<FrameSlot,3> frameSlots;
    size_t activeSlot=0,nextWorldSlot=0;bool contextsReady=false,synchronous=false;
    uint64_t graphicsCompleted=0;
    bool Buffered()const{return gameplay&&fqrender::nativeOnly&&!synchronous;}
    void SwapSlot(FrameSlot& slot) {
        std::swap(pool,slot.pool);std::swap(command,slot.command);std::swap(fence,slot.fence);std::swap(timestamps,slot.queries);
        std::swap(arena,slot.arena);std::swap(stereo,slot.stereo);std::swap(descriptors,slot.descriptors);
        descriptorCache.swap(slot.cache);std::swap(descriptorRevision,slot.descriptorRevision);
    }
    void SelectSlot(size_t index) {
        if(index==activeSlot)return;
        SwapSlot(frameSlots[activeSlot]);activeSlot=index;SwapSlot(frameSlots[activeSlot]);
    }
    VkFence SlotFence(size_t index)const{return index==activeSlot?fence:frameSlots[index].fence;}
    VkQueryPool SlotQueries(size_t index)const{return index==activeSlot?timestamps:frameSlots[index].queries;}
    VkDescriptorPool MakeDescriptorPool() {
        std::array<VkDescriptorPoolSize,3> sizes{{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,DescriptorCapacity},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,DescriptorCapacity},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,DescriptorCapacity*10}}};
        VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pi.maxSets=DescriptorCapacity;pi.poolSizeCount=uint32_t(sizes.size());pi.pPoolSizes=sizes.data();
        VkDescriptorPool result{};Check(vkCreateDescriptorPool(device,&pi,nullptr,&result),"frame descriptor pool");return result;
    }
    void CreateContexts() {
        if(contextsReady)return;
        for(size_t i=1;i<frameSlots.size();++i) {
            auto& slot=frameSlots[i];
            VkCommandPoolCreateInfo ci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};ci.queueFamilyIndex=family;ci.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            Check(vkCreateCommandPool(device,&ci,nullptr,&slot.pool),"buffered command pool");
            VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ai.commandPool=slot.pool;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=1;
            Check(vkAllocateCommandBuffers(device,&ai,&slot.command),"buffered commands");
            VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};fi.flags=VK_FENCE_CREATE_SIGNALED_BIT;
            Check(vkCreateFence(device,&fi,nullptr,&slot.fence),"buffered fence");
            slot.arena=MakeBuffer(32u*1024u*1024u,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,true);
            slot.stereo=MakeBuffer(432,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true);slot.descriptors=MakeDescriptorPool();
            if(timestampSupported){VkQueryPoolCreateInfo qi{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};qi.queryType=VK_QUERY_TYPE_TIMESTAMP;qi.queryCount=QueryCount;
                Check(vkCreateQueryPool(device,&qi,nullptr,&slot.queries),"buffered queries");}
        }
        contextsReady=true;
    }
    void DestroySlot(FrameSlot& slot) {
        Destroy(slot.arena);Destroy(slot.stereo);
        if(slot.descriptors)vkDestroyDescriptorPool(device,slot.descriptors,nullptr);
        if(slot.queries)vkDestroyQueryPool(device,slot.queries,nullptr);
        if(slot.fence)vkDestroyFence(device,slot.fence,nullptr);
        if(slot.pool)vkDestroyCommandPool(device,slot.pool,nullptr);
        slot={};
    }
    void PollFrames() {
        for(;;) {
            size_t oldest=frameSlots.size();
            for(size_t i=0;i<frameSlots.size();++i)if(frameSlots[i].pending&&(oldest==frameSlots.size()||frameSlots[i].serial<frameSlots[oldest].serial))oldest=i;
            if(oldest==frameSlots.size())return;
            const auto status=vkGetFenceStatus(device,SlotFence(oldest));if(status==VK_NOT_READY)return;
            Check(status,"poll frame fence");auto& slot=frameSlots[oldest];slot.pending=false;
            graphicsCompleted=slot.serial;ReportFrame(slot.sample,SlotQueries(oldest));
        }
    }
    void WaitSlot(size_t index) {
        if(frameSlots[index].pending){const auto ready=SlotFence(index);Check(vkWaitForFences(device,1,&ready,VK_TRUE,UINT64_MAX),"reuse buffered frame");}
        PollFrames();
    }
    void WaitFrames(){for(size_t i=0;i<frameSlots.size();++i)WaitSlot(i);}
    struct UploadBatch {
        VkCommandBuffer command{};VkFence fence{};uint64_t serial=0,bytes=0;uint32_t operations=0;
        std::vector<Buffer> staging;
    };
    UploadBatch uploadBatch;
    std::deque<UploadBatch> uploadFlight;
    uint64_t uploadSerial=0,uploadCompleted=0,uploadOperations=0,uploadSubmissions=0,uploadPressureWaits=0,uploadQueueIdles=0;
    uint64_t uploadBytes=0,uploadPeakBytes=0,uploadStagingBuffers=0,uploadPeakBuffers=0;
    uint32_t uploadBufferLimit=512;
    uint64_t uploadBatchLimit=8u*1024u*1024u,uploadMemoryLimit=64u*1024u*1024u;
    uint32_t uploadOperationLimit=128;
    uint64_t paletteAllocations=0,paletteUpdates=0;
    std::vector<Buffer> stagingPool;
    uint64_t stagingPoolBytes=0,stagingPoolLimit=16u*1024u*1024u,stagingAllocations=0,stagingReuses=0,stagingEvictions=0;
    // Buffered world/UI frames can retire >300 small palette buffers together.
    // Retain one full bounded upload working set, still subject to 16 MiB.
    size_t stagingPoolCountLimit=512;
    bool AsyncUploads() const{return fqrender::nativeOnly;}
    Buffer MakeStaging(VkDeviceSize bytes) {
        if(!AsyncUploads())return MakeBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
        PollUploads();
        VkDeviceSize capacity=4096;while(capacity<bytes)capacity*=2;
        auto found=std::find_if(stagingPool.begin(),stagingPool.end(),[&](const Buffer& b){return b.capacity==capacity;});
        if(found!=stagingPool.end()) {
            Buffer result=*found;stagingPoolBytes-=result.allocationBytes;
            *found=stagingPool.back();stagingPool.pop_back();
            result.size=bytes;++stagingReuses;return result;
        }
        ++stagingAllocations;
        auto result=MakeBuffer(capacity,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);result.size=bytes;return result;
    }
    void RecycleStaging(Buffer& staging) {
        if(staging.allocationBytes>stagingPoolLimit||!stagingPoolCountLimit){Destroy(staging);return;}
        // Prefer the small, frequently reused palette/geometry classes. Cold
        // large startup textures must not fill the cache and defeat that reuse.
        while(!stagingPool.empty()&&(stagingPoolBytes>stagingPoolLimit-staging.allocationBytes||stagingPool.size()>=stagingPoolCountLimit)) {
            auto largest=std::max_element(stagingPool.begin(),stagingPool.end(),[](const Buffer& a,const Buffer& b){return a.capacity<b.capacity;});
            if(largest->capacity<staging.capacity){Destroy(staging);return;}
            stagingPoolBytes-=largest->allocationBytes;Destroy(*largest);
            *largest=stagingPool.back();stagingPool.pop_back();++stagingEvictions;
        }
        stagingPoolBytes+=staging.allocationBytes;stagingPool.push_back(staging);staging={};
    }
    void ReleaseUploadBatch(UploadBatch& batch) {
        for(auto& staging:batch.staging){uploadBytes-=staging.allocationBytes;--uploadStagingBuffers;RecycleStaging(staging);}
        if(batch.fence)vkDestroyFence(device,batch.fence,nullptr);
        if(batch.command)vkFreeCommandBuffers(device,uploads,1,&batch.command);
        batch={};
    }
    void PollUploads() {
        while(!uploadFlight.empty()) {
            const auto status=vkGetFenceStatus(device,uploadFlight.front().fence);
            if(status==VK_NOT_READY)break;
            Check(status,"poll upload fence");uploadCompleted=uploadFlight.front().serial;
            ReleaseUploadBatch(uploadFlight.front());uploadFlight.pop_front();
        }
    }
    uint64_t FlushUploads() {
        if(uploadBatch.command) {
            Check(vkEndCommandBuffer(uploadBatch.command),"end batched uploads");
            VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};Check(vkCreateFence(device,&fi,nullptr,&uploadBatch.fence),"upload fence");
            uploadBatch.serial=++uploadSerial;
            uploadFlight.push_back(std::move(uploadBatch));uploadBatch={};
            auto& submitted=uploadFlight.back();
            VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};si.commandBufferCount=1;si.pCommandBuffers=&submitted.command;
            Check(vkQueueSubmit(queue,1,&si,submitted.fence),"submit batched uploads");++uploadSubmissions;
        }
        PollUploads();
        while((uploadBytes>uploadMemoryLimit||uploadStagingBuffers>uploadBufferLimit)&&!uploadFlight.empty()) {
            ++uploadPressureWaits;
            Check(vkWaitForFences(device,1,&uploadFlight.front().fence,VK_TRUE,UINT64_MAX),"upload staging backpressure");
            PollUploads();
        }
        return uploadSerial;
    }
    bool UploadsComplete(uint64_t ticket) {
        PollUploads();return ticket<=uploadCompleted;
    }
    void CancelUploads() {
        // Also valid after initialization/recording failures: never submit a partially recorded batch.
        if(device)vkDeviceWaitIdle(device);
        ReleaseUploadBatch(uploadBatch);
        for(auto& batch:uploadFlight)ReleaseUploadBatch(batch);
        uploadFlight.clear();uploadCompleted=uploadSerial;
    }
    template<class Fn>void UploadTransfer(Fn fn) {
        ++uploadOperations;
        if(!AsyncUploads()){++uploadQueueIdles;Transfer(fn);return;}
        PollUploads();
        if(!uploadBatch.command) {
            VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ai.commandPool=uploads;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=1;
            Check(vkAllocateCommandBuffers(device,&ai,&uploadBatch.command),"allocate batched uploads");
            VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            Check(vkBeginCommandBuffer(uploadBatch.command,&bi),"begin batched uploads");
        }
        fn(uploadBatch.command);++uploadBatch.operations;
    }
    void KeepStaging(Buffer& staging) {
        if(!AsyncUploads()){Destroy(staging);return;}
        uploadBatch.staging.push_back(staging);uploadBatch.bytes+=staging.size;uploadBytes+=staging.allocationBytes;++uploadStagingBuffers;
        uploadPeakBytes=std::max(uploadPeakBytes,uploadBytes);uploadPeakBuffers=std::max(uploadPeakBuffers,uploadStagingBuffers);staging={};
        if(uploadBatch.bytes>=uploadBatchLimit||uploadBatch.operations>=uploadOperationLimit||uploadBytes>uploadMemoryLimit||uploadStagingBuffers>uploadBufferLimit)FlushUploads();
    }
    uint32_t MemoryType(uint32_t bits,VkMemoryPropertyFlags flags) {
        for(uint32_t i=0;i<memory.memoryTypeCount;++i)
            if((bits&(1u<<i))&&(memory.memoryTypes[i].propertyFlags&flags)==flags)return i;
        throw std::runtime_error("Vulkan memory type unavailable");
    }
    Buffer MakeBuffer(VkDeviceSize size,VkBufferUsageFlags usage,bool host) {
        Buffer b{};b.size=b.capacity=size;
        try {
            VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};ci.size=size;ci.usage=usage;
            Check(vkCreateBuffer(device,&ci,nullptr,&b.buffer),"create buffer");
            VkMemoryRequirements req{};vkGetBufferMemoryRequirements(device,b.buffer,&req);b.allocationBytes=req.size;
            VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=req.size;
            ai.memoryTypeIndex=MemoryType(req.memoryTypeBits,host?
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT:VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            Check(vkAllocateMemory(device,&ai,nullptr,&b.memory),"allocate buffer");
            Check(vkBindBufferMemory(device,b.buffer,b.memory,0),"bind buffer");
            if(host)Check(vkMapMemory(device,b.memory,0,size,0,&b.mapped),"map buffer");
        }catch(...){Destroy(b);throw;}
        return b;
    }
    void Destroy(Buffer& b) {
        if(b.mapped)vkUnmapMemory(device,b.memory);
        if(b.buffer)vkDestroyBuffer(device,b.buffer,nullptr);
        if(b.memory)vkFreeMemory(device,b.memory,nullptr);
        b={};
    }
    void Destroy(Image& i) {
        if(i.view){++viewRevision;vkDestroyImageView(device,i.view,nullptr);}
        if(i.image)vkDestroyImage(device,i.image,nullptr);
        if(i.memory)vkFreeMemory(device,i.memory,nullptr);
        i={};
    }
    uint64_t RetireFrame()const{return submissionSerial+(begun?1u:0u);}
    uint64_t RetireUpload()const{return uploadSerial+(uploadBatch.command?1u:0u);}
    void Retire(Buffer& buffer){if((begun||AsyncUploads())&&buffer.buffer){retiredBuffers.push_back({buffer,RetireFrame(),RetireUpload()});buffer={};}else Destroy(buffer);}
    void Retire(Image& image){if((begun||AsyncUploads())&&image.image){retiredImages.push_back({image,RetireFrame(),RetireUpload()});image={};}else Destroy(image);}
    void RetireUi(UiTarget& ui){if(begun||AsyncUploads())retiredUi.push_back({ui,RetireFrame(),RetireUpload()});else DestroyUi(ui);ui={};}
    void Collect(bool force=false) {
        PollUploads();PollFrames();if(begun&&!force)return;
        auto collect=[&](auto& items,auto destroy){for(auto it=items.begin();it!=items.end();) {
            if(force||(it->frame<=graphicsCompleted&&it->upload<=uploadCompleted)){destroy(it->resource);it=items.erase(it);}else ++it;
        }};
        collect(retiredUi,[&](UiTarget& ui){DestroyUi(ui);});
        collect(retiredBuffers,[&](Buffer& b){Destroy(b);});collect(retiredImages,[&](Image& i){Destroy(i);});
    }
    Image MakeImage(uint32_t w,uint32_t h,VkFormat format,VkImageUsageFlags usage,
                    VkImageAspectFlags aspect,uint32_t layers=2,uint32_t levels=1,bool isCube=false) {
        Image image{};image.levels=levels;image.width=w;image.height=h;
        try {
            VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.imageType=VK_IMAGE_TYPE_2D;
            ci.format=format;ci.extent={w,h,1};ci.mipLevels=levels;ci.arrayLayers=layers;
            ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=usage;
            if(isCube)ci.flags=VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            Check(vkCreateImage(device,&ci,nullptr,&image.image),"create image");
            VkMemoryRequirements req{};vkGetImageMemoryRequirements(device,image.image,&req);
            VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=req.size;
            ai.memoryTypeIndex=MemoryType(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            Check(vkAllocateMemory(device,&ai,nullptr,&image.memory),"allocate image");
            Check(vkBindImageMemory(device,image.image,image.memory,0),"bind image");
            VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=image.image;
            vi.viewType=isCube?VK_IMAGE_VIEW_TYPE_CUBE:layers==1?VK_IMAGE_VIEW_TYPE_2D:VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            vi.format=format;vi.subresourceRange={aspect,0,levels,0,layers};
            Check(vkCreateImageView(device,&vi,nullptr,&image.view),"create image view");
        }catch(...){Destroy(image);throw;}
        return image;
    }
    template<class Fn>void Transfer(Fn fn) {
        FlushUploads();
        VkCommandBuffer cb{};VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool=uploads;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=1;
        Check(vkAllocateCommandBuffers(device,&ai,&cb),"allocate upload commands");
        try {
            VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            Check(vkBeginCommandBuffer(cb,&bi),"begin upload");fn(cb);Check(vkEndCommandBuffer(cb),"end upload");
            VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};si.commandBufferCount=1;si.pCommandBuffers=&cb;
            Check(vkQueueSubmit(queue,1,&si,VK_NULL_HANDLE),"submit upload");
            Check(vkQueueWaitIdle(queue),"wait upload");
        }catch(...){vkQueueWaitIdle(queue);vkFreeCommandBuffers(device,uploads,1,&cb);throw;}
        vkFreeCommandBuffers(device,uploads,1,&cb);
    }
    Buffer UploadBuffer(const void* data,size_t bytes,VkBufferUsageFlags usage) {
        UploadCpu uploadCpu(*this);
        Buffer staging=MakeStaging(bytes),result;
        try {
            std::memcpy(staging.mapped,data,bytes);
            result=MakeBuffer(bytes,usage|VK_BUFFER_USAGE_TRANSFER_DST_BIT,false);
            UploadTransfer([&](VkCommandBuffer cb){VkBufferCopy copy{0,0,bytes};vkCmdCopyBuffer(cb,staging.buffer,result.buffer,1,&copy);
                VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
                barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT|VK_ACCESS_INDEX_READ_BIT;
                barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
                barrier.buffer=result.buffer;barrier.size=VK_WHOLE_SIZE;
                vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,0,0,nullptr,1,&barrier,0,nullptr);});
            KeepStaging(staging);
        }catch(...){CancelUploads();Destroy(staging);Destroy(result);throw;}
        return result;
    }
    Image UploadTexture(uint32_t w,uint32_t h,const uint8_t* rgba,bool isCube=false,bool mips=true,bool repeat=true) {
        UploadCpu uploadCpu(*this);
        const uint32_t layers=isCube?6:1;const size_t bytes=size_t(w)*h*4*layers;
        uint32_t levels=1;if(mips)for(uint32_t n=std::max(w,h);n>1;n>>=1)++levels;
        VkFormatProperties properties{};vkGetPhysicalDeviceFormatProperties(physical,VK_FORMAT_R8G8B8A8_UNORM,&properties);
        const VkFormatFeatureFlags blit=VK_FORMAT_FEATURE_BLIT_SRC_BIT|VK_FORMAT_FEATURE_BLIT_DST_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        if(levels>1&&(properties.optimalTilingFeatures&blit)!=blit)throw std::runtime_error("RGBA8 mip generation unsupported");
        Buffer staging=MakeStaging(bytes);Image result;
        try {
            std::memcpy(staging.mapped,rgba,bytes);
            result=MakeImage(w,h,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
                             VK_IMAGE_ASPECT_COLOR_BIT,layers,levels,isCube);
            result.clamp=!repeat;
            UploadTransfer([&](VkCommandBuffer cb){
                VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
                barrier.image=result.image;barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,levels,0,layers};
                barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
                vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
                VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,layers};copy.imageExtent={w,h,1};
                vkCmdCopyBufferToImage(cb,staging.buffer,result.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
                int32_t mw=w,mh=h;
                for(uint32_t level=1;level<levels;++level) {
                    barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,level-1,1,0,layers};
                    barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
                    vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
                    VkImageBlit blit{};blit.srcSubresource={VK_IMAGE_ASPECT_COLOR_BIT,level-1,0,layers};blit.srcOffsets[1]={mw,mh,1};
                    blit.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,level,0,layers};blit.dstOffsets[1]={std::max(1,mw/2),std::max(1,mh/2),1};
                    vkCmdBlitImage(cb,result.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,result.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&blit,VK_FILTER_LINEAR);
                    barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    barrier.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
                    vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_VERTEX_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
                    mw=std::max(1,mw/2);mh=std::max(1,mh/2);
                }
                barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,levels-1,1,0,layers};
                barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
                vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_VERTEX_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
            });
            KeepStaging(staging);
        }catch(...){CancelUploads();Destroy(staging);Destroy(result);throw;}
        return result;
    }
    Image UploadPalette(const float* data,size_t rows) {
        UploadCpu uploadCpu(*this);
        ++paletteAllocations;
        const size_t bytes=rows*8*4*sizeof(float);
        Buffer staging=MakeStaging(bytes);Image image;
        try {
            std::memcpy(staging.mapped,data,bytes);
            image=MakeImage(8,uint32_t(rows),VK_FORMAT_R32G32B32A32_SFLOAT,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT,1);
            image.palette=true;
            UploadTransfer([&](VkCommandBuffer cb){
                VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.image=image.image;
                b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
                b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;
                b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
                vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
                VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={8,uint32_t(rows),1};
                vkCmdCopyBufferToImage(cb,staging.buffer,image.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
                b.oldLayout=b.newLayout;b.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
                vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,0,0,nullptr,0,nullptr,1,&b);
            });
            KeepStaging(staging);
        }catch(...){CancelUploads();Destroy(staging);Destroy(image);throw;}
        return image;
    }
    void UpdatePalette(Image& image,const float* data,size_t rows) {
        UploadCpu uploadCpu(*this);
        // Only before command recording: queued draws must retain their palette contents.
        if(begun)throw std::runtime_error("palette reuse during frame recording");
        Buffer staging=MakeStaging(rows*8*4*sizeof(float));
        try {
            std::memcpy(staging.mapped,data,size_t(staging.size));
            UploadTransfer([&](VkCommandBuffer cb){
                VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.image=image.image;
                b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
                b.oldLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                b.srcAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
                vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_VERTEX_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
                VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={8,uint32_t(rows),1};
                vkCmdCopyBufferToImage(cb,staging.buffer,image.image,b.newLayout,1,&copy);
                b.oldLayout=b.newLayout;b.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
                vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,0,0,nullptr,0,nullptr,1,&b);
            });
            KeepStaging(staging);++paletteUpdates;
        }catch(...){CancelUploads();Destroy(staging);throw;}
    }
    Image UploadCube(const Fo3RgbaCubeTexture& source) {
        UploadCpu uploadCpu(*this);
        if(source.width<=0||source.height!=source.width||source.mipLevels<=0)throw std::runtime_error("invalid cube");
        std::vector<uint8_t> pixels;std::vector<VkBufferImageCopy> regions;
        for(uint32_t face=0;face<6;++face) {
            if(source.rgbaLevels[face].size()!=size_t(source.mipLevels))throw std::runtime_error("cube mip count");
            uint32_t size=source.width;
            for(int mip=0;mip<source.mipLevels;++mip){
                const auto& data=source.rgbaLevels[face][mip];if(data.size()!=size_t(size)*size*4)throw std::runtime_error("cube mip size");
                VkBufferImageCopy copy{};copy.bufferOffset=pixels.size();copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,uint32_t(mip),face,1};copy.imageExtent={size,size,1};
                regions.push_back(copy);pixels.insert(pixels.end(),data.begin(),data.end());size=std::max(1u,size/2);
            }
        }
        Buffer staging=MakeStaging(pixels.size());Image image;
        try {
            std::memcpy(staging.mapped,pixels.data(),pixels.size());
            image=MakeImage(source.width,source.height,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT,6,source.mipLevels,true);
            image.clamp=true;
            UploadTransfer([&](VkCommandBuffer cb){
                VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.image=image.image;
                b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
                b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,uint32_t(source.mipLevels),0,6};b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;
                b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
                vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
                vkCmdCopyBufferToImage(cb,staging.buffer,image.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,uint32_t(regions.size()),regions.data());
                b.oldLayout=b.newLayout;b.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
                vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&b);
            });
            KeepStaging(staging);
        }catch(...){CancelUploads();Destroy(staging);Destroy(image);throw;}
        return image;
    }
    void Initialize(XrInstance xr,XrSystemId system,uint32_t w,uint32_t h) {
        width=w;height=h;xrInstance=xr;
        XrGraphicsRequirementsVulkan2KHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
        CheckXr(Proc<PFN_xrGetVulkanGraphicsRequirements2KHR>(xr,"xrGetVulkanGraphicsRequirements2KHR")(xr,system,&req),"Vulkan graphics requirements");
        const XrVersion requested=std::max<XrVersion>(req.minApiVersionSupported,XR_MAKE_VERSION(1,1,0));
        if(requested>req.maxApiVersionSupported)throw std::runtime_error("OpenXR does not support required Vulkan 1.1");
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="FalloutQuest";
        app.apiVersion=VK_MAKE_VERSION(XR_VERSION_MAJOR(requested),XR_VERSION_MINOR(requested),0);
        VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ici.pApplicationInfo=&app;
        XrVulkanInstanceCreateInfoKHR xici{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};xici.systemId=system;
        xici.pfnGetInstanceProcAddr=vkGetInstanceProcAddr;xici.vulkanCreateInfo=&ici;
        VkResult result=VK_SUCCESS;
        CheckXr(Proc<PFN_xrCreateVulkanInstanceKHR>(xr,"xrCreateVulkanInstanceKHR")(xr,&xici,&instance,&result),"xrCreateVulkanInstanceKHR");Check(result,"Vulkan instance");
        XrVulkanGraphicsDeviceGetInfoKHR gi{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};gi.systemId=system;gi.vulkanInstance=instance;
        CheckXr(Proc<PFN_xrGetVulkanGraphicsDevice2KHR>(xr,"xrGetVulkanGraphicsDevice2KHR")(xr,&gi,&physical),"OpenXR physical device");
        vkGetPhysicalDeviceProperties(physical,&properties);vkGetPhysicalDeviceMemoryProperties(physical,&memory);
        fqrender::capabilities={int(properties.limits.maxImageDimension2D),int(properties.limits.maxVertexInputAttributes),int(properties.limits.maxPerStageDescriptorSamplers)};
        if(properties.apiVersion<VK_API_VERSION_1_1)throw std::runtime_error("runtime-selected device requires Vulkan 1.1");
        uint32_t count=0;vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,nullptr);
        std::vector<VkQueueFamilyProperties> queues(count);vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,queues.data());
        auto it=std::find_if(queues.begin(),queues.end(),[](const auto& q){return q.queueCount&&(q.queueFlags&VK_QUEUE_GRAPHICS_BIT);});
        if(it==queues.end())throw std::runtime_error("Vulkan graphics queue unavailable");
        family=uint32_t(it-queues.begin());timestampBits=it->timestampValidBits;timestampSupported=timestampBits>0&&properties.limits.timestampComputeAndGraphics;
        uint32_t extensionCount=0;vkEnumerateDeviceExtensionProperties(physical,nullptr,&extensionCount,nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);vkEnumerateDeviceExtensionProperties(physical,nullptr,&extensionCount,extensions.data());
        const bool density=std::any_of(extensions.begin(),extensions.end(),[](const auto& e){return !std::strcmp(e.extensionName,VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME);});
        VkPhysicalDeviceFragmentDensityMapFeaturesEXT densityFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT};
        if(density){VkPhysicalDeviceFeatures2 f{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};f.pNext=&densityFeatures;vkGetPhysicalDeviceFeatures2(physical,&f);}
        foveationSupported=densityFeatures.fragmentDensityMap&&densityFeatures.fragmentDensityMapNonSubsampledImages;
#ifdef __ANDROID__
        char densityProp[PROP_VALUE_MAX]{};__system_property_get("debug.falloutquest.vulkan_foveation",densityProp);
        foveationRequested=std::strcmp(densityProp,"0")!=0;
#endif
        const bool enableDensity=foveationSupported&&foveationRequested;
        VkPhysicalDeviceMultiviewFeatures multiview{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES};
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};features.pNext=&multiview;
        vkGetPhysicalDeviceFeatures2(physical,&features);
        independentBlendAvailable=features.features.independentBlend;
        if(!multiview.multiview)throw std::runtime_error("Vulkan native multiview unavailable");
        multiview.multiviewGeometryShader=multiview.multiviewTessellationShader=VK_FALSE;
        densityFeatures.fragmentDensityMapDynamic=VK_FALSE;
        if(enableDensity)multiview.pNext=&densityFeatures;
        const float priority=1;VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qci.queueFamilyIndex=family;qci.queueCount=1;qci.pQueuePriorities=&priority;
        VkPhysicalDeviceFeatures enabled{};enabled.independentBlend=independentBlendAvailable;
        VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};const char* densityExtension=VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME;
        if(enableDensity){dci.enabledExtensionCount=1;dci.ppEnabledExtensionNames=&densityExtension;}
        dci.pEnabledFeatures=&enabled;dci.pNext=&multiview;dci.queueCreateInfoCount=1;dci.pQueueCreateInfos=&qci;
        XrVulkanDeviceCreateInfoKHR xdci{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};xdci.systemId=system;xdci.pfnGetInstanceProcAddr=vkGetInstanceProcAddr;
        xdci.vulkanPhysicalDevice=physical;xdci.vulkanCreateInfo=&dci;
        CheckXr(Proc<PFN_xrCreateVulkanDeviceKHR>(xr,"xrCreateVulkanDeviceKHR")(xr,&xdci,&device,&result),"xrCreateVulkanDeviceKHR");Check(result,"Vulkan device");
        vkGetDeviceQueue(device,family,0,&queue);
        CreatePipelineCache();
        VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pci.queueFamilyIndex=family;pci.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        Check(vkCreateCommandPool(device,&pci,nullptr,&pool),"command pool");Check(vkCreateCommandPool(device,&pci,nullptr,&uploads),"upload command pool");
        VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};cai.commandPool=pool;cai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;cai.commandBufferCount=1;
        Check(vkAllocateCommandBuffers(device,&cai,&command),"frame commands");
        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};fci.flags=VK_FENCE_CREATE_SIGNALED_BIT;Check(vkCreateFence(device,&fci,nullptr,&fence),"frame fence");
        VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};sci.magFilter=sci.minFilter=VK_FILTER_LINEAR;
        sci.mipmapMode=VK_SAMPLER_MIPMAP_MODE_LINEAR;sci.maxLod=VK_LOD_CLAMP_NONE;sci.addressModeU=sci.addressModeV=sci.addressModeW=VK_SAMPLER_ADDRESS_MODE_REPEAT;
        Check(vkCreateSampler(device,&sci,nullptr,&sampler),"sampler");
        sci.addressModeU=sci.addressModeV=sci.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        Check(vkCreateSampler(device,&sci,nullptr,&clamped),"clamp sampler");
        sci.addressModeV=VK_SAMPLER_ADDRESS_MODE_REPEAT;
        Check(vkCreateSampler(device,&sci,nullptr,&verticalRepeat),"vertical-repeat sampler");
        sci.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;sci.addressModeU=VK_SAMPLER_ADDRESS_MODE_REPEAT;
        Check(vkCreateSampler(device,&sci,nullptr,&horizontalRepeat),"horizontal-repeat sampler");
        sci.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.magFilter=sci.minFilter=VK_FILTER_NEAREST;sci.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
        Check(vkCreateSampler(device,&sci,nullptr,&nearest),"palette sampler");
        const std::array<uint8_t,24> pixels{255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255};
        white=UploadTexture(1,1,pixels.data());cube=UploadTexture(1,1,pixels.data(),true);
        arena=MakeBuffer(32u*1024u*1024u,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,true);
        stereo=MakeBuffer(432,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true);
        CreatePass();CreateDescriptors();
        if(timestampSupported){VkQueryPoolCreateInfo tci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};tci.queryType=VK_QUERY_TYPE_TIMESTAMP;// Reserve a stereo-width slot for each timestamp. Some drivers
            // retain the multiview query stride through the pass-end boundary.
            tci.queryCount=QueryCount;
            Check(vkCreateQueryPool(device,&tci,nullptr,&timestamps),"timestamp pool");}
        if(enableDensity){try{CreateDensityMap();CreateDensityPasses(false);}catch(const std::exception& e){VKLOG("FQVK FOVEATION unavailable=%s native full resolution retained",e.what());DestroyDensityPasses(densityMain);Destroy(densityMap);}}
        const bool elapsed=std::any_of(extensions.begin(),extensions.end(),[](const auto& e){return !std::strcmp(e.extensionName,"VK_QCOM_elapsed_timer_query");});
        VKLOG("FQVK CAP fragmentDensityMap=%d nonSubsampledImages=%d elapsedTimerExtension=%d foveationEnabled=%d",densityFeatures.fragmentDensityMap,densityFeatures.fragmentDensityMapNonSubsampledImages,elapsed,bool(densityMain.clear));
#ifdef __ANDROID__
        char visualProp[PROP_VALUE_MAX]{};
        __system_property_get("debug.falloutquest.vulkan_diag",visualProp);
        visualDiagnostics=uint32_t(std::strtoul(visualProp,nullptr,0))&7u;
        __system_property_get("debug.falloutquest.vulkan_present_copy",visualProp);
        legacyPresentation=std::atoi(visualProp)==1;
        __system_property_get("debug.falloutquest.vulkan_timing",visualProp);passTiming=std::atoi(visualProp)==1;
        __system_property_get("debug.falloutquest.vulkan_sync",visualProp);synchronous=std::atoi(visualProp)==1;
        __system_property_get("debug.falloutquest.vulkan_generic_shaders",visualProp);genericShaders=std::atoi(visualProp)==1;
#endif
        VKLOG("FQVK VISUAL diagnosticMask=%u forceNoCull=%u rawSceneNoFilm=%u reverseFrontFace=%u originalAssets=1",visualDiagnostics,(visualDiagnostics&1u)?1u:0u,(visualDiagnostics&2u)?1u:0u,(visualDiagnostics&4u)?1u:0u);
        VKLOG("FQVK WORLD PROBE device=%s api=%u.%u multiview=1 fragmentDensityExtension=%d foveationEnabled=%d size=%ux%u stereo=2 presentation=unbound parity=INCOMPLETE",
              properties.deviceName,VK_VERSION_MAJOR(properties.apiVersion),VK_VERSION_MINOR(properties.apiVersion),density,bool(densityMain.clear),width,height);
    }
    #include "fo3-vulkan-pipeline-cache.inc"

    void CreatePass() {
        VkFormatProperties format{};vkGetPhysicalDeviceFormatProperties(physical,VK_FORMAT_R16G16B16A16_SFLOAT,&format);
        const VkFormatFeatureFlags fp16Required=VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        if((format.optimalTilingFeatures&fp16Required)!=fp16Required)throw std::runtime_error("sampled FP16 color unavailable");
        vkGetPhysicalDeviceFormatProperties(physical,VK_FORMAT_D32_SFLOAT,&format);
        const VkFormatFeatureFlags depthRequired=VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if((format.optimalTilingFeatures&depthRequired)!=depthRequired)throw std::runtime_error("sampled D32 depth unavailable");
        color=MakeImage(width,height,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
        depth=MakeImage(width,height,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_DEPTH_BIT);
        sceneColor=MakeImage(width,height,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
        sceneDepth=MakeImage(width,height,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_DEPTH_BIT);
        std::array<VkAttachmentDescription,2> attachments{};
        attachments[0].format=VK_FORMAT_R16G16B16A16_SFLOAT;attachments[1].format=VK_FORMAT_D32_SFLOAT;
        for(auto& a:attachments){a.samples=VK_SAMPLE_COUNT_1_BIT;a.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;a.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;a.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;a.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;}
        attachments[0].storeOp=VK_ATTACHMENT_STORE_OP_STORE;attachments[0].finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachments[1].storeOp=VK_ATTACHMENT_STORE_OP_STORE;attachments[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkAttachmentReference c{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},d{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&c;sub.pDepthStencilAttachment=&d;
        const uint32_t mask=3;VkRenderPassMultiviewCreateInfo mv{VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO};mv.subpassCount=1;mv.pViewMasks=&mask;mv.correlationMaskCount=1;mv.pCorrelationMasks=&mask;
        VkSubpassDependency dependency{};dependency.srcSubpass=VK_SUBPASS_EXTERNAL;dependency.dstSubpass=0;
        dependency.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask=dependency.srcStageMask;dependency.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};ci.pNext=&mv;ci.attachmentCount=2;ci.pAttachments=attachments.data();ci.subpassCount=1;ci.pSubpasses=&sub;ci.dependencyCount=1;ci.pDependencies=&dependency;
        Check(vkCreateRenderPass(device,&ci,nullptr,&pass),"multiview render pass");
        for(auto& a:attachments){a.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;a.initialLayout=a.finalLayout;}
        Check(vkCreateRenderPass(device,&ci,nullptr,&resumePass),"water continuation pass");
        std::array<VkImageView,2> views{color.view,depth.view};VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fi.renderPass=pass;fi.attachmentCount=2;fi.pAttachments=views.data();fi.width=width;fi.height=height;fi.layers=1;
        Check(vkCreateFramebuffer(device,&fi,nullptr,&framebuffer),"multiview framebuffer");
    }
    void CreateDescriptors() {
        for(int kind=0;kind<13;++kind) {
            const size_t count=shaders::programs[kind].samplerCount;
            std::vector<VkDescriptorSetLayoutBinding> bindings(count+2);
            for(uint32_t i=0;i<bindings.size();++i){auto& b=bindings[i];b.binding=i;b.descriptorCount=1;b.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;
                b.descriptorType=i==0?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:i==1?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;}
            VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};ci.bindingCount=uint32_t(bindings.size());ci.pBindings=bindings.data();
            Check(vkCreateDescriptorSetLayout(device,&ci,nullptr,&sets[kind]),"descriptor layout");
            VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};li.setLayoutCount=1;li.pSetLayouts=&sets[kind];
            Check(vkCreatePipelineLayout(device,&li,nullptr,&layouts[kind]),"pipeline layout");
        }
        for(int kind=0;kind<13;++kind)for(int stage=0;stage<2;++stage) {
            const auto& shader=shaders::programs[kind];VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            ci.codeSize=stage?shader.fragmentBytes:shader.vertexBytes;ci.pCode=stage?shader.fragment:shader.vertex;
            Check(vkCreateShaderModule(device,&ci,nullptr,&modules[kind][stage]),"SPIR-V module");
        }
        for(size_t i=0;i<3;++i){const auto& shader=shaders::solidFragments[i];
            VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};ci.codeSize=shader.bytes;ci.pCode=shader.code;
            Check(vkCreateShaderModule(device,&ci,nullptr,&solidModules[i]),"solid fragment SPIR-V");}
        descriptors=MakeDescriptorPool();
    }
    VkPipeline Pipeline(PipelineKey key) {
        key.Normalize();
        const auto found=pipelines.find(key);if(found!=pipelines.end())return found->second;
        const int kind=key.ui?int(key.ui):key.water?3:key.sky?2:key.terrain?(key.fused?TerrainFusionKind:1):0;
        std::array<VkPipelineShaderStageCreateInfo,2> stages{};
        for(int i=0;i<2;++i){stages[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stages[i].stage=i?VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_VERTEX_BIT;stages[i].module=modules[kind][i];stages[i].pName="main";}
        if(key.noDiscard)stages[1].module=solidModules[key.fused?2:kind];
        const uint32_t materialConstants[]{key.lighting,uint32_t(key.noLocal),uint32_t(key.noEnvironment),key.paintCount};
        const VkSpecializationMapEntry materialEntries[]{{0,0,4},{1,4,4},{2,8,4},{3,12,4}};
        const VkSpecializationInfo materialSpecialization{4,materialEntries,sizeof(materialConstants),materialConstants};
        if(kind<2||kind==TerrainFusionKind)stages[1].pSpecializationInfo=&materialSpecialization;
        if(key.split&&(kind<4||key.fused))stages[1].module=splitModules[key.fused?(key.noDiscard?7:6):kind==3?5:key.noDiscard?kind+3:kind];
        VkVertexInputBindingDescription binding{0,key.stride*4,VK_VERTEX_INPUT_RATE_VERTEX};
        std::vector<VkVertexInputAttributeDescription> attributes;
        if(key.ui==11)attributes={{0,0,VK_FORMAT_R32G32B32_SFLOAT,0},{1,0,VK_FORMAT_R32G32B32_SFLOAT,12},{2,0,VK_FORMAT_R32G32_SFLOAT,24},{3,0,VK_FORMAT_R32G32B32A32_SFLOAT,32}};
        else if(key.ui){attributes={{0,0,VK_FORMAT_R32G32B32_SFLOAT,0},{1,0,VK_FORMAT_R32G32_SFLOAT,12}};if(key.ui==10)attributes.push_back({2,0,VK_FORMAT_R32_SFLOAT,20});}
        else if(key.water)attributes={{0,0,VK_FORMAT_R32G32B32_SFLOAT,0},{1,0,VK_FORMAT_R32G32_SFLOAT,12}};
        else if(key.sky)attributes={{0,0,VK_FORMAT_R32G32B32_SFLOAT,0}};
        else if(key.terrain){
            attributes={{0,0,VK_FORMAT_R32G32B32_SFLOAT,0},{1,0,VK_FORMAT_R32G32B32_SFLOAT,12},{2,0,VK_FORMAT_R32G32_SFLOAT,24},{3,0,VK_FORMAT_R32_SFLOAT,32},{4,0,VK_FORMAT_R32G32B32_SFLOAT,36}};
            if(key.fused)attributes.push_back({5,0,VK_FORMAT_R32G32B32_SFLOAT,key.stride==15?48u:0u});
        }
        else attributes={{0,0,VK_FORMAT_R32G32B32_SFLOAT,0},{1,0,VK_FORMAT_R32G32B32_SFLOAT,12},{2,0,VK_FORMAT_R32G32B32_SFLOAT,24},{3,0,VK_FORMAT_R32G32B32_SFLOAT,36},{4,0,VK_FORMAT_R32G32_SFLOAT,48},{5,0,VK_FORMAT_R32G32B32A32_SFLOAT,56}};
        // Unused skin/instance inputs must still have defined attributes. The
        // probe rejects draws needing those streams rather than render bind poses.
        if(!key.terrain&&!key.sky&&!key.water&&!key.ui)for(uint32_t i=6;i<12;++i) {
            const uint32_t stream=key.instanced&&i<10?1u:key.skinned&&i>=10?2u:0u;
            attributes.push_back({i,stream,VK_FORMAT_R32G32B32A32_SFLOAT,stream==1?(i-6)*16u:stream==2?(i-10)*16u:0u});
        }
        std::vector<VkVertexInputBindingDescription> bindings{binding};
        if(key.instanced)bindings.push_back({1,64,VK_VERTEX_INPUT_RATE_INSTANCE});
        if(key.skinned)bindings.push_back({2,32,VK_VERTEX_INPUT_RATE_VERTEX});
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};vi.vertexBindingDescriptionCount=uint32_t(bindings.size());vi.pVertexBindingDescriptions=bindings.data();vi.vertexAttributeDescriptionCount=uint32_t(attributes.size());vi.pVertexAttributeDescriptions=attributes.data();
        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=key.strip?VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport viewport{0,0,float(width),float(height),0,1};VkRect2D scissor{{0,0},{width,height}};
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=1;vp.pViewports=&viewport;vp.scissorCount=1;vp.pScissors=&scissor;
        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.polygonMode=VK_POLYGON_MODE_FILL;rs.lineWidth=1;
        rs.cullMode=(visualDiagnostics&1u)||!key.cull?VK_CULL_MODE_NONE:key.cullFace==GL_FRONT?VK_CULL_MODE_FRONT_BIT:
            key.cullFace==GL_FRONT_AND_BACK?VK_CULL_MODE_FRONT_AND_BACK:VK_CULL_MODE_BACK_BIT;
        rs.depthBiasEnable=key.offset;
        // Vulkan's signed framebuffer area has the opposite sign to GL's.
        // The vertex shader's Y flip already compensates for that difference:
        // GL_CCW stays VK_COUNTER_CLOCKWISE, including authored reversed NIFs.
        // UI RTT shaders deliberately omit that Y flip (texture orientation),
        // so those pipelines alone need the opposite winding interpretation.
        const bool shaderFlipsY=!(key.offscreen&&(key.ui||key.sky||key.water));
        const bool flipWindingDiagnostic=(visualDiagnostics&4u)!=0u;
        const bool clockwise=key.clockwise^!shaderFlipsY^flipWindingDiagnostic;
        rs.frontFace=clockwise?VK_FRONT_FACE_CLOCKWISE:VK_FRONT_FACE_COUNTER_CLOCKWISE;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};ds.depthTestEnable=key.depth;ds.depthWriteEnable=key.write;ds.depthCompareOp=Compare(key.depthFunc);
        VkPipelineColorBlendAttachmentState ba{};ba.colorWriteMask=0xf;ba.blendEnable=key.blend;ba.srcColorBlendFactor=Blend(key.src);ba.dstColorBlendFactor=Blend(key.dst);ba.colorBlendOp=BlendOperation(key.equation);ba.srcAlphaBlendFactor=Blend(key.srcA);ba.dstAlphaBlendFactor=Blend(key.dstA);ba.alphaBlendOp=BlendOperation(key.equationA);
        std::array<VkPipelineColorBlendAttachmentState,2> blendAttachments{};blendAttachments[0]=ba;
        // Every world pass uses identical blending in both HDR targets. Only UI
        // is separated: model pixels must never swap native/synthesised layers.
        blendAttachments[1]=ba;blendAttachments[1].colorWriteMask=key.ui?0:0xf;
        VkPipelineColorBlendStateCreateInfo bs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};bs.attachmentCount=key.split?2:1;bs.pAttachments=blendAttachments.data();
        const VkDynamicState dynamicStates[]{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_DEPTH_BIAS};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount=key.offset?3:2;dynamic.pDynamicStates=dynamicStates;
        VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};ci.stageCount=2;ci.pStages=stages.data();ci.pVertexInputState=&vi;ci.pInputAssemblyState=&ia;ci.pViewportState=&vp;ci.pRasterizationState=&rs;ci.pMultisampleState=&ms;ci.pDepthStencilState=&ds;ci.pColorBlendState=&bs;ci.layout=layouts[kind];ci.renderPass=key.foveated?(key.split?densitySplit.clear:densityMain.clear):key.split?splitPass:key.modal?(key.swapchain?modalXrPass:modalPass):key.swapchain?xrOverlayPass:key.offscreen?uiPass:pass;ci.pDynamicState=&dynamic;
        const auto compileStarted=NowUs();
        VkPipeline pipeline{};Check(vkCreateGraphicsPipelines(device,pipelineCache,1,&ci,nullptr,&pipeline),"world pipeline");pipelines.emplace(key,pipeline);pipelineCacheDirty=true;
        VKLOG("FQVK PIPELINE compileUs=%llu warmup=%d kind=%d split=%d foveated=%d paintCount=%u cachedPipelines=%zu",
            (unsigned long long)(NowUs()-compileStarted),warmingPipelines,kind,key.split,key.foveated,key.paintCount,pipelines.size());
        return pipeline;
    }
    void Register(uint32_t id,int kind) {
        if(!id)return;
        DrainDraws();
        if(!fqrender::locationLookup)throw std::runtime_error("missing CPU program binding resolver");
        const auto& shader=shaders::programs[kind];
        ProgramData p;p.terrain=kind==1||kind==TerrainFusionKind;p.fused=kind==TerrainFusionKind;p.sky=kind==2;p.water=kind==3;p.ui=kind>=9&&kind<=11?kind:0;p.bytes.resize(shader.uniformSize);
        for(size_t i=0;i<shader.fieldCount;++i){int location=fqrender::locationLookup(id,shader.fields[i].name);if(location>=0)p.fields.push_back({location,shader.fields[i]});}
        for(size_t i=0;i<shader.samplerCount;++i)p.samplers.push_back(fqrender::locationLookup(id,shader.samplers[i].name));
        ConfigureDiscardGuards(p,shader);
        p.units.resize(shader.samplerCount);programs[id]=std::move(p);
    }
    bool SolidEligible(const ProgramData& p,const std::vector<uint8_t>& bytes)const {
        if(genericShaders||p.ui||p.water||p.sky)return false;
        for(auto offset:p.discardOffsets)if(offset!=UINT32_MAX) {
            if(offset+sizeof(float)>bytes.size())return false;
            float value;std::memcpy(&value,bytes.data()+offset,sizeof(value));
            if(!std::isfinite(value)||value>0.5f)return false;
        }
        return true;
    }
    void Uniform(uint32_t id,int location,int kind,int bytes,const void* data) {
        auto it=programs.find(id);if(it==programs.end())return;auto& p=it->second;
        ++uniformCalls;
        if(!p.bindingsReady){
            for(size_t i=0;i<p.samplers.size();++i)if(p.samplers[i]>=0)p.bindings[p.samplers[i]]={0,0,0,int(i)};
            for(const auto& e:p.fields)if(e.location>=0)for(uint32_t i=0;i<e.field.count;++i)
                p.bindings[e.location+int(i)]={e.field.offset+i*(e.field.components==16?64u:16u),e.field.components,e.field.count-i,-1};
            p.bindingsReady=true;
        }
        const auto found=p.bindings.find(location);if(found==p.bindings.end()||!data||bytes<=0)return;
        const auto& f=found->second;
        if(f.unit>=0){if(kind==2&&bytes>=4){int unit;std::memcpy(&unit,data,4);if(p.units[size_t(f.unit)]==unit)++uniformUnchanged;else p.units[size_t(f.unit)]=unit;}return;}
        const uint32_t elementSize=f.components*4,n=std::min(f.count,uint32_t(bytes)/elementSize);
        bool changed=false;
        for(uint32_t i=0;i<n;++i){auto* dst=p.bytes.data()+f.offset+i*(f.components==16?64u:16u);const auto* src=static_cast<const uint8_t*>(data)+i*elementSize;
            if(std::memcmp(dst,src,elementSize)){std::memcpy(dst,src,elementSize);changed=true;}}
        if(!changed)++uniformUnchanged;
    }

    bool UsesSkin(const ProgramData& p,const std::vector<uint8_t>& bytes) {
        if(p.skinOffset==UINT32_MAX)return false;
        int mode=0;std::memcpy(&mode,bytes.data()+p.skinOffset,4);return mode!=0;
    }

    struct PendingDraw {fqrender::DrawCommand command;std::vector<uint8_t> uniforms;std::vector<int> units;};
    std::vector<PendingDraw> pending;
    void Queue(const fqrender::DrawCommand& command) {
        if(!begun||!passOpen||failed)return;
        const auto p=programs.find(command.program);
        if(p==programs.end()){++skipped;++skipReasons[0];if(!firstSkippedProgram)firstSkippedProgram=command.program;return;}
        const auto started=passTiming?NowUs():0;
        pending.push_back({command,p->second.bytes,p->second.units});
        if(passTiming)queueCpuUs+=NowUs()-started;
    }
    void DrainDraws() {
        for(const auto& item:pending)Draw(item);
        pending.clear();
    }
    void Draw(const PendingDraw& item) {
        const auto& draw=item.command;
        const auto program=draw.program,vao=draw.mesh,mode=draw.topology,type=draw.indexType;
        const auto first=draw.first,count=draw.count,instances=draw.instances;const auto offset=draw.indexOffset;
        if(!begun||!passOpen||failed)return;
        auto skip=[&](size_t reason){++skipped;++skipReasons[reason];};
        const auto pIt=programs.find(program);
        if(pIt==programs.end()){skip(0);if(!firstSkippedProgram)firstSkippedProgram=program;return;}
        const auto& p=pIt->second;
        const auto mIt=meshes.find(vao);
        if(mIt==meshes.end()||!mIt->second.vertices.buffer){skip(1);return;}
        if((mode!=GL_TRIANGLES&&mode!=GL_TRIANGLE_STRIP)||(type&&type!=GL_UNSIGNED_INT)){skip(2);return;}
        if(count<=0||instances<=0||(instances>1&&draw.transforms.size()<size_t(instances)*16)){skip(4);return;}
        if(UsesSkin(p,item.uniforms)&&!mIt->second.skin.buffer){skip(3);return;}
        const auto& m=mIt->second;
        // Only 15-float meshes carry fused paint attributes. Ordinary retained
        // batches use the original 3-sampler material and its common UBO prefix.
        const bool fused=p.fused&&m.stride==15;
        const int kind=p.ui?p.ui:p.water?3:p.sky?2:p.terrain?(fused?TerrainFusionKind:1):0;
        const uint32_t indexFirst=type?(p.terrain?0:uint32_t(offset/4)):uint32_t(first);
        if(indexFirst>m.count||uint32_t(count)>m.count-indexFirst||(type&&!m.indices.buffer)){skip(4);return;}
        const auto drawStarted=passTiming?NowUs():0;
        const auto& s=draw.state;
        if(splitFrame&&worldDepthReady&&!p.ui&&!postDone&&s.depth&&s.write)lateWorldDepthWrite=true;
        if(p.ui&&(densityActive||(splitFrame&&!worldDepthReady)))FullRateScene();
        else if(!p.ui&&splitFrame&&!sidecarPass&&!offscreen&&!postDone)ResumeSidecar();
        PipelineKey key;key.modal=modalActive;key.offscreen=offscreen;key.swapchain=directOutput||(compactFrame&&postDone);key.strip=mode==GL_TRIANGLE_STRIP;key.ui=uint32_t(p.ui);key.water=p.water;key.sky=p.sky;key.terrain=p.terrain;key.fused=fused;key.stride=m.stride;key.instanced=instances>1;key.skinned=UsesSkin(p,item.uniforms);
        key.split=sidecarPass&&!offscreen&&!postDone;key.foveated=densityActive&&!offscreen&&!postDone;
        if(!genericShaders&&(kind<2||p.fused)){
            if(p.lightingOffset!=UINT32_MAX){float value;std::memcpy(&value,item.uniforms.data()+p.lightingOffset,4);key.lighting=value>.5f?1:value<.5f?2:0;}
            if(p.environmentOffset!=UINT32_MAX){float value;std::memcpy(&value,item.uniforms.data()+p.environmentOffset,4);key.noEnvironment=value<=.5f;}
            if(p.localOffset!=UINT32_MAX){int count;std::memcpy(&count,item.uniforms.data()+p.localOffset,4);key.noLocal=count<=0;}
        }
        if(fused&&p.paintOffset!=UINT32_MAX){int count;std::memcpy(&count,item.uniforms.data()+p.paintOffset,4);key.paintCount=uint32_t(std::clamp(count,0,3));}
        key.depth=s.depth;key.write=s.write;key.blend=s.blend;key.cull=s.cull;key.clockwise=s.clockwise;
        key.depthFunc=s.depthFunc;key.src=s.src;key.dst=s.dst;key.srcA=s.srcAlpha;key.dstA=s.dstAlpha;
        key.cullFace=s.cullFace;key.equation=s.equation;key.equationA=s.equationAlpha;key.offset=s.offset;
        if(offscreen){key.depth=key.write=false;}
        key.noDiscard=SolidEligible(p,item.uniforms);
        const VkDeviceSize alignment=std::max<VkDeviceSize>(16,properties.limits.minUniformBufferOffsetAlignment);
        cursor=(cursor+alignment-1)&~(alignment-1);
        const size_t instanceBytes=key.instanced?size_t(instances)*64:0;
        const size_t uniformBytes=shaders::programs[kind].uniformSize;
        if(cursor+uniformBytes+instanceBytes>arena.size||draws>=8176){++skipped;++skipReasons[5];return;}
        std::memcpy(static_cast<uint8_t*>(arena.mapped)+cursor,item.uniforms.data(),uniformBytes);
        if(p.ui&&postDone&&!offscreen)for(const auto& field:p.fields)if(!std::strcmp(field.field.name,"uTint")){
            auto* tint=reinterpret_cast<float*>(static_cast<uint8_t*>(arena.mapped)+cursor+field.field.offset);
            for(int channel=0;channel<3;++channel)tint[channel]=fo3uicolor::Linear(tint[channel]);
        }
        if(p.water)for(const auto& e:p.fields) {
            float* out=reinterpret_cast<float*>(static_cast<uint8_t*>(arena.mapped)+cursor+e.field.offset);
            if(!std::strcmp(e.field.name,"uSceneSnapshotReady"))*out=snapshotReady?1.0f:0.0f;
            if(!std::strcmp(e.field.name,"uReflectionReady"))*out=0.0f; // planar native pass pending
            if(!std::strcmp(e.field.name,"uViewport")){out[0]=float(width);out[1]=float(height);}
        }
        // Native scene attachments are FP16 even when the compatibility
        // frontend's legacy RTT state is inactive. Match the actual blend target.
        if(fused&&!offscreen&&!postDone&&p.roundOffset!=UINT32_MAX){
            const int mode=1;std::memcpy(static_cast<uint8_t*>(arena.mapped)+cursor+p.roundOffset,&mode,4);
        }
        std::vector<VkDescriptorImageInfo> imageInfo(shaders::programs[kind].samplerCount);
        const auto* samplerMeta=shaders::programs[kind].samplers;
        for(size_t i=0;i<imageInfo.size();++i){
            VkImageView view=samplerMeta[i].cube?cube.view:white.view;
            const int unit=item.units[i];
            VkSampler chosenSampler=samplerMeta[i].cube?clamped:sampler;
            if(unit>=0&&unit<16){
                const auto& binding=samplerMeta[i].cube?s.cubes[unit]:s.textures[unit];
                auto tex=textures.find(binding);
                if(tex!=textures.end()){view=tex->second.view;if(tex->second.palette)chosenSampler=nearest;else if(tex->second.repeatT&&tex->second.repeatS)chosenSampler=sampler;else if(tex->second.repeatS)chosenSampler=horizontalRepeat;else if(tex->second.repeatT)chosenSampler=verticalRepeat;else if(tex->second.clamp)chosenSampler=clamped;}
            }
            if(p.water) {
                const std::string name=samplerMeta[i].name;
                if(name=="uSceneColor"||name=="uReflectionMap"){view=sceneColor.view;chosenSampler=clamped;}
                if(name=="uSceneDepth"){view=sceneDepth.view;chosenSampler=nearest;}
            }
            if(unit>=0&&unit<16&&!samplerMeta[i].cube) {
                auto ui=uiTargets.find(s.textures[unit]);
                if(ui!=uiTargets.end()&&ui->second.target.initialized){view=ui->second.sampled;chosenSampler=clamped;}
            }
            imageInfo[i]={chosenSampler,view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        }
        const auto set=Descriptor(kind,uint32_t(uniformBytes),imageInfo);
        const uint32_t dynamic=uint32_t(cursor);cursor+=uniformBytes;
        const VkDeviceSize instanceOffset=cursor;
        if(key.instanced){std::memcpy(static_cast<uint8_t*>(arena.mapped)+cursor,draw.transforms.data(),instanceBytes);cursor+=instanceBytes;}
        const VkViewport viewport{0,0,float(targetWidth),float(targetHeight),0,1};const VkRect2D scissor{{0,0},{targetWidth,targetHeight}};
        vkCmdSetViewport(command,0,1,&viewport);vkCmdSetScissor(command,0,1,&scissor);
        vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,Pipeline(key));
        vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,layouts[kind],0,1,&set,1,&dynamic);
        if(key.offset)vkCmdSetDepthBias(command,s.offsetUnits,0,s.offsetFactor);
        const VkDeviceSize zero=0;vkCmdBindVertexBuffers(command,0,1,&m.vertices.buffer,&zero);
        if(key.skinned)vkCmdBindVertexBuffers(command,2,1,&m.skin.buffer,&zero);
        if(key.instanced)vkCmdBindVertexBuffers(command,1,1,&arena.buffer,&instanceOffset);
        if(type){vkCmdBindIndexBuffer(command,m.indices.buffer,0,VK_INDEX_TYPE_UINT32);vkCmdDrawIndexed(command,count,instances,indexFirst,0,0);}
        else vkCmdDraw(command,count,instances,first,0);
        ++draws;vertices+=uint64_t(count)*instances;
        if(key.foveated)++foveatedDraws;
        if((kind<2||p.fused)&&(key.lighting||key.noLocal))++specializedDraws;
        if(kind<2||kind==TerrainFusionKind){if(key.noDiscard)++solidDraws;else ++guardedDraws;if(p.terrain){++landDraws;if(key.fused&&key.stride==15)++fusedLandDraws;}else ++worldDraws;}
        if(passTiming)drawCpuUs+=NowUs()-drawStarted;
    }
    bool StereoBoundsVisible(const float* vertices,size_t count,size_t stride)const {
        if(!begun||offscreen||!stereo.mapped)return true;
        const auto bounds=fo3terrain::BuildBounds(vertices,count*stride,stride);
        const float* cameras=static_cast<const float*>(stereo.mapped);
        return fo3terrain::MayIntersectClip(bounds,cameras)||fo3terrain::MayIntersectClip(bounds,cameras+16);
    }
    void WaterSnapshot() {
        if(!begun||!passOpen||snapshotReady||failed)return;
        DrainDraws();
        // Refraction requires arbitrary displaced screen taps, so retain an
        // immutable opaque image rather than sampling an active attachment.
        // Copy both views once, before any water CELL modifies the color target.
        EndPass();Phase(Snapshot);
        std::array<VkImageMemoryBarrier,4> before{};
        for(size_t i=0;i<before.size();++i) {
            auto& b=before[i];b.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            const bool isDepth=i%2;
            b.subresourceRange={isDepth?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,2};
            b.image=i==0?color.image:i==1?depth.image:i==2?sceneColor.image:sceneDepth.image;
            b.oldLayout=i<2?(isDepth?VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL):
                snapshotInitialized?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED;
            b.newLayout=i<2?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            b.srcAccessMask=i<2?(isDepth?VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT:VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT):
                snapshotInitialized?VK_ACCESS_SHADER_READ_BIT:0;
            b.dstAccessMask=i<2?VK_ACCESS_TRANSFER_READ_BIT:VK_ACCESS_TRANSFER_WRITE_BIT;
        }
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,4,before.data());
        for(size_t i=0;i<2;++i) {
            VkImageCopy copy{};copy.srcSubresource={i?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT,0,0,2};
            copy.dstSubresource=copy.srcSubresource;copy.extent={width,height,1};
            vkCmdCopyImage(command,i?depth.image:color.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                i?sceneDepth.image:sceneColor.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
        }
        auto after=before;
        for(size_t i=0;i<after.size();++i) {
            auto& b=after[i];b.oldLayout=before[i].newLayout;
            b.newLayout=i<2?(i==1?VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL):VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            b.srcAccessMask=before[i].dstAccessMask;
            b.dstAccessMask=i<2?(i==1?VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT:
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT):VK_ACCESS_SHADER_READ_BIT;
        }
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,0,nullptr,0,nullptr,4,after.data());
        VkRenderPassBeginInfo ri{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};ri.renderPass=densityActive?(sidecarPass?densitySplit.load:densityMain.load):sidecarPass?splitResumePass:resumePass;ri.framebuffer=densityActive?(sidecarPass?densitySplit.framebuffer:densityMain.framebuffer):sidecarPass?splitFramebuffer:framebuffer;ri.renderArea={{0,0},{width,height}};
        BeginPass(SceneResume,ri);
        snapshotReady=snapshotInitialized=true;
    }
    void DestroyPost(PostTarget& target) {
        if(target.framebuffer)vkDestroyFramebuffer(device,target.framebuffer,nullptr);
        Destroy(target.image);target={};
    }
    VkRenderPass MakePostPass(VkFormat format) {
        VkAttachmentDescription attachment{};attachment.format=format;attachment.samples=VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout=attachment.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference reference{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&reference;
        const uint32_t mask=3;VkRenderPassMultiviewCreateInfo mv{VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO};
        mv.subpassCount=1;mv.pViewMasks=&mask;mv.correlationMaskCount=1;mv.pCorrelationMasks=&mask;
        VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};ci.pNext=&mv;ci.attachmentCount=1;ci.pAttachments=&attachment;ci.subpassCount=1;ci.pSubpasses=&sub;
        VkRenderPass result{};Check(vkCreateRenderPass(device,&ci,nullptr,&result),"post render pass");return result;
    }
    VkRenderPass MakeOverlayPass(VkFormat format,bool freshDepth=false) {
        std::array<VkAttachmentDescription,2> attachments{};
        attachments[0].format=format;attachments[1].format=VK_FORMAT_D32_SFLOAT;
        for(auto& a:attachments){a.samples=VK_SAMPLE_COUNT_1_BIT;a.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;a.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
            a.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;a.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;}
        attachments[0].initialLayout=attachments[0].finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachments[1].initialLayout=attachments[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        if(freshDepth) {
            attachments[1].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachments[1].storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachments[1].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        }
        VkAttachmentReference colorRef{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},depthRef{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&colorRef;sub.pDepthStencilAttachment=&depthRef;
        const uint32_t mask=3;VkRenderPassMultiviewCreateInfo mv{VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO};mv.subpassCount=1;mv.pViewMasks=&mask;mv.correlationMaskCount=1;mv.pCorrelationMasks=&mask;
        VkSubpassDependency dependency{};dependency.srcSubpass=VK_SUBPASS_EXTERNAL;dependency.dstSubpass=0;
        dependency.srcStageMask=dependency.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};ci.pNext=&mv;ci.attachmentCount=2;ci.pAttachments=attachments.data();ci.subpassCount=1;ci.pSubpasses=&sub;ci.dependencyCount=1;ci.pDependencies=&dependency;
        VkRenderPass result{};Check(vkCreateRenderPass(device,&ci,nullptr,&result),"runtime overlay render pass");return result;
    }
    void MakePostTarget(PostTarget& target,uint32_t w,uint32_t h,bool adaptation=false) {
        target.width=w;target.height=h;
        target.image=MakeImage(w,h,adaptation?VK_FORMAT_R8G8B8A8_UNORM:VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
        VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};ci.renderPass=adaptation?adaptPass:postPass;
        ci.attachmentCount=1;ci.pAttachments=&target.image.view;ci.width=w;ci.height=h;ci.layers=1;
        Check(vkCreateFramebuffer(device,&ci,nullptr,&target.framebuffer),"post framebuffer");
    }
    void PostBarrier(Image& image,VkImageLayout oldLayout,VkImageLayout newLayout,VkAccessFlags src,VkAccessFlags dst,
                     VkPipelineStageFlags srcStage,VkPipelineStageFlags dstStage) {
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        b.image=image.image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,2};b.oldLayout=oldLayout;b.newLayout=newLayout;b.srcAccessMask=src;b.dstAccessMask=dst;
        vkCmdPipelineBarrier(command,srcStage,dstStage,0,0,nullptr,0,nullptr,1,&b);
    }
    void CreatePost() {
        if(postPass)return;
        postPass=MakePostPass(VK_FORMAT_R16G16B16A16_SFLOAT);adaptPass=MakePostPass(VK_FORMAT_R8G8B8A8_UNORM);
        for(auto& target:adapted)MakePostTarget(target,1,1,true);
        MakePostTarget(bloom[0],640,256);MakePostTarget(bloom[1],256,256);MakePostTarget(bloom[2],256,256);
        for(int kind=4;kind<9;++kind)
            postPipelines[kind-4]=MakePostPipeline(kind,kind==4?adaptPass:postPass);
    }
    void CreatePresented() {
        if(presented.image.image)return;
        MakePostTarget(presented,width,height);
        const VkImageView overlayViews[]{presented.image.view,depth.view};
        VkFramebufferCreateInfo overlay{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};overlay.renderPass=resumePass;overlay.attachmentCount=2;overlay.pAttachments=overlayViews;
        overlay.width=width;overlay.height=height;overlay.layers=1;
        Check(vkCreateFramebuffer(device,&overlay,nullptr,&postOverlayFramebuffer),"post overlay framebuffer");
    }
    VkPipeline MakePostPipeline(int kind,VkRenderPass targetPass) {
        std::array<VkPipelineShaderStageCreateInfo,2> stages{};
        for(int i=0;i<2;++i){stages[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stages[i].stage=i?VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_VERTEX_BIT;stages[i].module=modules[kind][i];stages[i].pName="main";}
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=vp.scissorCount=1;
        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.polygonMode=VK_POLYGON_MODE_FILL;rs.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState colorBlend{};colorBlend.colorWriteMask=0xf;
        VkPipelineColorBlendStateCreateInfo bs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};bs.attachmentCount=1;bs.pAttachments=&colorBlend;
        const VkDynamicState states[]{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dynamic.dynamicStateCount=2;dynamic.pDynamicStates=states;
        VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};ci.stageCount=2;ci.pStages=stages.data();ci.pVertexInputState=&vi;ci.pInputAssemblyState=&ia;
        ci.pViewportState=&vp;ci.pRasterizationState=&rs;ci.pMultisampleState=&ms;ci.pColorBlendState=&bs;ci.pDynamicState=&dynamic;
        ci.layout=layouts[kind];ci.renderPass=targetPass;
        VkPipeline result{};
        Check(vkCreateGraphicsPipelines(device,pipelineCache,1,&ci,nullptr,&result),"native fullscreen pipeline");
        return result;
    }
    using PostValues=std::unordered_map<std::string,std::vector<float>>;
    void PostDraw(int kind,PostTarget& target,const std::vector<VkDescriptorImageInfo>& images,const PostValues& values,bool compositor=false) {
        const auto& shader=shaders::programs[kind];
        const VkDeviceSize alignment=std::max<VkDeviceSize>(16,properties.limits.minUniformBufferOffsetAlignment);
        cursor=(cursor+alignment-1)&~(alignment-1);
        if(cursor+shader.uniformSize>arena.size)throw std::runtime_error("post uniform arena exhausted");
        auto* bytes=static_cast<uint8_t*>(arena.mapped)+cursor;std::memset(bytes,0,shader.uniformSize);
        for(size_t i=0;i<shader.fieldCount;++i) {
            const auto& field=shader.fields[i];const auto value=values.find(field.name);if(value==values.end())continue;
            if(field.integer){const int integer=int(value->second.at(0));std::memcpy(bytes+field.offset,&integer,4);}
            else std::memcpy(bytes+field.offset,value->second.data(),std::min<size_t>(field.components,value->second.size())*4);
        }
        const auto set=Descriptor(kind,shader.uniformSize,images);
        PostBarrier(target.image,target.initialized?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            target.initialized?VK_ACCESS_SHADER_READ_BIT:0,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            target.initialized?VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        VkRenderPassBeginInfo ri{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};ri.renderPass=compositor?xrPass:kind==4?adaptPass:postPass;ri.framebuffer=target.framebuffer;ri.renderArea={{0,0},{target.width,target.height}};
        BeginPass(compositor?(kind==8?Film:Output):kind==4?Adapt:kind==8?Film:Bloom,ri);
        const VkViewport viewport{0,0,float(target.width),float(target.height),0,1};const VkRect2D scissor{{0,0},{target.width,target.height}};
        vkCmdSetViewport(command,0,1,&viewport);vkCmdSetScissor(command,0,1,&scissor);
        vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,compositor?(kind==8?xrFilmPipeline:xrPipeline):postPipelines[kind-4]);
        const uint32_t offset=uint32_t(cursor);cursor+=shader.uniformSize;
        vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,layouts[kind],0,1,&set,1,&offset);
        vkCmdDraw(command,3,1,0,0);EndPass();
        if(!compositor)PostBarrier(target.image,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        // OpenXR requires COLOR_ATTACHMENT_OPTIMAL on release, not PRESENT_SRC_KHR.
        target.initialized=!compositor;
    }
    void PostProcess(const PostSettings& settings) {
        if(!begun||!passOpen||failed||postDone)return;
        DrainDraws();CreatePost();EndPass();RetainWorldDepth();
        PostBarrier(color,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        directOutput=gameplay&&xrSwapchain&&xrSrgb&&!legacyPresentation&&!splitFrame;
        if(directOutput)AcquireColor();else if(compactFrame)CreateCompactFinal();else CreatePresented();
        auto& filmTarget=directOutput?xrTargets[acquiredColor]:Finished();
        if(visualDiagnostics&2u) {
            // Diagnostic only: raw scene HDR sampled directly, without the
            // original adaptation/bloom/film. Values outside SDR are clipped
            // by the display; this is NOT an alternative production renderer.
            PostDraw(5,filmTarget,{{clamped,color.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}},{},directOutput);
        } else {
        // Retail/GLES adaptation is eye 0 once per stereo frame. Both native
        // history layers deliberately use that same source, avoiding eye rivalry.
        auto& previous=adapted[adaptIndex];
        if(!previous.initialized) {
            PostBarrier(previous.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
            const VkClearColorValue zero{{0,0,0,1}};const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,2};
            vkCmdClearColorImage(command,previous.image.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&zero,1,&range);
            PostBarrier(previous.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            previous.initialized=true;
        }
        auto sample=[&](Image& image,bool point=false){return VkDescriptorImageInfo{point?nearest:clamped,image.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};};
        const int next=1-adaptIndex;
        PostDraw(4,adapted[next],{sample(color),sample(previous.image,true)},{{"uTargetLum",{settings.upperLum}},{"uEyeAdaptSpeed",{settings.eyeSpeed}},{"uFirstFrame",{exposureFirst?1.0f:0.0f}}});
        adaptIndex=next;exposureFirst=false;
        PostDraw(5,bloom[0],{sample(color)},{});
        PostDraw(5,bloom[1],{sample(bloom[0].image,true)},{});
        PostDraw(6,bloom[2],{sample(bloom[1].image,true),sample(adapted[next].image,true)},{{"uBrightThreshold",{settings.brightThreshold}},{"uBrightScale",{settings.brightScale}}});
        PostDraw(7,bloom[1],{sample(bloom[2].image,true)},{});
        PostDraw(8,filmTarget,{sample(color),sample(adapted[next].image,true),sample(bloom[1].image)},
            splitPostValues={{"uFlags",{float(settings.flags)}},{"uInteriorMode",{float(settings.interior)}},{"uSaturation",{settings.saturation}},
             {"uContrastAvg",{settings.contrastAvg}},{"uContrast",{settings.contrast}},{"uBrightness",{settings.brightness}},
             {"uScriptFade",{settings.scriptFade[0],settings.scriptFade[1],settings.scriptFade[2],settings.scriptFade[3]}},
             {"uScriptTint",{settings.scriptTint[0],settings.scriptTint[1],settings.scriptTint[2],settings.scriptTint[3]}},
             {"uScriptGrade",{settings.scriptGrade[0],settings.scriptGrade[1],settings.scriptGrade[2]}},{"uScriptBlur",{settings.scriptBlur}},
             {"uTintColor",{settings.tint[0],settings.tint[1],settings.tint[2]}},{"uTintValue",{settings.tintValue}},
             {"uTargetLumQ1520",{settings.targetLum}},{"uPcBloomReadyQ1670",{1}}},directOutput||compactFrame);
        if(compactFrame)compactFinal.initialized=true;
        }
        postDone=true;
        // Notifications, XP, pause and performance HUD are deliberately drawn
        // after the film/gamma pass in the original game path. Retain that order.
        PostBarrier(filmTarget.image,(directOutput||compactFrame)?VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            (directOutput||compactFrame)?VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT:VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            (directOutput||compactFrame)?VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT:VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        VkRenderPassBeginInfo overlay{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};overlay.renderPass=(directOutput||compactFrame)?xrOverlayPass:resumePass;overlay.framebuffer=(directOutput||compactFrame)?filmTarget.overlay:postOverlayFramebuffer;overlay.renderArea={{0,0},{width,height}};
        BeginPass(Overlay,overlay);
    }
    bool BeginModalDepth() {
        DrainDraws();
        if(!begun||!passOpen||!postDone||offscreen||failed)return false;
        // Lazy, retained private depth: world depth remains available to the
        // compositor, and modal pieces still correctly occlude each other.
        if(!modalDepth.image)modalDepth=MakeImage(width,height,VK_FORMAT_D32_SFLOAT,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,VK_IMAGE_ASPECT_DEPTH_BIT);
        auto& renderPass=directOutput?modalXrPass:modalPass;
        if(!renderPass)renderPass=MakeOverlayPass(directOutput?xrColorFormat:VK_FORMAT_R16G16B16A16_SFLOAT,true);
        if(directOutput&&modalXrFramebuffers.size()!=xrTargets.size())modalXrFramebuffers.resize(xrTargets.size());
        auto& target=directOutput?xrTargets[acquiredColor]:Finished();
        auto& fb=directOutput?modalXrFramebuffers[acquiredColor]:modalFramebuffer;
        if(!fb) {
            const VkImageView views[]{target.image.view,modalDepth.view};
            VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fi.renderPass=renderPass;
            fi.attachmentCount=2;fi.pAttachments=views;fi.width=width;fi.height=height;fi.layers=1;
            Check(vkCreateFramebuffer(device,&fi,nullptr,&fb),"modal depth framebuffer");
        }
        EndPass();
        VkClearValue clear[2]{};clear[1].depthStencil={1,0};
        VkRenderPassBeginInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};info.renderPass=renderPass;
        info.framebuffer=fb;info.renderArea={{0,0},{width,height}};info.clearValueCount=2;info.pClearValues=clear;
        BeginPass(Overlay,info);modalActive=true;return true;
    }
    void DestroyUi(UiTarget& ui) {
        if(ui.sampled){++viewRevision;vkDestroyImageView(device,ui.sampled,nullptr);}
        DestroyPost(ui.target);ui={};
    }
    bool BeginUiTarget(uint32_t id,uint32_t w,uint32_t h) {
        if(begun||failed||!id||!w||!h)return false;
        if(!uiPass)uiPass=MakePostPass(VK_FORMAT_R8G8B8A8_UNORM);
        auto& ui=uiTargets[id];
        if(ui.target.image.image&&(ui.target.width!=w||ui.target.height!=h))RetireUi(ui);
        if(!ui.target.image.image) {
            ui.target.width=w;ui.target.height=h;
            ui.target.image=MakeImage(w,h,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
            VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};ci.renderPass=uiPass;ci.attachmentCount=1;ci.pAttachments=&ui.target.image.view;
            ci.width=w;ci.height=h;ci.layers=1;Check(vkCreateFramebuffer(device,&ci,nullptr,&ui.target.framebuffer),"Pip-Boy framebuffer");
            VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=ui.target.image.image;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=VK_FORMAT_R8G8B8A8_UNORM;
            vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};Check(vkCreateImageView(device,&vi,nullptr,&ui.sampled),"Pip-Boy sampled view");
        }
        offscreen=true;activeUi=&ui;targetWidth=w;targetHeight=h;
        const float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},eye[]{0,0,0};
        StartCommands(identity,identity,eye,eye);
        PostBarrier(ui.target.image,ui.target.initialized?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,ui.target.initialized?VK_ACCESS_SHADER_READ_BIT:0,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            ui.target.initialized?VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        VkRenderPassBeginInfo ri{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};ri.renderPass=uiPass;ri.framebuffer=ui.target.framebuffer;ri.renderArea={{0,0},{w,h}};
        BeginPass(Ui,ri);
        VkClearAttachment clear{};clear.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;clear.colorAttachment=0;clear.clearValue.color={{0,0,0,1}};
        const VkClearRect rect{{{0,0},{w,h}},0,1};vkCmdClearAttachments(command,1,&clear,1,&rect);
        return true;
    }
    void EndUiTarget() {
        if(!begun||!offscreen||!activeUi)return;
        DrainDraws();EndPass();
        PostBarrier(activeUi->target.image,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        activeUi->target.initialized=true;End();activeUi=nullptr;offscreen=false;
    }
    void StartCommands(const float* left,const float* right,const float* le,const float* re,bool loading=false) {
        if(begun)throw std::runtime_error("overlapping frame recording");
        loadingFrame=loading;modalActive=false;
        FlushUploads();
        if(Buffered()){CreateContexts();SelectSlot(offscreen?2:nextWorldSlot);if(!offscreen)nextWorldSlot=1-nextWorldSlot;}
        const auto reuseStarted=NowUs();WaitSlot(activeSlot);Collect();
        reuseWaitUs=NowUs()-reuseStarted;queueCpuUs=drawCpuUs=finishWaitUs=xrWaitUs=0;recordStarted=NowUs();
        queryCursor=4;timingDropped=0;activeSpan=-1;gpuSpans.clear();passMs.fill(0);
        Check(vkResetCommandPool(device,pool,0),"reset commands");PrepareDescriptors();
        float data[108]{};data[104]=offscreen?1.0f:0.0f;std::memcpy(data,left,64);std::memcpy(data+16,right,64);std::memcpy(data+32,le,12);std::memcpy(data+36,re,12);for(size_t eye=0;eye<2;++eye) {
            float* sky=data+40+eye*16;const float* world=data+eye*16;const float* position=data+32+eye*4;
            std::memcpy(sky,world,64);
            for(size_t row=0;row<4;++row)sky[12+row]+=world[row]*position[0]+world[4+row]*position[1]+world[8+row]*position[2];
        }
        // UI receives a combined object MVP from the original renderer. The
        // clip-space delta preserves that object transform for the second eye.
        double inverse[4][8]{};
        for(int row=0;row<4;++row)for(int col=0;col<4;++col){inverse[row][col]=left[col*4+row];inverse[row][col+4]=row==col?1:0;}
        for(int col=0;col<4;++col) {
            int pivot=col;for(int row=col+1;row<4;++row)if(std::fabs(inverse[row][col])>std::fabs(inverse[pivot][col]))pivot=row;
            if(std::fabs(inverse[pivot][col])<1e-12)throw std::runtime_error("singular left stereo camera");
            for(int j=0;j<8;++j)std::swap(inverse[col][j],inverse[pivot][j]);
            const double divisor=inverse[col][col];for(double& value:inverse[col])value/=divisor;
            for(int row=0;row<4;++row)if(row!=col){const double factor=inverse[row][col];for(int j=0;j<8;++j)inverse[row][j]-=factor*inverse[col][j];}
        }
        for(int col=0;col<4;++col)for(int row=0;row<4;++row) {
            data[72+col*4+row]=row==col?1.0f:0.0f;
            for(int k=0;k<4;++k)data[88+col*4+row]+=float(right[k*4+row]*inverse[k][col+4]);
        }
        std::memcpy(stereo.mapped,data,sizeof(data));
        pending.clear();postDone=false;directOutput=false;snapshotReady=false;cursor=0;foveatedDraws=specializedDraws=0;solidDraws=guardedDraws=worldDraws=landDraws=fusedLandDraws=0;draws=vertices=skipped=0;skipReasons.fill(0);firstSkippedProgram=0;
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        Check(vkBeginCommandBuffer(command,&bi),"begin frame");
        // Shared HDR/depth/history/UI targets are queue-ordered. Only CPU-written
        // command/uniform/descriptor/query resources rotate. This GPU dependency
        // protects shared images without stalling the CPU or duplicating HDR memory.
        VkMemoryBarrier ordered{VK_STRUCTURE_TYPE_MEMORY_BARRIER};ordered.srcAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
        ordered.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,1,&ordered,0,nullptr,0,nullptr);
        if(timestamps){vkCmdResetQueryPool(command,timestamps,0,passTiming?QueryCount:4);vkCmdWriteTimestamp(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,timestamps,0);}
        begun=true;
    }
    void Begin(const float* left,const float* right,const float* le,const float* re,bool loading=false,const float* horizon=nullptr,bool split=false) {
        splitFrame=split&&splitPass&&compositionChain&&!loading&&!(visualDiagnostics&2u);
        fqmotion::separatedComposition=splitFrame;compositionPresented=false;
        worldDepthReady=lateWorldDepthWrite=false;sidecarPass=splitFrame;compactFrame=splitFrame&&xrSrgb;
        densityActive=foveationRequested&&densityMain.clear&&(!splitFrame||densitySplit.clear)&&!loading;
        offscreen=false;targetWidth=width;targetHeight=height;StartCommands(left,right,le,re,loading);
        // Match the GLES clear from GetFo3Environment(), not hard-coded black.
        // The sky shader still paints the full authored sky where available.
        std::array<VkClearValue,4> clears{};
        // Software/probe validation uses a black offscreen reference;
        // only the real OpenXR gameplay scene has the GLES horizon clear.
        // Loading always uses the same black void as the standalone GLES UI.
        clears[0].color={{gameplay&&!loading&&horizon?horizon[0]:0.0f,
                          gameplay&&!loading&&horizon?horizon[1]:0.0f,
                          gameplay&&!loading&&horizon?horizon[2]:0.0f,1.0f}};
        clears[1].depthStencil={1,0};clears[2].color=clears[0].color;
        VkRenderPassBeginInfo ri{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};ri.renderPass=densityActive?(splitFrame?densitySplit.clear:densityMain.clear):splitFrame?splitPass:pass;ri.framebuffer=densityActive?(splitFrame?densitySplit.framebuffer:densityMain.framebuffer):splitFrame?splitFramebuffer:framebuffer;ri.renderArea={{0,0},{width,height}};ri.clearValueCount=splitFrame?3:2;ri.pClearValues=clears.data();
        BeginPass(Scene,ri);begun=true;
    }
    #include "fo3-vulkan-foveation.inc"
    #include "fo3-vulkan-composition.inc"
    #include "fo3-vulkan-spacewarp.inc"
    void End(PostTarget* output=nullptr,VkImage depthOutput=VK_NULL_HANDLE) {
        if(!begun)return;
        DrainDraws();begun=false;
        if(passOpen)EndPass();
        if(postDone&&!directOutput)PostBarrier(Finished().image,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        if(output&&splitFrame){ComposeSplit();}
        else if(output&&!directOutput) {
            ++presentationCopies;
            if(!loadingFrame&&!postDone)throw std::runtime_error("Vulkan gameplay missing native film pass");
            if(!postDone)PostBarrier(color,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            PostDraw(5,*output,{{clamped,postDone?presented.image.view:color.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}},{},true);
        }
        if(directOutput&&!(visualDiagnostics&2u))++directFilmFrames;
        if(depthOutput)CopyCompositorDepth(depthOutput);
        RenderMotion();
        EndTiming();
        if(timestamps)vkCmdWriteTimestamp(command,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,timestamps,2);
        Check(vkEndCommandBuffer(command),"end frame");Check(vkResetFences(device,1,&fence),"reset frame fence");
        FlushUploads(); // Same graphics queue; upload barriers precede every consuming frame.
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};si.commandBufferCount=1;si.pCommandBuffers=&command;
        Check(vkQueueSubmit(queue,1,&si,fence),"submit world probe");
        recordSpanUs=NowUs()-recordStarted;
        auto& slot=frameSlots[activeSlot];slot.pending=true;slot.serial=++submissionSerial;
        if(!offscreen)++frames;
        FrameSample sample;sample.copies=presentationCopies;sample.serial=submissionSerial;sample.frame=offscreen?frames+1:frames;
        sample.foveated=foveatedDraws;sample.specialized=specializedDraws;sample.compact=compactFrame&&!offscreen;sample.solid=solidDraws;sample.guarded=guardedDraws;sample.world=worldDraws;sample.land=landDraws;sample.fusedLand=fusedLandDraws;
        sample.record=recordSpanUs;sample.queueCpu=queueCpuUs;sample.drawCpu=drawCpuUs;sample.reuse=reuseWaitUs;sample.xrWait=xrWaitUs;
        sample.uploadCpu=uploadPrepareUs-lastUploadPrepareUs;lastUploadPrepareUs=uploadPrepareUs;
        sample.drawCount=draws;sample.vertexCount=vertices;sample.skipCount=skipped;sample.reasons=skipReasons;sample.firstSkip=firstSkippedProgram;
        sample.diagnostic=visualDiagnostics;sample.dropped=timingDropped;sample.offscreen=offscreen;sample.profile=passTiming;sample.supported=timestampSupported;
        sample.direct=directOutput&&!(visualDiagnostics&2u);sample.buffered=Buffered();sample.logFrame=!offscreen&&((frames-1)%120)==0;
        if(passTiming)sample.spans=gpuSpans;
        slot.sample=std::move(sample);
        if(!Buffered()) {
            const auto finishStarted=NowUs();Check(vkWaitForFences(device,1,&fence,VK_TRUE,UINT64_MAX),"finish synchronous submission");
            finishWaitUs=NowUs()-finishStarted;slot.sample.finish=finishWaitUs;
        }
        PollFrames();Collect();if(!offscreen&&!gameplay)Dump();
        if(!offscreen&&(frames%180)==0){
            SavePipelineCache();
            VKLOG("FQVK UNIFORMS calls=%llu unchanged=%llu",(unsigned long long)uniformCalls,(unsigned long long)uniformUnchanged);
        }
    }
    void ReportFrame(const FrameSample& sample,VkQueryPool queryPool) {
        if(sample.profile||sample.logFrame)VKLOG("FQVK MATERIAL submission=%llu worldDraws=%llu landDraws=%llu fusedLandDraws=%llu noDiscardDraws=%llu genericDraws=%llu genericForced=%d hdrColorSurfaceMiB=%.2f depthSurfaceMiB=%.2f stagingCacheLimit=%zu foveatedDraws=%llu lightingSpecializedDraws=%llu compactFilm=%d",
            (unsigned long long)sample.serial,(unsigned long long)sample.world,(unsigned long long)sample.land,(unsigned long long)sample.fusedLand,
            (unsigned long long)sample.solid,(unsigned long long)sample.guarded,genericShaders,
            double(width)*height*2*8/(1024*1024),double(width)*height*2*4/(1024*1024),stagingPoolCountLimit,(unsigned long long)sample.foveated,(unsigned long long)sample.specialized,sample.compact);
        const auto offscreen=sample.offscreen;const auto passTiming=sample.profile;
        const auto timestampSupported=sample.supported&&queryPool;
        const auto recordSpanUs=sample.record,queueCpuUs=sample.queueCpu,drawCpuUs=sample.drawCpu,reuseWaitUs=sample.reuse,finishWaitUs=sample.finish,xrWaitUs=sample.xrWait;
        const auto visualDiagnostics=sample.diagnostic;auto timingDropped=sample.dropped;const auto& gpuSpans=sample.spans;
        const auto draws=sample.drawCount,vertices=sample.vertexCount,skipped=sample.skipCount;
        const auto& skipReasons=sample.reasons;const auto firstSkippedProgram=sample.firstSkip;
        passMs.fill(0);if(passTiming&&queryPool)ReadTimings(queryPool,sample.spans,timingDropped);
        double gpuMs=-1;
        if(queryPool){uint64_t t[2]{};
            const auto begin=vkGetQueryPoolResults(device,queryPool,0,1,8,&t[0],8,VK_QUERY_RESULT_64_BIT);
            const auto end=vkGetQueryPoolResults(device,queryPool,2,1,8,&t[1],8,VK_QUERY_RESULT_64_BIT);
            if(begin==VK_SUCCESS&&end==VK_SUCCESS) {
                const uint64_t mask=timestampBits==64?~uint64_t{0}:(uint64_t{1}<<timestampBits)-1;
                gpuMs=double((t[1]-t[0])&mask)*properties.limits.timestampPeriod/1e6;
            }
        }
        if(passTiming) {
            VKLOG("FQVK TIMING submission=%llu frame=%llu offscreen=%d diagnosticMask=%u timestampSupported=%d stereoGpuMs=%.4f uploadCpuMs=%.3f recordSpanCpuMs=%.3f queueCpuMs=%.3f drawCpuMs=%.3f reuseWaitCpuMs=%.3f finishWaitCpuMs=%.3f xrWaitCpuMs=%.3f spans=%zu dropped=%u directFilm=%d buffered=%d completed=1 copies=%llu",
                (unsigned long long)sample.serial,(unsigned long long)sample.frame,offscreen,visualDiagnostics,timestampSupported,gpuMs,sample.uploadCpu/1000.0,recordSpanUs/1000.0,queueCpuUs/1000.0,drawCpuUs/1000.0,reuseWaitUs/1000.0,finishWaitUs/1000.0,xrWaitUs/1000.0,gpuSpans.size(),timingDropped,sample.direct,sample.buffered,(unsigned long long)sample.copies);
            VKLOG("FQVK GPU submission=%llu frame=%llu offscreen=%d timingVersion=2 scope=whole-render-passes sceneMs=%.4f sceneResumeMs=%.4f offscreenUiMs=%.4f adaptMs=%.4f bloomMs=%.4f filmMs=%.4f overlayMs=%.4f outputMs=%.4f snapshotCopyMs=%.4f depthCopyMs=%.4f motionMs=%.4f compositionMs=%.4f",
                (unsigned long long)sample.serial,(unsigned long long)sample.frame,offscreen,
                timestampSupported?passMs[Scene]:-1.0,timestampSupported?passMs[SceneResume]:-1.0,
                timestampSupported?passMs[Ui]:-1.0,timestampSupported?passMs[Adapt]:-1.0,timestampSupported?passMs[Bloom]:-1.0,
                timestampSupported?passMs[Film]:-1.0,timestampSupported?passMs[Overlay]:-1.0,timestampSupported?passMs[Output]:-1.0,
                timestampSupported?passMs[Snapshot]:-1.0,timestampSupported?passMs[DepthCopy]:-1.0,timestampSupported?passMs[Motion]:-1.0,timestampSupported?passMs[Composition]:-1.0);
        }
        if(sample.logFrame)VKLOG("FQVK RESOURCES uploadOperations=%llu uploadBatches=%llu uploadQueueIdles=%llu stagingBytes=%llu stagingPeakBytes=%llu stagingPressureWaits=%llu stagingBuffers=%llu stagingPeakBuffers=%llu descriptorAllocations=%llu descriptorHits=%llu descriptorResets=%llu paletteAllocations=%llu paletteUpdates=%llu stagingAllocations=%llu stagingReuses=%llu stagingEvictions=%llu stagingPoolBytes=%llu stagingPoolBuffers=%zu directFilmFrames=%llu presentationCopies=%llu",
            (unsigned long long)uploadOperations,(unsigned long long)uploadSubmissions,(unsigned long long)uploadQueueIdles,
            (unsigned long long)uploadBytes,(unsigned long long)uploadPeakBytes,(unsigned long long)uploadPressureWaits,(unsigned long long)uploadStagingBuffers,(unsigned long long)uploadPeakBuffers,(unsigned long long)descriptorAllocations,(unsigned long long)descriptorHits,(unsigned long long)descriptorResets,(unsigned long long)paletteAllocations,(unsigned long long)paletteUpdates,(unsigned long long)stagingAllocations,(unsigned long long)stagingReuses,(unsigned long long)stagingEvictions,(unsigned long long)stagingPoolBytes,stagingPool.size(),(unsigned long long)directFilmFrames,(unsigned long long)presentationCopies);

        if(sample.logFrame)VKLOG("FQVK FRAME frame=%llu stereoGpuMs=%.3f gpuScope=frameSpan sharedDraws=%llu vertexCorners=%llu skippedDraws=%llu skipProgram=%llu skipMesh=%llu skipPrimitive=%llu skipSkin=%llu skipRange=%llu skipCapacity=%llu firstSkippedProgram=%u diagnosticMask=%u presentation=%s sceneSubmission=cpu-commands benchmarkValid=0",
            (unsigned long long)sample.frame,gpuMs,(unsigned long long)draws,(unsigned long long)vertices,(unsigned long long)skipped,
            (unsigned long long)skipReasons[0],(unsigned long long)skipReasons[1],(unsigned long long)skipReasons[2],
            (unsigned long long)skipReasons[3],(unsigned long long)skipReasons[4],(unsigned long long)skipReasons[5],
            firstSkippedProgram,visualDiagnostics,gameplay?"Vulkan":"GLES-probe");
    }
    std::vector<uint16_t> Readback() {
        // Debug-only, explicit readback. No readback/copy occurs on ordinary frames.
        WaitFrames();
        if(directOutput||compactFrame)throw std::runtime_error("direct/compact output requires runtime image capture or legacy presentation comparison");
        const size_t bytes=size_t(width)*height*2*8;
        Buffer readback=MakeBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
        std::vector<uint16_t> data(bytes/2);
        try {
            Transfer([&](VkCommandBuffer cb){
                VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
                barrier.image=postDone?presented.image.image:color.image;barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,2};
                barrier.oldLayout=postDone?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
                vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
                VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,2};copy.imageExtent={width,height,1};
                vkCmdCopyImageToBuffer(cb,postDone?presented.image.image:color.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.buffer,1,&copy);
                VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};host.buffer=readback.buffer;host.size=VK_WHOLE_SIZE;
                host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
                host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
                vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
                barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;barrier.newLayout=postDone?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                barrier.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;barrier.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,0,nullptr,0,nullptr,1,&barrier);
            });
            std::memcpy(data.data(),readback.mapped,bytes);
        }catch(...){Destroy(readback);throw;}
        Destroy(readback);return data;
    }
    static float Half(uint16_t bits) {
        const bool negative=bits&0x8000;const unsigned exponent=(bits>>10)&31,mantissa=bits&1023;
        float value=exponent==0?std::ldexp(float(mantissa),-24):
            exponent==31?(mantissa?NAN:INFINITY):std::ldexp(float(1024+mantissa),int(exponent)-25);
        return negative?-value:value;
    }
    void Dump() {
#ifdef __ANDROID__
        char value[PROP_VALUE_MAX]{};__system_property_get("debug.falloutquest.vulkan_dump",value);
        if(loadingFrame||dumped||std::atoi(value)!=1||outputDirectory.empty()||!draws)return;
        const auto pixels=Readback();
        for(size_t eye=0;eye<2;++eye) {
            const std::string file=outputDirectory+"/vulkan-probe-"+(eye?"right":"left")+".pfm";
            std::ofstream out(file,std::ios::binary);if(!out)throw std::runtime_error("probe image output unavailable");
            out<<"PF\n"<<width<<" "<<height<<"\n-1.0\n";
            // PFM rows start at the bottom; Vulkan framebuffer rows start at the top.
            for(uint32_t row=height;row>0;--row)for(uint32_t x=0;x<width;++x) {
                const size_t pixel=(eye*size_t(width)*height+size_t(row-1)*width+x)*4;
                const float rgb[]{Half(pixels[pixel]),Half(pixels[pixel+1]),Half(pixels[pixel+2])};
                out.write(reinterpret_cast<const char*>(rgb),sizeof(rgb));
            }
            if(!out)throw std::runtime_error("probe image write failed");
            VKLOG("FQVK WORLD PROBE image=%s domain=%s",file.c_str(),postDone?"display-linear-after-imagespace":"scene-HDR-no-post");
        }
        dumped=true;
#endif
    }
    void CopyCompositorDepth(VkImage target) {
        Phase(DepthCopy);
        std::array<VkImageMemoryBarrier,2> barriers{};
        for(auto& b:barriers){b.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,2};}
        auto& source=barriers[0];auto& destination=barriers[1];
        source.image=depth.image;source.oldLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;source.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        source.srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;source.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
        destination.image=target;destination.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;destination.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;destination.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,2,barriers.data());
        VkImageCopy copy{};copy.srcSubresource=copy.dstSubresource={VK_IMAGE_ASPECT_DEPTH_BIT,0,0,2};copy.extent={width,height,1};
        vkCmdCopyImage(command,depth.image,source.newLayout,target,destination.newLayout,1,&copy);
        source.oldLayout=source.newLayout;source.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        source.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;source.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        destination.oldLayout=destination.newLayout;destination.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        destination.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;destination.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,0,0,nullptr,0,nullptr,2,barriers.data());
    }
    XrGraphicsBindingVulkan2KHR Binding() const {
        XrGraphicsBindingVulkan2KHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
        binding.instance=instance;binding.physicalDevice=physical;binding.device=device;
        binding.queueFamilyIndex=family;binding.queueIndex=0;return binding;
    }
    void DestroySwapchain() {
        if(device){if(vkDeviceWaitIdle(device)==VK_SUCCESS)PollFrames();else {for(auto& slot:frameSlots)slot.pending=false;graphicsCompleted=submissionSerial;}}
        DestroyCompositionSwapchain();
        colorAcquired=directOutput=false;
        for(auto it=pipelines.begin();it!=pipelines.end();) {
            if(it->first.swapchain){vkDestroyPipeline(device,it->second,nullptr);it=pipelines.erase(it);}else ++it;
        }
        for(auto fb:modalXrFramebuffers)if(fb)vkDestroyFramebuffer(device,fb,nullptr);
        modalXrFramebuffers.clear();
        if(modalXrPass)vkDestroyRenderPass(device,modalXrPass,nullptr);
        modalXrPass={};
        for(auto& target:xrTargets) {
            if(target.overlay)vkDestroyFramebuffer(device,target.overlay,nullptr);
            if(target.framebuffer)vkDestroyFramebuffer(device,target.framebuffer,nullptr);
            if(target.image.view)vkDestroyImageView(device,target.image.view,nullptr);
        }
        xrTargets.clear();
        if(xrFilmPipeline)vkDestroyPipeline(device,xrFilmPipeline,nullptr);
        if(xrOverlayPass)vkDestroyRenderPass(device,xrOverlayPass,nullptr);
        if(xrPipeline)vkDestroyPipeline(device,xrPipeline,nullptr);
        if(xrPass)vkDestroyRenderPass(device,xrPass,nullptr);
        xrPipeline={};xrPass={};xrOverlayPass={};xrFilmPipeline={};
        if(xrSwapchain)Proc<PFN_xrDestroySwapchain>(xrInstance,"xrDestroySwapchain")(xrSwapchain);
        if(xrDepthSwapchain)Proc<PFN_xrDestroySwapchain>(xrInstance,"xrDestroySwapchain")(xrDepthSwapchain);
        xrDepthSwapchain={};xrDepthImages.clear();xrSwapchain={};xrSession={};
    }
    void CreateSwapchain(XrSession session,bool submitDepth=false) {
        DestroySwapchain();gameplay=true;xrSession=session;
        auto formatsFn=Proc<PFN_xrEnumerateSwapchainFormats>(xrInstance,"xrEnumerateSwapchainFormats");
        uint32_t count=0;CheckXr(formatsFn(session,0,&count,nullptr),"Vulkan swapchain formats count");
        std::vector<int64_t> formats(count);CheckXr(formatsFn(session,count,&count,formats.data()),"Vulkan swapchain formats");
        // Film already returns display-linear light. Prefer an sRGB attachment
        // to encode that value on store, preserving the PC gamma-ramp shadows
        // that linear UNORM8 rounded to black. OpenXR decodes sRGB on sampling;
        // keep the shader's linear output unchanged for either attachment type.
        VkFormat format=VK_FORMAT_UNDEFINED;
        for(auto preferred:{VK_FORMAT_R8G8B8A8_SRGB,VK_FORMAT_B8G8R8A8_SRGB,VK_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_B8G8R8A8_UNORM})
            if(std::find(formats.begin(),formats.end(),int64_t(preferred))!=formats.end()){format=preferred;break;}
        if(format==VK_FORMAT_UNDEFINED)throw std::runtime_error("OpenXR has no supported RGBA/BGRA Vulkan swapchain format");
        xrColorFormat=format;
        xrSrgb=format==VK_FORMAT_R8G8B8A8_SRGB||format==VK_FORMAT_B8G8R8A8_SRGB;
        XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};info.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        info.format=format;info.sampleCount=1;info.width=width;info.height=height;info.faceCount=1;info.arraySize=2;info.mipCount=1;
        CheckXr(Proc<PFN_xrCreateSwapchain>(xrInstance,"xrCreateSwapchain")(session,&info,&xrSwapchain),"create Vulkan stereo swapchain");
        auto imagesFn=Proc<PFN_xrEnumerateSwapchainImages>(xrInstance,"xrEnumerateSwapchainImages");
        count=0;CheckXr(imagesFn(xrSwapchain,0,&count,nullptr),"Vulkan swapchain images count");
        if(!count)throw std::runtime_error("OpenXR returned no Vulkan swapchain images");
        std::vector<XrSwapchainImageVulkan2KHR> images(count,{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
        CheckXr(imagesFn(xrSwapchain,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())),"Vulkan swapchain images");
        xrPass=MakePostPass(format);xrPipeline=MakePostPipeline(5,xrPass);xrFilmPipeline=MakePostPipeline(8,xrPass);
        xrOverlayPass=MakeOverlayPass(format);
        xrTargets.resize(count);
        for(uint32_t i=0;i<count;++i) {
            auto& target=xrTargets[i];target.width=width;target.height=height;target.image.image=images[i].image;
            VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=images[i].image;
            vi.viewType=VK_IMAGE_VIEW_TYPE_2D_ARRAY;vi.format=format;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,2};
            Check(vkCreateImageView(device,&vi,nullptr,&target.image.view),"OpenXR stereo image view");
            VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fi.renderPass=xrPass;fi.attachmentCount=1;fi.pAttachments=&target.image.view;
            fi.width=width;fi.height=height;fi.layers=1;
            Check(vkCreateFramebuffer(device,&fi,nullptr,&target.framebuffer),"OpenXR multiview framebuffer");
            const VkImageView views[]{target.image.view,depth.view};fi.renderPass=xrOverlayPass;fi.attachmentCount=2;fi.pAttachments=views;
            Check(vkCreateFramebuffer(device,&fi,nullptr,&target.overlay),"OpenXR overlay framebuffer");
        }
        if(submitDepth&&std::find(formats.begin(),formats.end(),int64_t(VK_FORMAT_D32_SFLOAT))!=formats.end()) {
            info.format=VK_FORMAT_D32_SFLOAT;info.usageFlags=XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
            CheckXr(Proc<PFN_xrCreateSwapchain>(xrInstance,"xrCreateSwapchain")(session,&info,&xrDepthSwapchain),"create Vulkan stereo depth swapchain");
            count=0;CheckXr(imagesFn(xrDepthSwapchain,0,&count,nullptr),"Vulkan depth images count");
            if(!count)throw std::runtime_error("OpenXR returned no Vulkan depth images");
            xrDepthImages.resize(count,{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
            CheckXr(imagesFn(xrDepthSwapchain,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(xrDepthImages.data())),"Vulkan depth images");
        }
        if(splitPass)CreateCompositionSwapchain();
        VKLOG("FQ RENDERER session=Vulkan presentation=Vulkan experimental=1 arraySize=2 format=%d outputTransfer=%s samples=1 depthSubmission=%d sceneSubmission=cpu-commands glesContextRequired=0 presentationPath=%s passTiming=%d",int(format),
            (format==VK_FORMAT_R8G8B8A8_SRGB||format==VK_FORMAT_B8G8R8A8_SRGB)?"sRGB-attachment":"linear-UNORM-fallback",xrDepthSwapchain?1:0,xrSrgb&&!legacyPresentation?"direct":"copy",passTiming);
    }
    void AcquireColor() {
        if(colorAcquired)return;
        const auto started=NowUs();
        XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};uint32_t index=0;
        CheckXr(Proc<PFN_xrAcquireSwapchainImage>(xrInstance,"xrAcquireSwapchainImage")(xrSwapchain,&acquire,&index),"acquire Vulkan image");
        XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout=XR_INFINITE_DURATION;
        const auto ready=Proc<PFN_xrWaitSwapchainImage>(xrInstance,"xrWaitSwapchainImage")(xrSwapchain,&wait);
        // XR_TIMEOUT_EXPIRED is a positive result but does not grant image access.
        if(ready!=XR_SUCCESS)throw std::runtime_error("wait Vulkan image: "+std::to_string(ready));
        if(index>=xrTargets.size())throw std::runtime_error("OpenXR Vulkan image index out of range");
        acquiredColor=index;colorAcquired=true;xrWaitUs+=NowUs()-started;
    }
    bool Present(const XrView* views,XrCompositionLayerProjectionView* output,float nearZ,float farZ) {
        if(failed||!begun||!xrSwapchain)return false;
        if(!(nearZ>0&&farZ>nearZ&&std::isfinite(farZ)))throw std::runtime_error("invalid compositor depth range");
        AcquireColor();if(splitFrame)AcquireComposition();const uint32_t index=acquiredColor;
        XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout=XR_INFINITE_DURATION;
        uint32_t depthIndex=0;const bool submitDepth=xrDepthSwapchain&&!loadingFrame;compositionHasDepth=submitDepth;
        if(submitDepth) {
            const auto depthStarted=NowUs();
            CheckXr(Proc<PFN_xrAcquireSwapchainImage>(xrInstance,"xrAcquireSwapchainImage")(xrDepthSwapchain,&acquire,&depthIndex),"acquire Vulkan depth");
            const auto depthReady=Proc<PFN_xrWaitSwapchainImage>(xrInstance,"xrWaitSwapchainImage")(xrDepthSwapchain,&wait);
            if(depthReady!=XR_SUCCESS)throw std::runtime_error("wait Vulkan depth: "+std::to_string(depthReady));
            if(depthIndex>=xrDepthImages.size())throw std::runtime_error("OpenXR Vulkan depth index out of range");
            xrWaitUs+=NowUs()-depthStarted;
        }
        End(&xrTargets[index],submitDepth?xrDepthImages[depthIndex].image:VK_NULL_HANDLE);
        ReleaseMotion(); // Same session queue has received the motion commands.
        ReleaseComposition();
        // Commands are submitted on the session-bound queue before release.
        // OpenXR orders composition on that queue; CPU waits occur only at reuse.
        XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        if(submitDepth)CheckXr(Proc<PFN_xrReleaseSwapchainImage>(xrInstance,"xrReleaseSwapchainImage")(xrDepthSwapchain,&release),"release Vulkan depth");
        CheckXr(Proc<PFN_xrReleaseSwapchainImage>(xrInstance,"xrReleaseSwapchainImage")(xrSwapchain,&release),"release Vulkan image");
        colorAcquired=false;
        for(uint32_t eye=0;eye<2;++eye) {
            output[eye]={XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};output[eye].pose=views[eye].pose;output[eye].fov=views[eye].fov;
            output[eye].subImage={xrSwapchain,{{0,0},{int32_t(width),int32_t(height)}},eye};
            if(submitDepth) {
                auto& info=xrDepthInfo[eye];info.subImage={xrDepthSwapchain,{{0,0},{int32_t(width),int32_t(height)}},eye};
                info.minDepth=0;info.maxDepth=1;info.nearZ=nearZ;info.farZ=farZ;
                output[eye].next=splitFrame?nullptr:&info;
            }
        }
        fqmotion::separatedComposition=false;compositionPresented=splitFrame;
        return true;
    }
    ~Renderer() {
        SavePipelineCache(true);
        DestroyMotion();
        DestroySwapchain();
        if(device){CancelUploads();begun=false;DestroyDensityPasses(densityMain);Destroy(densityMap);
            SwapSlot(frameSlots[activeSlot]);
            for(auto& e:pipelines)vkDestroyPipeline(device,e.second,nullptr);
            for(auto& e:meshes){Destroy(e.second.vertices);Destroy(e.second.indices);Destroy(e.second.skin);}
            for(auto& e:textures)Destroy(e.second);
            Collect(true);
            for(auto& slot:frameSlots)DestroySlot(slot);
            for(auto& ui:uiTargets)DestroyUi(ui.second);
            if(uiPass)vkDestroyRenderPass(device,uiPass,nullptr);
            for(auto& target:adapted)DestroyPost(target);
            for(auto& target:bloom)DestroyPost(target);
            if(modalFramebuffer)vkDestroyFramebuffer(device,modalFramebuffer,nullptr);
            if(modalPass)vkDestroyRenderPass(device,modalPass,nullptr);
            Destroy(modalDepth);
            if(postOverlayFramebuffer)vkDestroyFramebuffer(device,postOverlayFramebuffer,nullptr);
            DestroyPost(presented);
            for(auto pipeline:postPipelines)if(pipeline)vkDestroyPipeline(device,pipeline,nullptr);
            if(postPass)vkDestroyRenderPass(device,postPass,nullptr);
            if(adaptPass)vkDestroyRenderPass(device,adaptPass,nullptr);
            for(auto& staging:stagingPool)Destroy(staging);
            stagingPool.clear();stagingPoolBytes=0;
            Destroy(arena);Destroy(stereo);Destroy(white);Destroy(cube);
            if(framebuffer)vkDestroyFramebuffer(device,framebuffer,nullptr);
            if(resumePass)vkDestroyRenderPass(device,resumePass,nullptr);
            if(pass)vkDestroyRenderPass(device,pass,nullptr);
            Destroy(color);Destroy(depth);Destroy(sceneColor);Destroy(sceneDepth);
            if(descriptors)vkDestroyDescriptorPool(device,descriptors,nullptr);
            for(auto module:solidModules)if(module)vkDestroyShaderModule(device,module,nullptr);
            for(int k=0;k<13;++k){if(layouts[k])vkDestroyPipelineLayout(device,layouts[k],nullptr);if(sets[k])vkDestroyDescriptorSetLayout(device,sets[k],nullptr);for(auto m:modules[k])if(m)vkDestroyShaderModule(device,m,nullptr);}
            if(timestamps)vkDestroyQueryPool(device,timestamps,nullptr);
            if(horizontalRepeat)vkDestroySampler(device,horizontalRepeat,nullptr);
            if(verticalRepeat)vkDestroySampler(device,verticalRepeat,nullptr);
            if(clamped)vkDestroySampler(device,clamped,nullptr);
            if(nearest)vkDestroySampler(device,nearest,nullptr);
            if(sampler)vkDestroySampler(device,sampler,nullptr);
            if(fence)vkDestroyFence(device,fence,nullptr);
            if(uploads)vkDestroyCommandPool(device,uploads,nullptr);
            if(pool)vkDestroyCommandPool(device,pool,nullptr);
            if(pipelineCache)vkDestroyPipelineCache(device,pipelineCache,nullptr);
            vkDestroyDevice(device,nullptr);
        }
        if(instance)vkDestroyInstance(instance,nullptr);
    }
};
std::unique_ptr<Renderer> renderer;
void Failure(const std::exception& e){VKLOG("FQVK FAILURE: %s; presentation=%s fallback=none",e.what(),renderer&&renderer->gameplay?"Vulkan-stopped":"GLES-probe-disabled");recording=false;if(renderer)renderer->failed=true;hooks={};}
}
bool InitializeWorldProbe(XrInstance instance,XrSystemId system,uint32_t w,uint32_t h,const char* outputDirectory) {
    ShutdownWorldProbe();
    try {
        renderer=std::make_unique<Renderer>();
        if(outputDirectory)renderer->outputDirectory=outputDirectory;
        renderer->Initialize(instance,system,w,h);
        hooks.flushUploads=[]()->uint64_t{try{return renderer->FlushUploads();}catch(const std::exception& e){Failure(e);return UINT64_MAX;}};
        hooks.uploadsComplete=[](uint64_t ticket){try{return renderer->UploadsComplete(ticket);}catch(const std::exception& e){Failure(e);return false;}};
        hooks.beginUiTarget=[](uint32_t id,uint32_t w,uint32_t h){try{return renderer->BeginUiTarget(id,w,h);}catch(const std::exception& e){Failure(e);return false;}};
        hooks.endUiTarget=[](){try{renderer->EndUiTarget();}catch(const std::exception& e){Failure(e);}};
        hooks.repeatS=[](uint32_t id){try{renderer->DrainDraws();auto found=renderer->textures.find(id);if(found!=renderer->textures.end())found->second.repeatS=true;}catch(const std::exception& e){Failure(e);}};
        hooks.repeatT=[](uint32_t id){try{renderer->DrainDraws();auto found=renderer->textures.find(id);if(found!=renderer->textures.end())found->second.repeatT=true;}catch(const std::exception& e){Failure(e);}};
        hooks.beginModalDepth=[](){try{return renderer->BeginModalDepth();}catch(const std::exception& e){Failure(e);return false;}};
        hooks.clearDepth=[](){try{auto& r=*renderer;r.DrainDraws();if(!r.begun||!r.passOpen||r.failed)return;
            VkClearAttachment attachment{};attachment.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;attachment.clearValue.depthStencil={1,0};
            const VkClearRect rect{{{0,0},{r.width,r.height}},0,1};vkCmdClearAttachments(r.command,1,&attachment,1,&rect);}catch(const std::exception& e){Failure(e);}};
        hooks.resetExposure=[](){renderer->exposureFirst=true;};
        hooks.frameTiming=[](double cadenceUs,double workUs,double endFrameUs){if(renderer&&renderer->passTiming)
            VKLOG("FQVK CPU submission=%llu frame=%llu cadenceMs=%.3f workSpanMs=%.3f xrEndFrameCpuMs=%.3f",
                (unsigned long long)renderer->submissionSerial,(unsigned long long)renderer->frames,cadenceUs/1000.0,workUs/1000.0,endFrameUs/1000.0);};
        hooks.post=[](const PostSettings& settings){try{renderer->PostProcess(settings);}catch(const std::exception& e){Failure(e);}};
        hooks.waterSnapshot=[](){try{renderer->WaterSnapshot();}catch(const std::exception& e){Failure(e);}};
        hooks.program=[](uint32_t id,int terrain){try{renderer->Register(id,terrain);}catch(const std::exception& e){Failure(e);}};
        hooks.uniform=[](uint32_t id,int location,int kind,int size,const void* bytes){renderer->Uniform(id,location,kind,size,bytes);};
        hooks.mesh=[](uint32_t vao,const float* vertices,size_t n,size_t stride,const uint32_t* indices,size_t indexCount){
            try {if(!vao||!vertices||!n)return;auto& r=*renderer;r.DrainDraws();
                auto& m=r.meshes[vao];r.Retire(m.vertices);r.Retire(m.indices);
                m.stride=uint32_t(stride);m.count=uint32_t(indexCount?indexCount:n);
                m.vertices=r.UploadBuffer(vertices,n*stride*4,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
                if(indexCount)m.indices=r.UploadBuffer(indices,indexCount*4,VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
            }catch(const std::exception& e){Failure(e);}};
        hooks.texture=[](uint32_t id,int w,int h,const uint8_t* rgba,unsigned flags){try{if(w<=0||h<=0||!rgba)return;auto& r=*renderer;r.DrainDraws();auto image=r.UploadTexture(w,h,rgba,false,flags&2,flags&1);r.Retire(r.textures[id]);r.textures[id]=image;}catch(const std::exception& e){Failure(e);}};
        hooks.dynamicMesh=[](uint32_t vao,const float* data,size_t floats){try{
            auto& r=*renderer;r.DrainDraws();auto found=r.meshes.find(vao);if(found==r.meshes.end())return;
            auto buffer=r.UploadBuffer(data,floats*4,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
            r.Retire(found->second.vertices);found->second.vertices=buffer;
        }catch(const std::exception& e){Failure(e);}};
        hooks.skin=[](uint32_t vao,uint32_t texture,const float* attributes,size_t count,const float* palette,size_t rows){try{
            auto& r=*renderer;r.DrainDraws();auto found=r.meshes.find(vao);if(found==r.meshes.end())return;
            if(attributes&&count){auto b=r.UploadBuffer(attributes,count*4,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);r.Retire(found->second.skin);found->second.skin=b;}
            auto paletteImage=r.textures.find(texture);
            if(r.AsyncUploads()&&!r.begun&&paletteImage!=r.textures.end()&&paletteImage->second.palette&&paletteImage->second.width==8&&paletteImage->second.height==rows)
                r.UpdatePalette(paletteImage->second,palette,rows);
            else {auto image=r.UploadPalette(palette,rows);r.Retire(r.textures[texture]);r.textures[texture]=image;}
        }catch(const std::exception& e){Failure(e);}};
        hooks.cube=[](uint32_t id,const Fo3RgbaCubeTexture& source){try{
            auto& r=*renderer;r.DrainDraws();auto image=r.UploadCube(source);r.Retire(r.textures[id]);r.textures[id]=image;
        }catch(const std::exception& e){Failure(e);}};
        hooks.draw=[](const fqrender::DrawCommand& command){try{renderer->Queue(command);}catch(const std::exception& e){Failure(e);}};
        hooks.erase=[](uint32_t id,bool texture){try{auto& r=*renderer;r.DrainDraws();
            if(texture){auto ui=r.uiTargets.find(id);if(ui!=r.uiTargets.end()){r.RetireUi(ui->second);r.uiTargets.erase(ui);}
                auto i=r.textures.find(id);if(i!=r.textures.end()){r.Retire(i->second);r.textures.erase(i);}}
            else{auto i=r.meshes.find(id);if(i!=r.meshes.end()){r.Retire(i->second.vertices);r.Retire(i->second.indices);r.Retire(i->second.skin);r.meshes.erase(i);}}}catch(const std::exception& e){Failure(e);}};
        return true;
    }catch(const std::exception& e){Failure(e);ShutdownWorldProbe();return false;}
}
void ShutdownWorldProbe(){hooks={};recording=false;gameplayPresentation=fqmotion::separatedComposition=false;renderer.reset();}
bool WorldProbeActive(){return renderer&&!renderer->failed;}
bool BeginWorldProbe(const float* l,const float* r,const float* le,const float* re,bool loading,const float* horizon,bool splitComposition){
    if(!WorldProbeActive())return false;
    try{renderer->Begin(l,r,le,re,loading,horizon,splitComposition);recording=true;return true;}catch(const std::exception& e){Failure(e);return false;}
}
bool StereoBoundsVisible(const float* vertices,size_t count,size_t stride){return !renderer||renderer->StereoBoundsVisible(vertices,count,stride);}
void PrewarmPipelines(){if(renderer)renderer->PrewarmPipelines();}
void EndWorldProbe(){recording=false;if(renderer&&renderer->failed){if(!renderer->gameplay)ShutdownWorldProbe();return;}if(!renderer||!renderer->begun)return;try{renderer->End();}catch(const std::exception& e){Failure(e);}}
bool GameplayBinding(XrGraphicsBindingVulkan2KHR& binding) {
    if(!WorldProbeActive())return false;
    renderer->gameplay=gameplayPresentation=true;binding=renderer->Binding();return true;
}
bool CreateGameplaySwapchain(XrSession session,bool submitDepth) {
    if(!WorldProbeActive())return false;
    try{renderer->CreateSwapchain(session,submitDepth);return true;}catch(const std::exception& e){Failure(e);return false;}
}
void DestroyGameplaySwapchain(){if(renderer)renderer->DestroySwapchain();}
bool PresentGameplay(const XrView* views,XrCompositionLayerProjectionView* output,float nearZ,float farZ) {
    recording=false;if(!WorldProbeActive())return false;
    try{return renderer->Present(views,output,nearZ,farZ);}catch(const std::exception& e){Failure(e);return false;}
}
bool InitializeSpacewarp(const std::array<MotionSwapchainTarget,2>& targets) {
    if(!WorldProbeActive())return false;
    try{renderer->InitializeMotion(targets,VK_FORMAT_D32_SFLOAT);renderer->CreateComposition();return true;}
    catch(const std::exception& e){VKLOG("FQ SPACEWARP Vulkan initialization failed: %s; native rendering retained",e.what());renderer->DestroyMotion();return false;}
}
bool SpacewarpOverlay(const XrView* views,XrCompositionLayerProjectionView* output){return renderer&&renderer->OverlayViews(views,output);}
void ShutdownSpacewarp(){if(renderer)renderer->DestroyMotion();}
void QueueSpacewarp(const fqmotion::Capture& capture,std::array<fqspacewarp::Coverage,2>& coverage){
    if(!WorldProbeActive()||!renderer->motionPass)return;
    renderer->motionCapture=&capture;renderer->motionCoverage=&coverage;
}

}
