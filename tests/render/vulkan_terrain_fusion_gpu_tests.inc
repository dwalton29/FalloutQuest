// Actual native material registration, descriptor snapshots, CPU fusion and
// indexed geometry. The retained v237 Vulkan material is the pixel oracle.
#include "rendering/terrain/terrain-layer-fusion.h"
#include "rendering/terrain/fo3-terrain-bounds.h"
struct FusionGpuBatch {
    int gridX=0,gridY=0,layerOrder=-1;bool alphaLayer=false,specularEnabled=false;
    size_t vertexFloats=12;std::string texturePath,normalPath;
    std::vector<float> vertices;std::vector<uint32_t> indices;
    std::vector<fo3terrain::PaintMaterial> paintMaterials;
};
std::vector<float> FusionDepth(fqvk::Image& image,VkImageLayout layout) {
    auto& r=*fqvk::renderer;auto buffer=r.MakeBuffer(64*64*2*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
    r.Transfer([&](VkCommandBuffer cb){
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.image=image.image;
        b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,2};
        b.oldLayout=layout;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;b.srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
        VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_DEPTH_BIT,0,0,2};copy.imageExtent={64,64,1};
        vkCmdCopyImageToBuffer(cb,image.image,b.newLayout,buffer.buffer,1,&copy);
        b.oldLayout=b.newLayout;b.newLayout=layout;b.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;b.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
        vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&b);
        VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};host.buffer=buffer.buffer;host.size=VK_WHOLE_SIZE;
        host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
    });
    std::vector<float> pixels(64*64*2);std::memcpy(pixels.data(),buffer.mapped,pixels.size()*4);r.Destroy(buffer);return pixels;
}
void TerrainFusionGpu(bool split,bool density=false) {
    emulateDensity=density;fqrender::nativeOnly=true;fqrender::driverCalls=0;fqgl::Reset();
    assert(fqvk::InitializeWorldProbe(reinterpret_cast<XrInstance>(uintptr_t(1)),1,64,64));
    auto& r=*fqvk::renderer;
    if(density){r.densityMap=r.MakeImage(4,4,VK_FORMAT_R8G8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);r.CreateDensityPasses(false);r.foveationRequested=true;}
    if(split){XrGraphicsBindingVulkan2KHR binding{};assert(fqvk::GameplayBinding(binding));inspectReleasedImages=false;
        assert(fqvk::CreateGameplaySwapchain(reinterpret_cast<XrSession>(uintptr_t(2)),false));r.CreateMotionPass(VK_FORMAT_D32_SFLOAT);r.CreateComposition();
        // Readback permission is test-only; production copy/render operations are unchanged.
        r.worldDepthSnapshot=r.MakeImage(64,64,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_DEPTH_BIT);}
    fqvk::Program(900,1);fqvk::Program(901,fqvk::TerrainFusionKind);
    assert(r.programs.at(901).fused&&r.programs.at(901).samplers.size()==9);
    const std::array<std::array<uint8_t,4>,5> diffuse{{{163,110,45,255},{61,150,90,255},{200,40,97,255},{30,60,180,255},{150,80,30,255}}};
    const std::array<std::array<uint8_t,4>,4> normals{{{128,128,255,20},{144,110,250,180},{92,157,245,120},{125,130,255,250}}};
    std::unordered_map<std::string,uint32_t> textures;
    for(size_t i=0;i<diffuse.size();++i){const auto id=uint32_t(920+i);std::vector<uint8_t> checker(64*64*4);
        for(size_t y=0;y<64;++y)for(size_t x=0;x<64;++x)for(size_t c=0;c<4;++c)
            checker[(y*64+x)*4+c]=c==3?255:uint8_t(unsigned(diffuse[i][c])*((x+y)%2?255:70)/255);
        fqvk::Texture(id,64,64,checker.data());assert(r.textures.at(id).levels==7);textures["d"+std::to_string(i)]=id;}
    for(size_t i=0;i<normals.size();++i){const auto id=uint32_t(930+i);fqvk::Texture(id,1,1,normals[i].data());textures["n"+std::to_string(i)]=id;}
    const uint8_t shadow[]{255,255,255,255};fqvk::Texture(940,1,1,shadow);
    auto sampler=[&](uint32_t program,const char* name,int unit){const int location=fqrender::UniformLocation(program,name);fqvk::hooks.uniform(program,location,2,4,&unit);};
    const float left[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},right[]{1,0,0,0,0,1,0,0,0,0,1,0,.17f,.08f,0,1},eye[]{0,0,2};
    for(int scenario=0;scenario<=5;++scenario)for(bool indexed:{false,true})for(int lights:{0,1,8})for(bool clipped:{false,true}) {
        const int layers=std::min(scenario,4);const bool mixed=scenario==4;
        std::vector<FusionGpuBatch> original;
        FusionGpuBatch base;base.texturePath="d0";base.normalPath="n0";base.specularEnabled=true;
        const auto quad=Quad(true);
        // Two separate base batches allow one full stack to fuse while a
        // neighbouring >3-layer stack remains on its original multipass path.
        for(int tile=0;tile<(mixed?2:1);++tile){auto b=base;
            b.vertices=quad;
            for(size_t v=0;v<4;++v){b.vertices[v*12]=b.vertices[v*12]*.45f+(tile? .42f:-.32f);b.vertices[v*12+2]=.1f*b.vertices[v*12];b.vertices[v*12+9]=.8f;b.vertices[v*12+6]=std::array<float,4>{0,8,8,0}[v];b.vertices[v*12+7]=std::array<float,4>{0,0,8,8}[v];}
            b.indices={0,1,2,0,2,3};original.push_back(std::move(b));
        }
        const size_t bases=original.size();
        for(int paint=0;paint<layers;++paint){FusionGpuBatch b;b.alphaLayer=true;b.layerOrder=paint;b.texturePath="d"+std::to_string(paint+1);b.normalPath="n"+std::to_string((paint+1)%4);b.specularEnabled=paint%2;
            for(size_t tile=0;tile<bases;++tile){if(mixed&&tile==0&&paint>0)continue;
                const auto first=uint32_t(b.vertices.size()/12);auto vertices=original[tile].vertices;
                for(size_t v=0;v<4;++v)vertices[v*12+8]=paint==1?0.f:std::array<float,4>{0,0,0,.9f}[v];
                b.vertices.insert(b.vertices.end(),vertices.begin(),vertices.end());for(auto i:original[tile].indices)b.indices.push_back(first+i);
            }original.push_back(std::move(b));
        }
        if(!indexed)for(auto& b:original){std::vector<float> expanded;for(auto i:b.indices)expanded.insert(expanded.end(),b.vertices.begin()+i*12,b.vertices.begin()+(i+1)*12);b.vertices=std::move(expanded);b.indices.clear();}
        auto fused=original;const auto stats=fo3terrain::FuseLayers(fused,[&](const auto& b){return textures.count(b.texturePath)!=0;});
        const bool expectFusion=layers>0&&(layers<4||mixed);assert(stats.groups==(expectFusion?1u:0u));
        if(layers==4)assert(stats.fallback>0); // A mixed retained/fused stack.
        for(auto& b:fused)if(b.vertexFloats==15)fqmesh::IndexExact<15>(b.vertices,b.indices);
        std::vector<uint16_t> reference;std::vector<float> referenceDepth;uint64_t legacyDraws=0;
        for(bool combined:{false,true}) {
            auto& batches=combined?fused:original;const uint32_t program=combined?901:900;
            for(const auto id:{900u,901u}) {
                Set(id,"uAmbientColor",{.4f,.3f,.2f});Set(id,"uSunlightColor",{4.f,3.f,2.f});Set(id,"uSunDirection",{.1f,.2f,1});
                Set(id,"uFogNearVertexQ1532",{0});Set(id,"uFogFarVertexQ1532",{2});Set(id,"uFogPowerVertexQ1532",{1.4f});Set(id,"uFogColor",{.3f,.4f,.6f});
                Set(id,"uLocalLightCount",{float(lights)});
                Set(id,"uLocalLightPosRadius",{.2f,.1f,1,5, -.4f,.2f,1,3, .8f,.1f,2,5, 20,20,20,1, .3f,.8f,1,4, -.5f,-.4f,2,6, .2f,-.6f,1,5, -.9f,.9f,2,5});
                Set(id,"uLocalLightColorFalloff",{.7f,.2f,.4f,1.1f, .1f,.2f,.1f,2, .2f,.1f,.1f,1, 1,1,1,1, .1f,.1f,.2f,3, .1f,.2f,.1f,1, .2f,.1f,.1f,2, .1f,.1f,.1f,1});
                Set(id,"uShadowsEnabled",{1});Set(id,"uShadowTexelSize",{1,1});Set(id,"uLightMvp",{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});
                Set(id,"uWaterReflectionClipEnabledQ2090",{clipped?1.f:0.f});Set(id,"uWaterReflectionPlaneYQ2090",{0});
                sampler(id,"uDiffuse",0);sampler(id,"uNormalGloss",1);sampler(id,"uShadowMap",3);
            }
            assert(fqvk::BeginWorldProbe(left,right,eye,eye,false,nullptr,split));assert(r.splitFrame==split);
            for(size_t pass=0;pass<2;++pass)for(size_t batch=0;batch<batches.size();++batch){const auto& b=batches[batch];
                if(b.vertices.empty()||size_t(b.alphaLayer)!=pass)continue;
                const auto mesh=uint32_t(1000+batch);fqvk::Mesh(mesh,b.vertices.data(),b.vertices.size()/b.vertexFloats,b.vertexFloats,b.indices.data(),b.indices.size());
                Set(program,"uUseTexture",{1});Set(program,"uUseNormal",{1});Set(program,"uTerrainSpecularEnabled",{b.specularEnabled?1.f:0.f});
                fqgl::state.texture2d[0].Change(textures.at(b.texturePath));fqgl::state.texture2d[1].Change(textures.at(b.normalPath));fqgl::state.texture2d[3].Change(940);
                if(combined){Set(program,"uRoundMode",{2});Set(program,"uPaintCount",{float(b.paintMaterials.size())});
                    const int diffuseUnits[]{2,5,7},normalUnits[]{4,6,8};
                    for(size_t p=0;p<b.paintMaterials.size();++p){const auto suffix=std::to_string(p);sampler(program,("uPaintDiffuse"+suffix).c_str(),diffuseUnits[p]);sampler(program,("uPaintNormal"+suffix).c_str(),normalUnits[p]);
                        Set(program,("uPaintFlags"+suffix).c_str(),{1,b.paintMaterials[p].specularEnabled?1.f:0.f});
                        fqgl::state.texture2d[diffuseUnits[p]].Change(textures.at(b.paintMaterials[p].texturePath));fqgl::state.texture2d[normalUnits[p]].Change(textures.at(b.paintMaterials[p].normalPath));
                    }
                }
                if(combined){
                    float selectedPositions[32]{},selectedColours[32]{},positions[32]{},colours[32]{};
                    const auto& referenceProgram=r.programs.at(900);
                    for(const auto& field:referenceProgram.fields){
                        if(!std::strcmp(field.field.name,"uLocalLightPosRadius"))std::memcpy(selectedPositions,referenceProgram.bytes.data()+field.field.offset,sizeof(selectedPositions));
                        if(!std::strcmp(field.field.name,"uLocalLightColorFalloff"))std::memcpy(selectedColours,referenceProgram.bytes.data()+field.field.offset,sizeof(selectedColours));
                    }
                    const auto bounds=fo3terrain::BuildBounds(b.vertices.data(),b.vertices.size(),b.vertexFloats);
                    const int count=fo3terrain::CullLights(bounds,lights,selectedPositions,selectedColours,positions,colours);
                    assert(count<=lights);Set(program,"uLocalLightCount",{float(count)});
                    for(const auto& field:r.programs.at(program).fields){
                        if(!std::strcmp(field.field.name,"uLocalLightPosRadius"))fqvk::hooks.uniform(program,field.location,1,count*16,positions);
                        if(!std::strcmp(field.field.name,"uLocalLightColorFalloff"))fqvk::hooks.uniform(program,field.location,1,count*16,colours);
                    }
                }
                fqgl::state.depth.Change(true);fqgl::state.depthWrite.Change(!b.alphaLayer);fqgl::state.depthFunc.Change(GL_LEQUAL);fqgl::state.cull.Change(false);fqgl::state.blend.Change(b.alphaLayer);
                fqgl::state.srcRgb.Change(GL_SRC_ALPHA);fqgl::state.dstRgb.Change(GL_ONE_MINUS_SRC_ALPHA);fqgl::state.srcAlpha.Change(GL_SRC_ALPHA);fqgl::state.dstAlpha.Change(GL_ONE_MINUS_SRC_ALPHA);
                Submit(program,mesh,GL_TRIANGLES,0,int(b.indices.empty()?b.vertices.size()/b.vertexFloats:b.indices.size()),b.indices.empty()?0:GL_UNSIGNED_INT,b.vertices.size()*4,1);
            }
            r.DrainDraws();
            for(const auto& entry:r.pipelines)if(entry.first.terrain){
                const auto& key=entry.first;
                assert(key.fused==(key.stride==15));
                if(key.fused)assert(key.paintCount>=1&&key.paintCount<=3);
            }
            for(const auto& entry:r.descriptorCache)if(entry.first.kind==1){
                assert(entry.first.images.size()==3);
                assert(entry.first.uniformBytes==fqvk::shaders::terrainUniformSize);
            }
            if(split)r.FullRateScene();
            fqvk::EndWorldProbe();assert(fqvk::WorldProbeActive()&&r.skipped==0);
            const auto pixels=r.Readback();const auto depths=FusionDepth(r.depth,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            if(!combined){reference=pixels;referenceDepth=depths;legacyDraws=r.draws;}
            else {
                assert(referenceDepth==depths);float error=0;size_t visible=0;
                for(size_t i=0;i<pixels.size();++i){error=std::max(error,std::fabs(fqvk::Renderer::Half(pixels[i])-fqvk::Renderer::Half(reference[i])));visible+=i%4==0&&fqvk::Renderer::Half(pixels[i])>.01f;}
                if(error>.004f)throw std::runtime_error("Fusion mismatch layers="+std::to_string(layers)+" error="+std::to_string(error));
                assert(visible>200&&r.draws<=legacyDraws);if(layers>0&&layers<4)assert(r.draws==1&&legacyDraws==uint64_t(layers+1));
                if(split){assert(r.worldDepthReady&&!r.lateWorldDepthWrite);assert(FusionDepth(r.worldDepthSnapshot,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)==depths);}
            }
        }
    }
    assert(fqrender::driverCalls==0);if(split)fqvk::DestroyGameplaySwapchain();fqvk::ShutdownWorldProbe();fqrender::nativeOnly=false;emulateDensity=false;inspectReleasedImages=true;fqgl::Reset();
    std::cout<<"Native terrain fusion: 0-4 paint layers, mixed fallback, indexed/nonindexed input, colour/depth parity in both eyes, original normals/specular/fog/shadows/lights, reflection clip, split="<<split<<" densityEmulation="<<density<<" passed\n";
}
