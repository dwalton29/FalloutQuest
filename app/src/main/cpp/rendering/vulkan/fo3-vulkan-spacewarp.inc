// Included inside Renderer. Motion shares the existing frame command buffer,
// upload queue, descriptor cache and fenced per-frame arena. No GLES/readback.
struct MotionConstants {
    fqspacewarp::Matrix currentVP,previousVP,clipReprojection,model,previousModel;
    std::array<float,4> scaleEye{},flags{},target{};
};
static_assert(sizeof(MotionConstants)==368);
struct MotionTarget {
    XrSwapchain motion=XR_NULL_HANDLE,depth=XR_NULL_HANDLE;
    uint32_t width=0,height=0,motionIndex=0,depthIndex=0;
    bool motionReady=false,depthReady=false;
    std::vector<VkImageView> motionViews,depthViews;
    std::vector<VkFramebuffer> framebuffers;
};
std::array<MotionTarget,2> motionTargets;
VkRenderPass motionPass{};
VkShaderModule motionModules[2][2]{};
std::unordered_map<unsigned,VkPipeline> motionPipelines;
struct MotionPose {Image palette;uint64_t used=0;};
std::unordered_map<uint64_t,MotionPose> motionPoses;
size_t motionPoseBytes=0;
const fqmotion::Capture* motionCapture=nullptr;
std::array<fqspacewarp::Coverage,2>* motionCoverage=nullptr;
uint64_t motionUploads=0,motionDraws=0;

