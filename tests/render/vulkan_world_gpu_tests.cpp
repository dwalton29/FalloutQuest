// Runs the production backend and original ported material shaders on a real
// Vulkan implementation (CI: Mesa lavapipe). Only the OpenXR device handshake
// is supplied by this test; it does not claim headset/compositor validation.
#include "rendering/gl-state-cache.h"
#include "rendering/vulkan/fo3-vulkan-renderer.cpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace {
XrResult XRAPI_CALL Requirements(XrInstance,XrSystemId,XrGraphicsRequirementsVulkan2KHR* out) {
    out->minApiVersionSupported=XR_MAKE_VERSION(1,1,0);out->maxApiVersionSupported=XR_MAKE_VERSION(1,3,0);return XR_SUCCESS;
}
XrResult XRAPI_CALL Instance(XrInstance,const XrVulkanInstanceCreateInfoKHR* info,VkInstance* out,VkResult* result) {
    fqvk::renderer->passTiming=true;
    auto create=*info->vulkanCreateInfo;
    VkValidationFeatureEnableEXT synchronization=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
    VkValidationFeaturesEXT validation{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
    std::vector<const char*> extensions;
    if(create.enabledExtensionCount)extensions.assign(create.ppEnabledExtensionNames,create.ppEnabledExtensionNames+create.enabledExtensionCount);
    if(const auto* layers=std::getenv("VK_INSTANCE_LAYERS");layers&&std::strstr(layers,"VK_LAYER_KHRONOS_validation")) {
        validation.enabledValidationFeatureCount=1;validation.pEnabledValidationFeatures=&synchronization;validation.pNext=create.pNext;create.pNext=&validation;
        extensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);create.enabledExtensionCount=uint32_t(extensions.size());create.ppEnabledExtensionNames=extensions.data();
    }
    *result=vkCreateInstance(&create,nullptr,out);return XR_SUCCESS;
}
XrResult XRAPI_CALL Physical(XrInstance,const XrVulkanGraphicsDeviceGetInfoKHR* info,VkPhysicalDevice* out) {
    uint32_t count=0;vkEnumeratePhysicalDevices(info->vulkanInstance,&count,nullptr);
    if(!count)return XR_ERROR_RUNTIME_FAILURE;
    std::vector<VkPhysicalDevice> devices(count);vkEnumeratePhysicalDevices(info->vulkanInstance,&count,devices.data());*out=devices[0];return XR_SUCCESS;
}
bool enableTimelineGate=false;
XrResult XRAPI_CALL Device(XrInstance,const XrVulkanDeviceCreateInfoKHR* info,VkDevice* out,VkResult* result) {
    auto create=*info->vulkanCreateInfo;
    VkPhysicalDeviceTimelineSemaphoreFeatures timeline{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES};
    std::vector<const char*> extensions;
    if(enableTimelineGate) {
        if(create.enabledExtensionCount)extensions.assign(create.ppEnabledExtensionNames,create.ppEnabledExtensionNames+create.enabledExtensionCount);
        extensions.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
        create.enabledExtensionCount=uint32_t(extensions.size());create.ppEnabledExtensionNames=extensions.data();
        timeline.timelineSemaphore=VK_TRUE;timeline.pNext=const_cast<void*>(create.pNext);create.pNext=&timeline;
    }
    *result=vkCreateDevice(info->vulkanPhysicalDevice,&create,nullptr,out);return XR_SUCCESS;
}
// Fake OpenXR compositor with real software-GPU images. This tests the native
// image/view/render/release path, not a real OpenXR runtime or headset.
std::vector<fqvk::Image> runtimeImages,runtimeDepth;
uint32_t nextDepth=1,acquiredDepth=0,depthReleases=0;
bool depthAcquired=false,depthWaited=false,offerDepth=false;
std::vector<uint8_t> submittedPixels;
bool inspectModalDepth=false;std::vector<float> modalWorldDepth;
uint32_t nextImage=0,acquiredImage=0,releases=0,destroys=0;
bool acquired=false,waited=false,timeoutWait=false,unsupportedFormat=false;
bool offerSrgb=false,inspectReleasedImages=true;
VkFormat offeredColor=VK_FORMAT_R8G8B8A8_UNORM,createdColor=VK_FORMAT_UNDEFINED;
XrResult XRAPI_CALL Formats(XrSession,uint32_t capacity,uint32_t* count,int64_t* out) {
    std::vector<int64_t> formats{unsupportedFormat?VK_FORMAT_R32G32_SFLOAT:offeredColor};
    if(offerSrgb)formats.push_back(VK_FORMAT_R8G8B8A8_SRGB);
    if(offerDepth)formats.push_back(VK_FORMAT_D32_SFLOAT);
    *count=uint32_t(formats.size());
    if(capacity)std::copy(formats.begin(),formats.end(),out);
    return XR_SUCCESS;
}
XrResult XRAPI_CALL Swapchain(XrSession,const XrSwapchainCreateInfo* info,XrSwapchain* out) {
    assert(info->arraySize==2&&info->sampleCount==1);
    if(info->format==VK_FORMAT_D32_SFLOAT) {
        assert(info->usageFlags==(XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT));
        assert(runtimeDepth.empty());
        for(int i=0;i<2;++i)runtimeDepth.push_back(fqvk::renderer->MakeImage(info->width,info->height,VK_FORMAT_D32_SFLOAT,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_DEPTH_BIT));
        *out=reinterpret_cast<XrSwapchain>(uintptr_t(4));return XR_SUCCESS;
    }
    assert(info->usageFlags==XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT&&runtimeImages.empty());
    createdColor=VkFormat(info->format);
    for(int i=0;i<3;++i)runtimeImages.push_back(fqvk::renderer->MakeImage(info->width,info->height,VkFormat(info->format),
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_COLOR_BIT));
    *out=reinterpret_cast<XrSwapchain>(uintptr_t(3));return XR_SUCCESS;
}
XrResult XRAPI_CALL Images(XrSwapchain handle,uint32_t capacity,uint32_t* count,XrSwapchainImageBaseHeader* out) {
    const auto& images=handle==reinterpret_cast<XrSwapchain>(uintptr_t(4))?runtimeDepth:runtimeImages;
    *count=uint32_t(images.size());
    if(capacity)for(size_t i=0;i<images.size();++i) {
        auto* image=reinterpret_cast<XrSwapchainImageVulkan2KHR*>(out)+i;
        assert(image->type==XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR);image->image=images[i].image;
    }
    return XR_SUCCESS;
}
XrResult XRAPI_CALL Acquire(XrSwapchain handle,const XrSwapchainImageAcquireInfo*,uint32_t* index) {
    if(handle==reinterpret_cast<XrSwapchain>(uintptr_t(4))){assert(!depthAcquired);depthAcquired=true;depthWaited=false;acquiredDepth=nextDepth++%2;*index=acquiredDepth;return XR_SUCCESS;}
    assert(!acquired);acquired=true;waited=false;acquiredImage=nextImage++%3;*index=acquiredImage;return XR_SUCCESS;
}
XrResult XRAPI_CALL Wait(XrSwapchain handle,const XrSwapchainImageWaitInfo*) {
    if(handle==reinterpret_cast<XrSwapchain>(uintptr_t(4))){assert(depthAcquired&&!depthWaited);depthWaited=true;return XR_SUCCESS;}
    assert(acquired&&!waited);if(timeoutWait)return XR_TIMEOUT_EXPIRED;waited=true;return XR_SUCCESS;
}
std::vector<uint8_t> RuntimePixels(uint32_t index) {
    auto& r=*fqvk::renderer;std::vector<uint8_t> pixels;
    auto readback=r.MakeBuffer(64*64*2*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
    r.Transfer([&](VkCommandBuffer cb){
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.image=runtimeImages[index].image;
        b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,2};b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        b.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        b.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
        VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,2};copy.imageExtent={64,64,1};
        vkCmdCopyImageToBuffer(cb,b.image,b.newLayout,readback.buffer,1,&copy);
        b.oldLayout=b.newLayout;b.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        b.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;b.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,0,nullptr,0,nullptr,1,&b);
        VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};host.buffer=readback.buffer;host.size=VK_WHOLE_SIZE;
        host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
    });
    pixels.assign(static_cast<uint8_t*>(readback.mapped),static_cast<uint8_t*>(readback.mapped)+64*64*2*4);
    if(createdColor==VK_FORMAT_B8G8R8A8_UNORM||createdColor==VK_FORMAT_B8G8R8A8_SRGB)
        for(size_t i=0;i<pixels.size();i+=4)std::swap(pixels[i],pixels[i+2]);
    r.Destroy(readback);return pixels;
}
XrResult XRAPI_CALL Release(XrSwapchain handle,const XrSwapchainImageReleaseInfo*) {
    if(!inspectReleasedImages) {
        auto& r=*fqvk::renderer;assert(r.frameSlots[r.activeSlot].serial==r.submissionSerial&&!r.begun);
        if(handle==reinterpret_cast<XrSwapchain>(uintptr_t(4))){assert(depthAcquired&&depthWaited);depthAcquired=depthWaited=false;++depthReleases;}
        else {assert(acquired&&waited);acquired=waited=false;++releases;}
        return XR_SUCCESS;
    }
    if(handle==reinterpret_cast<XrSwapchain>(uintptr_t(4))) {
        assert(depthAcquired&&depthWaited);auto& r=*fqvk::renderer;assert(r.frameSlots[r.activeSlot].serial==r.submissionSerial);
        auto buffer=r.MakeBuffer(64*64*2*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
        r.Transfer([&](VkCommandBuffer cb){
            VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.image=runtimeDepth[acquiredDepth].image;
            b.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,2};b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            b.oldLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
            VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_DEPTH_BIT,0,0,2};copy.imageExtent={64,64,1};
            vkCmdCopyImageToBuffer(cb,b.image,b.newLayout,buffer.buffer,1,&copy);
            b.oldLayout=b.newLayout;b.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;b.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;b.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,0,0,nullptr,0,nullptr,1,&b);
            VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};host.buffer=buffer.buffer;host.size=VK_WHOLE_SIZE;
            host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
            vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
        });
        const float* depth=static_cast<const float*>(buffer.mapped);
        if(inspectModalDepth)modalWorldDepth.assign(depth,depth+64*64*2);
        else {
        assert(std::fabs(depth[16*64+32]-.5f)<.001f&&std::fabs(depth[64*64+16*64+55]-.5f)<.001f);
        assert(depth[64*64+16*64+32]==1&&depth[48*64+32]==1);
        }
        r.Destroy(buffer);depthAcquired=depthWaited=false;++depthReleases;return XR_SUCCESS;
    }
    assert(acquired&&waited);
    auto& r=*fqvk::renderer;
    // The app submits on the session queue before release; the fake compositor
    // reads later on that same queue, without requiring a CPU-side frame wait.
    assert(r.frameSlots[r.activeSlot].serial==r.submissionSerial&&!r.begun);
    submittedPixels=RuntimePixels(acquiredImage);
    acquired=waited=false;++releases;return XR_SUCCESS;
}
XrResult XRAPI_CALL DestroySwapchain(XrSwapchain handle) {
    if(handle==reinterpret_cast<XrSwapchain>(uintptr_t(4))) {
        for(auto& image:runtimeDepth)fqvk::renderer->Destroy(image);
        runtimeDepth.clear();depthAcquired=depthWaited=false;++destroys;return XR_SUCCESS;
    }
    for(auto& image:runtimeImages)fqvk::renderer->Destroy(image);
    runtimeImages.clear();acquired=waited=false;++destroys;return XR_SUCCESS;
}

}
extern "C" XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance,const char* name,PFN_xrVoidFunction* out) {
    if(!std::strcmp(name,"xrGetVulkanGraphicsRequirements2KHR"))*out=reinterpret_cast<PFN_xrVoidFunction>(Requirements);
    else if(!std::strcmp(name,"xrCreateVulkanInstanceKHR"))*out=reinterpret_cast<PFN_xrVoidFunction>(Instance);
    else if(!std::strcmp(name,"xrGetVulkanGraphicsDevice2KHR"))*out=reinterpret_cast<PFN_xrVoidFunction>(Physical);
    else if(!std::strcmp(name,"xrCreateVulkanDeviceKHR"))*out=reinterpret_cast<PFN_xrVoidFunction>(Device);
    else if(!std::strcmp(name,"xrEnumerateSwapchainFormats"))*out=reinterpret_cast<PFN_xrVoidFunction>(Formats);
    else if(!std::strcmp(name,"xrCreateSwapchain"))*out=reinterpret_cast<PFN_xrVoidFunction>(Swapchain);
    else if(!std::strcmp(name,"xrEnumerateSwapchainImages"))*out=reinterpret_cast<PFN_xrVoidFunction>(Images);
    else if(!std::strcmp(name,"xrAcquireSwapchainImage"))*out=reinterpret_cast<PFN_xrVoidFunction>(Acquire);
    else if(!std::strcmp(name,"xrWaitSwapchainImage"))*out=reinterpret_cast<PFN_xrVoidFunction>(Wait);
    else if(!std::strcmp(name,"xrReleaseSwapchainImage"))*out=reinterpret_cast<PFN_xrVoidFunction>(Release);
    else if(!std::strcmp(name,"xrDestroySwapchain"))*out=reinterpret_cast<PFN_xrVoidFunction>(DestroySwapchain);
    else return XR_ERROR_FUNCTION_UNSUPPORTED;
    return XR_SUCCESS;
}
namespace {
void Submit(uint32_t p,uint32_t v,uint32_t mode,int first,int n,uint32_t type,size_t offset,int instances) {
    fqvk::hooks.draw(fqgl::Command(p,v,mode,type,offset,first,n,instances));
}
}
namespace {
void Program(uint32_t id,int kind) {
    const auto& shader=fqvk::shaders::programs[kind];
    fqvk::ProgramData p;p.terrain=kind==1;p.sky=kind==2;p.water=kind==3;p.ui=kind>=9?kind:0;
    fqvk::ConfigureDiscardGuards(p,shader);
    p.bytes.resize(shader.uniformSize);
    for(size_t i=0;i<shader.fieldCount;++i)p.fields.push_back({int(i*64),shader.fields[i]});
    p.units.resize(shader.samplerCount);
    for(size_t i=0;i<shader.samplerCount;++i) {
        const std::string name=shader.samplers[i].name;
        p.units[i]=name=="uActorPalette"?6:name=="uNormalGloss"?1:name=="uGlow"?2:
            name=="uShadowMap"?3:name=="uEnvironmentCubeQ2050"?4:name=="uEnvironmentMaskQ2050"?5:0;
    }
    fqvk::renderer->programs.emplace(id,std::move(p));
}
void Set(uint32_t id,const char* name,std::initializer_list<float> values) {
    const auto& p=fqvk::renderer->programs.at(id);
    for(const auto& e:p.fields)if(!std::strcmp(name,e.field.name)) {
        if(e.field.integer){int value=int(*values.begin());fqvk::hooks.uniform(id,e.location,2,4,&value);}
        else fqvk::hooks.uniform(id,e.location,1,int(values.size()*4),values.begin());
        return;
    }
    throw std::runtime_error(std::string("missing test uniform ")+name);
}
std::vector<float> Quad(bool terrain,float alpha=1) {
    std::vector<float> out;
    for(const auto& p:std::array<std::array<float,2>,4>{{{{-.6f,-.6f}},{{.6f,-.6f}},{{.6f,.6f}},{{-.6f,.6f}}}}) {
        if(terrain)out.insert(out.end(),{p[0],p[1],0, 0,0,1, 0,0,alpha, 1,1,1});
        else out.insert(out.end(),{p[0],p[1],0, 0,0,1, 1,0,0, 0,1,0, 0,0, 1,1,1,alpha});
    }
    return out;
}
float Pixel(const std::vector<uint16_t>& image,int eye,int x,int y,int component) {
    return fqvk::Renderer::Half(image[(eye*64*64+y*64+x)*4+component]);
}
void Near(float value,float expected) {if(std::fabs(value-expected)>.012f)throw std::runtime_error("pixel mismatch: "+std::to_string(value)+" expected "+std::to_string(expected));}
void Scene(bool terrain,bool skin=false,bool instanced=false,bool authoredCull=false) {
    fqgl::Reset();
    Program(1,terrain);const auto quad=Quad(terrain);const uint32_t indices[]{0,1,2,0,2,3};
    fqvk::Mesh(9,quad.data(),4,terrain?12:18,indices,6);
    uint8_t red[]{255,0,0,255};fqvk::Texture(5,1,1,red);
    fqgl::state.texture2d[0].Change(5);
    fqgl::state.depth.Change(true);fqgl::state.depthWrite.Change(true);fqgl::state.depthFunc.Change(GL_LEQUAL);
    fqgl::state.blend.Change(false);fqgl::state.cull.Change(authoredCull);
    fqgl::state.cullFace.Change(GL_BACK);fqgl::state.frontFace.Change(GL_CCW);
    Set(1,"uAmbientColor",{1,1,1});Set(1,"uSunlightColor",{0,0,0});Set(1,"uSunDirection",{0,0,1});
    Set(1,"uFogNearVertexQ1532",{10000});Set(1,"uFogFarVertexQ1532",{10001});Set(1,"uFogPowerVertexQ1532",{1});
    if(terrain)Set(1,"uUseTexture",{1});
    else {Set(1,"uNoLighting",{1});Set(1,"uMaterialAlpha",{1});Set(1,"uPipScreenGain",{1});Set(1,"uSunDirectionVertexQ1540",{0,0,1});}
    std::vector<float> weights(4*8);
    float palette[32]{};palette[0]=palette[5]=palette[10]=palette[15]=1;
    palette[16]=palette[21]=palette[26]=palette[31]=1;
    if(skin) {
        for(size_t i=0;i<4;++i)weights[i*8]=1;
        fqvk::Skin(9,7,weights.data(),weights.size(),palette,1);
        fqgl::state.texture2d[6].Change(7);Set(1,"uActorSkinMode",{1});
    }
    const float instances[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,
                            1,0,0,0,0,1,0,0,0,0,1,0,1.2f,0,0,1};
    if(instanced){Set(1,"uInstancingEnabledQ2016",{1});fqvk::instanceMatrices=instances;fqvk::instanceMatrixCount=2;}
    const float left[]{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    const float right[]{1,0,0,0, 0,1,0,0, 0,0,1,0, .8f,0,0,1};
    const float eye[]{0,0,2};assert(fqvk::BeginWorldProbe(left,right,eye,eye));
    // Terrain's GL EBO points past vertex bytes; native EBO has its own buffer.
    Submit(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,terrain?quad.size()*4:0,instanced?2:1);
    if(instanced){fqvk::instanceMatrices=nullptr;fqvk::instanceMatrixCount=0;} // queued transforms own their data
    if(terrain) {
        // Authored overlay with depth equality and no depth writes. A uniform
        // update after an earlier draw must not retroactively alter that draw.
        auto paint=Quad(true,.5f);fqvk::Mesh(10,paint.data(),4,12,indices,6);
        uint8_t green[]{0,255,0,255};fqvk::Texture(6,1,1,green);
        fqgl::state.texture2d[0].Change(6);fqgl::state.blend.Change(true);fqgl::state.depthWrite.Change(false);
        fqgl::state.srcRgb.Change(GL_SRC_ALPHA);fqgl::state.dstRgb.Change(GL_ONE_MINUS_SRC_ALPHA);
        fqgl::state.srcAlpha.Change(GL_SRC_ALPHA);fqgl::state.dstAlpha.Change(GL_ONE_MINUS_SRC_ALPHA);
        Submit(1,10,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,paint.size()*4,1);
    }
    fqvk::EndWorldProbe();assert(fqvk::WorldProbeActive());
    const auto pixels=fqvk::renderer->Readback();
    Near(Pixel(pixels,0,32,32,0),terrain?.5f:1);Near(Pixel(pixels,0,32,32,1),terrain?.5f:0);
    Near(Pixel(pixels,1,32,32,0),0); // gl_ViewIndex really selected the right camera.
    Near(Pixel(pixels,1,55,32,0),terrain?.5f:1);
    assert(fqvk::renderer->draws==uint64_t(terrain?2:1));
    fqvk::instanceMatrices=nullptr;fqvk::instanceMatrixCount=0;
    if(instanced)Near(Pixel(pixels,0,62,32,0),1);
    if(skin) {
        // Update the same live palette without rebuilding the mesh/animation.
        palette[12]=.8f;
        const auto view=fqvk::renderer->textures.at(7).view;
        const auto descriptors=fqvk::renderer->descriptorAllocations;
        const auto allocations=fqvk::renderer->stagingAllocations;
        fqvk::Skin(9,7,nullptr,0,palette,1);
        if(fqrender::nativeOnly)assert(fqvk::renderer->textures.at(7).view==view&&fqvk::renderer->paletteUpdates==1);
        assert(fqvk::BeginWorldProbe(left,left,eye,eye));
        Submit(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
        fqvk::EndWorldProbe();
        if(fqrender::nativeOnly)assert(fqvk::renderer->descriptorAllocations==descriptors&&fqvk::renderer->stagingAllocations==allocations);
        const auto moved=fqvk::renderer->Readback();
        Near(Pixel(moved,0,32,32,0),0);Near(Pixel(moved,0,55,32,0),1);
    }
}
}
void Sky() {
    fqgl::Reset();Program(2,2);
    auto source=Quad(false);std::vector<float> geometry;
    for(size_t i=0;i<source.size();i+=18)geometry.insert(geometry.end(),source.begin()+i,source.begin()+i+3);
    const uint32_t indices[]{0,1,2,0,2,3};fqvk::Mesh(12,geometry.data(),4,3,indices,6);
    Set(2,"uMode",{0});
    Set(2,"uSkyUpper",{.2f,.3f,.4f});Set(2,"uSkyLower",{.2f,.3f,.4f});Set(2,"uHorizon",{.2f,.3f,.4f});
    fqgl::state.depth.Change(false);fqgl::state.depthWrite.Change(false);fqgl::state.cull.Change(false);fqgl::state.blend.Change(false);
    const float left[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const float right[]{1,0,0,0,0,1,0,0,0,0,1,0,.8f,0,0,1};
    const float le[]{0,0,0},re[]{-.8f,0,0};
    assert(fqvk::BeginWorldProbe(left,right,le,re));Submit(2,12,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);fqvk::EndWorldProbe();
    const auto pixels=fqvk::renderer->Readback();
    for(int eye=0;eye<2;++eye){Near(Pixel(pixels,eye,32,24,0),.31f);Near(Pixel(pixels,eye,32,24,1),.465f);Near(Pixel(pixels,eye,32,24,2),.62f);}
}

void Water() {
    Scene(false); // establishes the original world material and resident NIF layout
    Program(3,3);
    std::vector<float> geometry;
    for(const auto& xy:std::array<std::array<float,2>,4>{{{{-1,-1}},{{1,-1}},{{1,1}},{{-1,1}}}})
        geometry.insert(geometry.end(),{xy[0],xy[1],-.2f,xy[0],xy[1]});
    const uint32_t indices[]{0,1,2,0,2,3};fqvk::Mesh(13,geometry.data(),4,5,indices,6);
    uint8_t flat[]{128,128,128,255};fqvk::Texture(14,1,1,flat,true,false);
    Set(3,"uSunDirection",{0,1,0});Set(3,"uSunColor",{0,0,0});
    Set(3,"uUnitsPerMetre",{1});Set(3,"uClipNearFar",{.1f,10});
    Set(3,"uSceneFogNearFar",{1000,1001});Set(3,"uSceneFogPower",{1});
    Set(3,"uNormalUvScale",{1});Set(3,"uNoiseScale",{0});
    Set(3,"uDepthFalloff",{0,1e9f});Set(3,"uWaterFogNearFar",{0,1e9f});
    Set(3,"uWaterFogAmount",{0});Set(3,"uShininess",{1});
    const float left[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const float right[]{1,0,0,0,0,1,0,0,0,0,1,0,.8f,0,0,1};
    const float eye[]{0,0,2};
    // Repeat to exercise SHADER_READ -> TRANSFER_DST when reusing snapshots.
    for(int frame=0;frame<2;++frame) {
        assert(fqvk::BeginWorldProbe(left,right,eye,eye));
        fqgl::state.depthWrite.Change(true);fqgl::state.texture2d[0].Change(5);
        Submit(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
        fqvk::WaterSnapshot();assert(fqvk::renderer->snapshotReady);
        // Snapshot must be once per frame even with multiple water cells.
        fqvk::WaterSnapshot();
        fqgl::state.depthWrite.Change(false);fqgl::state.texture2d[0].Change(14);
        Submit(3,13,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
        fqvk::EndWorldProbe();assert(fqvk::WorldProbeActive());
        const auto pixels=fqvk::renderer->Readback();
        Near(Pixel(pixels,0,32,32,0),1);Near(Pixel(pixels,0,32,32,1),0);
        Near(Pixel(pixels,1,32,32,0),0);Near(Pixel(pixels,1,55,32,0),1);
    }
    // A farther water plane must remain behind the stored opaque depth after
    // leaving and resuming the render pass, while visible in uncovered pixels.
    for(size_t i=2;i<geometry.size();i+=5)geometry[i]=.2f;
    fqvk::Mesh(13,geometry.data(),4,5,indices,6);
    Set(3,"uShallow",{0,1,0});Set(3,"uDeep",{0,1,0});
    Set(3,"uDepthFalloff",{-1,0});Set(3,"uWaterFogNearFar",{0,.1f});Set(3,"uWaterFogAmount",{1});
    const float above[]{0,2,2};
    assert(fqvk::BeginWorldProbe(left,left,above,above));
    fqgl::state.depthWrite.Change(true);fqgl::state.texture2d[0].Change(5);
    Submit(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
    fqvk::WaterSnapshot();fqgl::state.depthWrite.Change(false);fqgl::state.texture2d[0].Change(14);
    Submit(3,13,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);fqvk::EndWorldProbe();
    const auto occluded=fqvk::renderer->Readback();
    Near(Pixel(occluded,0,32,32,0),1);Near(Pixel(occluded,0,32,32,1),0);
    assert(Pixel(occluded,0,5,32,1)>.1f);
}

// Two opposing opaque faces of a closed box. Normal GL_CCW/back-cull must
// retain the near outside face, rather than expose the farther inside texture.
void AuthoredCulling() {
    Scene(false);
    auto box=Quad(false);auto back=Quad(false);
    for(size_t i=0;i<4;++i){box[i*18+2]=-.3f;back[i*18+2]=.3f;}
    box.insert(box.end(),back.begin(),back.end());
    const uint32_t indices[]{0,1,2,0,2,3,4,6,5,4,7,6,
        0,4,5,0,5,1,1,5,6,1,6,2,2,6,7,2,7,3,3,7,4,3,4,0};
    fqvk::Mesh(50,box.data(),8,18,indices,36);
    const uint8_t green[]{0,255,0,255};fqvk::Texture(51,1,1,green);
    const float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},eye[]{0,0,2};
    for(unsigned cull:{GL_BACK,GL_FRONT,GL_FRONT_AND_BACK})for(bool clockwise:{false,true}) {
        assert(fqvk::BeginWorldProbe(identity,identity,eye,eye));
        fqgl::state.cull.Change(true);fqgl::state.cullFace.Change(cull);
        fqgl::state.frontFace.Change(clockwise?GL_CW:GL_CCW);
        fqgl::state.texture2d[0].Change(5);Submit(1,50,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
        fqgl::state.texture2d[0].Change(51);Submit(1,50,GL_TRIANGLES,0,30,GL_UNSIGNED_INT,24,1);
        fqvk::EndWorldProbe();const auto pixels=fqvk::renderer->Readback();
        const bool front=cull==GL_BACK?!clockwise:cull==GL_FRONT?clockwise:false;
        const bool backVisible=cull!=GL_FRONT_AND_BACK&&!front;
        for(int e=0;e<2;++e){Near(Pixel(pixels,e,32,32,0),front?1:0);Near(Pixel(pixels,e,32,32,1),backVisible?1:0);}
    }
    assert(fqvk::BeginWorldProbe(identity,identity,eye,eye));
    fqgl::state.cull.Change(false);fqgl::state.texture2d[0].Change(5);
    Submit(1,50,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
    fqgl::state.texture2d[0].Change(51);Submit(1,50,GL_TRIANGLES,0,30,GL_UNSIGNED_INT,24,1);
    fqvk::EndWorldProbe();const auto pixels=fqvk::renderer->Readback();
    Near(Pixel(pixels,0,32,32,0),1);Near(Pixel(pixels,0,32,32,1),0);
    std::cout<<"Authored culling: opaque closed box, GL_CCW/GL_CW, front/back/both and two-sided depth passed\n";
}

float DisplayLinear(float value) {
    const float code=std::floor(std::clamp(value,0.0f,1.0f)*255+.5f)/255;
    const float gamma=std::pow(code,.76f);
    return gamma<=.04045f?gamma/12.92f:std::pow((gamma+.055f)/1.055f,2.4f);
}
void Post() {
    Scene(false);
    const uint8_t red[]{102,0,0,255};fqvk::Texture(5,1,1,red);
    const float left[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const float right[]{1,0,0,0,0,1,0,0,0,0,1,0,.8f,0,0,1};const float eye[]{0,0,2};
    fqvk::PostSettings settings;settings.interior=1;settings.flags=4;settings.tintValue=1;
    settings.tint[0]=settings.tint[2]=0;settings.tint[1]=1;settings.brightThreshold=.1f;
    for(int frame=0;frame<3;++frame) {
        assert(fqvk::BeginWorldProbe(left,right,eye,eye));
        fqgl::state.texture2d[0].Change(5);Submit(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
        settings.brightScale=frame==0?0.0f:1.0f;
        if(frame==2)fqvk::ResetExposure();
        fqvk::PostProcess(settings);assert(fqvk::renderer->postDone);fqvk::EndWorldProbe();
        const auto pixels=fqvk::renderer->Readback();
        const float redCombined=.4f+(frame==0?0.0f:.3f*.5f/settings.targetLum);
        const float expected=DisplayLinear(redCombined*.298999995f);
        Near(Pixel(pixels,0,32,32,0),0);Near(Pixel(pixels,0,32,32,1),expected);
        Near(Pixel(pixels,1,32,32,1),0);Near(Pixel(pixels,1,55,32,1),expected);
        if(frame)assert(Pixel(pixels,0,32,32,1)>DisplayLinear(.4f*.298999995f)+.01f);
    }
}

void Ui(bool animated) {
    Scene(false);Program(15,animated?10:9);
    std::vector<float> geometry;
    for(const auto& xy:std::array<std::array<float,2>,4>{{{{-.6f,-.6f}},{{.6f,-.6f}},{{.6f,.6f}},{{-.6f,.6f}}}}) {
        geometry.insert(geometry.end(),{xy[0],xy[1],-.2f,0,0});if(animated)geometry.push_back(0);
    }
    const uint32_t indices[]{0,1,2,0,2,3};fqvk::Mesh(16,geometry.data(),4,animated?6:5,indices,6);
    uint8_t white[]{255,255,255,255};fqvk::Texture(17,1,1,white,false,false);
    Set(15,"uMvp",{1,0,0,0,0,1,0,0,0,0,1,0,.2f,0,0,1});
    Set(15,"uTint",{0,1,0,animated?1.0f:.5f});if(animated)Set(15,"uAge",{1});
    const float left[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const float right[]{1,0,0,0,0,1,0,0,0,0,1,0,.8f,0,0,1};const float eye[]{0,0,2};
    assert(fqvk::BeginWorldProbe(left,right,eye,eye));
    fqgl::state.texture2d[0].Change(5);Submit(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
    fqgl::state.texture2d[0].Change(17);fqgl::state.depthWrite.Change(false);fqgl::state.blend.Change(true);
    fqgl::state.srcRgb.Change(GL_SRC_ALPHA);fqgl::state.dstRgb.Change(GL_ONE_MINUS_SRC_ALPHA);
    fqgl::state.srcAlpha.Change(GL_ONE);fqgl::state.dstAlpha.Change(GL_ONE_MINUS_SRC_ALPHA);
    Submit(15,16,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);fqvk::EndWorldProbe();
    const auto pixels=fqvk::renderer->Readback();
    Near(Pixel(pixels,0,32,32,0),.5f);Near(Pixel(pixels,0,32,32,1),.5f);
    Near(Pixel(pixels,0,17,32,1),0); // object translation is retained
    Near(Pixel(pixels,1,32,32,1),0);Near(Pixel(pixels,1,55,32,1),.5f);
}

void Loading() {
    Scene(false);Program(18,11);
    std::vector<float> geometry;
    for(const auto& xy:std::array<std::array<float,2>,4>{{{{-.6f,-.6f}},{{.6f,-.6f}},{{-.6f,.6f}},{{.6f,.6f}}}})
        geometry.insert(geometry.end(),{xy[0],xy[1],.2f,0,0,1,0,0,1,1,1,1});
    fqvk::Mesh(19,geometry.data(),4,12,nullptr,0);
    const uint8_t white[]{255,255,255,255};fqvk::Texture(20,1,1,white,false,false);
    Set(18,"uMvp",{1,0,0,0,0,1,0,0,0,0,1,0,.2f,0,0,1});
    Set(18,"uUiTransform",{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});
    Set(18,"uTint",{0,1,0,1});Set(18,"uUnlit",{1});Set(18,"uFade",{.5f});
    Set(18,"uCutoff",{.5f});Set(18,"uClipUi",{1});Set(18,"uClipHalf",{.3f,.6f});
    const float left[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const float right[]{1,0,0,0,0,1,0,0,0,0,1,0,.8f,0,0,1};const float eye[]{0,0,2};
    assert(fqvk::BeginWorldProbe(left,right,eye,eye,true));
    fqgl::state.texture2d[0].Change(5);Submit(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
    fqvk::ClearDepth();
    fqgl::state.texture2d[0].Change(20);
    Submit(18,19,GL_TRIANGLE_STRIP,0,4,0,0,1);fqvk::EndWorldProbe();
    const auto pixels=fqvk::renderer->Readback();
    Near(Pixel(pixels,0,32,32,1),.5f);Near(Pixel(pixels,1,55,32,1),.5f);
    Near(Pixel(pixels,1,32,32,1),0);Near(Pixel(pixels,0,50,32,1),0);
}

void PipboyTarget() {
    Scene(false);Program(21,9);
    std::vector<float> ui;
    auto rect=[&](float y0,float y1){for(const auto& xy:std::array<std::array<float,2>,6>{{{{-1,y0}},{{1,y0}},{{1,y1}},{{-1,y0}},{{1,y1}},{{-1,y1}}}})ui.insert(ui.end(),{xy[0],xy[1],0,0,0});};
    rect(-1,0);rect(0,1);fqvk::Mesh(22,ui.data(),12,5,nullptr,0);
    const uint8_t white[]{255,255,255,255};fqvk::Texture(23,1,1,white,false,false);
    Set(21,"uMvp",{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});
    auto screen=Quad(false);for(size_t i=0;i<4;++i){screen[i*18+12]=.5f;screen[i*18+13]=(screen[i*18+1]+.6f)/1.2f;}
    const uint32_t indices[]{0,1,2,0,2,3};fqvk::Mesh(24,screen.data(),4,18,indices,6);
    for(int frame=0;frame<2;++frame) {
        {
            fqvk::UiTargetScope target(30,32,16);assert(target.active&&fqvk::recording);
            fqgl::state.texture2d[0].Change(23);fqgl::state.blend.Change(false);
            fqgl::state.cull.Change(true);fqgl::state.cullFace.Change(GL_BACK);fqgl::state.frontFace.Change(GL_CCW);
            Set(21,"uTint",frame?std::initializer_list<float>{0,1,0,1}:std::initializer_list<float>{1,0,0,1});
            Submit(21,22,GL_TRIANGLES,0,6,0,0,1);
            Set(21,"uTint",frame?std::initializer_list<float>{1,0,0,1}:std::initializer_list<float>{0,1,0,1});
            Submit(21,22,GL_TRIANGLES,6,6,0,0,1);
        }
        assert(!fqvk::recording&&fqvk::renderer->uiTargets.at(30).target.initialized);
        const float left[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},eye[]{0,0,2};
        assert(fqvk::BeginWorldProbe(left,left,eye,eye));fqgl::state.texture2d[0].Change(30);
        Submit(1,24,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);fqvk::EndWorldProbe();
        const auto pixels=fqvk::renderer->Readback();
        for(int eyeIndex=0;eyeIndex<2;++eyeIndex) {
            Near(Pixel(pixels,eyeIndex,32,20,frame?0:1),1);
            Near(Pixel(pixels,eyeIndex,32,44,frame?1:0),1);
            Near(Pixel(pixels,eyeIndex,32,20,frame?1:0),0);
        }
    }
    fqvk::hooks.erase(30,true);assert(fqvk::renderer->uiTargets.empty());
}

void PostOverlay() {
    Scene(false);Program(31,9);
    const float ui[]{-.1f,-.1f,0,0,0,.1f,-.1f,0,0,0,.1f,.1f,0,0,0,-.1f,-.1f,0,0,0,.1f,.1f,0,0,0,-.1f,.1f,0,0,0};
    fqvk::Mesh(32,ui,6,5,nullptr,0);const uint8_t white[]{255,255,255,255};fqvk::Texture(33,1,1,white,false,false);
    Set(31,"uMvp",{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});Set(31,"uTint",{0,0,1,1});
    const float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},eye[]{0,0,2};
    assert(fqvk::BeginWorldProbe(identity,identity,eye,eye));fqgl::state.texture2d[0].Change(5);
    Submit(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
    fqvk::PostProcess({});assert(fqvk::renderer->postDone&&fqvk::renderer->passOpen);
    fqgl::state.texture2d[0].Change(33);fqgl::state.depth.Change(false);fqgl::state.blend.Change(false);
    Submit(31,32,GL_TRIANGLES,0,6,0,0,1);fqvk::EndWorldProbe();
    const auto pixels=fqvk::renderer->Readback();
    for(int eyeIndex=0;eyeIndex<2;++eyeIndex){Near(Pixel(pixels,eyeIndex,32,32,2),1);Near(Pixel(pixels,eyeIndex,32,32,0),0);assert(Pixel(pixels,eyeIndex,48,32,2)<.1f);}
}

void HeadlessSceneSubmission() {
    // Exercise the real shared frontend and program registration, without EGL
    // initialization or a GLES context. Original shaders execute through Vulkan.
    fqrender::nativeOnly=true;fqrender::driverCalls=0;fqgl::Reset();
    assert(fqvk::InitializeWorldProbe(reinterpret_cast<XrInstance>(uintptr_t(1)),1,64,64));
    const GLuint program=glCreateProgram();fqvk::Program(program,0);
    glUseProgram(program);
    auto scalar=[&](const char* name,float value){glUniform1f(glGetUniformLocation(program,name),value);};
    auto integer=[&](const char* name,int value){glUniform1i(glGetUniformLocation(program,name),value);};
    auto vector=[&](const char* name,float x,float y,float z){glUniform3f(glGetUniformLocation(program,name),x,y,z);};
    vector("uAmbientColor",1,1,1);vector("uSunlightColor",0,0,0);vector("uSunDirectionVertexQ1540",0,0,1);
    scalar("uFogNearVertexQ1532",10000);scalar("uFogFarVertexQ1532",10001);scalar("uFogPowerVertexQ1532",1);
    scalar("uNoLighting",1);scalar("uMaterialAlpha",1);scalar("uPipScreenGain",1);integer("uDiffuse",0);
    const float light[]{2,3,4,5};glUniform4fv(glGetUniformLocation(program,"uLocalLightPosRadius[1]"),1,light);
    bool checkedArray=false;
    for(const auto& field:fqvk::renderer->programs.at(program).fields)if(!std::strcmp(field.field.name,"uLocalLightPosRadius")) {
        const auto& bytes=fqvk::renderer->programs.at(program).bytes;float before=1,current=0,after=1;
        std::memcpy(&before,bytes.data()+field.field.offset,4);std::memcpy(&current,bytes.data()+field.field.offset+16,4);std::memcpy(&after,bytes.data()+field.field.offset+32,4);
        assert(before==0&&current==2&&after==0);checkedArray=true;
    }
    assert(checkedArray);
    GLuint vao=0,texture=0;glGenVertexArrays(1,&vao);glBindVertexArray(vao);
    auto quad=Quad(false);const uint32_t indices[]{0,1,2,0,2,3};fqvk::Mesh(vao,quad.data(),4,18,indices,6);
    glGenTextures(1,&texture);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);
    const uint8_t red[]{255,0,0,255};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,red);
    fqvk::Texture(texture,1,1,red);
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_TRUE);
    const float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},eye[]{0,0,2};
    assert(fqvk::BeginWorldProbe(identity,identity,eye,eye));
    glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,nullptr);
    assert(fqvk::renderer->pending.size()==1&&fqvk::renderer->draws==0);
    // Mutating live uniforms and CPU state after queueing cannot alter the draw.
    scalar("uMaterialAlpha",0);glDisable(GL_DEPTH_TEST);glBindTexture(GL_TEXTURE_2D,0);
    fqvk::EndWorldProbe();assert(fqvk::renderer->pending.empty());
    const auto pixels=fqvk::renderer->Readback();
    Near(Pixel(pixels,0,32,32,0),1);Near(Pixel(pixels,1,32,32,0),1);
    // The native renderer owns program uniforms; GLES and EGL never provided them.
    assert(fqrender::driverCalls==0);
    glDeleteTextures(1,&texture);glDeleteVertexArrays(1,&vao);glDeleteProgram(program);
    fqvk::ShutdownWorldProbe();assert(fqrender::driverCalls==0);
    fqrender::nativeOnly=false;fqgl::Reset();
    std::cout<<"Context-free scene submission: CPU handles, named uniforms, queued state snapshots, native original material output and zero GLES driver calls passed\n";
}

void SolidMaterialParity() {
    const auto xr=reinterpret_cast<XrInstance>(uintptr_t(1));
    const float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},eye[]{0,0,2};
    for(bool terrain:{false,true}) {
        assert(fqvk::InitializeWorldProbe(xr,1,64,64));Scene(terrain);
        auto& r=*fqvk::renderer;
        const uint8_t normal[]{128,128,255,128};fqvk::Texture(44,1,1,normal);fqgl::state.texture2d[1].Change(44);
        Set(1,"uSunlightColor",{.5f,.4f,.3f});Set(1,"uLocalLightCount",{1});
        Set(1,"uLocalLightPosRadius",{.2f,.1f,1,5});Set(1,"uLocalLightColorFalloff",{.1f,.2f,.3f,1});
        if(terrain){Set(1,"uUseNormal",{1});Set(1,"uTerrainSpecularEnabled",{1});}
        else {Set(1,"uNoLighting",{0});Set(1,"uSpecularEnabled",{1});Set(1,"uGlossiness",{16});}
        for(int guard=0;guard<(terrain?2:4);++guard) {
            Set(1,"uWaterReflectionClipEnabledQ2090",{guard==1?1.0f:0.0f});Set(1,"uWaterReflectionPlaneYQ2090",{.1f});
            if(!terrain) {
                Set(1,"uNativeLodClipEnabledQ1810",{guard==2?1.0f:0.0f});Set(1,"uNativeLodClipCellCountQ1900",{1});
                Set(1,"uNativeLodClipCellsQ1900",{-.2f,.2f,-.2f,.2f});
                Set(1,"uAlphaTest",{guard==3?1.0f:0.5f});Set(1,"uAlphaThreshold",{1.1f});
            }
            std::vector<uint16_t> reference;
            for(bool generic:{true,false}) {
                r.genericShaders=generic;
                assert(fqvk::BeginWorldProbe(identity,identity,eye,eye));
                fqgl::state.texture2d[0].Change(5);fqgl::state.depthWrite.Change(true);fqgl::state.blend.Change(false);
                Submit(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
                // Selection must use the queued uniform snapshot, not live state.
                if(guard==0)Set(1,"uWaterReflectionClipEnabledQ2090",{1});
                fqvk::EndWorldProbe();
                if(guard==0)Set(1,"uWaterReflectionClipEnabledQ2090",{0});
                const auto pixels=r.Readback();
                if(generic)reference=pixels;
                else {
                    assert(reference==pixels);
                    assert(r.solidDraws==uint64_t(guard==0)&&r.guardedDraws==uint64_t(guard!=0));
                    if(guard==1){Near(Pixel(pixels,0,32,44,0),0);assert(Pixel(pixels,0,32,20,0)>.1f);}
                    if(guard==2){Near(Pixel(pixels,0,32,32,0),0);assert(Pixel(pixels,0,20,32,0)>.1f);}
                    if(guard==3)Near(Pixel(pixels,0,32,32,0),0);
                }
            }
        }
        fqvk::ShutdownWorldProbe();
    }
    std::cout<<"Solid material variants: exact stereo pixels against generic shaders, original lighting, alpha cutouts, LOD and reflection clips passed\n";
}

void AsyncResources() {
    fqrender::nativeOnly=true;fqgl::Reset();
    const auto xr=reinterpret_cast<XrInstance>(uintptr_t(1));
    assert(fqvk::InitializeWorldProbe(xr,1,64,64));Scene(false);
    auto& r=*fqvk::renderer;
    assert(r.uploadOperations>=5&&r.uploadSubmissions<r.uploadOperations&&r.uploadQueueIdles==0);
    assert(r.uploadBytes==0&&r.uploadCompleted==r.uploadSerial);
    assert(r.stagingPoolBytes>0&&r.stagingPoolBytes<=r.stagingPoolLimit);
    assert(r.passTiming&&!r.gpuSpans.empty()&&r.timingDropped==0);
    for(double ms:r.passMs)assert(std::isfinite(ms)&&ms>=0);
    const float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},eye[]{0,0,2};
    // A cold large upload must not crowd out recurring small palette staging.
    for(auto& buffer:r.stagingPool)r.Destroy(buffer);
    r.stagingPool.clear();r.stagingPoolBytes=0;r.stagingPoolLimit=16384;r.stagingPoolCountLimit=4;
    auto cold=r.MakeStaging(16000);r.RecycleStaging(cold);
    const auto evictions=r.stagingEvictions;
    auto hot=r.MakeStaging(128);const auto hotHandle=hot.buffer;r.RecycleStaging(hot);
    assert(r.stagingEvictions==evictions+1&&r.stagingPoolBytes<=r.stagingPoolLimit);
    const auto allocations=r.stagingAllocations;
    for(int i=0;i<16;++i){auto reused=r.MakeStaging(128);assert(reused.buffer==hotHandle);r.RecycleStaging(reused);}
    assert(r.stagingAllocations==allocations);
    r.stagingPoolLimit=16u*1024u*1024u;r.stagingPoolCountLimit=512;
    // More than 128 palettes can retire together with two world frames in
    // flight. Preserve the entire hot burst instead of evicting/reallocating it.
    std::vector<fqvk::Buffer> burst;
    for(int i=0;i<384;++i)burst.push_back(r.MakeStaging(128));
    for(auto& buffer:burst)r.RecycleStaging(buffer);
    const auto burstAllocations=r.stagingAllocations,burstEvictions=r.stagingEvictions;
    for(int repeat=0;repeat<3;++repeat) {
        burst.clear();
        for(int i=0;i<384;++i){auto buffer=r.MakeStaging(128);std::memcpy(buffer.mapped,&i,sizeof(i));burst.push_back(buffer);}
        for(int i=0;i<384;++i){int actual=-1;std::memcpy(&actual,burst[i].mapped,sizeof(actual));assert(actual==i);r.RecycleStaging(burst[i]);}
        assert(r.stagingAllocations==burstAllocations&&r.stagingEvictions==burstEvictions);
        assert(r.stagingPoolBytes<=r.stagingPoolLimit&&r.stagingPool.size()<=r.stagingPoolCountLimit);
    }
    const auto allocated=r.descriptorAllocations,hits=r.descriptorHits;
    for(int frame=0;frame<3;++frame) {
        assert(fqvk::BeginWorldProbe(identity,identity,eye,eye));
        Submit(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);Submit(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
        fqvk::EndWorldProbe();
    }
    assert(r.descriptorAllocations==allocated&&r.descriptorHits>=hits+6);
    const uint8_t green[]{0,255,0,255},blue[]{0,0,255,255};
    const auto resetsBeforeReplacement=r.descriptorResets;
    fqvk::Texture(5,1,1,green);fqvk::Texture(5,1,1,blue); // replace an image still referenced by a pending upload
    assert(r.uploadBatch.operations==2&&!r.retiredImages.empty());
    const auto ticket=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);assert(ticket);
    assert(r.stagingReuses>=2);
    const auto serial=r.uploadSerial;assert(uintptr_t(ticket)==serial+1);
    const auto status=glClientWaitSync(ticket,0,0);assert(status==GL_ALREADY_SIGNALED||status==GL_TIMEOUT_EXPIRED);
    assert(fqvk::BeginWorldProbe(identity,identity,eye,eye));Submit(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);fqvk::EndWorldProbe();
    assert(glClientWaitSync(ticket,0,0)==GL_ALREADY_SIGNALED);glDeleteSync(ticket);
    assert(r.uploadBytes==0&&r.uploadStagingBuffers==0&&r.retiredImages.empty());
    const auto resets=r.descriptorResets;
    assert(fqvk::BeginWorldProbe(identity,identity,eye,eye));Submit(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);fqvk::EndWorldProbe();
    assert(r.descriptorResets>=resets&&r.descriptorResets>resetsBeforeReplacement&&r.descriptorRevision==r.viewRevision); // upload completion may allow collection before frame 5 or frame 6
    const auto pixels=r.Readback();Near(Pixel(pixels,0,32,32,2),1);Near(Pixel(pixels,1,32,32,2),1);
    // Exercise bounded staging/backpressure with rapidly replaced native images.
    r.uploadBatchLimit=1;r.uploadMemoryLimit=16;r.uploadBufferLimit=2;
    for(int i=0;i<32;++i)fqvk::Texture(5,1,1,i%2?green:blue);
    r.FlushUploads();assert(r.uploadBytes<=r.uploadMemoryLimit&&r.uploadStagingBuffers<=r.uploadBufferLimit&&r.uploadQueueIdles==0);
    assert(fqvk::BeginWorldProbe(identity,identity,eye,eye));Submit(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);fqvk::EndWorldProbe();
    const auto changed=r.Readback();Near(Pixel(changed,0,32,32,1),1);Near(Pixel(changed,1,32,32,1),1);
    assert(r.uploadBytes==0&&r.uploadStagingBuffers==0&&r.retiredImages.empty());
    fqvk::ShutdownWorldProbe();
    // Teardown before any frame must safely cancel unsubmitted uploads.
    assert(fqvk::InitializeWorldProbe(xr,1,64,64));assert(fqvk::renderer->uploadBatch.command);
    fqvk::ShutdownWorldProbe();fqrender::nativeOnly=false;fqgl::Reset();
    std::cout<<"Asynchronous native resources: batched uploads, real completion tickets, bounded staging, replacement/teardown lifetime, descriptor reuse/invalidation and updated pixels passed\n";
}

// Check actual swapchain bytes, not just the intermediate FP16 image. A
// linear 8-bit target rounded the first PC gamma-ramp shadow steps to black.
void HdrPresentation(VkFormat advertised,bool alsoSrgb) {
    fqrender::nativeOnly=true;fqgl::Reset();offeredColor=advertised;offerSrgb=alsoSrgb;
    const auto xr=reinterpret_cast<XrInstance>(uintptr_t(1));
    const auto session=reinterpret_cast<XrSession>(uintptr_t(2));
    assert(fqvk::InitializeWorldProbe(xr,1,64,64));Scene(false);
    assert(fqvk::CreateGameplaySwapchain(session));
    const VkFormat expected=alsoSrgb?VK_FORMAT_R8G8B8A8_SRGB:advertised;
    if(createdColor!=expected)throw std::runtime_error("HDR presentation did not select the advertised sRGB format");
    const bool srgb=expected==VK_FORMAT_R8G8B8A8_SRGB||expected==VK_FORMAT_B8G8R8A8_SRGB;
    auto quad=Quad(false);
    for(size_t i=0;i<4;++i){quad[i*18]/=.6f;quad[i*18+1]/=.6f;quad[i*18+12]=(quad[i*18]+1)*.5f;}
    const uint32_t indices[]{0,1,2,0,2,3};fqvk::Mesh(60,quad.data(),4,18,indices,6);
    uint8_t gradient[64*4]{};
    for(int i=0;i<64;++i){gradient[i*4]=gradient[i*4+1]=gradient[i*4+2]=uint8_t(i);gradient[i*4+3]=255;}
    fqvk::Texture(61,64,1,gradient,false,false);fqgl::state.texture2d[0].Change(61);
    const float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},eye[]{0,0,2};
    std::array<XrView,2> views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};
    for(auto& view:views){view.pose.orientation.w=1;view.fov={-.7f,.7f,.8f,-.8f};}
    std::array<XrCompositionLayerProjectionView,2> output{};
    auto encode=[](float x){return x<=.0031308f?x*12.92f:1.055f*std::pow(x,1/2.4f)-.055f;};
    for(bool loading:{false,true}) {
        assert(fqvk::BeginWorldProbe(identity,identity,eye,eye,loading));
        Submit(1,60,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
        if(!loading){fqvk::PostSettings settings;settings.interior=1;settings.brightScale=0;fqvk::PostProcess(settings);}
        assert(fqvk::PresentGameplay(views.data(),output.data(),.04f,1000));
        for(int e=0;e<2;++e)for(int x=0;x<64;++x) {
            const float linear=loading?x/255.0f:DisplayLinear(x/255.0f);
            const int code=int(std::lround((srgb?encode(linear):linear)*255));
            for(int c=0;c<3;++c) {
                const int actual=submittedPixels[(e*64*64+32*64+x)*4+c];
                if(std::abs(actual-code)>1)throw std::runtime_error("HDR swapchain pixel="+std::to_string(actual)+" expected="+std::to_string(code));
                if(srgb&&x>0&&x<8)assert(actual>0);
            }
        }
    }
    fqvk::DestroyGameplaySwapchain();fqvk::ShutdownWorldProbe();offerSrgb=false;offeredColor=VK_FORMAT_R8G8B8A8_UNORM;
    fqrender::nativeOnly=false;fqgl::Reset();
    std::cout<<"HDR swapchain: gamma-ramp shadow gradient, exact single transfer, both eyes, loading and format="<<expected<<" passed\n";
}

std::vector<uint8_t> FilmOverlayOutput(bool legacy,bool synchronous=false,bool modal=false) {
    fqrender::nativeOnly=true;fqgl::Reset();offerSrgb=true;
    const auto xr=reinterpret_cast<XrInstance>(uintptr_t(1));const auto session=reinterpret_cast<XrSession>(uintptr_t(2));
    assert(fqvk::InitializeWorldProbe(xr,1,64,64));auto& r=*fqvk::renderer;r.legacyPresentation=legacy;r.synchronous=synchronous;
    Scene(false);offerDepth=modal;inspectModalDepth=modal;
    assert(fqvk::CreateGameplaySwapchain(session,modal));Program(65,9);
    if(modal) {
        Program(70,11);std::vector<float> geometry;
        for(const auto& xy:std::array<std::array<float,2>,4>{{{{-.3f,-.3f}},{{.3f,-.3f}},{{-.3f,.3f}},{{.3f,.3f}}}})
            geometry.insert(geometry.end(),{xy[0],xy[1],.2f,0,0,1,0,0,1,1,1,1});
        fqvk::Mesh(71,geometry.data(),4,12,nullptr,0);
        for(size_t i=0;i<4;++i)geometry[i*12+2]=.4f;
        fqvk::Mesh(72,geometry.data(),4,12,nullptr,0);
        Set(70,"uMvp",{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});
        Set(70,"uUnlit",{1});Set(70,"uFade",{1});Set(70,"uCutoff",{0});Set(70,"uClipUi",{0});
    }
    const uint8_t colour[]{25,45,75,255};fqvk::Texture(5,1,1,colour,false,false);
    auto near=Quad(false);std::vector<float> ui;
    for(size_t i=0;i<4;++i)ui.insert(ui.end(),{near[i*18]*.6f,near[i*18+1]*.6f,-.2f,0,0});
    const uint32_t indices[]{0,1,2,0,2,3};fqvk::Mesh(66,ui.data(),4,5,indices,6);
    for(size_t i=0;i<4;++i)ui[i*5+2]=.2f;
    fqvk::Mesh(67,ui.data(),4,5,indices,6);
    const uint8_t white[]{255,255,255,255};fqvk::Texture(68,1,1,white,false,false);
    Set(65,"uMvp",{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});
    const float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},right[]{1,0,0,0,0,1,0,0,0,0,1,0,.4f,0,0,1},eye[]{0,0,2};
    std::array<XrView,2> views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};
    for(auto& view:views){view.pose.orientation.w=1;view.fov={-.7f,.7f,.8f,-.8f};}
    std::array<XrCompositionLayerProjectionView,2> output{};std::vector<uint8_t> result;
    for(int frame=0;frame<4;++frame) {
        if(frame==2){fqvk::DestroyGameplaySwapchain();assert(fqvk::CreateGameplaySwapchain(session,modal));}
        r.passTiming=(frame%2)==0;
        assert(fqvk::BeginWorldProbe(identity,right,eye,eye));
        fqgl::state.depth.Change(true);fqgl::state.depthWrite.Change(true);fqgl::state.blend.Change(false);
        fqgl::state.texture2d[0].Change(5);Submit(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
        fqvk::PostSettings settings;settings.brightScale=0;fqvk::PostProcess(settings);
        fqgl::state.texture2d[0].Change(68);fqgl::state.depthWrite.Change(false);fqgl::state.blend.Change(true);
        fqgl::state.srcRgb.Change(GL_SRC_ALPHA);fqgl::state.dstRgb.Change(GL_ONE_MINUS_SRC_ALPHA);
        fqgl::state.srcAlpha.Change(GL_ONE);fqgl::state.dstAlpha.Change(GL_ONE_MINUS_SRC_ALPHA);
        Set(65,"uTint",{.1f,.25f,.35f,.37f});Submit(65,66,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
        Set(65,"uTint",{.23f,.08f,.31f,.41f});Submit(65,66,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
        Set(65,"uTint",{1,0,1,1});Submit(65,67,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1); // occluded behind world depth
        if(modal) {
            assert(fqvk::BeginModalDepth());
            fqgl::state.depthWrite.Change(true);fqgl::state.blend.Change(false);
            Set(70,"uTint",{0,0,1,1});Submit(70,71,GL_TRIANGLE_STRIP,0,4,0,0,1);
            Set(70,"uTint",{0,1,0,1});Submit(70,72,GL_TRIANGLE_STRIP,0,4,0,0,1);
        }
        assert(fqvk::PresentGameplay(views.data(),output.data(),.04f,1000));
        if(modal) {
            // Both modal models sit behind the world. Blue is in front of
            // green: fresh modal depth must show blue in BOTH eyes, while
            // the compositor continues receiving the original world depth.
            for(int eye=0;eye<2;++eye) {
                const size_t at=(eye*64*64+32*64+(eye?45:32))*4;
                assert(submittedPixels[at]<2&&submittedPixels[at+1]<2&&submittedPixels[at+2]>253);
                assert(std::fabs(modalWorldDepth[eye*64*64+32*64+(eye?45:32)]-.5f)<.001f);
            }
        }
        assert(r.timingDropped==0&&r.passMs[r.Film]>=0&&r.passMs[r.Ui]>=0);
        assert(r.directOutput==!legacy&&r.colorAcquired==false);
        assert(bool(r.presented.image.image)==legacy);
        result.insert(result.end(),submittedPixels.begin(),submittedPixels.end());
    }
    assert(legacy?(r.presentationCopies==4&&r.directFilmFrames==0):(r.presentationCopies==0&&r.directFilmFrames==4));
    fqvk::DestroyGameplaySwapchain();fqvk::ShutdownWorldProbe();offerSrgb=false;fqrender::nativeOnly=false;fqgl::Reset();
    inspectModalDepth=offerDepth=false;
    return result;
}
void DirectPresentation() {
    const auto legacy=FilmOverlayOutput(true),direct=FilmOverlayOutput(false),synchronous=FilmOverlayOutput(false,true);
    const auto modalLegacy=FilmOverlayOutput(true,false,true),modalDirect=FilmOverlayOutput(false,false,true);
    assert(modalLegacy.size()==modalDirect.size());
    for(size_t i=0;i<modalLegacy.size();++i)assert(std::abs(int(modalLegacy[i])-int(modalDirect[i]))<=2);
    assert(direct==synchronous);assert(legacy.size()==direct.size());
    int largest=0;for(size_t i=0;i<legacy.size();++i)largest=std::max(largest,std::abs(int(legacy[i])-int(direct[i])));
    if(largest>2)throw std::runtime_error("direct film/overlay difference exceeds two display codes: "+std::to_string(largest));
    std::cout<<"Direct presentation: original film, translucent overlays, private modal depth over world, retained compositor depth, stereo, recreation and synchronous/buffered equality; max display-code difference="<<largest<<" passed\n";
}
void BufferedOwnership() {
    enableTimelineGate=true;
    fqrender::nativeOnly=true;offerSrgb=true;inspectReleasedImages=false;
    const auto xr=reinterpret_cast<XrInstance>(uintptr_t(1));const auto session=reinterpret_cast<XrSession>(uintptr_t(2));
    assert(fqvk::InitializeWorldProbe(xr,1,64,64));Scene(false);auto& r=*fqvk::renderer;
    assert(fqvk::CreateGameplaySwapchain(session));
    // Warm pipelines, descriptor variants and resident resources before the
    // gate. Cold driver compilation may itself block behind the GPU gate;
    // this test isolates the engine's steady-state submission/lifetime rules.
    Program(90,9);const float ui[]{-1,-1,0,0,0,1,-1,0,0,0,1,1,0,0,0,-1,-1,0,0,0,1,1,0,0,0,-1,1,0,0,0};
    fqvk::Mesh(91,ui,6,5,nullptr,0);const uint8_t white[]{255,255,255,255};fqvk::Texture(92,1,1,white,false,false);
    Set(90,"uMvp",{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});Set(90,"uTint",{1,0,0,1});
    {fqvk::UiTargetScope target(99,32,16);assert(target.active);fqgl::state.texture2d[0].Change(92);Submit(90,91,GL_TRIANGLES,0,6,0,0,1);}
    const uint8_t green[]{0,255,0,255};fqvk::Texture(6,1,1,green);
    auto moved=Quad(false);for(size_t i=0;i<4;++i)moved[i*18+1]+=.2f;
    const uint32_t indices[]{0,1,2,0,2,3};fqvk::Mesh(94,moved.data(),4,18,indices,6);
    auto panel=Quad(false);for(size_t i=0;i<4;++i){panel[i*18]=panel[i*18]*.3f-.7f;panel[i*18+1]=panel[i*18+1]*.3f+.7f;}
    fqvk::Mesh(93,panel.data(),4,18,indices,6);
    const float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},right[]{1,0,0,0,0,1,0,0,0,0,1,0,.8f,0,0,1},eye[]{0,0,2};
    std::array<XrView,2> views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};for(auto& view:views){view.pose.orientation.w=1;view.fov={-.7f,.7f,.8f,-.8f};}
    std::array<XrCompositionLayerProjectionView,2> output{};fqvk::PostSettings settings;settings.interior=1;settings.brightScale=0;
    for(int warm=0;warm<2;++warm) {
        assert(fqvk::BeginWorldProbe(identity,warm?right:identity,eye,eye));
        fqgl::state.texture2d[0].Change(warm?6:5);Submit(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
        if(warm){fqgl::state.texture2d[0].Change(99);Submit(1,93,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);}
        fqvk::PostProcess(settings);assert(fqvk::PresentGameplay(views.data(),output.data(),.04f,1000));
    }
    r.WaitFrames();
    // A timeline wait is legal before the host signals it. Unlike vkSetEvent,
    // host signalling does not require the waiting submission to be non-pending.
    VkSemaphore gate{};VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};type.semaphoreType=VK_SEMAPHORE_TYPE_TIMELINE;
    VkSemaphoreCreateInfo ci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};ci.pNext=&type;
    assert(vkCreateSemaphore(r.device,&ci,nullptr,&gate)==VK_SUCCESS);
    const auto signal=reinterpret_cast<PFN_vkSignalSemaphoreKHR>(vkGetDeviceProcAddr(r.device,"vkSignalSemaphoreKHR"));assert(signal);
    const uint64_t value=1;const VkPipelineStageFlags stage=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkTimelineSemaphoreSubmitInfo timeline{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};timeline.waitSemaphoreValueCount=1;timeline.pWaitSemaphoreValues=&value;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.pNext=&timeline;submit.waitSemaphoreCount=1;submit.pWaitSemaphores=&gate;submit.pWaitDstStageMask=&stage;
    assert(vkQueueSubmit(r.queue,1,&submit,VK_NULL_HANDLE)==VK_SUCCESS);
    std::mutex mutex;std::condition_variable ready;bool queued=false,timedOut=false;
    std::thread watchdog([&]{std::unique_lock<std::mutex> lock(mutex);timedOut=!ready.wait_for(lock,std::chrono::seconds(10),[&]{return queued;});
        VkSemaphoreSignalInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO};info.semaphore=gate;info.value=value;assert(signal(r.device,&info)==VK_SUCCESS);});
    std::array<uint32_t,2> images{};VkBuffer firstArena{},firstStereo{};VkDescriptorPool firstPool{};
    for(int frame=0;frame<2;++frame) {
        if(frame) {
            r.Retire(r.textures.at(5));r.textures.erase(5);
            r.Retire(r.meshes.at(9).vertices);r.meshes.at(9).vertices=r.meshes.at(94).vertices;r.meshes.at(94).vertices={};
            assert(!r.retiredImages.empty()&&!r.retiredBuffers.empty());
            Set(90,"uTint",{0,1,0,1});
            {fqvk::UiTargetScope uiTarget(99,32,16);assert(uiTarget.active&&r.activeSlot==2);
                fqgl::state.texture2d[0].Change(92);Submit(90,91,GL_TRIANGLES,0,6,0,0,1);}
        }
        assert(fqvk::BeginWorldProbe(identity,frame?right:identity,eye,eye));
        if(!frame){firstArena=r.arena.buffer;firstStereo=r.stereo.buffer;firstPool=r.descriptors;}
        else assert(firstArena!=r.arena.buffer&&firstStereo!=r.stereo.buffer&&firstPool!=r.descriptors);
        fqgl::state.texture2d[0].Change(frame?6:5);Submit(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
        if(frame){fqgl::state.texture2d[0].Change(99);Submit(1,93,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);}
        fqvk::PostProcess(settings);assert(fqvk::PresentGameplay(views.data(),output.data(),.04f,1000));images[frame]=acquiredImage;
        assert(vkGetFenceStatus(r.device,r.fence)==VK_NOT_READY&&r.finishWaitUs==0);
    }
    assert(r.frameSlots[0].pending&&r.frameSlots[1].pending&&r.frameSlots[2].pending);
    {std::lock_guard<std::mutex> lock(mutex);queued=true;}ready.notify_one();watchdog.join();assert(!timedOut);
    r.WaitFrames();r.Collect();assert(r.retiredImages.empty()&&r.retiredBuffers.empty());
    const auto first=RuntimePixels(images[0]),second=RuntimePixels(images[1]);
    auto pixel=[](const auto& data,int e,int x,int y,int c){return data[(e*64*64+y*64+x)*4+c];};
    assert(pixel(first,0,32,32,0)==255&&pixel(first,1,32,32,0)==255&&pixel(first,0,32,48,0)==255);
    assert(pixel(second,0,32,32,1)==255&&pixel(second,1,55,32,1)==255&&pixel(second,1,32,32,1)==0);
    assert(pixel(second,0,32,48,1)==0&&pixel(second,0,9,9,1)==255); // moved geometry and queued UI sampling
    vkDestroySemaphore(r.device,gate,nullptr);
    const auto resets=r.descriptorResets;
    assert(fqvk::BeginWorldProbe(identity,identity,eye,eye));assert(r.descriptorResets>resets);
    fqgl::state.texture2d[0].Change(6);Submit(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);fqvk::PostProcess(settings);
    assert(fqvk::PresentGameplay(views.data(),output.data(),.04f,1000));
    fqvk::DestroyGameplaySwapchain();fqvk::ShutdownWorldProbe();offerSrgb=false;inspectReleasedImages=true;fqrender::nativeOnly=false;fqgl::Reset();
    enableTimelineGate=false;
    std::cout<<"Buffered ownership: GPU held pending while two world slots and UI submit/release; immutable uniforms, retirement, ordered UI sampling and delayed pixels passed\n";
}

void EarlyAcquireFailure() {
    fqrender::nativeOnly=true;offerSrgb=true;
    const auto xr=reinterpret_cast<XrInstance>(uintptr_t(1));
    assert(fqvk::InitializeWorldProbe(xr,1,64,64));Scene(false);
    assert(fqvk::CreateGameplaySwapchain(reinterpret_cast<XrSession>(uintptr_t(2))));
    const float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},eye[]{0,0,2};
    assert(fqvk::BeginWorldProbe(identity,identity,eye,eye));
    Submit(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);timeoutWait=true;fqvk::PostProcess({});
    assert(fqvk::renderer->failed&&acquired&&!waited&&!fqvk::WorldProbeActive());
    fqvk::DestroyGameplaySwapchain();fqvk::ShutdownWorldProbe();timeoutWait=false;offerSrgb=false;fqrender::nativeOnly=false;fqgl::Reset();
}
void TimingCapacity() {
    assert(fqvk::InitializeWorldProbe(reinterpret_cast<XrInstance>(uintptr_t(1)),1,64,64));Scene(false);
    const float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},eye[]{0,0,2};
    auto& r=*fqvk::renderer;assert(fqvk::BeginWorldProbe(identity,identity,eye,eye));
    Submit(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);r.DrainDraws();
    bool insideRejected=false;try{r.Phase(r.Ui);}catch(const std::runtime_error&){insideRejected=true;}assert(insideRejected);
    r.queryCursor=r.QueryCount;
    r.EndPass();r.Phase(r.Ui);fqvk::EndWorldProbe();assert(r.timingDropped>0);
    vkDestroyQueryPool(r.device,r.timestamps,nullptr);r.timestamps={};r.timestampSupported=false;
    assert(fqvk::BeginWorldProbe(identity,identity,eye,eye));Submit(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
    fqvk::EndWorldProbe();assert(r.gpuSpans.empty()&&r.timingDropped==0);
    fqvk::ShutdownWorldProbe();
}

void GameplayPresentation(bool withDepth) {
    fqrender::nativeOnly=true;fqgl::Reset();
    const auto xr=reinterpret_cast<XrInstance>(uintptr_t(1));const auto session=reinterpret_cast<XrSession>(uintptr_t(2));
    assert(fqvk::InitializeWorldProbe(xr,1,64,64));Scene(false);
    XrGraphicsBindingVulkan2KHR binding{};assert(fqvk::GameplayBinding(binding));
    assert(binding.type==XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR&&binding.device==fqvk::renderer->device&&binding.queueIndex==0);
    offerDepth=withDepth;assert(fqvk::CreateGameplaySwapchain(session,true));
    std::array<XrView,2> views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};views[0].pose.orientation.w=views[1].pose.orientation.w=1;
    views[1].pose.position.x=.064f;views[0].fov={-.7f,.7f,.8f,-.8f};views[1].fov=views[0].fov;
    std::array<XrCompositionLayerProjectionView,2> output{};
    const float left[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const float right[]{1,0,0,0,0,1,0,0,0,0,1,0,.8f,0,0,1},eye[]{0,0,2};
    auto upper=Quad(false);for(size_t i=0;i<4;++i)upper[i*18+1]=upper[i*18+1]*.5f+.5f;
    const uint32_t indices[]{0,1,2,0,2,3};fqvk::Mesh(40,upper.data(),4,18,indices,6);
    const uint32_t initialReleases=releases,initialDepthReleases=depthReleases;
    for(int frame=0;frame<6;++frame) {
        const bool loading=frame<3;
        // A valid exterior/interior horizon must never colour the loading void.
        const float horizon[]{.35f,.6f,.8f};
        assert(fqvk::BeginWorldProbe(left,right,eye,eye,loading,loading?horizon:nullptr));fqgl::state.texture2d[0].Change(5);
        Submit(1,40,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
        if(!loading)fqvk::PostProcess({});
        assert(fqvk::PresentGameplay(views.data(),output.data(),.04f,125000.0f/70.0f));
        assert(releases==initialReleases+frame+1&&!acquired&&!fqvk::recording);
        for(uint32_t e=0;e<2;++e) {
            assert(output[e].subImage.swapchain==fqvk::renderer->xrSwapchain&&output[e].subImage.imageArrayIndex==e);
            assert(output[e].subImage.imageRect.extent.width==64);
            if(loading||!withDepth)assert(!output[e].next);
            else {
                const auto* depth=static_cast<const XrCompositionLayerDepthInfoKHR*>(output[e].next);
                assert(depth&&depth->subImage.swapchain==fqvk::renderer->xrDepthSwapchain&&depth->subImage.imageArrayIndex==e);
                assert(depth->minDepth==0&&depth->maxDepth==1&&depth->nearZ==.04f&&depth->farZ==125000.0f/70.0f);
            }
            assert(output[e].pose.position.x==views[e].pose.position.x&&output[e].fov.angleUp==views[e].fov.angleUp);
        }
        auto pixel=[&](int e,int x,int y){return submittedPixels[(e*64*64+y*64+x)*4];};
        assert(pixel(0,32,16)>100&&pixel(1,55,16)>100);
        assert(pixel(1,32,16)<5&&pixel(0,32,48)<5); // stereo separation + upright orientation
        if(loading)for(int e=0;e<2;++e)for(int c=0;c<3;++c)
            assert(submittedPixels[(e*64*64+48*64+32)*4+c]==0);

    }
    assert(depthReleases-initialDepthReleases==(withDepth?3u:0u)); // Loading never submits world depth.
    // Runtime suspension can retain the resources; rebuilding swapchains keeps resident NIF assets.
    const auto mesh=fqvk::renderer->meshes.at(40).vertices.buffer;
    fqvk::DestroyGameplaySwapchain();assert(runtimeImages.empty());
    assert(fqvk::CreateGameplaySwapchain(session,true));assert(fqvk::renderer->meshes.at(40).vertices.buffer==mesh);
    assert(fqvk::BeginWorldProbe(left,right,eye,eye,true));
    timeoutWait=true;const auto before=releases;
    assert(!fqvk::PresentGameplay(views.data(),output.data(),.04f,125000.0f/70.0f));
    assert(acquired&&!waited&&releases==before&&!fqvk::WorldProbeActive());
    // A failed Vulkan session retains its device until session teardown. No GLES fallback.
    assert(fqvk::renderer&&fqvk::renderer->device==binding.device);
    fqvk::DestroyGameplaySwapchain();fqvk::ShutdownWorldProbe();timeoutWait=false;
    assert(fqvk::InitializeWorldProbe(xr,1,64,64));unsupportedFormat=true;
    assert(!fqvk::CreateGameplaySwapchain(session));assert(runtimeImages.empty());
    fqvk::ShutdownWorldProbe();unsupportedFormat=false;offerDepth=false;fqrender::nativeOnly=false;fqgl::Reset();
    std::cout<<"Vulkan compositor software test: layered UNORM output, image rotation, upright stereo, film output, submit-before-release, independent depth indices, D32 copies/metadata, projection views, recreation, timeout and format rejection passed\n";
}

int main() {
    for(auto format:{VK_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_B8G8R8A8_UNORM,VK_FORMAT_B8G8R8A8_SRGB})HdrPresentation(format,false);
    HdrPresentation(VK_FORMAT_R8G8B8A8_UNORM,true);
    BufferedOwnership();DirectPresentation();EarlyAcquireFailure();TimingCapacity();
    SolidMaterialParity();AsyncResources();HeadlessSceneSubmission();
    GameplayPresentation(false);GameplayPresentation(true);
    for(bool asynchronous:{false,true}) {
    fqrender::nativeOnly=asynchronous;
    for(int mode=0;mode<4;++mode) {
        assert(fqvk::InitializeWorldProbe(reinterpret_cast<XrInstance>(uintptr_t(1)),1,64,64));
        Scene(mode==1,mode==2,mode==3,true);fqvk::ShutdownWorldProbe();assert(!fqvk::WorldProbeActive());
    }
    assert(fqvk::InitializeWorldProbe(reinterpret_cast<XrInstance>(uintptr_t(1)),1,64,64));AuthoredCulling();fqvk::ShutdownWorldProbe();
    assert(fqvk::InitializeWorldProbe(reinterpret_cast<XrInstance>(uintptr_t(1)),1,64,64));Sky();fqvk::ShutdownWorldProbe();
    assert(fqvk::InitializeWorldProbe(reinterpret_cast<XrInstance>(uintptr_t(1)),1,64,64));Water();fqvk::ShutdownWorldProbe();
    assert(fqvk::InitializeWorldProbe(reinterpret_cast<XrInstance>(uintptr_t(1)),1,64,64));Post();fqvk::ShutdownWorldProbe();
    for(bool animated:{false,true}){assert(fqvk::InitializeWorldProbe(reinterpret_cast<XrInstance>(uintptr_t(1)),1,64,64));Ui(animated);fqvk::ShutdownWorldProbe();}
    assert(fqvk::InitializeWorldProbe(reinterpret_cast<XrInstance>(uintptr_t(1)),1,64,64));Loading();fqvk::ShutdownWorldProbe();
    assert(fqvk::InitializeWorldProbe(reinterpret_cast<XrInstance>(uintptr_t(1)),1,64,64));PipboyTarget();fqvk::ShutdownWorldProbe();
    assert(fqvk::InitializeWorldProbe(reinterpret_cast<XrInstance>(uintptr_t(1)),1,64,64));PostOverlay();fqvk::ShutdownWorldProbe();
    }
    fqrender::nativeOnly=false;fqgl::Reset();
    std::cout<<"Vulkan native multiview: original world/LAND shaders, indexed NIF layout, per-eye cameras, depth equality, ordered paint blending, layered water refraction, retained opaque depth, original exposure/bloom/film/gamma, transformed UI/notification alpha, loading clipping/depth, Pip-Boy RTT sampling/orientation and recreation passed\n";
}
