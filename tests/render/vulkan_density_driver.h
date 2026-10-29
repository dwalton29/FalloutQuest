#pragma once
#include <vulkan/vulkan.h>
#include <unordered_set>
#include <cassert>
#include <vector>
// Lavapipe has no FDM. Inspect the production FDM API descriptions, then strip
// only the density attachment so real GPU tests can exercise pass transitions,
// pipelines, water LOAD, stereo HUD and teardown. No reduced-rate claim here.
inline bool emulateDensity=false;
inline unsigned densityDescriptions=0;
inline std::unordered_set<VkRenderPass> densityPassHandles;
inline VkResult DensityCreateRenderPass(VkDevice device,const VkRenderPassCreateInfo* info,const VkAllocationCallbacks* allocator,VkRenderPass* output){
    if(!emulateDensity)return vkCreateRenderPass(device,info,allocator,output);
    const VkRenderPassMultiviewCreateInfo* mv=nullptr;const VkRenderPassFragmentDensityMapCreateInfoEXT* density=nullptr;
    for(auto* p=static_cast<const VkBaseInStructure*>(info->pNext);p;p=p->pNext){
        if(p->sType==VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO)mv=reinterpret_cast<const VkRenderPassMultiviewCreateInfo*>(p);
        if(p->sType==VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT)density=reinterpret_cast<const VkRenderPassFragmentDensityMapCreateInfoEXT*>(p);
    }
    if(!density)return vkCreateRenderPass(device,info,allocator,output);
    assert(mv&&mv->pViewMasks[0]==3&&density->fragmentDensityMapAttachment.attachment+1==info->attachmentCount);
    const auto& a=info->pAttachments[info->attachmentCount-1];assert(a.format==VK_FORMAT_R8G8_UNORM&&a.loadOp==VK_ATTACHMENT_LOAD_OP_LOAD&&a.storeOp==VK_ATTACHMENT_STORE_OP_DONT_CARE&&a.initialLayout==VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT&&a.finalLayout==a.initialLayout);
    auto copy=*info;auto stereo=*mv;stereo.pNext=nullptr;copy.pNext=&stereo;--copy.attachmentCount;
    std::vector<VkSubpassDependency> dependencies(info->pDependencies,info->pDependencies+info->dependencyCount);
    for(auto& d:dependencies){assert(d.dstStageMask&VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT);assert(d.dstAccessMask&VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT);d.dstStageMask&=~VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT;d.dstAccessMask&=~VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT;}copy.pDependencies=dependencies.data();
    const auto result=vkCreateRenderPass(device,&copy,allocator,output);if(result==VK_SUCCESS){densityPassHandles.insert(*output);++densityDescriptions;}return result;
}
inline VkResult DensityCreateFramebuffer(VkDevice device,const VkFramebufferCreateInfo* info,const VkAllocationCallbacks* allocator,VkFramebuffer* output){
    auto copy=*info;if(densityPassHandles.count(info->renderPass))--copy.attachmentCount;return vkCreateFramebuffer(device,&copy,allocator,output);
}
inline void DensityDestroyRenderPass(VkDevice device,VkRenderPass pass,const VkAllocationCallbacks* allocator){densityPassHandles.erase(pass);vkDestroyRenderPass(device,pass,allocator);}
#define vkCreateRenderPass DensityCreateRenderPass
#define vkCreateFramebuffer DensityCreateFramebuffer
#define vkDestroyRenderPass DensityDestroyRenderPass