void DestroyMotion() {
    if(!device)return;
    if(motionPass)vkDeviceWaitIdle(device); // Teardown must also tolerate device loss.
    DestroyComposition();
    motionCapture=nullptr;motionCoverage=nullptr;
    for(auto& target:motionTargets){
        for(auto f:target.framebuffers)if(f)vkDestroyFramebuffer(device,f,nullptr);
        for(auto v:target.motionViews)if(v)vkDestroyImageView(device,v,nullptr);
        for(auto v:target.depthViews)if(v)vkDestroyImageView(device,v,nullptr);
        target={};
    }
    for(auto& p:motionPoses)Destroy(p.second.palette);
    motionPoses.clear();motionPoseBytes=0;
    for(auto& p:motionPipelines)vkDestroyPipeline(device,p.second,nullptr);
    motionPipelines.clear();
    for(auto& program:motionModules)for(auto& m:program){if(m)vkDestroyShaderModule(device,m,nullptr);m={};}
    if(layouts[12])vkDestroyPipelineLayout(device,layouts[12],nullptr);
    layouts[12]={};
    if(sets[12])vkDestroyDescriptorSetLayout(device,sets[12],nullptr);
    sets[12]={};
    if(motionPass)vkDestroyRenderPass(device,motionPass,nullptr);
    motionPass={};
}
void CreateMotionPass(VkFormat depthFormat) {
    std::array<VkAttachmentDescription,2> a{};
    a[0].format=VK_FORMAT_R16G16B16A16_SFLOAT;a[1].format=depthFormat;
    for(auto& attachment:a){attachment.samples=VK_SAMPLE_COUNT_1_BIT;attachment.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;attachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;}
    a[0].finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;a[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkAttachmentReference colorRef{0,a[0].finalLayout},depthRef{1,a[1].finalLayout};
    VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&colorRef;sub.pDepthStencilAttachment=&depthRef;
    VkSubpassDependency dependency{};dependency.srcSubpass=VK_SUBPASS_EXTERNAL;dependency.dstSubpass=0;
    dependency.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo pi{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};pi.attachmentCount=2;pi.pAttachments=a.data();pi.subpassCount=1;pi.pSubpasses=&sub;pi.dependencyCount=1;pi.pDependencies=&dependency;
    Check(vkCreateRenderPass(device,&pi,nullptr,&motionPass),"motion render pass");
    std::array<VkDescriptorSetLayoutBinding,6> bindings{};
    for(uint32_t i=0;i<bindings.size();++i){bindings[i].binding=i;bindings[i].descriptorCount=1;bindings[i].stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;bindings[i].descriptorType=i==0?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:i==1?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;}
    VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};ci.bindingCount=uint32_t(bindings.size());ci.pBindings=bindings.data();
    Check(vkCreateDescriptorSetLayout(device,&ci,nullptr,&sets[12]),"motion descriptor layout");
    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};li.setLayoutCount=1;li.pSetLayouts=&sets[12];
    Check(vkCreatePipelineLayout(device,&li,nullptr,&layouts[12]),"motion pipeline layout");
    const uint32_t* code[2][2]{{shaders::motion_static_vert,shaders::motion_static_frag},{shaders::motion_dynamic_vert,shaders::motion_dynamic_frag}};
    const size_t bytes[2][2]{{sizeof(shaders::motion_static_vert),sizeof(shaders::motion_static_frag)},{sizeof(shaders::motion_dynamic_vert),sizeof(shaders::motion_dynamic_frag)}};
    for(unsigned p=0;p<2;++p)for(unsigned s=0;s<2;++s){VkShaderModuleCreateInfo mi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};mi.pCode=code[p][s];mi.codeSize=bytes[p][s];Check(vkCreateShaderModule(device,&mi,nullptr,&motionModules[p][s]),"motion shader module");}
}
void InitializeMotion(const std::array<MotionSwapchainTarget,2>& targets,VkFormat depthFormat) {
    DestroyMotion();CreateMotionPass(depthFormat);
    for(unsigned eye=0;eye<2;++eye){
        const auto& source=targets[eye];auto& t=motionTargets[eye];t.motion=source.motion;t.depth=source.depth;t.width=source.width;t.height=source.height;
        auto views=[&](XrSwapchain chain,VkFormat format,VkImageAspectFlags aspect,std::vector<VkImageView>& out){
            const auto enumerate=Proc<PFN_xrEnumerateSwapchainImages>(xrInstance,"xrEnumerateSwapchainImages");uint32_t count=0;
            CheckXr(enumerate(chain,0,&count,nullptr),"motion image count");if(!count)throw std::runtime_error("empty motion swapchain");
            std::vector<XrSwapchainImageVulkan2KHR> images(count,{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
            CheckXr(enumerate(chain,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())),"motion Vulkan images");
            for(const auto& image:images){VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=image.image;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=format;vi.subresourceRange={aspect,0,1,0,1};VkImageView view{};Check(vkCreateImageView(device,&vi,nullptr,&view),"motion runtime view");out.push_back(view);}
        };
        views(t.motion,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_ASPECT_COLOR_BIT,t.motionViews);
        views(t.depth,depthFormat,VK_IMAGE_ASPECT_DEPTH_BIT,t.depthViews);
        for(auto mv:t.motionViews)for(auto dv:t.depthViews){
            VkImageView attachments[]{mv,dv};VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fi.renderPass=motionPass;fi.attachmentCount=2;fi.pAttachments=attachments;fi.width=t.width;fi.height=t.height;fi.layers=1;VkFramebuffer fb{};Check(vkCreateFramebuffer(device,&fi,nullptr,&fb),"motion framebuffer");t.framebuffers.push_back(fb);
        }
    }
}
VkPipeline MotionPipeline(unsigned key) {
    const auto found=motionPipelines.find(key);if(found!=motionPipelines.end())return found->second;
    const bool full=key==32;VkPipelineShaderStageCreateInfo stages[2]{};
    for(unsigned i=0;i<2;++i){stages[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stages[i].stage=i?VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_VERTEX_BIT;stages[i].module=motionModules[full?0:1][i];stages[i].pName="main";}
    const VkVertexInputBindingDescription bindings[]{{0,72,VK_VERTEX_INPUT_RATE_VERTEX},{1,(key&8)?32u:72u,VK_VERTEX_INPUT_RATE_VERTEX},{2,(key&16)?12u:72u,VK_VERTEX_INPUT_RATE_VERTEX}};
    const VkVertexInputAttributeDescription attributes[]{{0,0,VK_FORMAT_R32G32B32_SFLOAT,0},{4,0,VK_FORMAT_R32G32_SFLOAT,48},{5,0,VK_FORMAT_R32G32B32A32_SFLOAT,56},{10,1,VK_FORMAT_R32G32B32A32_SFLOAT,0},{11,1,VK_FORMAT_R32G32B32A32_SFLOAT,16},{12,2,VK_FORMAT_R32G32B32_SFLOAT,0}};
    VkPipelineVertexInputStateCreateInfo vertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};if(!full){vertex.vertexBindingDescriptionCount=3;vertex.pVertexBindingDescriptions=bindings;vertex.vertexAttributeDescriptionCount=6;vertex.pVertexAttributeDescriptions=attributes;}
    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};viewport.viewportCount=viewport.scissorCount=1;
    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.polygonMode=VK_POLYGON_MODE_FILL;rs.lineWidth=1;rs.cullMode=(key&1)?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_NONE;rs.frontFace=(key&2)?VK_FRONT_FACE_CLOCKWISE:VK_FRONT_FACE_COUNTER_CLOCKWISE;rs.depthBiasEnable=(key&4)!=0;
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};ds.depthTestEnable=VK_TRUE;ds.depthWriteEnable=full;ds.depthCompareOp=VK_COMPARE_OP_ALWAYS;
    VkPipelineColorBlendAttachmentState ba{};ba.colorWriteMask=0xf;
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};blend.attachmentCount=1;blend.pAttachments=&ba;
    const VkDynamicState dynamicStates[]{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_DEPTH_BIAS};VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dynamic.dynamicStateCount=3;dynamic.pDynamicStates=dynamicStates;
    VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};ci.stageCount=2;ci.pStages=stages;ci.pVertexInputState=&vertex;ci.pInputAssemblyState=&ia;ci.pViewportState=&viewport;ci.pRasterizationState=&rs;ci.pMultisampleState=&ms;ci.pDepthStencilState=&ds;ci.pColorBlendState=&blend;ci.pDynamicState=&dynamic;ci.layout=layouts[12];ci.renderPass=motionPass;
    VkPipeline pipeline{};Check(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&ci,nullptr,&pipeline),"motion pipeline");motionPipelines[key]=pipeline;return pipeline;
}
bool MotionBind(const MotionConstants& constants,const std::vector<VkDescriptorImageInfo>& images,unsigned pipeline) {
    if(descriptorCache.size()>=DescriptorCapacity)return false;
    const auto alignment=std::max<VkDeviceSize>(16,properties.limits.minUniformBufferOffsetAlignment);cursor=(cursor+alignment-1)&~(alignment-1);
    if(cursor+sizeof(constants)>arena.size)return false;
    std::memcpy(static_cast<uint8_t*>(arena.mapped)+cursor,&constants,sizeof(constants));const uint32_t offset=uint32_t(cursor);cursor+=sizeof(constants);
    const auto set=Descriptor(12,sizeof(constants),images);
    vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,MotionPipeline(pipeline));
    vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,layouts[12],0,1,&set,1,&offset);return true;
}
struct PreparedMotion {const fqmotion::Draw* draw;const fqspacewarp::Drawable* shape;const fqspacewarp::Drawable* previous;VkImageView previousPalette;VkDeviceSize previousOffset;};
std::vector<PreparedMotion> PrepareMotion(const fqmotion::Capture& capture) {
    const auto serial=capture.history->CurrentFrame().serial;
    // A visible actor can return after native fallback/modal frames. Its image
    // capacity is still reusable; refresh visibility before retiring departures.
    for(const auto& draw:capture.draws){const auto p=motionPoses.find(draw.identity);if(p!=motionPoses.end())p->second.used=serial;}
    for(auto i=motionPoses.begin();i!=motionPoses.end();){if(i->second.used+1<serial){motionPoseBytes-=size_t(i->second.palette.height)*sizeof(fqskin::PaletteRow);Retire(i->second.palette);i=motionPoses.erase(i);}else ++i;}
    std::vector<PreparedMotion> prepared;prepared.reserve(capture.draws.size());
    for(const auto& draw:capture.draws){
        const auto mesh=meshes.find(draw.mesh);const auto* shape=capture.history->Current(draw.identity);
        if(mesh==meshes.end()||mesh->second.stride!=18||!shape)continue;
        const auto* previous=capture.history->Previous(draw.identity,shape->geometry);if(!previous)previous=shape;
        if(draw.vertices<=0||draw.count<=0||size_t(draw.count)>mesh->second.count)continue;
        VkDeviceSize previousOffset=0;VkImageView previousPalette=white.view;
        if(!previous->deformedPositions.empty()){
            if(previous->deformedPositions.size()!=size_t(draw.vertices)*3)continue;
            const auto bytes=previous->deformedPositions.size()*sizeof(float);cursor=(cursor+15)&~VkDeviceSize(15);
            if(bytes>fqspacewarp::History::MaxHistoryBytes||cursor+bytes>arena.size)continue;
            previousOffset=cursor;std::memcpy(static_cast<uint8_t*>(arena.mapped)+cursor,previous->deformedPositions.data(),bytes);cursor+=bytes;motionUploads+=bytes;
        }
        if(draw.skinMode){
            if(!mesh->second.skin.buffer||!textures.count(draw.paletteTexture)||previous->palette.empty()||previous->palette.size()!=shape->palette.size())continue;
            auto& pose=motionPoses[draw.identity];const auto bytes=previous->palette.size()*sizeof(fqskin::PaletteRow);
            const auto old=size_t(pose.palette.height)*sizeof(fqskin::PaletteRow);
            if(bytes>fqspacewarp::History::MaxHistoryBytes||motionPoseBytes-old+bytes>fqspacewarp::History::MaxHistoryBytes)continue;
            if(!pose.palette.image||pose.palette.height!=previous->palette.size()){
                Retire(pose.palette);pose.palette=UploadPalette(reinterpret_cast<const float*>(previous->palette.data()),previous->palette.size());motionPoseBytes=motionPoseBytes-old+bytes;
            }else UpdatePalette(pose.palette,reinterpret_cast<const float*>(previous->palette.data()),previous->palette.size());
            pose.used=serial;previousPalette=pose.palette.view;motionUploads+=bytes;
        }
        prepared.push_back({&draw,shape,previous,previousPalette,previousOffset});
    }
    return prepared;
}
bool MotionEye(unsigned eye,VkFramebuffer framebuffer,const fqmotion::Capture& capture,const std::vector<PreparedMotion>& prepared) {
    const auto& frame=capture.history->CurrentFrame();const auto& prior=capture.history->SubmittedFrames()?capture.history->PreviousFrame():frame;
    MotionConstants c{};c.currentVP=frame.viewProjection[eye];c.previousVP=prior.viewProjection[eye];
    if(!fqspacewarp::ClipReprojection(c.currentVP,c.previousVP,c.clipReprojection))return false;
    // Both Vulkan extensions consume native NDC: [-1,1] XY, [0,1] Z.
    c.scaleEye={1,-1,.5f,float(eye)};auto& t=motionTargets[eye];c.target={float(t.width),float(t.height),0,0};
    VkRenderPassBeginInfo bi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};bi.renderPass=motionPass;bi.framebuffer=framebuffer;bi.renderArea.extent={t.width,t.height};
    BeginPass(Motion,bi);
    const VkViewport viewport{0,0,float(t.width),float(t.height),0,1};const VkRect2D scissor{{0,0},{t.width,t.height}};
    vkCmdSetViewport(command,0,1,&viewport);vkCmdSetScissor(command,0,1,&scissor);
    std::vector<VkDescriptorImageInfo> images{{nearest,worldDepthReady?worldDepthSnapshot.view:depth.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{sampler,white.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{nearest,white.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{nearest,white.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
    bool complete=MotionBind(c,images,32);if(complete)vkCmdDraw(command,3,1,0,0);
    for(const auto& item:prepared){
        const auto& d=*item.draw;const auto& m=meshes.at(d.mesh);
        c.model=item.shape->model;c.previousModel=item.previous->model;c.flags={float(d.skinMode),float(d.alphaTest),float(d.vertexAlpha),d.alpha};c.target[2]=d.threshold;
        const auto diffuse=textures.find(d.diffuse);if(d.alphaTest&&diffuse==textures.end()){complete=false;continue;}
        images[1].imageView=diffuse==textures.end()?white.view:diffuse->second.view;
        images[1].sampler=diffuse==textures.end()?sampler:diffuse->second.repeatT?verticalRepeat:diffuse->second.clamp?clamped:sampler;
        images[2].imageView=d.skinMode?textures.at(d.paletteTexture).view:white.view;images[3].imageView=item.previousPalette;
        const bool positions=!item.previous->deformedPositions.empty();const unsigned key=unsigned(d.cull)|(unsigned(d.clockwise)<<1)|(unsigned(d.decal)<<2)|(unsigned(d.skinMode!=0)<<3)|(unsigned(positions)<<4);
        if(!MotionBind(c,images,key)){complete=false;continue;}
        if(d.decal)vkCmdSetDepthBias(command,-1,0,-.65f);
        const VkBuffer buffers[]{m.vertices.buffer,d.skinMode?m.skin.buffer:m.vertices.buffer,positions?arena.buffer:m.vertices.buffer};const VkDeviceSize offsets[]{0,0,positions?item.previousOffset:0};
        vkCmdBindVertexBuffers(command,0,3,buffers,offsets);
        if(d.indexed){if(!m.indices.buffer){complete=false;continue;}vkCmdBindIndexBuffer(command,m.indices.buffer,0,VK_INDEX_TYPE_UINT32);vkCmdDrawIndexed(command,d.count,1,0,0,0);}
        else vkCmdDraw(command,d.count,1,0,0);
        ++motionDraws;
    }
    EndPass();return complete&&prepared.size()==capture.draws.size();
}
void RenderMotion() {
    if(!motionCapture||!motionCoverage)return;
    const auto& capture=*motionCapture;auto* result=motionCoverage;motionCapture=nullptr;motionCoverage=nullptr;
    if(!capture.history||!capture.mainDrawn||!motionPass)return;
    const auto uploadBefore=motionUploads,drawBefore=motionDraws;
    // End() has drained colour draws and closed their pass. Pose transfers are
    // queued before this command buffer, once for both eyes.
    const auto prepared=PrepareMotion(capture);
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.image=depth.image;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,2};
    b.oldLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;b.srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    if(!worldDepthReady)vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&b);
    for(unsigned eye=0;eye<2;++eye){auto& t=motionTargets[eye];
        auto acquire=[&](XrSwapchain chain,uint32_t& index,bool& ready){XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};CheckXr(Proc<PFN_xrAcquireSwapchainImage>(xrInstance,"xrAcquireSwapchainImage")(chain,&ai,&index),"acquire motion");XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wi.timeout=XR_INFINITE_DURATION;const auto status=Proc<PFN_xrWaitSwapchainImage>(xrInstance,"xrWaitSwapchainImage")(chain,&wi);if(status!=XR_SUCCESS)throw std::runtime_error("motion wait failed");ready=true;};
        acquire(t.motion,t.motionIndex,t.motionReady);acquire(t.depth,t.depthIndex,t.depthReady);
        if(t.motionIndex>=t.motionViews.size()||t.depthIndex>=t.depthViews.size())throw std::runtime_error("motion index out of range");
        const bool rendered=MotionEye(eye,t.framebuffers[t.motionIndex*t.depthViews.size()+t.depthIndex],capture,prepared);
        (*result)[eye]=capture.Coverage(rendered&&!lateWorldDepthWrite);
    }
    b.oldLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;b.srcAccessMask=VK_ACCESS_SHADER_READ_BIT;b.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    if(!worldDepthReady)vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,0,0,nullptr,0,nullptr,1,&b);
    if((capture.history->CurrentFrame().serial%180)==1)VKLOG("FQ SPACEWARP motion backend=Vulkan serial=%llu coverage=%d/%d draws=%llu uploadBytes=%llu previousPaletteBytes=%zu queue=shared cpuWait=0 worldColour=shared uiOnlyLayer=1 reprojection=precomposed worldDepthRetained=%d lateDepthWrite=%d",
        (unsigned long long)capture.history->CurrentFrame().serial,(*result)[0].Complete(capture.history->CurrentFrame().serial),(*result)[1].Complete(capture.history->CurrentFrame().serial),(unsigned long long)(motionDraws-drawBefore),(unsigned long long)(motionUploads-uploadBefore),motionPoseBytes,worldDepthReady,lateWorldDepthWrite);
}
void ReleaseMotion() {
    for(auto& t:motionTargets){XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        if(t.depthReady){CheckXr(Proc<PFN_xrReleaseSwapchainImage>(xrInstance,"xrReleaseSwapchainImage")(t.depth,&ri),"release motion depth");t.depthReady=false;}
        if(t.motionReady){CheckXr(Proc<PFN_xrReleaseSwapchainImage>(xrInstance,"xrReleaseSwapchainImage")(t.motion,&ri),"release motion");t.motionReady=false;}
    }
}
