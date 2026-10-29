// The original complete image is preserved. A second projection layer carries
// pixels changed by transparent world materials/water/UI; only the opaque
// projection receives frame-synthesis metadata. Both use the original film.
bool independentBlendAvailable=false,splitFrame=false,compositionPresented=false;
VkRenderPass splitPass{},splitResumePass{},compositionPass{};
VkFramebuffer splitFramebuffer{};
Image splitColour,splitDepth;
VkShaderModule splitModules[5]{},compositionModule{};
VkPipeline compositionPipeline{};
XrSwapchain compositionChain=XR_NULL_HANDLE;
std::vector<VkImageView> compositionViews;
std::vector<VkFramebuffer> compositionFrames;
uint32_t compositionIndex=0;
bool compositionAcquired=false,compositionHasDepth=false;
PostValues splitPostValues;
PostTarget compactFinal;
bool compactFrame=false;
PostTarget& Finished(){return compactFrame?compactFinal:presented;}
void CreateCompactFinal(){
    if(compactFinal.image.image)return;
    compactFinal.width=width;compactFinal.height=height;
    compactFinal.image=MakeImage(width,height,xrColorFormat,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
    VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fi.renderPass=xrPass;fi.attachmentCount=1;fi.pAttachments=&compactFinal.image.view;fi.width=width;fi.height=height;fi.layers=1;
    Check(vkCreateFramebuffer(device,&fi,nullptr,&compactFinal.framebuffer),"compact film framebuffer");
    const VkImageView views[]{compactFinal.image.view,depth.view};fi.renderPass=xrOverlayPass;fi.attachmentCount=2;fi.pAttachments=views;
    Check(vkCreateFramebuffer(device,&fi,nullptr,&compactFinal.overlay),"compact HUD framebuffer");
}

void DestroyCompositionSwapchain() {
    if(!device)return;
    if(compactFinal.overlay)vkDestroyFramebuffer(device,compactFinal.overlay,nullptr);
    compactFinal.overlay={};DestroyPost(compactFinal);
    for(auto fb:compositionFrames)if(fb)vkDestroyFramebuffer(device,fb,nullptr);
    compositionFrames.clear();
    for(auto view:compositionViews)if(view)vkDestroyImageView(device,view,nullptr);
    compositionViews.clear();
    if(compositionPipeline)vkDestroyPipeline(device,compositionPipeline,nullptr);
    compositionPipeline={};
    if(compositionPass)vkDestroyRenderPass(device,compositionPass,nullptr);
    compositionPass={};
    if(compositionChain)Proc<PFN_xrDestroySwapchain>(xrInstance,"xrDestroySwapchain")(compositionChain);
    compositionChain=XR_NULL_HANDLE;compositionAcquired=compositionPresented=false;
    fqmotion::separatedComposition=false;
}
void DestroyComposition() {
    DestroyCompositionSwapchain();
    DestroyDensityPasses(densitySplit);
    if(splitFramebuffer)vkDestroyFramebuffer(device,splitFramebuffer,nullptr);
    splitFramebuffer={};
    if(splitPass)vkDestroyRenderPass(device,splitPass,nullptr);
    splitPass={};
    if(splitResumePass)vkDestroyRenderPass(device,splitResumePass,nullptr);
    splitResumePass={};
    Destroy(splitColour);Destroy(splitDepth);
    for(auto& module:splitModules){if(module)vkDestroyShaderModule(device,module,nullptr);module={};}
    if(compositionModule)vkDestroyShaderModule(device,compositionModule,nullptr);
    compositionModule={};
}
void CreateCompositionSwapchain() {
    if(!xrSwapchain||!splitPass)return;
    XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};info.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;info.format=xrColorFormat;
    info.sampleCount=info.faceCount=info.mipCount=1;info.arraySize=2;info.width=width;info.height=height;
    CheckXr(Proc<PFN_xrCreateSwapchain>(xrInstance,"xrCreateSwapchain")(xrSession,&info,&compositionChain),"create native effects layer");
    const auto enumerate=Proc<PFN_xrEnumerateSwapchainImages>(xrInstance,"xrEnumerateSwapchainImages");uint32_t count=0;
    CheckXr(enumerate(compositionChain,0,&count,nullptr),"effects image count");if(!count)throw std::runtime_error("empty effects swapchain");
    std::vector<XrSwapchainImageVulkan2KHR> images(count,{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
    CheckXr(enumerate(compositionChain,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())),"effects images");
    for(auto image:images){VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=image.image;vi.viewType=VK_IMAGE_VIEW_TYPE_2D_ARRAY;vi.format=xrColorFormat;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,2};VkImageView view{};Check(vkCreateImageView(device,&vi,nullptr,&view),"effects view");compositionViews.push_back(view);}
    std::array<VkAttachmentDescription,2> attachments{};
    for(auto& a:attachments){a.format=xrColorFormat;a.samples=VK_SAMPLE_COUNT_1_BIT;a.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;a.storeOp=VK_ATTACHMENT_STORE_OP_STORE;a.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;a.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;a.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;a.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;}
    const VkAttachmentReference refs[]{{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},{1,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};
    VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=2;sub.pColorAttachments=refs;
    const uint32_t mask=3;VkRenderPassMultiviewCreateInfo mv{VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO};mv.subpassCount=1;mv.pViewMasks=&mask;mv.correlationMaskCount=1;mv.pCorrelationMasks=&mask;
    VkSubpassDependency dep{};dep.srcSubpass=VK_SUBPASS_EXTERNAL;dep.dstSubpass=0;dep.srcStageMask=dep.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;dep.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo pi{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};pi.pNext=&mv;pi.attachmentCount=2;pi.pAttachments=attachments.data();pi.subpassCount=1;pi.pSubpasses=&sub;pi.dependencyCount=1;pi.pDependencies=&dep;
    Check(vkCreateRenderPass(device,&pi,nullptr,&compositionPass),"effects composition pass");
    for(auto& base:xrTargets)for(auto overlay:compositionViews){VkImageView views[]{base.image.view,overlay};VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fi.renderPass=compositionPass;fi.attachmentCount=2;fi.pAttachments=views;fi.width=width;fi.height=height;fi.layers=1;VkFramebuffer fb{};Check(vkCreateFramebuffer(device,&fi,nullptr,&fb),"effects pair framebuffer");compositionFrames.push_back(fb);}
    std::array<VkPipelineShaderStageCreateInfo,2> stages{};
    for(unsigned i=0;i<2;++i){stages[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stages[i].stage=i?VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_VERTEX_BIT;stages[i].module=i?compositionModule:modules[8][0];stages[i].pName="main";}
    VkPipelineVertexInputStateCreateInfo vertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=vp.scissorCount=1;
    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.polygonMode=VK_POLYGON_MODE_FILL;rs.lineWidth=1;
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
    std::array<VkPipelineColorBlendAttachmentState,2> blendAttachments{};for(auto& a:blendAttachments)a.colorWriteMask=0xf;
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};blend.attachmentCount=2;blend.pAttachments=blendAttachments.data();
    const VkDynamicState states[]{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};ds.dynamicStateCount=2;ds.pDynamicStates=states;
    const uint32_t compact=xrSrgb?1:0;const VkSpecializationMapEntry entry{0,0,4};const VkSpecializationInfo specialization{1,&entry,4,&compact};stages[1].pSpecializationInfo=&specialization;
    VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};ci.stageCount=2;ci.pStages=stages.data();ci.pVertexInputState=&vertex;ci.pInputAssemblyState=&ia;ci.pViewportState=&vp;ci.pRasterizationState=&rs;ci.pMultisampleState=&ms;ci.pColorBlendState=&blend;ci.pDynamicState=&ds;ci.layout=layouts[12];ci.renderPass=compositionPass;
    Check(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&ci,nullptr,&compositionPipeline),"native effects composition pipeline");
}
void CreateComposition() {
    if(!independentBlendAvailable)throw std::runtime_error("independent attachment writes unavailable");
    VkFormatProperties format{};vkGetPhysicalDeviceFormatProperties(physical,VK_FORMAT_R32_SFLOAT,&format);
    if((format.optimalTilingFeatures&(VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))!=(VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))throw std::runtime_error("opaque depth colour target unavailable");
    splitColour=MakeImage(width,height,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
    splitDepth=MakeImage(width,height,VK_FORMAT_R32_SFLOAT,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
    std::array<VkAttachmentDescription,4> a{};
    a[0].format=a[2].format=VK_FORMAT_R16G16B16A16_SFLOAT;a[1].format=VK_FORMAT_D32_SFLOAT;a[3].format=VK_FORMAT_R32_SFLOAT;
    for(unsigned i=0;i<4;++i){a[i].samples=VK_SAMPLE_COUNT_1_BIT;a[i].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;a[i].storeOp=VK_ATTACHMENT_STORE_OP_STORE;a[i].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;a[i].stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;a[i].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;a[i].finalLayout=i==1?VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;}
    const VkAttachmentReference refs[]{{0,a[0].finalLayout},{2,a[2].finalLayout},{3,a[3].finalLayout}},d{1,a[1].finalLayout};
    VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=3;sub.pColorAttachments=refs;sub.pDepthStencilAttachment=&d;
    const uint32_t mask=3;VkRenderPassMultiviewCreateInfo mv{VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO};mv.subpassCount=1;mv.pViewMasks=&mask;mv.correlationMaskCount=1;mv.pCorrelationMasks=&mask;
    VkSubpassDependency dep{};dep.srcSubpass=VK_SUBPASS_EXTERNAL;dep.dstSubpass=0;dep.srcStageMask=dep.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;dep.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;dep.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo pi{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};pi.pNext=&mv;pi.attachmentCount=4;pi.pAttachments=a.data();pi.subpassCount=1;pi.pSubpasses=&sub;pi.dependencyCount=1;pi.pDependencies=&dep;
    Check(vkCreateRenderPass(device,&pi,nullptr,&splitPass),"opaque sidecar render pass");
    for(auto& attachment:a){attachment.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;attachment.initialLayout=attachment.finalLayout;}
    Check(vkCreateRenderPass(device,&pi,nullptr,&splitResumePass),"opaque sidecar continuation");
    const VkImageView views[]{color.view,depth.view,splitColour.view,splitDepth.view};VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fi.renderPass=splitPass;fi.attachmentCount=4;fi.pAttachments=views;fi.width=width;fi.height=height;fi.layers=1;
    Check(vkCreateFramebuffer(device,&fi,nullptr,&splitFramebuffer),"opaque sidecar framebuffer");
    const uint32_t* code[]{shaders::world_split_frag,shaders::terrain_split_frag,shaders::sky_split_frag,shaders::world_solid_split_frag,shaders::terrain_solid_split_frag,shaders::composition_split_frag};
    const size_t bytes[]{sizeof(shaders::world_split_frag),sizeof(shaders::terrain_split_frag),sizeof(shaders::sky_split_frag),sizeof(shaders::world_solid_split_frag),sizeof(shaders::terrain_solid_split_frag),sizeof(shaders::composition_split_frag)};
    for(unsigned i=0;i<6;++i){VkShaderModuleCreateInfo mi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};mi.pCode=code[i];mi.codeSize=bytes[i];Check(vkCreateShaderModule(device,&mi,nullptr,i<5?&splitModules[i]:&compositionModule),"sidecar shader");}
    CreateDensityPasses(true);
    CreateCompositionSwapchain();
}
void AcquireComposition() {
    XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};CheckXr(Proc<PFN_xrAcquireSwapchainImage>(xrInstance,"xrAcquireSwapchainImage")(compositionChain,&ai,&compositionIndex),"acquire effects layer");
    XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wi.timeout=XR_INFINITE_DURATION;
    if(Proc<PFN_xrWaitSwapchainImage>(xrInstance,"xrWaitSwapchainImage")(compositionChain,&wi)!=XR_SUCCESS)throw std::runtime_error("effects layer wait failed");
    if(compositionIndex>=compositionViews.size())throw std::runtime_error("effects image index invalid");
    compositionAcquired=true;
}
void ComposeSplit() {
    Phase(Composition);
    PostBarrier(splitColour,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    const auto& shader=shaders::programs[8];const auto align=std::max<VkDeviceSize>(16,properties.limits.minUniformBufferOffsetAlignment);cursor=(cursor+align-1)&~(align-1);
    if(cursor+shader.uniformSize>arena.size)throw std::runtime_error("composition uniform capacity exhausted");
    auto* bytes=static_cast<uint8_t*>(arena.mapped)+cursor;std::memset(bytes,0,shader.uniformSize);
    for(size_t i=0;i<shader.fieldCount;++i){const auto& f=shader.fields[i];const auto v=splitPostValues.find(f.name);if(v==splitPostValues.end())continue;if(f.integer){const int n=int(v->second[0]);std::memcpy(bytes+f.offset,&n,4);}else std::memcpy(bytes+f.offset,v->second.data(),std::min<size_t>(f.components,v->second.size())*4);}
    const std::vector<VkDescriptorImageInfo> images{{clamped,splitColour.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{nearest,adapted[adaptIndex].image.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{clamped,bloom[1].image.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{nearest,Finished().image.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
    const auto set=Descriptor(12,shader.uniformSize,images);const uint32_t offset=uint32_t(cursor);cursor+=shader.uniformSize;
    VkRenderPassBeginInfo bi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};bi.renderPass=compositionPass;bi.framebuffer=compositionFrames[acquiredColor*compositionViews.size()+compositionIndex];bi.renderArea.extent={width,height};BeginPass(Composition,bi);
    const VkViewport viewport{0,0,float(width),float(height),0,1};const VkRect2D scissor{{0,0},{width,height}};vkCmdSetViewport(command,0,1,&viewport);vkCmdSetScissor(command,0,1,&scissor);
    vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,compositionPipeline);vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,layouts[12],0,1,&set,1,&offset);vkCmdDraw(command,3,1,0,0);EndPass();
}
void ReleaseComposition() {
    if(!compositionAcquired)return;
    XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};CheckXr(Proc<PFN_xrReleaseSwapchainImage>(xrInstance,"xrReleaseSwapchainImage")(compositionChain,&ri),"release effects layer");compositionAcquired=false;
}
bool OverlayViews(const XrView* views,XrCompositionLayerProjectionView* output) {
    if(!compositionPresented)return false;
    for(unsigned e=0;e<2;++e){output[e]={XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};output[e].pose=views[e].pose;output[e].fov=views[e].fov;output[e].subImage={compositionChain,{{0,0},{int32_t(width),int32_t(height)}},e};output[e].next=compositionHasDepth?&xrDepthInfo[e]:nullptr;}
    return true;
}
