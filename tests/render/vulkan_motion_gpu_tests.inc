// Exercises the actual native producer, shader modules, resource staging and
// borrowed runtime targets. Only OpenXR image acquisition/release is simulated.
void NativeMotion() {
    fqrender::nativeOnly=true;fqgl::Reset();
    assert(fqvk::InitializeWorldProbe(reinterpret_cast<XrInstance>(uintptr_t(1)),1,64,64));
    auto& r=*fqvk::renderer;
    XrGraphicsBindingVulkan2KHR binding{};assert(fqvk::GameplayBinding(binding));
    offerDepth=true;inspectReleasedImages=false;
    assert(fqvk::CreateGameplaySwapchain(reinterpret_cast<XrSession>(uintptr_t(2)),true));
    std::array<fqvk::MotionSwapchainTarget,2> targets{};
    std::array<std::vector<fqvk::Image>,2> motionImages,depthImages;
    for(unsigned e=0;e<2;++e){auto& t=targets[e];t.width=t.height=32;t.motion=reinterpret_cast<XrSwapchain>(uintptr_t(100+e*2));t.depth=reinterpret_cast<XrSwapchain>(uintptr_t(101+e*2));
        for(int i=0;i<2;++i){auto image=r.MakeImage(32,32,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_COLOR_BIT,1);runtimeMotion[e*2].push_back(image.image);motionImages[e].push_back(image);}
        for(int i=0;i<3;++i){auto image=r.MakeImage(32,32,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_DEPTH_BIT,1);runtimeMotion[e*2+1].push_back(image.image);depthImages[e].push_back(image);}
    }
    assert(fqvk::InitializeSpacewarp(targets));
    auto quad=Quad(false);const uint32_t indices[]{0,1,2,0,2,3};fqvk::Mesh(20,quad.data(),4,18,indices,6);
    std::vector<float> skin(4*8);for(int i=0;i<4;++i)skin[i*8]=1;
    fqskin::PaletteRow identity;fqskin::Pack(identity,fqskin::Identity(),fqskin::Identity());
    fqvk::Skin(20,21,skin.data(),skin.size(),identity.data(),1);
    auto read=[&](fqvk::Image& image,bool depth){auto buffer=r.MakeBuffer(32*32*(depth?4:8),VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
        r.Transfer([&](VkCommandBuffer cb){VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.image=image.image;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.subresourceRange={depth?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};b.oldLayout=depth?VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;b.srcAccessMask=depth?VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT:VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(cb,depth?VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT:VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
            VkBufferImageCopy copy{};copy.imageSubresource={b.subresourceRange.aspectMask,0,0,1};copy.imageExtent={32,32,1};vkCmdCopyImageToBuffer(cb,b.image,b.newLayout,buffer.buffer,1,&copy);
            b.oldLayout=b.newLayout;b.newLayout=depth?VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;b.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;b.dstAccessMask=depth?VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT:VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,depth?VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT:VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,0,nullptr,0,nullptr,1,&b);
            VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};host.buffer=buffer.buffer;host.size=VK_WHOLE_SIZE;host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
        });std::vector<float> pixels(32*32*(depth?1:4));if(depth)std::memcpy(pixels.data(),buffer.mapped,pixels.size()*4);else for(size_t i=0;i<pixels.size();++i)pixels[i]=fqvk::Renderer::Half(static_cast<uint16_t*>(buffer.mapped)[i]);r.Destroy(buffer);return pixels;};
    auto matrix=fqskin::Identity();float eye[3]{};
    const unsigned initialReleases=motionReleases;
    for(int mode=0;mode<7;++mode){
        // Rigid, weighted palette, facial/CPU deformation, cutout, occluded.
        const bool skinned=mode==1||mode>=5,deformed=mode==2,cutout=mode==3,occluded=mode==4;
        fqspacewarp::History history;fqspacewarp::Drawable prior;prior.geometry=1;
        if(skinned){auto bone=fqskin::Identity();bone[12]=-.1f;fqskin::PaletteRow row;fqskin::Pack(row,bone,fqskin::Identity());prior.palette={row};}
        if(deformed)for(unsigned i=0;i<4;++i){prior.deformedPositions.push_back(quad[i*18]-.2f);prior.deformedPositions.push_back(quad[i*18+1]);prior.deformedPositions.push_back(0);}
        fqspacewarp::Frame f;f.scene=1;f.world=mode%2?0u:0x3cu;f.sceneReady=f.focused=true;f.viewProjection[0][12]=.25f;f.viewProjection[1][12]=-.4f;
        for(unsigned s=1;s<=2;++s){f.serial=s;f.displayTime=s*10000000;assert(history.Begin(f));assert(history.Record(1,prior));history.EyeRendered(0);history.EyeRendered(1);assert(history.Finish(true));}
        auto current=prior;current.model[12]=.1f;current.model[13]=.2f;if(skinned)current.palette={identity};if(deformed)for(unsigned i=0;i<4;++i)current.deformedPositions[i*3]+=.2f;
        f.serial=3;f.displayTime=30000000;f.viewProjection={matrix,matrix};assert(history.Begin(f));
        fqmotion::Capture capture;capture.Begin(history);capture.Main();fqmotion::Draw d;d.identity=1;d.mesh=20;d.vbo=1;d.vertices=4;d.count=6;d.indexed=true;d.cull=false;d.skinMode=skinned?(mode==6?2:1):0;d.paletteTexture=21;d.alphaTest=cutout;d.diffuse=22;assert(capture.Record(d,current));
        uint8_t white[4]{255,255,255,uint8_t(cutout?0:255)};assert(fqvk::Texture(22,1,1,white));
        assert(fqvk::BeginWorldProbe(matrix.data(),matrix.data(),eye,eye));
        VkClearAttachment clear{};clear.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;clear.clearValue.depthStencil={occluded?.2f:.5f,0};VkClearRect rect{{{0,0},{64,64}},0,1};vkCmdClearAttachments(r.command,1,&clear,1,&rect);
        std::array<fqspacewarp::Coverage,2> coverage{};const auto uploads=r.motionUploads,allocations=r.paletteAllocations;
        fqvk::PostSettings post;fqvk::PostProcess(post);
        fqvk::QueueSpacewarp(capture,coverage);
        std::array<XrView,2> views{};std::array<XrCompositionLayerProjectionView,2> output{};
        assert(fqvk::PresentGameplay(views.data(),output.data(),.04f,1000));
        assert(output[0].next&&output[1].next&&r.finishWaitUs==0);
        assert(coverage[0].Complete(3)&&coverage[1].Complete(3));
        if(skinned)assert(r.motionUploads-uploads==sizeof(fqskin::PaletteRow));
        if(mode>=5)assert(r.paletteAllocations==allocations); // Reuse the previous-pose image across frames.
        if(deformed)assert(r.motionUploads-uploads==4*3*sizeof(float));
        history.EyeRendered(0);history.EyeRendered(1);assert(history.CanSynthesize(coverage));
        for(unsigned e=0;e<2;++e){auto& t=r.motionTargets[e];assert(t.motionIndex==motionCycles%2&&t.depthIndex==(motionCycles+1)%3);
            auto pixels=read(motionImages[e][t.motionIndex],false);auto depths=read(depthImages[e][t.depthIndex],true);
            const float camera=e?.4f:-.25f;Near(pixels[0],camera);Near(pixels[1],0);
            const size_t centre=(16*32+16)*4;Near(pixels[centre],camera+((cutout||occluded)?0:.1f+(skinned?.1f:deformed?.2f:0)));Near(pixels[centre+1],cutout||occluded?0:-.2f);Near(pixels[centre+2],0);Near(depths[16*32+16],occluded?.2f:.5f);
        }
        ++motionCycles;
    }
    assert(motionReleases-initialReleases==28);fqvk::ShutdownSpacewarp();
    for(auto& images:motionImages)for(auto& image:images)r.Destroy(image);
    for(auto& images:depthImages)for(auto& image:images)r.Destroy(image);
    for(auto& images:runtimeMotion)images.clear();
    fqvk::DestroyGameplaySwapchain();offerDepth=false;inspectReleasedImages=true;
    fqvk::ShutdownWorldProbe();fqrender::nativeOnly=false;fqgl::Reset();
    std::cout<<"Vulkan SpaceWarp pixels: stereo native axes, rigid motion, skin palettes, facial positions, cutouts, occlusion, source depth, independent runtime indices and once-per-frame pose uploads passed\n";
}
