#include "fo3-vulkan-renderer.h"
#include "fo3-vulkan-bridge.h"
#include "fo3-vulkan-shaders.h"
#include <GLES3/gl3.h> // Legacy numeric material enums only; no GLES calls.
#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <fstream>
#include <cmath>
#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
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
struct Buffer {VkBuffer buffer{};VkDeviceMemory memory{};void* mapped{};VkDeviceSize size{};};
struct Image {VkImage image{};VkImageView view{};VkDeviceMemory memory{};uint32_t levels=1,width=0,height=0;bool palette=false,clamp=false,repeatT=false;};
struct PostTarget {Image image;VkFramebuffer framebuffer{};uint32_t width=0,height=0;bool initialized=false;};
struct UiTarget {PostTarget target;VkImageView sampled{};};
struct MeshData {Buffer vertices,indices,skin;uint32_t count=0,stride=0;};
struct ProgramData {
    bool terrain=false,sky=false,water=false;int ui=0;
    std::vector<uint8_t> bytes;
    struct Location {int location;shaders::Field field;};
    std::vector<Location> fields;
    std::vector<int> samplers;
    std::vector<int> units;
};
struct PipelineKey {
    uint32_t ui=0;bool strip=false,offscreen=false;
    bool offset=false,water=false,sky=false,skinned=false,instanced=false,terrain=false,depth=true,write=true,blend=false,cull=false,clockwise=false;
    uint32_t cullFace=GL_BACK,equation=GL_FUNC_ADD,equationA=GL_FUNC_ADD,stride=0,depthFunc=GL_LEQUAL,src=GL_ONE,dst=GL_ZERO,srcA=GL_ONE,dstA=GL_ZERO;
    bool operator==(const PipelineKey& k)const {
        return offscreen==k.offscreen&&strip==k.strip&&ui==k.ui&&water==k.water&&offset==k.offset&&cullFace==k.cullFace&&equation==k.equation&&equationA==k.equationA&&sky==k.sky && skinned==k.skinned && instanced==k.instanced && terrain==k.terrain && depth==k.depth && write==k.write && blend==k.blend &&
            cull==k.cull && clockwise==k.clockwise && stride==k.stride && depthFunc==k.depthFunc &&
            src==k.src && dst==k.dst && srcA==k.srcA && dstA==k.dstA;
    }
};
struct PipelineHash {size_t operator()(const PipelineKey& k)const {
    size_t h=uint32_t(k.offscreen)*4093+uint32_t(k.strip)*2017+k.ui*1009+uint32_t(k.water)*997+k.stride+uint32_t(k.instanced)*31+uint32_t(k.skinned)+uint32_t(k.sky)*101;for(auto v:{uint32_t(k.terrain),uint32_t(k.depth),uint32_t(k.write),uint32_t(k.blend),
        uint32_t(k.cull),uint32_t(k.clockwise),uint32_t(k.offset),k.cullFace,k.equation,k.equationA,k.depthFunc,k.src,k.dst,k.srcA,k.dstA}) { h=(h*16777619u)^v; }
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
    VkRenderPass pass{},resumePass{};VkFramebuffer framebuffer{};Image color,depth,white,cube,sceneColor,sceneDepth;
    bool snapshotReady=false,snapshotInitialized=false;
    VkSampler sampler{},nearest{},clamped{},verticalRepeat{};VkDescriptorSetLayout sets[12]{};VkPipelineLayout layouts[12]{};
    VkShaderModule modules[12][2]{};VkDescriptorPool descriptors{};
    VkQueryPool timestamps{};bool timestampSupported=false;uint32_t timestampBits=0;
    Buffer arena,stereo;VkDeviceSize cursor=0;uint32_t width=0,height=0;
    std::unordered_map<uint32_t,MeshData> meshes;
    std::unordered_map<uint32_t,Image> textures;
    std::unordered_map<uint32_t,ProgramData> programs;
    std::unordered_map<PipelineKey,VkPipeline,PipelineHash> pipelines;
    VkRenderPass postPass{},adaptPass{};
    VkPipeline postPipelines[5]{};VkFramebuffer postOverlayFramebuffer{};
    PostTarget adapted[2],bloom[3],presented;
    std::unordered_map<uint32_t,UiTarget> uiTargets;std::vector<UiTarget> retiredUi;
    UiTarget* activeUi=nullptr;bool offscreen=false;uint32_t targetWidth=0,targetHeight=0;VkRenderPass uiPass{};
    int adaptIndex=0;bool exposureFirst=true,postDone=false,passOpen=false;
    std::vector<Buffer> retiredBuffers;std::vector<Image> retiredImages;
    uint64_t draws=0,vertices=0,skipped=0,frames=0;bool begun=false;
    bool failed=false;
    // The OpenXR runtime owns these images. Only our views/framebuffers are destroyed.
    XrInstance xrInstance{};XrSession xrSession{};XrSwapchain xrSwapchain{};
    std::vector<PostTarget> xrTargets;
    XrSwapchain xrDepthSwapchain{};
    std::vector<XrSwapchainImageVulkan2KHR> xrDepthImages;
    XrCompositionLayerDepthInfoKHR xrDepthInfo[2]{{XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR},{XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR}};
    VkRenderPass xrPass{};VkPipeline xrPipeline{};
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
    bool descriptorsDirty=false;
    void PrepareDescriptors() {
        // Called after the frame fence. No cached set is reset while in flight.
        if(descriptorsDirty||descriptorCache.size()>DescriptorCapacity-FrameDescriptorReserve) {
            Check(vkResetDescriptorPool(device,descriptors,0),"recycle descriptors");
            descriptorCache.clear();descriptorsDirty=false;++descriptorResets;
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
    struct UploadBatch {
        VkCommandBuffer command{};VkFence fence{};uint64_t serial=0,bytes=0;uint32_t operations=0;
        std::vector<Buffer> staging;
    };
    UploadBatch uploadBatch;
    std::deque<UploadBatch> uploadFlight;
    uint64_t uploadSerial=0,uploadCompleted=0,uploadOperations=0,uploadSubmissions=0,uploadPressureWaits=0,uploadQueueIdles=0;
    uint64_t uploadBytes=0,uploadPeakBytes=0;
    uint64_t uploadBatchLimit=8u*1024u*1024u,uploadMemoryLimit=64u*1024u*1024u;
    uint32_t uploadOperationLimit=128;
    uint64_t paletteAllocations=0,paletteUpdates=0;
    bool AsyncUploads() const{return fqrender::nativeOnly;}
    void ReleaseUploadBatch(UploadBatch& batch) {
        for(auto& staging:batch.staging){uploadBytes-=staging.size;Destroy(staging);}
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
        while(uploadBytes>uploadMemoryLimit&&!uploadFlight.empty()) {
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
        uploadBatch.staging.push_back(staging);uploadBatch.bytes+=staging.size;uploadBytes+=staging.size;
        uploadPeakBytes=std::max(uploadPeakBytes,uploadBytes);staging={};
        if(uploadBatch.bytes>=uploadBatchLimit||uploadBatch.operations>=uploadOperationLimit)FlushUploads();
    }
    uint32_t MemoryType(uint32_t bits,VkMemoryPropertyFlags flags) {
        for(uint32_t i=0;i<memory.memoryTypeCount;++i)
            if((bits&(1u<<i))&&(memory.memoryTypes[i].propertyFlags&flags)==flags)return i;
        throw std::runtime_error("Vulkan memory type unavailable");
    }
    Buffer MakeBuffer(VkDeviceSize size,VkBufferUsageFlags usage,bool host) {
        Buffer b{};b.size=size;
        try {
            VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};ci.size=size;ci.usage=usage;
            Check(vkCreateBuffer(device,&ci,nullptr,&b.buffer),"create buffer");
            VkMemoryRequirements req{};vkGetBufferMemoryRequirements(device,b.buffer,&req);
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
        if(i.view){descriptorsDirty=true;vkDestroyImageView(device,i.view,nullptr);}
        if(i.image)vkDestroyImage(device,i.image,nullptr);
        if(i.memory)vkFreeMemory(device,i.memory,nullptr);
        i={};
    }
    void Retire(Buffer& buffer){if((begun||AsyncUploads())&&buffer.buffer){retiredBuffers.push_back(buffer);buffer={};}else Destroy(buffer);}
    void Retire(Image& image){if((begun||AsyncUploads())&&image.image){retiredImages.push_back(image);image={};}else Destroy(image);}
    void Collect(){PollUploads();if(begun||uploadBatch.command||!uploadFlight.empty())return;for(auto& ui:retiredUi)DestroyUi(ui);retiredUi.clear();for(auto& b:retiredBuffers)Destroy(b);for(auto& i:retiredImages)Destroy(i);retiredBuffers.clear();retiredImages.clear();}
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
        Buffer staging=MakeBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true),result;
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
        const uint32_t layers=isCube?6:1;const size_t bytes=size_t(w)*h*4*layers;
        uint32_t levels=1;if(mips)for(uint32_t n=std::max(w,h);n>1;n>>=1)++levels;
        VkFormatProperties properties{};vkGetPhysicalDeviceFormatProperties(physical,VK_FORMAT_R8G8B8A8_UNORM,&properties);
        const VkFormatFeatureFlags blit=VK_FORMAT_FEATURE_BLIT_SRC_BIT|VK_FORMAT_FEATURE_BLIT_DST_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        if(levels>1&&(properties.optimalTilingFeatures&blit)!=blit)throw std::runtime_error("RGBA8 mip generation unsupported");
        Buffer staging=MakeBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);Image result;
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
        ++paletteAllocations;
        const size_t bytes=rows*8*4*sizeof(float);
        Buffer staging=MakeBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);Image image;
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
        // Only before command recording: queued draws must retain their palette contents.
        if(begun)throw std::runtime_error("palette reuse during frame recording");
        Buffer staging=MakeBuffer(rows*8*4*sizeof(float),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
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
        Buffer staging=MakeBuffer(pixels.size(),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);Image image;
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
        VkPhysicalDeviceMultiviewFeatures multiview{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES};
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};features.pNext=&multiview;
        vkGetPhysicalDeviceFeatures2(physical,&features);
        if(!multiview.multiview)throw std::runtime_error("Vulkan native multiview unavailable");
        multiview.multiviewGeometryShader=multiview.multiviewTessellationShader=VK_FALSE;
        const float priority=1;VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qci.queueFamilyIndex=family;qci.queueCount=1;qci.pQueuePriorities=&priority;
        VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dci.pNext=&multiview;dci.queueCreateInfoCount=1;dci.pQueueCreateInfos=&qci;
        XrVulkanDeviceCreateInfoKHR xdci{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};xdci.systemId=system;xdci.pfnGetInstanceProcAddr=vkGetInstanceProcAddr;
        xdci.vulkanPhysicalDevice=physical;xdci.vulkanCreateInfo=&dci;
        CheckXr(Proc<PFN_xrCreateVulkanDeviceKHR>(xr,"xrCreateVulkanDeviceKHR")(xr,&xdci,&device,&result),"xrCreateVulkanDeviceKHR");Check(result,"Vulkan device");
        vkGetDeviceQueue(device,family,0,&queue);
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
        sci.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.magFilter=sci.minFilter=VK_FILTER_NEAREST;sci.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
        Check(vkCreateSampler(device,&sci,nullptr,&nearest),"palette sampler");
        const std::array<uint8_t,24> pixels{255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255};
        white=UploadTexture(1,1,pixels.data());cube=UploadTexture(1,1,pixels.data(),true);
        arena=MakeBuffer(32u*1024u*1024u,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,true);
        stereo=MakeBuffer(432,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true);
        CreatePass();CreateDescriptors();
        if(timestampSupported){VkQueryPoolCreateInfo tci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};tci.queryType=VK_QUERY_TYPE_TIMESTAMP;// Reserve a stereo-width slot for each timestamp. Some drivers
            // retain the multiview query stride through the pass-end boundary.
            tci.queryCount=4;
            Check(vkCreateQueryPool(device,&tci,nullptr,&timestamps),"timestamp pool");}
        uint32_t extensionCount=0;vkEnumerateDeviceExtensionProperties(physical,nullptr,&extensionCount,nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);vkEnumerateDeviceExtensionProperties(physical,nullptr,&extensionCount,extensions.data());
        const bool density=std::any_of(extensions.begin(),extensions.end(),[](const auto& e){return !std::strcmp(e.extensionName,"VK_EXT_fragment_density_map");});
        VKLOG("FQVK WORLD PROBE device=%s api=%u.%u multiview=1 fragmentDensityExtension=%d foveationEnabled=0 size=%ux%u stereo=2 presentation=unbound parity=INCOMPLETE",
              properties.deviceName,VK_VERSION_MAJOR(properties.apiVersion),VK_VERSION_MINOR(properties.apiVersion),density,width,height);
    }
    void CreatePass() {
        VkFormatProperties format{};vkGetPhysicalDeviceFormatProperties(physical,VK_FORMAT_R16G16B16A16_SFLOAT,&format);
        const VkFormatFeatureFlags fp16Required=VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        if((format.optimalTilingFeatures&fp16Required)!=fp16Required)throw std::runtime_error("sampled FP16 color unavailable");
        vkGetPhysicalDeviceFormatProperties(physical,VK_FORMAT_D32_SFLOAT,&format);
        const VkFormatFeatureFlags depthRequired=VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if((format.optimalTilingFeatures&depthRequired)!=depthRequired)throw std::runtime_error("sampled D32 depth unavailable");
        color=MakeImage(width,height,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
        depth=MakeImage(width,height,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_DEPTH_BIT);
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
        for(int kind=0;kind<12;++kind) {
            const size_t count=shaders::programs[kind].samplerCount;
            std::vector<VkDescriptorSetLayoutBinding> bindings(count+2);
            for(uint32_t i=0;i<bindings.size();++i){auto& b=bindings[i];b.binding=i;b.descriptorCount=1;b.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;
                b.descriptorType=i==0?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:i==1?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;}
            VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};ci.bindingCount=uint32_t(bindings.size());ci.pBindings=bindings.data();
            Check(vkCreateDescriptorSetLayout(device,&ci,nullptr,&sets[kind]),"descriptor layout");
            VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};li.setLayoutCount=1;li.pSetLayouts=&sets[kind];
            Check(vkCreatePipelineLayout(device,&li,nullptr,&layouts[kind]),"pipeline layout");
        }
        for(int kind=0;kind<12;++kind)for(int stage=0;stage<2;++stage) {
            const auto& shader=shaders::programs[kind];VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            ci.codeSize=stage?shader.fragmentBytes:shader.vertexBytes;ci.pCode=stage?shader.fragment:shader.vertex;
            Check(vkCreateShaderModule(device,&ci,nullptr,&modules[kind][stage]),"SPIR-V module");
        }
        std::array<VkDescriptorPoolSize,3> sizes{{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,DescriptorCapacity},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,DescriptorCapacity},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,DescriptorCapacity*10}}};
        VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pi.maxSets=DescriptorCapacity;pi.poolSizeCount=uint32_t(sizes.size());pi.pPoolSizes=sizes.data();
        Check(vkCreateDescriptorPool(device,&pi,nullptr,&descriptors),"frame descriptor pool");
    }
    VkPipeline Pipeline(const PipelineKey& key) {
        const auto found=pipelines.find(key);if(found!=pipelines.end())return found->second;
        const int kind=key.ui?int(key.ui):key.water?3:key.sky?2:key.terrain?1:0;
        std::array<VkPipelineShaderStageCreateInfo,2> stages{};
        for(int i=0;i<2;++i){stages[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stages[i].stage=i?VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_VERTEX_BIT;stages[i].module=modules[kind][i];stages[i].pName="main";}
        VkVertexInputBindingDescription binding{0,key.stride*4,VK_VERTEX_INPUT_RATE_VERTEX};
        std::vector<VkVertexInputAttributeDescription> attributes;
        if(key.ui==11)attributes={{0,0,VK_FORMAT_R32G32B32_SFLOAT,0},{1,0,VK_FORMAT_R32G32B32_SFLOAT,12},{2,0,VK_FORMAT_R32G32_SFLOAT,24},{3,0,VK_FORMAT_R32G32B32A32_SFLOAT,32}};
        else if(key.ui){attributes={{0,0,VK_FORMAT_R32G32B32_SFLOAT,0},{1,0,VK_FORMAT_R32G32_SFLOAT,12}};if(key.ui==10)attributes.push_back({2,0,VK_FORMAT_R32_SFLOAT,20});}
        else if(key.water)attributes={{0,0,VK_FORMAT_R32G32B32_SFLOAT,0},{1,0,VK_FORMAT_R32G32_SFLOAT,12}};
        else if(key.sky)attributes={{0,0,VK_FORMAT_R32G32B32_SFLOAT,0}};
        else if(key.terrain)attributes={{0,0,VK_FORMAT_R32G32B32_SFLOAT,0},{1,0,VK_FORMAT_R32G32B32_SFLOAT,12},{2,0,VK_FORMAT_R32G32_SFLOAT,24},{3,0,VK_FORMAT_R32_SFLOAT,32},{4,0,VK_FORMAT_R32G32B32_SFLOAT,36}};
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
        rs.cullMode=!key.cull?VK_CULL_MODE_NONE:key.cullFace==GL_FRONT?VK_CULL_MODE_FRONT_BIT:
            key.cullFace==GL_FRONT_AND_BACK?VK_CULL_MODE_FRONT_AND_BACK:VK_CULL_MODE_BACK_BIT;
        rs.depthBiasEnable=key.offset;
        // Vertex shader flips Y, so Vulkan front-face winding is inverted.
        rs.frontFace=key.clockwise?VK_FRONT_FACE_COUNTER_CLOCKWISE:VK_FRONT_FACE_CLOCKWISE;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};ds.depthTestEnable=key.depth;ds.depthWriteEnable=key.write;ds.depthCompareOp=Compare(key.depthFunc);
        VkPipelineColorBlendAttachmentState ba{};ba.colorWriteMask=0xf;ba.blendEnable=key.blend;ba.srcColorBlendFactor=Blend(key.src);ba.dstColorBlendFactor=Blend(key.dst);ba.colorBlendOp=BlendOperation(key.equation);ba.srcAlphaBlendFactor=Blend(key.srcA);ba.dstAlphaBlendFactor=Blend(key.dstA);ba.alphaBlendOp=BlendOperation(key.equationA);
        VkPipelineColorBlendStateCreateInfo bs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};bs.attachmentCount=1;bs.pAttachments=&ba;
        const VkDynamicState dynamicStates[]{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_DEPTH_BIAS};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount=key.offset?3:2;dynamic.pDynamicStates=dynamicStates;
        VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};ci.stageCount=2;ci.pStages=stages.data();ci.pVertexInputState=&vi;ci.pInputAssemblyState=&ia;ci.pViewportState=&vp;ci.pRasterizationState=&rs;ci.pMultisampleState=&ms;ci.pDepthStencilState=&ds;ci.pColorBlendState=&bs;ci.layout=layouts[kind];ci.renderPass=key.offscreen?uiPass:pass;ci.pDynamicState=&dynamic;
        VkPipeline pipeline{};Check(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&ci,nullptr,&pipeline),"world pipeline");pipelines.emplace(key,pipeline);return pipeline;
    }
    void Register(uint32_t id,int kind) {
        if(!id)return;
        DrainDraws();
        if(!fqrender::locationLookup)throw std::runtime_error("missing CPU program binding resolver");
        const auto& shader=shaders::programs[kind];
        ProgramData p;p.terrain=kind==1;p.sky=kind==2;p.water=kind==3;p.ui=kind>=9?kind:0;p.bytes.resize(shader.uniformSize);
        for(size_t i=0;i<shader.fieldCount;++i){int location=fqrender::locationLookup(id,shader.fields[i].name);if(location>=0)p.fields.push_back({location,shader.fields[i]});}
        for(size_t i=0;i<shader.samplerCount;++i)p.samplers.push_back(fqrender::locationLookup(id,shader.samplers[i].name));
        p.units.resize(shader.samplerCount);programs[id]=std::move(p);
    }
    void Uniform(uint32_t id,int location,int kind,int bytes,const void* data) {
        auto it=programs.find(id);if(it==programs.end())return;auto& p=it->second;
        if(kind==2)for(size_t i=0;i<p.samplers.size();++i)if(p.samplers[i]==location)std::memcpy(&p.units[i],data,4);
        for(const auto& entry:p.fields)if(location>=entry.location&&location<entry.location+int(entry.field.count)){
            const auto& f=entry.field;const uint32_t elementSize=f.components*4;
            const uint32_t start=uint32_t(location-entry.location);
            const uint32_t n=std::min(f.count-start,uint32_t(bytes)/elementSize);
            for(uint32_t i=0;i<n;++i)std::memcpy(p.bytes.data()+f.offset+(start+i)*(f.components==16?64:16),static_cast<const uint8_t*>(data)+i*elementSize,elementSize);
            break;
        }
    }
    bool UsesSkin(const ProgramData& p,const std::vector<uint8_t>& bytes) {
        for(const auto& e:p.fields)if(!std::strcmp(e.field.name,"uActorSkinMode")) {
            int mode=0;std::memcpy(&mode,bytes.data()+e.field.offset,4);if(mode)return true;
        }
        return false;
    }
    struct PendingDraw {fqrender::DrawCommand command;std::vector<uint8_t> uniforms;std::vector<int> units;};
    std::vector<PendingDraw> pending;
    void Queue(const fqrender::DrawCommand& command) {
        if(!begun||!passOpen||failed)return;
        const auto p=programs.find(command.program);
        if(p==programs.end()){++skipped;return;}
        pending.push_back({command,p->second.bytes,p->second.units});
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
        const auto pIt=programs.find(program);if(pIt==programs.end()){++skipped;return;}
        const auto& p=pIt->second;
        const auto mIt=meshes.find(vao);if(mIt==meshes.end()||(mode!=GL_TRIANGLES&&mode!=GL_TRIANGLE_STRIP)||count<=0||instances<=0||(instances>1&&draw.transforms.size()<size_t(instances)*16)||(UsesSkin(p,item.uniforms)&&!mIt->second.skin.buffer)){++skipped;return;}
        if(type && type!=GL_UNSIGNED_INT){++skipped;return;}
        const auto& m=mIt->second;const int kind=p.ui?p.ui:p.water?3:p.sky?2:p.terrain?1:0;
        const uint32_t indexFirst=type?(p.terrain?0:uint32_t(offset/4)):uint32_t(first);
        if(indexFirst+uint32_t(count)>m.count){++skipped;return;}
        const auto& s=draw.state;
        PipelineKey key;key.offscreen=offscreen;key.strip=mode==GL_TRIANGLE_STRIP;key.ui=uint32_t(p.ui);key.water=p.water;key.sky=p.sky;key.terrain=p.terrain;key.stride=m.stride;key.instanced=instances>1;key.skinned=UsesSkin(p,item.uniforms);
        key.depth=s.depth;key.write=s.write;key.blend=s.blend;key.cull=s.cull;key.clockwise=s.clockwise;
        key.depthFunc=s.depthFunc;key.src=s.src;key.dst=s.dst;key.srcA=s.srcAlpha;key.dstA=s.dstAlpha;
        key.cullFace=s.cullFace;key.equation=s.equation;key.equationA=s.equationAlpha;key.offset=s.offset;
        if(offscreen){key.depth=key.write=false;}
        const VkDeviceSize alignment=std::max<VkDeviceSize>(16,properties.limits.minUniformBufferOffsetAlignment);
        cursor=(cursor+alignment-1)&~(alignment-1);
        const size_t instanceBytes=key.instanced?size_t(instances)*64:0;
        if(cursor+item.uniforms.size()+instanceBytes>arena.size||draws>=8176){++skipped;return;}
        std::memcpy(static_cast<uint8_t*>(arena.mapped)+cursor,item.uniforms.data(),item.uniforms.size());
        if(p.water)for(const auto& e:p.fields) {
            float* out=reinterpret_cast<float*>(static_cast<uint8_t*>(arena.mapped)+cursor+e.field.offset);
            if(!std::strcmp(e.field.name,"uSceneSnapshotReady"))*out=snapshotReady?1.0f:0.0f;
            if(!std::strcmp(e.field.name,"uReflectionReady"))*out=0.0f; // planar native pass pending
            if(!std::strcmp(e.field.name,"uViewport")){out[0]=float(width);out[1]=float(height);}
        }
        std::vector<VkDescriptorImageInfo> imageInfo(item.units.size());
        const auto* samplerMeta=shaders::programs[kind].samplers;
        for(size_t i=0;i<item.units.size();++i){
            VkImageView view=samplerMeta[i].cube?cube.view:white.view;
            const int unit=item.units[i];
            VkSampler chosenSampler=samplerMeta[i].cube?clamped:sampler;
            if(unit>=0&&unit<16){
                const auto& binding=samplerMeta[i].cube?s.cubes[unit]:s.textures[unit];
                auto tex=textures.find(binding);
                if(tex!=textures.end()){view=tex->second.view;if(tex->second.palette)chosenSampler=nearest;else if(tex->second.repeatT)chosenSampler=verticalRepeat;else if(tex->second.clamp)chosenSampler=clamped;}
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
        const auto set=Descriptor(kind,uint32_t(item.uniforms.size()),imageInfo);
        const uint32_t dynamic=uint32_t(cursor);cursor+=item.uniforms.size();
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
    }
    void WaterSnapshot() {
        if(!begun||!passOpen||snapshotReady||failed)return;
        DrainDraws();
        // Refraction requires arbitrary displaced screen taps, so retain an
        // immutable opaque image rather than sampling an active attachment.
        // Copy both views once, before any water CELL modifies the color target.
        vkCmdEndRenderPass(command);
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
        VkRenderPassBeginInfo ri{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};ri.renderPass=resumePass;ri.framebuffer=framebuffer;ri.renderArea={{0,0},{width,height}};
        vkCmdBeginRenderPass(command,&ri,VK_SUBPASS_CONTENTS_INLINE);
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
        MakePostTarget(presented,width,height);
        const VkImageView overlayViews[]{presented.image.view,depth.view};
        VkFramebufferCreateInfo overlay{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};overlay.renderPass=resumePass;overlay.attachmentCount=2;overlay.pAttachments=overlayViews;
        overlay.width=width;overlay.height=height;overlay.layers=1;
        Check(vkCreateFramebuffer(device,&overlay,nullptr,&postOverlayFramebuffer),"post overlay framebuffer");
        for(int kind=4;kind<9;++kind)
            postPipelines[kind-4]=MakePostPipeline(kind,kind==4?adaptPass:postPass);
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
        Check(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&ci,nullptr,&result),"native fullscreen pipeline");
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
        vkCmdBeginRenderPass(command,&ri,VK_SUBPASS_CONTENTS_INLINE);
        const VkViewport viewport{0,0,float(target.width),float(target.height),0,1};const VkRect2D scissor{{0,0},{target.width,target.height}};
        vkCmdSetViewport(command,0,1,&viewport);vkCmdSetScissor(command,0,1,&scissor);
        vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,compositor?xrPipeline:postPipelines[kind-4]);
        const uint32_t offset=uint32_t(cursor);cursor+=shader.uniformSize;
        vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,layouts[kind],0,1,&set,1,&offset);
        vkCmdDraw(command,3,1,0,0);vkCmdEndRenderPass(command);
        if(!compositor)PostBarrier(target.image,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        // OpenXR requires COLOR_ATTACHMENT_OPTIMAL on release, not PRESENT_SRC_KHR.
        target.initialized=!compositor;
    }
    void PostProcess(const PostSettings& settings) {
        if(!begun||!passOpen||failed||postDone)return;
        DrainDraws();CreatePost();vkCmdEndRenderPass(command);passOpen=false;
        PostBarrier(color,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
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
        PostDraw(8,presented,{sample(color),sample(adapted[next].image,true),sample(bloom[1].image)},
            {{"uFlags",{float(settings.flags)}},{"uInteriorMode",{float(settings.interior)}},{"uSaturation",{settings.saturation}},
             {"uContrastAvg",{settings.contrastAvg}},{"uContrast",{settings.contrast}},{"uBrightness",{settings.brightness}},
             {"uTintColor",{settings.tint[0],settings.tint[1],settings.tint[2]}},{"uTintValue",{settings.tintValue}},
             {"uTargetLumQ1520",{settings.targetLum}},{"uPcBloomReadyQ1670",{1}}});
        postDone=true;
        // Notifications, XP, pause and performance HUD are deliberately drawn
        // after the film/gamma pass in the original game path. Retain that order.
        PostBarrier(presented.image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        VkRenderPassBeginInfo overlay{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};overlay.renderPass=resumePass;overlay.framebuffer=postOverlayFramebuffer;overlay.renderArea={{0,0},{width,height}};
        vkCmdBeginRenderPass(command,&overlay,VK_SUBPASS_CONTENTS_INLINE);passOpen=true;
    }
    void DestroyUi(UiTarget& ui) {
        if(ui.sampled)vkDestroyImageView(device,ui.sampled,nullptr);
        DestroyPost(ui.target);ui={};
    }
    bool BeginUiTarget(uint32_t id,uint32_t w,uint32_t h) {
        if(begun||failed||!id||!w||!h)return false;
        Check(vkWaitForFences(device,1,&fence,VK_TRUE,UINT64_MAX),"wait UI target");
        if(!uiPass)uiPass=MakePostPass(VK_FORMAT_R8G8B8A8_UNORM);
        auto& ui=uiTargets[id];
        if(ui.target.image.image&&(ui.target.width!=w||ui.target.height!=h))DestroyUi(ui);
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
        vkCmdBeginRenderPass(command,&ri,VK_SUBPASS_CONTENTS_INLINE);passOpen=true;
        VkClearAttachment clear{};clear.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;clear.colorAttachment=0;clear.clearValue.color={{0,0,0,1}};
        const VkClearRect rect{{{0,0},{w,h}},0,1};vkCmdClearAttachments(command,1,&clear,1,&rect);
        return true;
    }
    void EndUiTarget() {
        if(!begun||!offscreen||!activeUi)return;
        DrainDraws();vkCmdEndRenderPass(command);passOpen=false;
        PostBarrier(activeUi->target.image,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        activeUi->target.initialized=true;End();activeUi=nullptr;offscreen=false;
    }
    void StartCommands(const float* left,const float* right,const float* le,const float* re,bool loading=false) {
        loadingFrame=loading;
        FlushUploads();
        Check(vkWaitForFences(device,1,&fence,VK_TRUE,UINT64_MAX),"wait frame");
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
        pending.clear();postDone=false;snapshotReady=false;cursor=0;draws=vertices=skipped=0;
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        Check(vkBeginCommandBuffer(command,&bi),"begin frame");
        if(timestamps){vkCmdResetQueryPool(command,timestamps,0,4);vkCmdWriteTimestamp(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,timestamps,0);}
        begun=true;
    }
    void Begin(const float* left,const float* right,const float* le,const float* re,bool loading=false) {
        offscreen=false;targetWidth=width;targetHeight=height;StartCommands(left,right,le,re,loading);
        std::array<VkClearValue,2> clears{};clears[0].color={{0,0,0,1}};clears[1].depthStencil={1,0};
        VkRenderPassBeginInfo ri{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};ri.renderPass=pass;ri.framebuffer=framebuffer;ri.renderArea={{0,0},{width,height}};ri.clearValueCount=2;ri.pClearValues=clears.data();
        vkCmdBeginRenderPass(command,&ri,VK_SUBPASS_CONTENTS_INLINE);begun=passOpen=true;
    }
    void End(PostTarget* output=nullptr,VkImage depthOutput=VK_NULL_HANDLE) {
        if(!begun)return;
        DrainDraws();begun=false;
        if(passOpen)vkCmdEndRenderPass(command);
        passOpen=false;
        if(postDone)PostBarrier(presented.image,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        if(output) {
            if(!loadingFrame&&!postDone)throw std::runtime_error("Vulkan gameplay missing native film pass");
            if(!postDone)PostBarrier(color,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            PostDraw(5,*output,{{clamped,postDone?presented.image.view:color.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}},{},true);
        }
        if(depthOutput)CopyCompositorDepth(depthOutput);
        if(timestamps)vkCmdWriteTimestamp(command,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,timestamps,2);
        Check(vkEndCommandBuffer(command),"end frame");Check(vkResetFences(device,1,&fence),"reset frame fence");
        FlushUploads(); // Same graphics queue; upload barriers precede every consuming frame.
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};si.commandBufferCount=1;si.pCommandBuffers=&command;
        Check(vkQueueSubmit(queue,1,&si,fence),"submit world probe");Check(vkWaitForFences(device,1,&fence,VK_TRUE,UINT64_MAX),"finish world probe");
        double gpuMs=-1;
        if(timestamps){uint64_t t[2]{};
            const auto begin=vkGetQueryPoolResults(device,timestamps,0,1,8,&t[0],8,VK_QUERY_RESULT_64_BIT);
            const auto end=vkGetQueryPoolResults(device,timestamps,2,1,8,&t[1],8,VK_QUERY_RESULT_64_BIT);
            if(begin==VK_SUCCESS&&end==VK_SUCCESS) {
                const uint64_t mask=timestampBits==64?~uint64_t{0}:(uint64_t{1}<<timestampBits)-1;
                gpuMs=double((t[1]-t[0])&mask)*properties.limits.timestampPeriod/1e6;
            }
        }
        Collect();
        if(!offscreen&&(frames%120)==0)VKLOG("FQVK RESOURCES uploadOperations=%llu uploadBatches=%llu uploadQueueIdles=%llu stagingBytes=%llu stagingPeakBytes=%llu stagingPressureWaits=%llu descriptorAllocations=%llu descriptorHits=%llu descriptorResets=%llu paletteAllocations=%llu paletteUpdates=%llu",
            (unsigned long long)uploadOperations,(unsigned long long)uploadSubmissions,(unsigned long long)uploadQueueIdles,
            (unsigned long long)uploadBytes,(unsigned long long)uploadPeakBytes,(unsigned long long)uploadPressureWaits,(unsigned long long)descriptorAllocations,(unsigned long long)descriptorHits,(unsigned long long)descriptorResets,(unsigned long long)paletteAllocations,(unsigned long long)paletteUpdates);
        if(!offscreen&&!gameplay)Dump();
        if(!offscreen&&(frames++%120)==0)VKLOG("FQVK FRAME frame=%llu stereoGpuMs=%.3f gpuScope=frameSpan sharedDraws=%llu vertexCorners=%llu skippedDraws=%llu presentation=%s sceneSubmission=cpu-commands benchmarkValid=0",
            (unsigned long long)frames,gpuMs,(unsigned long long)draws,(unsigned long long)vertices,(unsigned long long)skipped,gameplay?"Vulkan":"GLES-probe");
    }
    std::vector<uint16_t> Readback() {
        // Debug-only, explicit readback. No readback/copy occurs on ordinary frames.
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
        if(device)vkDeviceWaitIdle(device);
        for(auto& target:xrTargets) {
            if(target.framebuffer)vkDestroyFramebuffer(device,target.framebuffer,nullptr);
            if(target.image.view)vkDestroyImageView(device,target.image.view,nullptr);
        }
        xrTargets.clear();
        if(xrPipeline)vkDestroyPipeline(device,xrPipeline,nullptr);
        if(xrPass)vkDestroyRenderPass(device,xrPass,nullptr);
        xrPipeline={};xrPass={};
        if(xrSwapchain)Proc<PFN_xrDestroySwapchain>(xrInstance,"xrDestroySwapchain")(xrSwapchain);
        if(xrDepthSwapchain)Proc<PFN_xrDestroySwapchain>(xrInstance,"xrDestroySwapchain")(xrDepthSwapchain);
        xrDepthSwapchain={};xrDepthImages.clear();xrSwapchain={};xrSession={};
    }
    void CreateSwapchain(XrSession session,bool submitDepth=false) {
        DestroySwapchain();gameplay=true;xrSession=session;
        auto formatsFn=Proc<PFN_xrEnumerateSwapchainFormats>(xrInstance,"xrEnumerateSwapchainFormats");
        uint32_t count=0;CheckXr(formatsFn(session,0,&count,nullptr),"Vulkan swapchain formats count");
        std::vector<int64_t> formats(count);CheckXr(formatsFn(session,count,&count,formats.data()),"Vulkan swapchain formats");
        // Match the existing final display-linear output, without an extra sRGB encode.
        VkFormat format=VK_FORMAT_UNDEFINED;
        for(auto preferred:{VK_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_B8G8R8A8_UNORM})
            if(std::find(formats.begin(),formats.end(),int64_t(preferred))!=formats.end()){format=preferred;break;}
        if(format==VK_FORMAT_UNDEFINED)throw std::runtime_error("OpenXR has no supported linear UNORM Vulkan swapchain format");
        XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};info.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        info.format=format;info.sampleCount=1;info.width=width;info.height=height;info.faceCount=1;info.arraySize=2;info.mipCount=1;
        CheckXr(Proc<PFN_xrCreateSwapchain>(xrInstance,"xrCreateSwapchain")(session,&info,&xrSwapchain),"create Vulkan stereo swapchain");
        auto imagesFn=Proc<PFN_xrEnumerateSwapchainImages>(xrInstance,"xrEnumerateSwapchainImages");
        count=0;CheckXr(imagesFn(xrSwapchain,0,&count,nullptr),"Vulkan swapchain images count");
        if(!count)throw std::runtime_error("OpenXR returned no Vulkan swapchain images");
        std::vector<XrSwapchainImageVulkan2KHR> images(count,{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
        CheckXr(imagesFn(xrSwapchain,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())),"Vulkan swapchain images");
        xrPass=MakePostPass(format);xrPipeline=MakePostPipeline(5,xrPass);
        xrTargets.resize(count);
        for(uint32_t i=0;i<count;++i) {
            auto& target=xrTargets[i];target.width=width;target.height=height;target.image.image=images[i].image;
            VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=images[i].image;
            vi.viewType=VK_IMAGE_VIEW_TYPE_2D_ARRAY;vi.format=format;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,2};
            Check(vkCreateImageView(device,&vi,nullptr,&target.image.view),"OpenXR stereo image view");
            VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fi.renderPass=xrPass;fi.attachmentCount=1;fi.pAttachments=&target.image.view;
            fi.width=width;fi.height=height;fi.layers=1;
            Check(vkCreateFramebuffer(device,&fi,nullptr,&target.framebuffer),"OpenXR multiview framebuffer");
        }
        if(submitDepth&&std::find(formats.begin(),formats.end(),int64_t(VK_FORMAT_D32_SFLOAT))!=formats.end()) {
            info.format=VK_FORMAT_D32_SFLOAT;info.usageFlags=XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
            CheckXr(Proc<PFN_xrCreateSwapchain>(xrInstance,"xrCreateSwapchain")(session,&info,&xrDepthSwapchain),"create Vulkan stereo depth swapchain");
            count=0;CheckXr(imagesFn(xrDepthSwapchain,0,&count,nullptr),"Vulkan depth images count");
            if(!count)throw std::runtime_error("OpenXR returned no Vulkan depth images");
            xrDepthImages.resize(count,{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
            CheckXr(imagesFn(xrDepthSwapchain,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(xrDepthImages.data())),"Vulkan depth images");
        }
        VKLOG("FQ RENDERER session=Vulkan presentation=Vulkan experimental=1 arraySize=2 format=%d samples=1 depthSubmission=%d sceneSubmission=cpu-commands glesContextRequired=0",int(format),xrDepthSwapchain?1:0);
    }
    bool Present(const XrView* views,XrCompositionLayerProjectionView* output,float nearZ,float farZ) {
        if(failed||!begun||!xrSwapchain)return false;
        if(!(nearZ>0&&farZ>nearZ&&std::isfinite(farZ)))throw std::runtime_error("invalid compositor depth range");
        XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};uint32_t index=0;
        CheckXr(Proc<PFN_xrAcquireSwapchainImage>(xrInstance,"xrAcquireSwapchainImage")(xrSwapchain,&acquire,&index),"acquire Vulkan image");
        XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout=XR_INFINITE_DURATION;
        const auto ready=Proc<PFN_xrWaitSwapchainImage>(xrInstance,"xrWaitSwapchainImage")(xrSwapchain,&wait);
        // XR_TIMEOUT_EXPIRED is a positive result but does not grant image access.
        if(ready!=XR_SUCCESS)throw std::runtime_error("wait Vulkan image: "+std::to_string(ready));
        if(index>=xrTargets.size())throw std::runtime_error("OpenXR Vulkan image index out of range");
        uint32_t depthIndex=0;const bool submitDepth=xrDepthSwapchain&&!loadingFrame;
        if(submitDepth) {
            CheckXr(Proc<PFN_xrAcquireSwapchainImage>(xrInstance,"xrAcquireSwapchainImage")(xrDepthSwapchain,&acquire,&depthIndex),"acquire Vulkan depth");
            const auto depthReady=Proc<PFN_xrWaitSwapchainImage>(xrInstance,"xrWaitSwapchainImage")(xrDepthSwapchain,&wait);
            if(depthReady!=XR_SUCCESS)throw std::runtime_error("wait Vulkan depth: "+std::to_string(depthReady));
            if(depthIndex>=xrDepthImages.size())throw std::runtime_error("OpenXR Vulkan depth index out of range");
        }
        End(&xrTargets[index],submitDepth?xrDepthImages[depthIndex].image:VK_NULL_HANDLE);
        // Fence completes before either release; no GLES texture/readback involved.
        XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        if(submitDepth)CheckXr(Proc<PFN_xrReleaseSwapchainImage>(xrInstance,"xrReleaseSwapchainImage")(xrDepthSwapchain,&release),"release Vulkan depth");
        CheckXr(Proc<PFN_xrReleaseSwapchainImage>(xrInstance,"xrReleaseSwapchainImage")(xrSwapchain,&release),"release Vulkan image");
        for(uint32_t eye=0;eye<2;++eye) {
            output[eye]={XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};output[eye].pose=views[eye].pose;output[eye].fov=views[eye].fov;
            output[eye].subImage={xrSwapchain,{{0,0},{int32_t(width),int32_t(height)}},eye};
            if(submitDepth) {
                auto& info=xrDepthInfo[eye];info.subImage={xrDepthSwapchain,{{0,0},{int32_t(width),int32_t(height)}},eye};
                info.minDepth=0;info.maxDepth=1;info.nearZ=nearZ;info.farZ=farZ;
                output[eye].next=&info;
            }
        }
        return true;
    }
    ~Renderer() {
        DestroySwapchain();
        if(device){CancelUploads();begun=false;
            for(auto& e:pipelines)vkDestroyPipeline(device,e.second,nullptr);
            for(auto& e:meshes){Destroy(e.second.vertices);Destroy(e.second.indices);Destroy(e.second.skin);}
            for(auto& e:textures)Destroy(e.second);
            Collect();
            for(auto& ui:uiTargets)DestroyUi(ui.second);
            if(uiPass)vkDestroyRenderPass(device,uiPass,nullptr);
            for(auto& target:adapted)DestroyPost(target);
            for(auto& target:bloom)DestroyPost(target);
            if(postOverlayFramebuffer)vkDestroyFramebuffer(device,postOverlayFramebuffer,nullptr);
            DestroyPost(presented);
            for(auto pipeline:postPipelines)if(pipeline)vkDestroyPipeline(device,pipeline,nullptr);
            if(postPass)vkDestroyRenderPass(device,postPass,nullptr);
            if(adaptPass)vkDestroyRenderPass(device,adaptPass,nullptr);
            Destroy(arena);Destroy(stereo);Destroy(white);Destroy(cube);
            if(framebuffer)vkDestroyFramebuffer(device,framebuffer,nullptr);
            if(resumePass)vkDestroyRenderPass(device,resumePass,nullptr);
            if(pass)vkDestroyRenderPass(device,pass,nullptr);
            Destroy(color);Destroy(depth);Destroy(sceneColor);Destroy(sceneDepth);
            if(descriptors)vkDestroyDescriptorPool(device,descriptors,nullptr);
            for(int k=0;k<12;++k){if(layouts[k])vkDestroyPipelineLayout(device,layouts[k],nullptr);if(sets[k])vkDestroyDescriptorSetLayout(device,sets[k],nullptr);for(auto m:modules[k])if(m)vkDestroyShaderModule(device,m,nullptr);}
            if(timestamps)vkDestroyQueryPool(device,timestamps,nullptr);
            if(verticalRepeat)vkDestroySampler(device,verticalRepeat,nullptr);
            if(clamped)vkDestroySampler(device,clamped,nullptr);
            if(nearest)vkDestroySampler(device,nearest,nullptr);
            if(sampler)vkDestroySampler(device,sampler,nullptr);
            if(fence)vkDestroyFence(device,fence,nullptr);
            if(uploads)vkDestroyCommandPool(device,uploads,nullptr);
            if(pool)vkDestroyCommandPool(device,pool,nullptr);
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
        renderer=std::make_unique<Renderer>();renderer->Initialize(instance,system,w,h);
        if(outputDirectory)renderer->outputDirectory=outputDirectory;
        hooks.flushUploads=[]()->uint64_t{try{return renderer->FlushUploads();}catch(const std::exception& e){Failure(e);return UINT64_MAX;}};
        hooks.uploadsComplete=[](uint64_t ticket){try{return renderer->UploadsComplete(ticket);}catch(const std::exception& e){Failure(e);return false;}};
        hooks.beginUiTarget=[](uint32_t id,uint32_t w,uint32_t h){try{return renderer->BeginUiTarget(id,w,h);}catch(const std::exception& e){Failure(e);return false;}};
        hooks.endUiTarget=[](){try{renderer->EndUiTarget();}catch(const std::exception& e){Failure(e);}};
        hooks.repeatT=[](uint32_t id){try{renderer->DrainDraws();auto found=renderer->textures.find(id);if(found!=renderer->textures.end())found->second.repeatT=true;}catch(const std::exception& e){Failure(e);}};
        hooks.clearDepth=[](){try{auto& r=*renderer;r.DrainDraws();if(!r.begun||!r.passOpen||r.failed)return;
            VkClearAttachment attachment{};attachment.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;attachment.clearValue.depthStencil={1,0};
            const VkClearRect rect{{{0,0},{r.width,r.height}},0,1};vkCmdClearAttachments(r.command,1,&attachment,1,&rect);}catch(const std::exception& e){Failure(e);}};
        hooks.resetExposure=[](){renderer->exposureFirst=true;};
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
            if(texture){auto ui=r.uiTargets.find(id);if(ui!=r.uiTargets.end()){if(r.begun)r.retiredUi.push_back(ui->second);else r.DestroyUi(ui->second);r.uiTargets.erase(ui);}
                auto i=r.textures.find(id);if(i!=r.textures.end()){r.Retire(i->second);r.textures.erase(i);}}
            else{auto i=r.meshes.find(id);if(i!=r.meshes.end()){r.Retire(i->second.vertices);r.Retire(i->second.indices);r.Retire(i->second.skin);r.meshes.erase(i);}}}catch(const std::exception& e){Failure(e);}};
        return true;
    }catch(const std::exception& e){Failure(e);ShutdownWorldProbe();return false;}
}
void ShutdownWorldProbe(){hooks={};recording=false;gameplayPresentation=false;renderer.reset();}
bool WorldProbeActive(){return renderer&&!renderer->failed;}
bool BeginWorldProbe(const float* l,const float* r,const float* le,const float* re,bool loading){
    if(!WorldProbeActive())return false;
    try{renderer->Begin(l,r,le,re,loading);recording=true;return true;}catch(const std::exception& e){Failure(e);return false;}
}
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

}
