// Both snapshots are idle until water requests its first copy. Render the early
// world into them and expand once before water, HUD, film or compositor depth.
WorldResolution resolution;
bool adaptive=true,reduced=false,qualitySample=false;
double frameBudgetMs=0;
VkRect2D worldRect{};
VkFramebuffer lowFramebuffers[2]{};
VkPipeline resolvePipelines[2]{};
int renderedStep=0;

VkFramebuffer LowFramebuffer(bool density) {
    auto& result=lowFramebuffers[density];
    if(result)return result;
    const VkImageView views[]{sceneColor.view,sceneDepth.view,densityMap.view};
    VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    info.renderPass=density?densityMain.clear:pass;
    info.width=width;info.height=height;info.layers=1;
    info.attachmentCount=density?3:2;info.pAttachments=views;
    Check(vkCreateFramebuffer(device,&info,nullptr,&result),"world resolution framebuffer");
    return result;
}

VkPipeline ResolvePipeline(bool split) {
    auto& pipeline=resolvePipelines[split];
    if(pipeline)return pipeline;
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.pCode=split?shaders::world_resolve_split_frag:shaders::world_resolve_frag;
    info.codeSize=split?sizeof(shaders::world_resolve_split_frag):sizeof(shaders::world_resolve_frag);
    VkShaderModule module{};
    Check(vkCreateShaderModule(device,&info,nullptr,&module),"world upscale fragment");
    try{pipeline=MakePostPipeline(4,split?splitPass:pass,module,split);}
    catch(...){vkDestroyShaderModule(device,module,nullptr);throw;}
    vkDestroyShaderModule(device,module,nullptr);
    return pipeline;
}

void ExpandWorld() {
    if(!reduced)return;
    EndPass();
    const auto fragment=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    const auto read=VK_ACCESS_SHADER_READ_BIT;
    PostBarrier(sceneColor,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,read,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,fragment);
    // PostBarrier is colour-only. Depth has its own aspect and producer stages.
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.image=sceneDepth.image;barrier.oldLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;barrier.srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask=read;barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,2};
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        fragment,0,0,nullptr,0,nullptr,1,&barrier);
    snapshotInitialized=true;

    reduced=false;densityActive=false;sidecarPass=splitFrame;
    const VkClearValue clears[3]{};
    VkRenderPassBeginInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    info.renderPass=splitFrame?splitPass:pass;info.framebuffer=splitFrame?splitFramebuffer:framebuffer;
    info.renderArea.extent={width,height};info.pClearValues=clears;info.clearValueCount=splitFrame?3:2;
    BeginPass(SceneResume,info);
    const VkViewport full{0,0,float(width),float(height),0,1};
    vkCmdSetViewport(command,0,1,&full);vkCmdSetScissor(command,0,1,&info.renderArea);
    const auto align=std::max<VkDeviceSize>(16,properties.limits.minUniformBufferOffsetAlignment);
    cursor=(cursor+align-1)&~(align-1);
    if(cursor+16>arena.size)throw std::runtime_error("world upscale uniform arena exhausted");
    const float rect[]{float(worldRect.offset.x),float(worldRect.offset.y),float(worldRect.extent.width),float(worldRect.extent.height)};
    std::memcpy(static_cast<uint8_t*>(arena.mapped)+cursor,rect,sizeof(rect));
    const uint32_t offset=uint32_t(cursor);cursor+=16;
    const auto set=Descriptor(4,16,{{clamped,sceneColor.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                                    {nearest,sceneDepth.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}});
    vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,layouts[4],0,1,&set,1,&offset);
    vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,ResolvePipeline(splitFrame));
    vkCmdDraw(command,3,1,0,0);
}
