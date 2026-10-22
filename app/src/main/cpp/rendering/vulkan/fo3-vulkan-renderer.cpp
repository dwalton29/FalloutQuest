#include "fo3-vulkan-renderer.h"
#include "fo3-vulkan-bridge.h"
#include "fo3-vulkan-shaders.h"
#include "rendering/gl-state-cache.h"
#include <algorithm>
#include <array>
#include <cstring>
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
struct Image {VkImage image{};VkImageView view{};VkDeviceMemory memory{};uint32_t levels=1;bool palette=false;};
struct MeshData {Buffer vertices,indices,skin;uint32_t count=0,stride=0;};
struct ProgramData {
    bool terrain=false;
    std::vector<uint8_t> bytes;
    struct Location {int location;shaders::Field field;};
    std::vector<Location> fields;
    std::vector<int> samplers;
    std::vector<int> units;
};
struct PipelineKey {
    bool skinned=false,instanced=false,terrain=false,depth=true,write=true,blend=false,cull=false,clockwise=false;
    uint32_t stride=0,depthFunc=GL_LEQUAL,src=GL_ONE,dst=GL_ZERO,srcA=GL_ONE,dstA=GL_ZERO;
    bool operator==(const PipelineKey& k)const {
        return skinned==k.skinned && instanced==k.instanced && terrain==k.terrain && depth==k.depth && write==k.write && blend==k.blend &&
            cull==k.cull && clockwise==k.clockwise && stride==k.stride && depthFunc==k.depthFunc &&
            src==k.src && dst==k.dst && srcA==k.srcA && dstA==k.dstA;
    }
};
struct PipelineHash {size_t operator()(const PipelineKey& k)const {
    size_t h=k.stride+uint32_t(k.instanced)*31+uint32_t(k.skinned);for(auto v:{uint32_t(k.terrain),uint32_t(k.depth),uint32_t(k.write),uint32_t(k.blend),
        uint32_t(k.cull),uint32_t(k.clockwise),k.depthFunc,k.src,k.dst,k.srcA,k.dstA}) { h=(h*16777619u)^v; }
    return h;
}};
VkBlendFactor Blend(uint32_t x) {
    switch(x) {
        case GL_ZERO:return VK_BLEND_FACTOR_ZERO;case GL_ONE:return VK_BLEND_FACTOR_ONE;
        case GL_SRC_ALPHA:return VK_BLEND_FACTOR_SRC_ALPHA;case GL_ONE_MINUS_SRC_ALPHA:return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case GL_DST_ALPHA:return VK_BLEND_FACTOR_DST_ALPHA;case GL_ONE_MINUS_DST_ALPHA:return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case GL_SRC_COLOR:return VK_BLEND_FACTOR_SRC_COLOR;case GL_ONE_MINUS_SRC_COLOR:return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case GL_DST_COLOR:return VK_BLEND_FACTOR_DST_COLOR;case GL_ONE_MINUS_DST_COLOR:return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        default:throw std::runtime_error("unsupported blend factor");
    }
}
VkCompareOp Compare(uint32_t x) {
    switch(x) {
        case GL_LESS:return VK_COMPARE_OP_LESS;case GL_LEQUAL:return VK_COMPARE_OP_LESS_OR_EQUAL;
        case GL_EQUAL:return VK_COMPARE_OP_EQUAL;case GL_ALWAYS:return VK_COMPARE_OP_ALWAYS;
        default:throw std::runtime_error("unsupported depth function");
    }
}
class Renderer {
public:
    VkInstance instance{};VkPhysicalDevice physical{};VkDevice device{};VkQueue queue{};uint32_t family=0;
    VkPhysicalDeviceProperties properties{};VkPhysicalDeviceMemoryProperties memory{};
    VkCommandPool pool{},uploads{};VkCommandBuffer command{};VkFence fence{};
    VkRenderPass pass{};VkFramebuffer framebuffer{};Image color,depth,white,cube;
    VkSampler sampler{},nearest{};VkDescriptorSetLayout sets[2]{};VkPipelineLayout layouts[2]{};
    VkShaderModule modules[2][2]{};VkDescriptorPool descriptors{};
    VkQueryPool timestamps{};bool timestampSupported=false;
    Buffer arena,stereo;VkDeviceSize cursor=0;uint32_t width=0,height=0;
    std::unordered_map<uint32_t,MeshData> meshes;
    std::unordered_map<uint32_t,Image> textures;
    std::unordered_map<uint32_t,ProgramData> programs;
    std::unordered_map<PipelineKey,VkPipeline,PipelineHash> pipelines;
    std::vector<Buffer> retiredBuffers;std::vector<Image> retiredImages;
    uint64_t draws=0,vertices=0,skipped=0,frames=0;bool begun=false;
    bool failed=false;
    std::string outputDirectory;
    bool dumped=false;
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
        if(i.view)vkDestroyImageView(device,i.view,nullptr);
        if(i.image)vkDestroyImage(device,i.image,nullptr);
        if(i.memory)vkFreeMemory(device,i.memory,nullptr);
        i={};
    }
    void Retire(Buffer& buffer){if(begun&&buffer.buffer){retiredBuffers.push_back(buffer);buffer={};}else Destroy(buffer);}
    void Retire(Image& image){if(begun&&image.image){retiredImages.push_back(image);image={};}else Destroy(image);}
    void Collect(){for(auto& b:retiredBuffers)Destroy(b);for(auto& i:retiredImages)Destroy(i);retiredBuffers.clear();retiredImages.clear();}
    Image MakeImage(uint32_t w,uint32_t h,VkFormat format,VkImageUsageFlags usage,
                    VkImageAspectFlags aspect,uint32_t layers=2,uint32_t levels=1,bool isCube=false) {
        Image image{};image.levels=levels;
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
            Transfer([&](VkCommandBuffer cb){VkBufferCopy copy{0,0,bytes};vkCmdCopyBuffer(cb,staging.buffer,result.buffer,1,&copy);
                VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
                barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT|VK_ACCESS_INDEX_READ_BIT;
                barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
                barrier.buffer=result.buffer;barrier.size=VK_WHOLE_SIZE;
                vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,0,0,nullptr,1,&barrier,0,nullptr);});
        }catch(...){Destroy(staging);Destroy(result);throw;}
        Destroy(staging);return result;
    }
    Image UploadTexture(uint32_t w,uint32_t h,const uint8_t* rgba,bool isCube=false) {
        const uint32_t layers=isCube?6:1;const size_t bytes=size_t(w)*h*4*layers;
        uint32_t levels=1;for(uint32_t n=std::max(w,h);n>1;n>>=1)++levels;
        VkFormatProperties properties{};vkGetPhysicalDeviceFormatProperties(physical,VK_FORMAT_R8G8B8A8_UNORM,&properties);
        const VkFormatFeatureFlags blit=VK_FORMAT_FEATURE_BLIT_SRC_BIT|VK_FORMAT_FEATURE_BLIT_DST_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        if(levels>1&&(properties.optimalTilingFeatures&blit)!=blit)throw std::runtime_error("RGBA8 mip generation unsupported");
        Buffer staging=MakeBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);Image result;
        try {
            std::memcpy(staging.mapped,rgba,bytes);
            result=MakeImage(w,h,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
                             VK_IMAGE_ASPECT_COLOR_BIT,layers,levels,isCube);
            Transfer([&](VkCommandBuffer cb){
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
        }catch(...){Destroy(staging);Destroy(result);throw;}
        Destroy(staging);return result;
    }
    Image UploadPalette(const float* data,size_t rows) {
        const size_t bytes=rows*8*4*sizeof(float);
        Buffer staging=MakeBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);Image image;
        try {
            std::memcpy(staging.mapped,data,bytes);
            image=MakeImage(8,uint32_t(rows),VK_FORMAT_R32G32B32A32_SFLOAT,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT,1);
            image.palette=true;
            Transfer([&](VkCommandBuffer cb){
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
        }catch(...){Destroy(staging);Destroy(image);throw;}
        Destroy(staging);return image;
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
            Transfer([&](VkCommandBuffer cb){
                VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.image=image.image;
                b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
                b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,uint32_t(source.mipLevels),0,6};b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;
                b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
                vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
                vkCmdCopyBufferToImage(cb,staging.buffer,image.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,uint32_t(regions.size()),regions.data());
                b.oldLayout=b.newLayout;b.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
                vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&b);
            });
        }catch(...){Destroy(staging);Destroy(image);throw;}
        Destroy(staging);return image;
    }
    void Initialize(XrInstance xr,XrSystemId system,uint32_t w,uint32_t h) {
        width=w;height=h;
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
        uint32_t count=0;vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,nullptr);
        std::vector<VkQueueFamilyProperties> queues(count);vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,queues.data());
        auto it=std::find_if(queues.begin(),queues.end(),[](const auto& q){return q.queueCount&&(q.queueFlags&VK_QUEUE_GRAPHICS_BIT);});
        if(it==queues.end())throw std::runtime_error("Vulkan graphics queue unavailable");
        family=uint32_t(it-queues.begin());timestampSupported=it->timestampValidBits==64&&properties.limits.timestampComputeAndGraphics;
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
        sci.magFilter=sci.minFilter=VK_FILTER_NEAREST;sci.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
        Check(vkCreateSampler(device,&sci,nullptr,&nearest),"palette sampler");
        const std::array<uint8_t,24> pixels{255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255};
        white=UploadTexture(1,1,pixels.data());cube=UploadTexture(1,1,pixels.data(),true);
        arena=MakeBuffer(32u*1024u*1024u,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,true);
        stereo=MakeBuffer(160,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true);
        CreatePass();CreateDescriptors();
        if(timestampSupported){VkQueryPoolCreateInfo tci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};tci.queryType=VK_QUERY_TYPE_TIMESTAMP;tci.queryCount=2;
            Check(vkCreateQueryPool(device,&tci,nullptr,&timestamps),"timestamp pool");}
        uint32_t extensionCount=0;vkEnumerateDeviceExtensionProperties(physical,nullptr,&extensionCount,nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);vkEnumerateDeviceExtensionProperties(physical,nullptr,&extensionCount,extensions.data());
        const bool density=std::any_of(extensions.begin(),extensions.end(),[](const auto& e){return !std::strcmp(e.extensionName,"VK_EXT_fragment_density_map");});
        VKLOG("FQVK WORLD PROBE device=%s api=%u.%u multiview=1 fragmentDensityExtension=%d foveationEnabled=0 size=%ux%u stereo=2 defaultRenderer=GLES parity=INCOMPLETE",
              properties.deviceName,VK_VERSION_MAJOR(properties.apiVersion),VK_VERSION_MINOR(properties.apiVersion),density,width,height);
    }
    void CreatePass() {
        VkFormatProperties format{};vkGetPhysicalDeviceFormatProperties(physical,VK_FORMAT_R16G16B16A16_SFLOAT,&format);
        if(!(format.optimalTilingFeatures&VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))throw std::runtime_error("FP16 color unavailable");
        vkGetPhysicalDeviceFormatProperties(physical,VK_FORMAT_D32_SFLOAT,&format);
        if(!(format.optimalTilingFeatures&VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))throw std::runtime_error("D32 depth unavailable");
        color=MakeImage(width,height,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
        depth=MakeImage(width,height,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,VK_IMAGE_ASPECT_DEPTH_BIT);
        std::array<VkAttachmentDescription,2> attachments{};
        attachments[0].format=VK_FORMAT_R16G16B16A16_SFLOAT;attachments[1].format=VK_FORMAT_D32_SFLOAT;
        for(auto& a:attachments){a.samples=VK_SAMPLE_COUNT_1_BIT;a.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;a.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;a.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;a.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;}
        attachments[0].storeOp=VK_ATTACHMENT_STORE_OP_STORE;attachments[0].finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachments[1].storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;attachments[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkAttachmentReference c{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},d{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&c;sub.pDepthStencilAttachment=&d;
        const uint32_t mask=3;VkRenderPassMultiviewCreateInfo mv{VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO};mv.subpassCount=1;mv.pViewMasks=&mask;mv.correlationMaskCount=1;mv.pCorrelationMasks=&mask;
        VkSubpassDependency dependency{};dependency.srcSubpass=VK_SUBPASS_EXTERNAL;dependency.dstSubpass=0;
        dependency.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask=dependency.srcStageMask;dependency.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};ci.pNext=&mv;ci.attachmentCount=2;ci.pAttachments=attachments.data();ci.subpassCount=1;ci.pSubpasses=&sub;ci.dependencyCount=1;ci.pDependencies=&dependency;
        Check(vkCreateRenderPass(device,&ci,nullptr,&pass),"multiview render pass");
        std::array<VkImageView,2> views{color.view,depth.view};VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fi.renderPass=pass;fi.attachmentCount=2;fi.pAttachments=views.data();fi.width=width;fi.height=height;fi.layers=1;
        Check(vkCreateFramebuffer(device,&fi,nullptr,&framebuffer),"multiview framebuffer");
    }
    void CreateDescriptors() {
        for(int kind=0;kind<2;++kind) {
            const size_t count=kind?std::size(shaders::terrainSamplers):std::size(shaders::worldSamplers);
            std::vector<VkDescriptorSetLayoutBinding> bindings(count+2);
            for(uint32_t i=0;i<bindings.size();++i){auto& b=bindings[i];b.binding=i;b.descriptorCount=1;b.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;
                b.descriptorType=i==0?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:i==1?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;}
            VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};ci.bindingCount=uint32_t(bindings.size());ci.pBindings=bindings.data();
            Check(vkCreateDescriptorSetLayout(device,&ci,nullptr,&sets[kind]),"descriptor layout");
            VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};li.setLayoutCount=1;li.pSetLayouts=&sets[kind];
            Check(vkCreatePipelineLayout(device,&li,nullptr,&layouts[kind]),"pipeline layout");
        }
        auto module=[&](auto& words,VkShaderModule& target){VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};ci.codeSize=sizeof(words);ci.pCode=words;Check(vkCreateShaderModule(device,&ci,nullptr,&target),"SPIR-V shader module");};
        module(shaders::world_vert,modules[0][0]);module(shaders::world_frag,modules[0][1]);module(shaders::terrain_vert,modules[1][0]);module(shaders::terrain_frag,modules[1][1]);
        std::array<VkDescriptorPoolSize,3> sizes{{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,8192},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,8192},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,81920}}};
        VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pi.maxSets=8192;pi.poolSizeCount=uint32_t(sizes.size());pi.pPoolSizes=sizes.data();
        Check(vkCreateDescriptorPool(device,&pi,nullptr,&descriptors),"frame descriptor pool");
    }
    VkPipeline Pipeline(const PipelineKey& key) {
        const auto found=pipelines.find(key);if(found!=pipelines.end())return found->second;
        const int kind=key.terrain?1:0;
        std::array<VkPipelineShaderStageCreateInfo,2> stages{};
        for(int i=0;i<2;++i){stages[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stages[i].stage=i?VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_VERTEX_BIT;stages[i].module=modules[kind][i];stages[i].pName="main";}
        VkVertexInputBindingDescription binding{0,key.stride*4,VK_VERTEX_INPUT_RATE_VERTEX};
        std::vector<VkVertexInputAttributeDescription> attributes;
        if(key.terrain)attributes={{0,0,VK_FORMAT_R32G32B32_SFLOAT,0},{1,0,VK_FORMAT_R32G32B32_SFLOAT,12},{2,0,VK_FORMAT_R32G32_SFLOAT,24},{3,0,VK_FORMAT_R32_SFLOAT,32},{4,0,VK_FORMAT_R32G32B32_SFLOAT,36}};
        else attributes={{0,0,VK_FORMAT_R32G32B32_SFLOAT,0},{1,0,VK_FORMAT_R32G32B32_SFLOAT,12},{2,0,VK_FORMAT_R32G32B32_SFLOAT,24},{3,0,VK_FORMAT_R32G32B32_SFLOAT,36},{4,0,VK_FORMAT_R32G32_SFLOAT,48},{5,0,VK_FORMAT_R32G32B32A32_SFLOAT,56}};
        // Unused skin/instance inputs must still have defined attributes. The
        // probe rejects draws needing those streams rather than render bind poses.
        if(!key.terrain)for(uint32_t i=6;i<12;++i) {
            const uint32_t stream=key.instanced&&i<10?1u:key.skinned&&i>=10?2u:0u;
            attributes.push_back({i,stream,VK_FORMAT_R32G32B32A32_SFLOAT,stream==1?(i-6)*16u:stream==2?(i-10)*16u:0u});
        }
        std::vector<VkVertexInputBindingDescription> bindings{binding};
        if(key.instanced)bindings.push_back({1,64,VK_VERTEX_INPUT_RATE_INSTANCE});
        if(key.skinned)bindings.push_back({2,32,VK_VERTEX_INPUT_RATE_VERTEX});
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};vi.vertexBindingDescriptionCount=uint32_t(bindings.size());vi.pVertexBindingDescriptions=bindings.data();vi.vertexAttributeDescriptionCount=uint32_t(attributes.size());vi.pVertexAttributeDescriptions=attributes.data();
        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport viewport{0,0,float(width),float(height),0,1};VkRect2D scissor{{0,0},{width,height}};
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=1;vp.pViewports=&viewport;vp.scissorCount=1;vp.pScissors=&scissor;
        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.polygonMode=VK_POLYGON_MODE_FILL;rs.lineWidth=1;
        rs.cullMode=key.cull?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_NONE;
        // Vertex shader flips Y, so Vulkan front-face winding is inverted.
        rs.frontFace=key.clockwise?VK_FRONT_FACE_COUNTER_CLOCKWISE:VK_FRONT_FACE_CLOCKWISE;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};ds.depthTestEnable=key.depth;ds.depthWriteEnable=key.write;ds.depthCompareOp=Compare(key.depthFunc);
        VkPipelineColorBlendAttachmentState ba{};ba.colorWriteMask=0xf;ba.blendEnable=key.blend;ba.srcColorBlendFactor=Blend(key.src);ba.dstColorBlendFactor=Blend(key.dst);ba.colorBlendOp=VK_BLEND_OP_ADD;ba.srcAlphaBlendFactor=Blend(key.srcA);ba.dstAlphaBlendFactor=Blend(key.dstA);ba.alphaBlendOp=VK_BLEND_OP_ADD;
        VkPipelineColorBlendStateCreateInfo bs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};bs.attachmentCount=1;bs.pAttachments=&ba;
        VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};ci.stageCount=2;ci.pStages=stages.data();ci.pVertexInputState=&vi;ci.pInputAssemblyState=&ia;ci.pViewportState=&vp;ci.pRasterizationState=&rs;ci.pMultisampleState=&ms;ci.pDepthStencilState=&ds;ci.pColorBlendState=&bs;ci.layout=layouts[kind];ci.renderPass=pass;
        VkPipeline pipeline{};Check(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&ci,nullptr,&pipeline),"world pipeline");pipelines.emplace(key,pipeline);return pipeline;
    }
    void Register(uint32_t id,bool terrain) {
        if(!id)return;
        ProgramData p;p.terrain=terrain;p.bytes.resize(terrain?shaders::terrainUniformSize:shaders::worldUniformSize);
        const auto* fields=terrain?shaders::terrainFields:shaders::worldFields;
        const size_t count=terrain?std::size(shaders::terrainFields):std::size(shaders::worldFields);
        for(size_t i=0;i<count;++i){int location=glGetUniformLocation(id,fields[i].name);if(location>=0)p.fields.push_back({location,fields[i]});}
        const auto* samplers=terrain?shaders::terrainSamplers:shaders::worldSamplers;
        const size_t samplerCount=terrain?std::size(shaders::terrainSamplers):std::size(shaders::worldSamplers);
        for(size_t i=0;i<samplerCount;++i)p.samplers.push_back(glGetUniformLocation(id,samplers[i].name));
        p.units.resize(samplerCount);programs[id]=std::move(p);
    }
    void Uniform(uint32_t id,int location,int kind,int bytes,const void* data) {
        auto it=programs.find(id);if(it==programs.end())return;auto& p=it->second;
        if(kind==2)for(size_t i=0;i<p.samplers.size();++i)if(p.samplers[i]==location)std::memcpy(&p.units[i],data,4);
        for(const auto& entry:p.fields)if(entry.location==location){
            const auto& f=entry.field;const uint32_t elementSize=f.components*4;
            const uint32_t n=std::min(f.count,uint32_t(bytes)/elementSize);
            for(uint32_t i=0;i<n;++i)std::memcpy(p.bytes.data()+f.offset+i*(f.components==16?64:16),static_cast<const uint8_t*>(data)+i*elementSize,elementSize);
            break;
        }
    }
    bool UsesSkin(const ProgramData& p) {
        for(const auto& e:p.fields)if(!std::strcmp(e.field.name,"uActorSkinMode")) {
            int mode=0;std::memcpy(&mode,p.bytes.data()+e.field.offset,4);if(mode)return true;
        }
        return false;
    }
    void Draw(uint32_t program,uint32_t vao,uint32_t mode,int first,int count,uint32_t type,size_t offset,int instances) {
        if(!begun||failed)return;
        const auto pIt=programs.find(program);if(pIt==programs.end()){++skipped;return;}
        const auto mIt=meshes.find(vao);if(mIt==meshes.end()||mode!=GL_TRIANGLES||count<=0||instances<=0||(instances>1&&(!instanceMatrices||instanceMatrixCount<size_t(instances)))||(UsesSkin(pIt->second)&&!mIt->second.skin.buffer)){++skipped;return;}
        if(type && type!=GL_UNSIGNED_INT){++skipped;return;}
        const auto& m=mIt->second;auto& p=pIt->second;const int kind=p.terrain?1:0;
        // Mirrored terrain packs indices after vertices in GL; native buffers are separate.
        const uint32_t indexFirst=type?(p.terrain?0:uint32_t(offset/4)):uint32_t(first);
        if(indexFirst+uint32_t(count)>m.count){++skipped;return;}
        const auto& s=fqgl::state;
        PipelineKey key;key.terrain=p.terrain;key.stride=m.stride;key.instanced=instances>1;key.skinned=UsesSkin(p);
        key.depth=s.depth.known?s.depth.value:true;key.write=s.depthWrite.known?s.depthWrite.value:true;
        key.blend=s.blend.known&&s.blend.value;key.cull=s.cull.known&&s.cull.value;
        key.clockwise=s.frontFace.known&&s.frontFace.value==GL_CW;key.depthFunc=s.depthFunc.known?s.depthFunc.value:GL_LESS;
        key.src=s.srcRgb.known?s.srcRgb.value:GL_ONE;key.dst=s.dstRgb.known?s.dstRgb.value:GL_ZERO;
        key.srcA=s.srcAlpha.known?s.srcAlpha.value:GL_ONE;key.dstA=s.dstAlpha.known?s.dstAlpha.value:GL_ZERO;
        if((s.cullFace.known&&s.cullFace.value!=GL_BACK)||(s.offset.known&&s.offset.value)) {++skipped;return;}
        const VkDeviceSize alignment=std::max<VkDeviceSize>(16,properties.limits.minUniformBufferOffsetAlignment);
        cursor=(cursor+alignment-1)&~(alignment-1);
        const size_t instanceBytes=key.instanced?size_t(instances)*64:0;
        if(cursor+p.bytes.size()+instanceBytes>arena.size||draws>=8192){++skipped;return;}
        std::memcpy(static_cast<uint8_t*>(arena.mapped)+cursor,p.bytes.data(),p.bytes.size());
        VkDescriptorSet set{};VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};ai.descriptorPool=descriptors;ai.descriptorSetCount=1;ai.pSetLayouts=&sets[kind];
        Check(vkAllocateDescriptorSets(device,&ai,&set),"allocate draw descriptors");
        std::array<VkDescriptorBufferInfo,2> bufferInfo{{{arena.buffer,0,p.bytes.size()},{stereo.buffer,0,160}}};
        std::vector<VkDescriptorImageInfo> imageInfo(p.units.size());
        const auto* samplerMeta=p.terrain?shaders::terrainSamplers:shaders::worldSamplers;
        for(size_t i=0;i<p.units.size();++i){
            VkImageView view=samplerMeta[i].cube?cube.view:white.view;
            const int unit=p.units[i];
            VkSampler chosenSampler=sampler;
            if(unit>=0&&unit<16){
                const auto& binding=samplerMeta[i].cube?s.textureCube[unit]:s.texture2d[unit];
                auto tex=textures.find(binding.value);
                if(tex!=textures.end()){view=tex->second.view;if(tex->second.palette)chosenSampler=nearest;}
            }
            imageInfo[i]={chosenSampler,view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        }
        std::vector<VkWriteDescriptorSet> writes(p.units.size()+2);
        for(uint32_t i=0;i<writes.size();++i){auto& w=writes[i];w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;w.dstSet=set;w.dstBinding=i;w.descriptorCount=1;
            w.descriptorType=i==0?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:i==1?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            if(i<2)w.pBufferInfo=&bufferInfo[i];else w.pImageInfo=&imageInfo[i-2];}
        vkUpdateDescriptorSets(device,uint32_t(writes.size()),writes.data(),0,nullptr);
        const uint32_t dynamic=uint32_t(cursor);cursor+=p.bytes.size();
        const VkDeviceSize instanceOffset=cursor;
        if(key.instanced){std::memcpy(static_cast<uint8_t*>(arena.mapped)+cursor,instanceMatrices,instanceBytes);cursor+=instanceBytes;}
        vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,Pipeline(key));
        vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,layouts[kind],0,1,&set,1,&dynamic);
        const VkDeviceSize zero=0;vkCmdBindVertexBuffers(command,0,1,&m.vertices.buffer,&zero);
        if(key.skinned)vkCmdBindVertexBuffers(command,2,1,&m.skin.buffer,&zero);
        if(key.instanced)vkCmdBindVertexBuffers(command,1,1,&arena.buffer,&instanceOffset);
        if(type){vkCmdBindIndexBuffer(command,m.indices.buffer,0,VK_INDEX_TYPE_UINT32);vkCmdDrawIndexed(command,count,instances,indexFirst,0,0);}
        else vkCmdDraw(command,count,instances,first,0);
        ++draws;vertices+=uint64_t(count)*instances;
    }
    void Begin(const float* left,const float* right,const float* le,const float* re) {
        Check(vkWaitForFences(device,1,&fence,VK_TRUE,UINT64_MAX),"wait frame");
        Check(vkResetCommandPool(device,pool,0),"reset commands");Check(vkResetDescriptorPool(device,descriptors,0),"reset descriptors");
        float data[40]{};std::memcpy(data,left,64);std::memcpy(data+16,right,64);std::memcpy(data+32,le,12);std::memcpy(data+36,re,12);std::memcpy(stereo.mapped,data,sizeof(data));
        cursor=0;draws=vertices=skipped=0;
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        Check(vkBeginCommandBuffer(command,&bi),"begin frame");
        if(timestamps){vkCmdResetQueryPool(command,timestamps,0,2);vkCmdWriteTimestamp(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,timestamps,0);}
        std::array<VkClearValue,2> clears{};clears[0].color={{0,0,0,1}};clears[1].depthStencil={1,0};
        VkRenderPassBeginInfo ri{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};ri.renderPass=pass;ri.framebuffer=framebuffer;ri.renderArea={{0,0},{width,height}};ri.clearValueCount=2;ri.pClearValues=clears.data();
        vkCmdBeginRenderPass(command,&ri,VK_SUBPASS_CONTENTS_INLINE);begun=true;
    }
    void End() {
        if(!begun)return;
        begun=false;
        vkCmdEndRenderPass(command);if(timestamps)vkCmdWriteTimestamp(command,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,timestamps,1);
        Check(vkEndCommandBuffer(command),"end frame");Check(vkResetFences(device,1,&fence),"reset frame fence");
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};si.commandBufferCount=1;si.pCommandBuffers=&command;
        Check(vkQueueSubmit(queue,1,&si,fence),"submit world probe");Check(vkWaitForFences(device,1,&fence,VK_TRUE,UINT64_MAX),"finish world probe");
        double gpuMs=-1;
        if(timestamps){uint64_t t[2]{};if(vkGetQueryPoolResults(device,timestamps,0,2,sizeof(t),t,8,VK_QUERY_RESULT_64_BIT)==VK_SUCCESS)gpuMs=double(t[1]-t[0])*properties.limits.timestampPeriod/1e6;}
        Collect();
        Dump();
        if((frames++%120)==0)VKLOG("FQVK WORLD PROBE frame=%llu partialStereoGpuMs=%.3f sharedDraws=%llu vertexCorners=%llu skippedDraws=%llu passes=world+LAND presentation=GLES concurrentApiWork=1 benchmarkValid=0",
            (unsigned long long)frames,gpuMs,(unsigned long long)draws,(unsigned long long)vertices,(unsigned long long)skipped);
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
                barrier.image=color.image;barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,2};
                barrier.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
                vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
                VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,2};copy.imageExtent={width,height,1};
                vkCmdCopyImageToBuffer(cb,color.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.buffer,1,&copy);
                VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};host.buffer=readback.buffer;host.size=VK_WHOLE_SIZE;
                host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
                host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
                vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
                barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
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
        if(dumped||std::atoi(value)!=1||outputDirectory.empty()||!draws)return;
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
            VKLOG("FQVK WORLD PROBE image=%s domain=scene-HDR-no-post",file.c_str());
        }
        dumped=true;
