struct DensityPasses {VkRenderPass clear{},load{};VkFramebuffer framebuffer{};};
DensityPasses densityMain,densitySplit;
Image densityMap;
bool foveationSupported=false,densityActive=false,sidecarPass=false;
#ifdef __ANDROID__
bool foveationRequested=true;
#else
bool foveationRequested=false; // Software parity tests keep a full-rate reference.
#endif
void DestroyDensityPasses(DensityPasses& p) {
    if(p.framebuffer)vkDestroyFramebuffer(device,p.framebuffer,nullptr);
    if(p.clear)vkDestroyRenderPass(device,p.clear,nullptr);
    if(p.load)vkDestroyRenderPass(device,p.load,nullptr);
    p={};
}
void CreateDensityMap() {
    VkFormatProperties format{};vkGetPhysicalDeviceFormatProperties(physical,VK_FORMAT_R8G8_UNORM,&format);
    if(!(format.optimalTilingFeatures&VK_FORMAT_FEATURE_FRAGMENT_DENSITY_MAP_BIT_EXT))throw std::runtime_error("R8G8 density format unsupported");
    VkPhysicalDeviceFragmentDensityMapPropertiesEXT density{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT};
    VkPhysicalDeviceProperties2 props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};props.pNext=&density;vkGetPhysicalDeviceProperties2(physical,&props);
    const auto tile=density.maxFragmentDensityTexelSize;
    if(!tile.width||!tile.height)throw std::runtime_error("invalid density texel size");
    const uint32_t w=(width+tile.width-1)/tile.width,h=(height+tile.height-1)/tile.height;
    auto pixels=DensityPixels(w,h);Buffer staging=MakeStaging(pixels.size());
    try {
        std::memcpy(staging.mapped,pixels.data(),pixels.size());
        densityMap=MakeImage(w,h,VK_FORMAT_R8G8_UNORM,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT,VK_IMAGE_ASPECT_COLOR_BIT);
        // Static map: complete the upload before vkCmdBeginRenderPass may read it.
        Transfer([&](VkCommandBuffer cb){
            VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.image=densityMap.image;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,2};
            b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
            VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,2};copy.imageExtent={w,h,1};vkCmdCopyBufferToImage(cb,staging.buffer,b.image,b.newLayout,1,&copy);
            b.oldLayout=b.newLayout;b.newLayout=VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT;
            vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT,0,0,nullptr,0,nullptr,1,&b);
        });
        RecycleStaging(staging);
        VKLOG("FQVK FOVEATION mapCreated=1 map=%ux%u layers=2 centreRadius=0.75 middle=2x2 corners=4x4 nonSubsampled=1 hud=full motion=full",w,h);
    }catch(...){Destroy(staging);Destroy(densityMap);throw;}
}
void CreateDensityPasses(bool split) {
    if(!densityMap.image)return;
    auto& target=split?densitySplit:densityMain;
    const unsigned n=split?3:2;
    std::vector<VkAttachmentDescription> a(n+1);
    a[0].format=VK_FORMAT_R16G16B16A16_SFLOAT;a[1].format=VK_FORMAT_D32_SFLOAT;
    if(split)a[2].format=a[0].format;
    for(unsigned i=0;i<n;++i){a[i].samples=VK_SAMPLE_COUNT_1_BIT;a[i].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;a[i].storeOp=VK_ATTACHMENT_STORE_OP_STORE;a[i].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;a[i].stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;a[i].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;a[i].finalLayout=i==1?VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;}
    auto& map=a[n];map.format=VK_FORMAT_R8G8_UNORM;map.samples=VK_SAMPLE_COUNT_1_BIT;map.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;map.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;map.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;map.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;map.initialLayout=map.finalLayout=VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
    const VkAttachmentReference refs[]{{0,a[0].finalLayout},{2,a[0].finalLayout}},d{1,a[1].finalLayout};
    VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=split?2:1;sub.pColorAttachments=refs;sub.pDepthStencilAttachment=&d;
    VkRenderPassFragmentDensityMapCreateInfoEXT density{VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT};density.fragmentDensityMapAttachment={n,map.finalLayout};
    const uint32_t mask=3;VkRenderPassMultiviewCreateInfo mv{VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO};mv.pNext=&density;mv.subpassCount=1;mv.pViewMasks=&mask;mv.correlationMaskCount=1;mv.pCorrelationMasks=&mask;
    VkSubpassDependency dep{};dep.srcSubpass=VK_SUBPASS_EXTERNAL;dep.dstSubpass=0;dep.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;dep.dstStageMask=dep.srcStageMask|VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT;dep.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;dep.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT|VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT;
    VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};ci.pNext=&mv;ci.attachmentCount=uint32_t(a.size());ci.pAttachments=a.data();ci.subpassCount=1;ci.pSubpasses=&sub;ci.dependencyCount=1;ci.pDependencies=&dep;
    Check(vkCreateRenderPass(device,&ci,nullptr,&target.clear),"foveated scene pass");
    for(unsigned i=0;i<n;++i){a[i].loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;a[i].initialLayout=a[i].finalLayout;}
    Check(vkCreateRenderPass(device,&ci,nullptr,&target.load),"foveated scene continuation");
    std::vector<VkImageView> views{color.view,depth.view};if(split){views.push_back(splitColour.view);}views.push_back(densityMap.view);
    VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fi.renderPass=target.clear;fi.attachmentCount=uint32_t(views.size());fi.pAttachments=views.data();fi.width=width;fi.height=height;fi.layers=1;
    Check(vkCreateFramebuffer(device,&fi,nullptr,&target.framebuffer),"foveated scene framebuffer");
}
void FullRateScene() {
    if(offscreen||postDone||(!densityActive&&!(splitFrame&&!worldDepthReady)))return;
    EndPass();RetainWorldDepth();densityActive=sidecarPass=false;
    VkRenderPassBeginInfo bi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};bi.renderPass=resumePass;bi.framebuffer=framebuffer;bi.renderArea.extent={width,height};BeginPass(SceneResume,bi);
}
// An uncommon world draw after UI still gets correct opaque sidecar coverage.
void ResumeSidecar(){
    EndPass();sidecarPass=true;
    VkRenderPassBeginInfo bi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};bi.renderPass=splitResumePass;bi.framebuffer=splitFramebuffer;bi.renderArea.extent={width,height};BeginPass(SceneResume,bi);
}
