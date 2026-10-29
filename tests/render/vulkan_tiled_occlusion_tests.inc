#include "rendering/tiled-occlusion.h"
void TiledOcclusionGpu(){
    assert(fqvk::InitializeWorldProbe(reinterpret_cast<XrInstance>(uintptr_t(1)),1,64,64));
    fqgl::Reset();Program(1,0);
    Set(1,"uNoLighting",{1});Set(1,"uMaterialAlpha",{1});Set(1,"uPipScreenGain",{1});
    Set(1,"uSunDirectionVertexQ1540",{0,0,1});Set(1,"uFogNearVertexQ1532",{10000});Set(1,"uFogFarVertexQ1532",{10001});Set(1,"uFogPowerVertexQ1532",{1});
    const uint8_t red[]{255,0,0,255},green[]{0,255,0,255};fqvk::Texture(51,1,1,red);fqvk::Texture(52,1,1,green);
    const uint32_t indices[]{0,1,2,0,2,3};
    auto mesh=[&](uint32_t id,float l,float r,float b,float t,float z){auto vertices=Quad(false);for(size_t i=0;i<4;++i){vertices[i*18]=(i==0||i==3)?l:r;vertices[i*18+1]=i<2?b:t;vertices[i*18+2]=z;}fqvk::Mesh(id,vertices.data(),4,18,indices,6);};
    fqgl::state.depth.Change(true);fqgl::state.depthWrite.Change(true);fqgl::state.depthFunc.Change(GL_LEQUAL);fqgl::state.blend.Change(false);fqgl::state.cull.Change(false);
    size_t culled=0,retained=0;
    for(int scenario=0;scenario<5;++scenario){
        const float seam=.013f,gap=scenario==1?.07f:0;
        const fo3occ::Point panels[2][4]{{{-.9f,-.9f,-.5f},{seam-gap,-.9f,-.5f},{seam-gap,.9f,-.5f},{-.9f,.9f,-.5f}},
            {{seam+gap,-.9f,scenario==2?.8f:-.4f},{.9f,-.9f,scenario==2?.8f:-.4f},{.9f,.9f,scenario==2?.8f:-.4f},{seam+gap,.9f,scenario==2?.8f:-.4f}}};
        mesh(61,-.22f,.22f,-.15f,.15f,.5f);
        mesh(62,-.9f,seam-gap,-.9f,.9f,-.5f);mesh(63,seam+gap,.9f,-.9f,.9f,scenario==2?.8f:-.4f);
        float cameras[2][16]{{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},{1,0,0,0,0,1,0,0,0,0,1,0,.08f,0,0,1}};
        if(scenario==3)cameras[1][12]=.8f;
        if(scenario==4)for(auto& camera:cameras){camera[0]=camera[5]=std::cos(.37f);camera[1]=std::sin(.37f);camera[4]=-std::sin(.37f);}
        fo3occ::TiledBuffer occlusion[2];bool hidden=true;
        for(size_t eye=0;eye<2;++eye){for(const auto& panel:panels){fo3occ::Polygon p;assert(fo3occ::ProjectPolygon(cameras[eye],panel,4,false,false,p));occlusion[eye].Add(p);}
            fo3occ::Rect bounds;uint32_t tests=0;
            hidden&=fo3occ::ProjectBounds(cameras[eye],{-.22f,.22f,-.15f,.15f,.5f,.5f},bounds)&&occlusion[eye].Hidden(bounds,tests,40000);
        }
        if(scenario==0||scenario==4){assert(hidden);++culled;}else{assert(!hidden);++retained;}
        std::vector<uint16_t> reference;std::vector<float> referenceDepth;
        for(bool enabled:{false,true}){
            const float eye[]{0,0,2};assert(fqvk::BeginWorldProbe(cameras[0],cameras[1],eye,eye));
            if(!enabled||!hidden){fqgl::state.texture2d[0].Change(52);Submit(1,61,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);}
            fqgl::state.texture2d[0].Change(51);Submit(1,62,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);Submit(1,63,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
            fqvk::EndWorldProbe();const auto pixels=fqvk::renderer->Readback();
            const auto depth=FusionDepth(fqvk::renderer->depth,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            if(!enabled){reference=pixels;referenceDepth=depth;}else{assert(reference==pixels);assert(referenceDepth==depth);}
        }
    }
    assert(culled&&retained);fqvk::ShutdownWorldProbe();fqgl::Reset();
    std::cout<<"Tiled occlusion GPU: exact stereo pixels against unculled rendering, shared seam, genuine gap, mixed depth and one-eye reveal passed\n";
}