#endif
    }
    ~Renderer() {
        if(device){vkDeviceWaitIdle(device);
            for(auto& e:pipelines)vkDestroyPipeline(device,e.second,nullptr);
            for(auto& e:meshes){Destroy(e.second.vertices);Destroy(e.second.indices);Destroy(e.second.skin);}
            for(auto& e:textures)Destroy(e.second);
            Collect();
            Destroy(arena);Destroy(stereo);Destroy(white);Destroy(cube);
            if(framebuffer)vkDestroyFramebuffer(device,framebuffer,nullptr);
            if(pass)vkDestroyRenderPass(device,pass,nullptr);
            Destroy(color);Destroy(depth);
            if(descriptors)vkDestroyDescriptorPool(device,descriptors,nullptr);
            for(int k=0;k<2;++k){if(layouts[k])vkDestroyPipelineLayout(device,layouts[k],nullptr);if(sets[k])vkDestroyDescriptorSetLayout(device,sets[k],nullptr);for(auto m:modules[k])if(m)vkDestroyShaderModule(device,m,nullptr);}
            if(timestamps)vkDestroyQueryPool(device,timestamps,nullptr);
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
void Failure(const std::exception& e){VKLOG("FQVK WORLD PROBE disabled: %s; GLES gameplay continues",e.what());recording=false;if(renderer)renderer->failed=true;hooks={};}
}
bool InitializeWorldProbe(XrInstance instance,XrSystemId system,uint32_t w,uint32_t h,const char* outputDirectory) {
    ShutdownWorldProbe();
    try {
        renderer=std::make_unique<Renderer>();renderer->Initialize(instance,system,w,h);
        if(outputDirectory)renderer->outputDirectory=outputDirectory;
        hooks.program=[](uint32_t id,bool terrain){try{renderer->Register(id,terrain);}catch(const std::exception& e){Failure(e);}};
        hooks.uniform=[](uint32_t id,int location,int kind,int size,const void* bytes){renderer->Uniform(id,location,kind,size,bytes);};
        hooks.mesh=[](uint32_t vao,const float* vertices,size_t n,size_t stride,const uint32_t* indices,size_t indexCount){
            try {if(!vao||!vertices||!n)return;auto& r=*renderer;
                auto& m=r.meshes[vao];r.Retire(m.vertices);r.Retire(m.indices);
                m.stride=uint32_t(stride);m.count=uint32_t(indexCount?indexCount:n);
                m.vertices=r.UploadBuffer(vertices,n*stride*4,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
                if(indexCount)m.indices=r.UploadBuffer(indices,indexCount*4,VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
            }catch(const std::exception& e){Failure(e);}};
        hooks.texture=[](uint32_t id,int w,int h,const uint8_t* rgba){try{if(w<=0||h<=0||!rgba)return;auto& r=*renderer;auto image=r.UploadTexture(w,h,rgba);r.Retire(r.textures[id]);r.textures[id]=image;}catch(const std::exception& e){Failure(e);}};
        hooks.dynamicMesh=[](uint32_t vao,const float* data,size_t floats){try{
            auto& r=*renderer;auto found=r.meshes.find(vao);if(found==r.meshes.end())return;
            auto buffer=r.UploadBuffer(data,floats*4,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
            r.Retire(found->second.vertices);found->second.vertices=buffer;
        }catch(const std::exception& e){Failure(e);}};
        hooks.skin=[](uint32_t vao,uint32_t texture,const float* attributes,size_t count,const float* palette,size_t rows){try{
            auto& r=*renderer;auto found=r.meshes.find(vao);if(found==r.meshes.end())return;
            if(attributes&&count){auto b=r.UploadBuffer(attributes,count*4,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);r.Retire(found->second.skin);found->second.skin=b;}
            auto image=r.UploadPalette(palette,rows);r.Retire(r.textures[texture]);r.textures[texture]=image;
        }catch(const std::exception& e){Failure(e);}};
        hooks.cube=[](uint32_t id,const Fo3RgbaCubeTexture& source){try{
            auto& r=*renderer;auto image=r.UploadCube(source);r.Retire(r.textures[id]);r.textures[id]=image;
        }catch(const std::exception& e){Failure(e);}};
        hooks.draw=[](uint32_t p,uint32_t v,uint32_t m,int f,int n,uint32_t t,size_t o,int i){try{renderer->Draw(p,v,m,f,n,t,o,i);}catch(const std::exception& e){Failure(e);}};
        hooks.erase=[](uint32_t id,bool texture){auto& r=*renderer;
            if(texture){auto i=r.textures.find(id);if(i!=r.textures.end()){r.Retire(i->second);r.textures.erase(i);}}
            else{auto i=r.meshes.find(id);if(i!=r.meshes.end()){r.Retire(i->second.vertices);r.Retire(i->second.indices);r.Retire(i->second.skin);r.meshes.erase(i);}}};
        return true;
    }catch(const std::exception& e){Failure(e);ShutdownWorldProbe();return false;}
}
void ShutdownWorldProbe(){hooks={};recording=false;renderer.reset();}
bool WorldProbeActive(){return renderer&&!renderer->failed;}
bool BeginWorldProbe(const float* l,const float* r,const float* le,const float* re){
    if(!WorldProbeActive())return false;
    try{renderer->Begin(l,r,le,re);recording=true;return true;}catch(const std::exception& e){Failure(e);return false;}
}
void EndWorldProbe(){recording=false;if(renderer&&renderer->failed){ShutdownWorldProbe();return;}if(!renderer||!renderer->begun)return;try{renderer->End();}catch(const std::exception& e){Failure(e);}}
}
