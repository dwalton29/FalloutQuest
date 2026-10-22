// Runs the production backend and original ported material shaders on a real
// Vulkan implementation (CI: Mesa lavapipe). Only the OpenXR device handshake
// is supplied by this test; it does not claim headset/compositor validation.
#include "rendering/vulkan/fo3-vulkan-renderer.cpp"
#include <cassert>
#include <iostream>

namespace {
XrResult XRAPI_CALL Requirements(XrInstance,XrSystemId,XrGraphicsRequirementsVulkan2KHR* out) {
    out->minApiVersionSupported=XR_MAKE_VERSION(1,1,0);out->maxApiVersionSupported=XR_MAKE_VERSION(1,3,0);return XR_SUCCESS;
}
XrResult XRAPI_CALL Instance(XrInstance,const XrVulkanInstanceCreateInfoKHR* info,VkInstance* out,VkResult* result) {
    *result=vkCreateInstance(info->vulkanCreateInfo,nullptr,out);return XR_SUCCESS;
}
XrResult XRAPI_CALL Physical(XrInstance,const XrVulkanGraphicsDeviceGetInfoKHR* info,VkPhysicalDevice* out) {
    uint32_t count=0;vkEnumeratePhysicalDevices(info->vulkanInstance,&count,nullptr);
    if(!count)return XR_ERROR_RUNTIME_FAILURE;
    std::vector<VkPhysicalDevice> devices(count);vkEnumeratePhysicalDevices(info->vulkanInstance,&count,devices.data());*out=devices[0];return XR_SUCCESS;
}
XrResult XRAPI_CALL Device(XrInstance,const XrVulkanDeviceCreateInfoKHR* info,VkDevice* out,VkResult* result) {
    *result=vkCreateDevice(info->vulkanPhysicalDevice,info->vulkanCreateInfo,nullptr,out);return XR_SUCCESS;
}
}
extern "C" XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance,const char* name,PFN_xrVoidFunction* out) {
    if(!std::strcmp(name,"xrGetVulkanGraphicsRequirements2KHR"))*out=reinterpret_cast<PFN_xrVoidFunction>(Requirements);
    else if(!std::strcmp(name,"xrCreateVulkanInstanceKHR"))*out=reinterpret_cast<PFN_xrVoidFunction>(Instance);
    else if(!std::strcmp(name,"xrGetVulkanGraphicsDevice2KHR"))*out=reinterpret_cast<PFN_xrVoidFunction>(Physical);
    else if(!std::strcmp(name,"xrCreateVulkanDeviceKHR"))*out=reinterpret_cast<PFN_xrVoidFunction>(Device);
    else return XR_ERROR_FUNCTION_UNSUPPORTED;
    return XR_SUCCESS;
}
namespace {
void Program(uint32_t id,bool terrain) {
    fqvk::ProgramData p;p.terrain=terrain;
    p.bytes.resize(terrain?fqvk::shaders::terrainUniformSize:fqvk::shaders::worldUniformSize);
    const auto* fields=terrain?fqvk::shaders::terrainFields:fqvk::shaders::worldFields;
    const size_t count=terrain?std::size(fqvk::shaders::terrainFields):std::size(fqvk::shaders::worldFields);
    for(size_t i=0;i<count;++i)p.fields.push_back({int(i),fields[i]});
    const size_t samplers=terrain?std::size(fqvk::shaders::terrainSamplers):std::size(fqvk::shaders::worldSamplers);
    p.units.resize(samplers);
    const auto* names=terrain?fqvk::shaders::terrainSamplers:fqvk::shaders::worldSamplers;
    for(size_t i=0;i<samplers;++i) {
        const std::string name=names[i].name;
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
void Scene(bool terrain,bool skin=false,bool instanced=false) {
    fqgl::Reset();
    Program(1,terrain);const auto quad=Quad(terrain);const uint32_t indices[]{0,1,2,0,2,3};
    fqvk::Mesh(9,quad.data(),4,terrain?12:18,indices,6);
    uint8_t red[]{255,0,0,255};fqvk::Texture(5,1,1,red);
    fqgl::state.texture2d[0].Change(5);
    fqgl::state.depth.Change(true);fqgl::state.depthWrite.Change(true);fqgl::state.depthFunc.Change(GL_LEQUAL);
    fqgl::state.blend.Change(false);fqgl::state.cull.Change(false);
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
    fqvk::hooks.draw(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,terrain?quad.size()*4:0,instanced?2:1);
    if(terrain) {
        // Authored overlay with depth equality and no depth writes. A uniform
        // update after an earlier draw must not retroactively alter that draw.
        auto paint=Quad(true,.5f);fqvk::Mesh(10,paint.data(),4,12,indices,6);
        uint8_t green[]{0,255,0,255};fqvk::Texture(6,1,1,green);
        fqgl::state.texture2d[0].Change(6);fqgl::state.blend.Change(true);fqgl::state.depthWrite.Change(false);
        fqgl::state.srcRgb.Change(GL_SRC_ALPHA);fqgl::state.dstRgb.Change(GL_ONE_MINUS_SRC_ALPHA);
        fqgl::state.srcAlpha.Change(GL_SRC_ALPHA);fqgl::state.dstAlpha.Change(GL_ONE_MINUS_SRC_ALPHA);
        fqvk::hooks.draw(1,10,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,paint.size()*4,1);
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
        fqvk::Skin(9,7,nullptr,0,palette,1);
        assert(fqvk::BeginWorldProbe(left,left,eye,eye));
        fqvk::hooks.draw(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
        fqvk::EndWorldProbe();const auto moved=fqvk::renderer->Readback();
        Near(Pixel(moved,0,32,32,0),0);Near(Pixel(moved,0,55,32,0),1);
    }
}
}
int main() {
    for(int mode=0;mode<4;++mode) {
        assert(fqvk::InitializeWorldProbe(reinterpret_cast<XrInstance>(uintptr_t(1)),1,64,64));
        Scene(mode==1,mode==2,mode==3);fqvk::ShutdownWorldProbe();assert(!fqvk::WorldProbeActive());
    }
    std::cout<<"Vulkan native multiview: original world/LAND shaders, indexed NIF layout, per-eye cameras, depth equality, ordered paint blending and recreation passed\n";
}
