// Exercises the actual native producer, shader modules, resource staging and
// borrowed runtime targets. Only OpenXR image acquisition/release is simulated.
void NativeMotion(bool srgb=false,bool densityEmulation=false) {
    emulateDensity=densityEmulation;const auto descriptions=densityDescriptions;
    offerSrgb=srgb;
    fqrender::nativeOnly=true;fqgl::Reset();
    assert(fqvk::InitializeWorldProbe(reinterpret_cast<XrInstance>(uintptr_t(1)),1,64,64));
    auto& r=*fqvk::renderer;
    if(densityEmulation){
        const auto pixels=fqvk::DensityPixels(64,64);assert(pixels.size()==64*64*4&&pixels[0]==63&&pixels[(32*64+32)*2]==255);
        assert(std::equal(pixels.begin(),pixels.begin()+64*64*2,pixels.begin()+64*64*2));
        for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x){const auto v=pixels[(y*64+x)*2];assert(v==255||v==127||v==63);assert(v==pixels[((63-y)*64+(63-x))*2]);if(v!=255)assert(v*(v==127?2:4)<=255);}
        r.densityMap=r.MakeImage(4,4,VK_FORMAT_R8G8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);r.CreateDensityPasses(false);r.foveationRequested=true;
    }
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
    for(int mode=0;mode<8;++mode){
        // Rigid, weighted palette, facial/CPU deformation, cutout, occluded.
        const bool skinned=mode==1||mode==5||mode==6,deformed=mode==2,cutout=mode==3,occluded=mode==4;
        fqspacewarp::History history;fqspacewarp::Drawable prior;prior.geometry=1;
        if(skinned){auto bone=fqskin::Identity();bone[12]=-.1f;fqskin::PaletteRow row;fqskin::Pack(row,bone,fqskin::Identity());prior.palette={row};}
        if(deformed)for(unsigned i=0;i<4;++i){prior.deformedPositions.push_back(quad[i*18]-.2f);prior.deformedPositions.push_back(quad[i*18+1]);prior.deformedPositions.push_back(0);}
        fqspacewarp::Frame f;f.scene=1;f.world=mode%2?0u:0x3cu;f.sceneReady=f.focused=true;f.viewProjection[0][12]=.25f;f.viewProjection[1][12]=-.4f;
        for(unsigned s=1;s<=2;++s){f.serial=s;f.displayTime=s*10000000;assert(history.Begin(f));assert(history.Record(1,prior));history.EyeRendered(0);history.EyeRendered(1);assert(history.Finish(true));}
        auto current=prior;current.model[12]=.1f;current.model[13]=.2f;if(skinned)current.palette={identity};if(deformed)for(unsigned i=0;i<4;++i)current.deformedPositions[i*3]+=.2f;
        f.serial=3;f.displayTime=30000000;f.viewProjection={matrix,matrix};assert(history.Begin(f));
        fqmotion::Capture capture;capture.Begin(history);capture.Main();fqmotion::Draw d;d.identity=1;d.mesh=mode==7?999:20;d.vbo=1;d.vertices=4;d.count=6;d.indexed=true;d.cull=false;d.skinMode=skinned?(mode==6?2:1):0;d.paletteTexture=21;d.alphaTest=cutout;d.diffuse=22;assert(capture.Record(d,current));
        uint8_t white[4]{255,255,255,uint8_t(cutout?0:255)};assert(fqvk::Texture(22,1,1,white));
        assert(fqvk::BeginWorldProbe(matrix.data(),matrix.data(),eye,eye));
        VkClearAttachment clear{};clear.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;clear.clearValue.depthStencil={occluded?.2f:.5f,0};VkClearRect rect{{{0,0},{64,64}},0,1};vkCmdClearAttachments(r.command,1,&clear,1,&rect);
        std::array<fqspacewarp::Coverage,2> coverage{};const auto uploads=r.motionUploads,allocations=r.paletteAllocations;
        fqvk::PostSettings post;fqvk::PostProcess(post);
        fqvk::QueueSpacewarp(capture,coverage);
        std::array<XrView,2> views{};std::array<XrCompositionLayerProjectionView,2> output{};
        assert(fqvk::PresentGameplay(views.data(),output.data(),.04f,1000));
        assert(output[0].next&&output[1].next&&r.finishWaitUs==0);
        assert(coverage[0].Complete(3)==(mode!=7)&&coverage[1].Complete(3)==(mode!=7));
        if(skinned)assert(r.motionUploads-uploads==sizeof(fqskin::PaletteRow));
        if(mode==5||mode==6)assert(r.paletteAllocations==allocations); // Reuse the previous-pose image across frames.
        if(deformed)assert(r.motionUploads-uploads==4*3*sizeof(float));
        history.EyeRendered(0);history.EyeRendered(1);assert(history.CanSynthesize(coverage)==(mode!=7));
        for(unsigned e=0;e<2;++e){auto& t=r.motionTargets[e];assert(t.motionIndex==motionCycles%2&&t.depthIndex==(motionCycles+1)%3);
            auto pixels=read(motionImages[e][t.motionIndex],false);auto depths=read(depthImages[e][t.depthIndex],true);
            const float camera=e?.4f:-.25f;Near(pixels[0],camera);Near(pixels[1],0);
            const size_t centre=(16*32+16)*4;Near(pixels[centre],camera+((cutout||occluded||mode==7)?0:.1f+(skinned?.1f:deformed?.2f:0)));Near(pixels[centre+1],cutout||occluded||mode==7?0:-.2f);Near(pixels[centre+2],0);Near(depths[16*32+16],occluded?.2f:.5f);
        }
        ++motionCycles;
    }
    assert(motionReleases-initialReleases==32);
    // A stationary camera at real wasteland coordinates must generate exactly
    // zero static motion. The old inverse -> float world -> previous VP path
    // loses precision here even when current and previous matrices are equal.
    for(unsigned mode=0;mode<4;++mode){
        auto projection=fqskin::Identity();const float n=.04f,far=125000.f/70;
        projection[0]=1.13f;projection[5]=1.07f;projection[10]=-(far+n)/(far-n);projection[11]=-1;projection[14]=-2*far*n/(far-n);projection[15]=0;
        auto multiply=[](const auto& a,const auto& b){fqspacewarp::Matrix m{};for(unsigned c=0;c<4;++c)for(unsigned r=0;r<4;++r)for(unsigned k=0;k<4;++k)m[c*4+r]+=a[k*4+r]*b[c*4+k];return m;};
        auto view=fqskin::Identity();view[12]=mode==2?-1200.f:-157.12f;view[13]=mode==2?400.f:31.88f;view[14]=mode==2?-890.f:8.79f;
        if(mode==1){const float angle=.13f;view[5]=view[10]=std::cos(angle);view[6]=std::sin(angle);view[9]=-std::sin(angle);}
        const auto prior=multiply(projection,view);auto current=prior;
        if(mode==3)current[13]-=projection[5]*.01f; // Upward tracked head translation, with no artificial app motion.
        fqspacewarp::History h;fqspacewarp::Frame f;f.scene=1;f.world=0x3c;f.sceneReady=f.focused=true;f.viewProjection={prior,prior};
        for(unsigned i=1;i<=2;++i){f.serial=i;f.displayTime=i*10000000;assert(h.Begin(f));h.EyeRendered(0);h.EyeRendered(1);assert(h.Finish(true));}
        f.serial=3;f.displayTime=30000000;f.viewProjection={current,current};assert(h.Begin(f));fqmotion::Capture capture;capture.Begin(h);capture.Main();
        assert(fqvk::BeginWorldProbe(current.data(),current.data(),eye,eye));
        VkClearAttachment clear{};clear.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;clear.clearValue.depthStencil={.99f,0};VkClearRect rect{{{0,0},{64,64}},0,1};vkCmdClearAttachments(r.command,1,&clear,1,&rect);
        std::array<fqspacewarp::Coverage,2> coverage{};fqvk::PostProcess({});fqvk::QueueSpacewarp(capture,coverage);
        std::array<XrView,2> views{};std::array<XrCompositionLayerProjectionView,2> output{};assert(fqvk::PresentGameplay(views.data(),output.data(),.04f,1000));
        for(unsigned e=0;e<2;++e){assert(coverage[e].Complete(3));const auto pixels=read(motionImages[e][r.motionTargets[e].motionIndex],false);
            for(size_t at=0;at<pixels.size();at+=4){if(mode<3){assert(pixels[at]==0&&pixels[at+1]==0&&pixels[at+2]==0);}else{const float z=-projection[14]/(2*.99f-1+projection[10]);const float headY=-((current[13]-prior[13])/projection[5]);Near(pixels[at+1],projection[5]*headY/(-z));assert(pixels[at+1]>0);}}}
        ++motionCycles;
    }
    std::cout<<"Vulkan tracked-head stability: exact zero motion for stationary perspective cameras at wasteland/distant origins, pitched stationary poses and correct vertical translation in both eyes passed\n";
    // Reproduce all three blockers from the headset log in ONE real GPU frame.
    // Original water/refraction, blended world material and post-film HUD must
    // remain pixel-equivalent while the opaque motion/depth coverage succeeds.
    Water();Program(65,9);
    const uint8_t white[]{255,255,255,255};fqvk::Texture(68,1,1,white,false,false);
    const float ui[]{-.25f,-.85f,-.2f,0,0,.25f,-.85f,-.2f,0,0,.25f,-.55f,-.2f,0,0,-.25f,-.55f,-.2f,0,0};
    fqvk::Mesh(66,ui,4,5,indices,6);Set(65,"uMvp",{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});Set(65,"uTint",{.1f,.8f,.2f,.65f});
    auto glass=Quad(false);for(unsigned i=0;i<4;++i){glass[i*18]=glass[i*18]*.5f-.55f;glass[i*18+2]=-.1f;}
    fqvk::Mesh(69,glass.data(),4,18,indices,6);
    fqspacewarp::History combinedHistory;fqspacewarp::Frame combinedFrame;combinedFrame.scene=1;combinedFrame.world=0x3c;combinedFrame.sceneReady=combinedFrame.focused=true;
    for(unsigned i=1;i<=2;++i){combinedFrame.serial=i;combinedFrame.displayTime=i*10000000;assert(combinedHistory.Begin(combinedFrame));combinedHistory.EyeRendered(0);combinedHistory.EyeRendered(1);assert(combinedHistory.Finish(true));}
    std::vector<uint8_t> reference;unsigned effectsPixels=0,opaquePixels=0;
    for(unsigned frame=0;frame<6;++frame){
        if(frame==4){fqvk::DestroyGameplaySwapchain();assert(fqvk::CreateGameplaySwapchain(reinterpret_cast<XrSession>(uintptr_t(2)),true));}
        if(frame==4){combinedFrame.world=0;for(unsigned warm=0;warm<2;++warm){combinedFrame.serial=frame+3+warm;combinedFrame.displayTime=combinedFrame.serial*10000000;assert(combinedHistory.Begin(combinedFrame));combinedHistory.EyeRendered(0);combinedHistory.EyeRendered(1);assert(combinedHistory.Finish(true));}}
        const bool split=frame%2,showUi=frame>=2;combinedFrame.serial=frame+3+(frame>=4?2:0);combinedFrame.displayTime=combinedFrame.serial*10000000;
        assert(combinedHistory.Begin(combinedFrame));fqmotion::Capture capture;capture.Begin(combinedHistory);capture.Main();
        assert(fqvk::BeginWorldProbe(matrix.data(),matrix.data(),eye,eye,false,nullptr,split));
        assert(fqmotion::separatedComposition==split);
        capture.Block(fqmotion::Transparency|fqmotion::Water|fqmotion::Ui);
        assert(capture.reasons==(split?0u:28u));
        fqgl::state.depth.Change(true);fqgl::state.depthWrite.Change(true);fqgl::state.blend.Change(false);fqgl::state.texture2d[0].Change(5);
        Submit(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
        fqvk::WaterSnapshot();fqgl::state.depthWrite.Change(false);fqgl::state.texture2d[0].Change(14);
        Submit(3,13,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
        fqgl::state.blend.Change(true);fqgl::state.srcRgb.Change(GL_SRC_ALPHA);fqgl::state.dstRgb.Change(GL_ONE_MINUS_SRC_ALPHA);fqgl::state.srcAlpha.Change(GL_ONE);fqgl::state.dstAlpha.Change(GL_ONE_MINUS_SRC_ALPHA);
        Set(1,"uMaterialAlpha",{.4f});fqgl::state.texture2d[0].Change(68);
        {fqrender::SynthesisOverlayScope authoredTransparency(true);Submit(1,69,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);}
        Set(1,"uMaterialAlpha",{1});
        // A reflected NIF surface must remain in the same world layer as its
        // diffuse pass; otherwise head reprojection separates the two images.
        fqgl::state.srcRgb.Change(GL_ONE);fqgl::state.dstRgb.Change(GL_ONE);
        Set(1,"uEnvironmentPassQ2050",{1});Set(1,"uEnvironmentScaleQ2050",{.2f});
        Submit(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);Set(1,"uEnvironmentPassQ2050",{0});
        fqgl::state.srcRgb.Change(GL_SRC_ALPHA);fqgl::state.dstRgb.Change(GL_ONE_MINUS_SRC_ALPHA);
        // Pre-film health/UI must switch off scene foveation without losing depth.
        if(showUi){fqgl::state.texture2d[0].Change(68);fqgl::state.depthWrite.Change(true);Submit(65,66,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);r.DrainDraws();assert(!r.densityActive);assert(!r.sidecarPass);if(split)assert(r.worldDepthReady);
        {fqgl::state.depthWrite.Change(frame>=4);Submit(1,69,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);r.DrainDraws();assert(r.sidecarPass==split);Submit(65,66,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);r.DrainDraws();assert(r.sidecarPass==split);}}
        fqvk::PostSettings settings;settings.interior=1;settings.brightScale=0;fqvk::PostProcess(settings);
        assert(r.compactFrame==(split&&srgb));if(r.compactFrame)assert(r.compactFinal.image.image);
        if(showUi){fqgl::state.depth.Change(false);fqgl::state.texture2d[0].Change(68);Submit(65,66,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);}
        std::array<fqspacewarp::Coverage,2> coverage{};if(split)fqvk::QueueSpacewarp(capture,coverage);
        std::array<XrView,2> views{};std::array<XrCompositionLayerProjectionView,2> output{},overlay{};
        assert(fqvk::PresentGameplay(views.data(),output.data(),.04f,1000));
        assert(!fqmotion::separatedComposition&&fqvk::SpacewarpOverlay(views.data(),overlay.data())==split);
        combinedHistory.EyeRendered(0);combinedHistory.EyeRendered(1);
        if(!split){reference=RuntimePixels(acquiredImage);assert(output[0].next&&output[1].next);}
        else {
            assert(coverage[0].Complete(combinedFrame.serial)==(frame<4)&&coverage[1].Complete(combinedFrame.serial)==(frame<4));
            assert(combinedHistory.CanSynthesize(coverage)==(frame<4));assert(r.lateWorldDepthWrite==(frame>=4));assert(!output[0].next&&!output[1].next);
            const auto base=RuntimePixels(acquiredImage),effects=RuntimePixels(effectsIndex,true);
            for(unsigned e=0;e<2;++e){assert(overlay[e].next==&r.xrDepthInfo[e]);assert(overlay[e].subImage.imageArrayIndex==e);assert(overlay[e].subImage.swapchain==r.compositionChain);
                const auto depths=read(depthImages[e][r.motionTargets[e].depthIndex],true);Near(depths[16*32+16],.5f);Near(depths[0],1.f);Near(depths[27*32+16],1.f);}
            int largest=0;for(size_t at=0;at<base.size();at+=4){assert(effects[at+3]==0||effects[at+3]==255);const bool effect=effects[at+3]!=0;if(!showUi)assert(!effect);effect?++effectsPixels:++opaquePixels;
                for(unsigned c=0;c<3;++c){if(!effect)assert(effects[at+c]==0);largest=std::max(largest,std::abs(int((effect?effects:base)[at+c])-int(reference[at+c])));}}
            if(largest>2)throw std::runtime_error("opaque/effects composition changed original colour by "+std::to_string(largest)+" display codes");
            ++motionCycles;
        }
        assert(combinedHistory.Finish(true));
    }
    assert(effectsPixels>0&&opaquePixels>0);
    // Separating effects never conceals a real opaque geometry/resource failure.
    ++combinedFrame.serial;combinedFrame.displayTime+=10000000;assert(combinedHistory.Begin(combinedFrame));fqmotion::separatedComposition=true;fqmotion::Capture unsupported;unsupported.Begin(combinedHistory);unsupported.Block(fqmotion::Resources|fqmotion::Transparency);assert(unsupported.reasons==fqmotion::Resources);fqmotion::separatedComposition=false;
    std::cout<<"Vulkan combined SpaceWarp: world-only reflected NIF/water/transparency never leak into native layer; original HUD, opaque depth, complete coverage, stereo colour parity, native depth routing, exterior/interior and swapchain recreation passed\n";
    if(densityEmulation){
        std::array<XrView,2> views{};std::array<XrCompositionLayerProjectionView,2> output{};
        assert(fqvk::BeginWorldProbe(matrix.data(),matrix.data(),eye,eye,true));assert(!r.densityActive&&!r.compactFrame);
        assert(fqvk::PresentGameplay(views.data(),output.data(),.04f,1000));
        r.foveationRequested=false;
        assert(fqvk::BeginWorldProbe(matrix.data(),matrix.data(),eye,eye));assert(!r.densityActive);fqvk::PostProcess({});
        assert(fqvk::PresentGameplay(views.data(),output.data(),.04f,1000));assert(r.foveatedDraws==0);
    }
    r.WaitFrames();
    // The old colour-only reserve did not leave space for motion descriptors.
    for(unsigned i=0;r.descriptorCache.size()<fqvk::Renderer::DescriptorCapacity-fqvk::Renderer::FrameDescriptorReserve;++i)
        r.descriptorCache.emplace(fqvk::Renderer::DescriptorKey{-1,i,{}},VK_NULL_HANDLE);
    r.PrepareDescriptors();assert(r.descriptorCache.empty());
    for(unsigned i=0;i<fqvk::Renderer::DescriptorCapacity;++i)
        r.descriptorCache.emplace(fqvk::Renderer::DescriptorKey{-1,i,{}},VK_NULL_HANDLE);
    fqvk::Renderer::MotionConstants constants{};const auto cursor=r.cursor;
    assert(!r.MotionBind(constants,{},32)&&r.cursor==cursor);
    r.PrepareDescriptors();fqvk::ShutdownSpacewarp();
    for(auto& images:motionImages)for(auto& image:images)r.Destroy(image);
    for(auto& images:depthImages)for(auto& image:images)r.Destroy(image);
    for(auto& images:runtimeMotion)images.clear();
    fqvk::DestroyGameplaySwapchain();offerDepth=false;inspectReleasedImages=true;
    fqvk::ShutdownWorldProbe();if(densityEmulation){assert(densityDescriptions-descriptions==4&&densityPassHandles.empty());std::cout<<"Vulkan FDM attachment emulation: stereo density policy, native/split water continuation, full-rate pre/post-film HUD, compact sRGB colour parity and teardown passed\n";}emulateDensity=false;offerSrgb=false;fqrender::nativeOnly=false;fqgl::Reset();
    std::cout<<"Vulkan SpaceWarp pixels: stereo native axes, rigid motion, skin palettes, facial positions, cutouts, occlusion, source depth, independent runtime indices and once-per-frame pose uploads passed\n";
}
