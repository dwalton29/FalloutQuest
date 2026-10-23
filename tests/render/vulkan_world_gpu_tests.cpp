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
void Program(uint32_t id,int kind) {
    const auto& shader=fqvk::shaders::programs[kind];
    fqvk::ProgramData p;p.terrain=kind==1;p.sky=kind==2;p.water=kind==3;p.ui=kind>=9?kind:0;
    p.bytes.resize(shader.uniformSize);
    for(size_t i=0;i<shader.fieldCount;++i)p.fields.push_back({int(i),shader.fields[i]});
    p.units.resize(shader.samplerCount);
    for(size_t i=0;i<shader.samplerCount;++i) {
        const std::string name=shader.samplers[i].name;
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
void Sky() {
    fqgl::Reset();Program(2,2);
    auto source=Quad(false);std::vector<float> geometry;
    for(size_t i=0;i<source.size();i+=18)geometry.insert(geometry.end(),source.begin()+i,source.begin()+i+3);
    const uint32_t indices[]{0,1,2,0,2,3};fqvk::Mesh(12,geometry.data(),4,3,indices,6);
    Set(2,"uMode",{0});
    Set(2,"uSkyUpper",{.2f,.3f,.4f});Set(2,"uSkyLower",{.2f,.3f,.4f});Set(2,"uHorizon",{.2f,.3f,.4f});
    fqgl::state.depth.Change(false);fqgl::state.depthWrite.Change(false);fqgl::state.cull.Change(false);fqgl::state.blend.Change(false);
    const float left[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const float right[]{1,0,0,0,0,1,0,0,0,0,1,0,.8f,0,0,1};
    const float le[]{0,0,0},re[]{-.8f,0,0};
    assert(fqvk::BeginWorldProbe(left,right,le,re));fqvk::hooks.draw(2,12,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);fqvk::EndWorldProbe();
    const auto pixels=fqvk::renderer->Readback();
    for(int eye=0;eye<2;++eye){Near(Pixel(pixels,eye,32,24,0),.31f);Near(Pixel(pixels,eye,32,24,1),.465f);Near(Pixel(pixels,eye,32,24,2),.62f);}
}

void Water() {
    Scene(false); // establishes the original world material and resident NIF layout
    Program(3,3);
    std::vector<float> geometry;
    for(const auto& xy:std::array<std::array<float,2>,4>{{{{-1,-1}},{{1,-1}},{{1,1}},{{-1,1}}}})
        geometry.insert(geometry.end(),{xy[0],xy[1],-.2f,xy[0],xy[1]});
    const uint32_t indices[]{0,1,2,0,2,3};fqvk::Mesh(13,geometry.data(),4,5,indices,6);
    uint8_t flat[]{128,128,128,255};fqvk::Texture(14,1,1,flat,true,false);
    Set(3,"uSunDirection",{0,1,0});Set(3,"uSunColor",{0,0,0});
    Set(3,"uUnitsPerMetre",{1});Set(3,"uClipNearFar",{.1f,10});
    Set(3,"uSceneFogNearFar",{1000,1001});Set(3,"uSceneFogPower",{1});
    Set(3,"uNormalUvScale",{1});Set(3,"uNoiseScale",{0});
    Set(3,"uDepthFalloff",{0,1e9f});Set(3,"uWaterFogNearFar",{0,1e9f});
    Set(3,"uWaterFogAmount",{0});Set(3,"uShininess",{1});
    const float left[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const float right[]{1,0,0,0,0,1,0,0,0,0,1,0,.8f,0,0,1};
    const float eye[]{0,0,2};
    // Repeat to exercise SHADER_READ -> TRANSFER_DST when reusing snapshots.
    for(int frame=0;frame<2;++frame) {
        assert(fqvk::BeginWorldProbe(left,right,eye,eye));
        fqgl::state.depthWrite.Change(true);fqgl::state.texture2d[0].Change(5);
        fqvk::hooks.draw(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
        fqvk::WaterSnapshot();assert(fqvk::renderer->snapshotReady);
        // Snapshot must be once per frame even with multiple water cells.
        fqvk::WaterSnapshot();
        fqgl::state.depthWrite.Change(false);fqgl::state.texture2d[0].Change(14);
        fqvk::hooks.draw(3,13,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
        fqvk::EndWorldProbe();assert(fqvk::WorldProbeActive());
        const auto pixels=fqvk::renderer->Readback();
        Near(Pixel(pixels,0,32,32,0),1);Near(Pixel(pixels,0,32,32,1),0);
        Near(Pixel(pixels,1,32,32,0),0);Near(Pixel(pixels,1,55,32,0),1);
    }
    // A farther water plane must remain behind the stored opaque depth after
    // leaving and resuming the render pass, while visible in uncovered pixels.
    for(size_t i=2;i<geometry.size();i+=5)geometry[i]=.2f;
    fqvk::Mesh(13,geometry.data(),4,5,indices,6);
    Set(3,"uShallow",{0,1,0});Set(3,"uDeep",{0,1,0});
    Set(3,"uDepthFalloff",{-1,0});Set(3,"uWaterFogNearFar",{0,.1f});Set(3,"uWaterFogAmount",{1});
    const float above[]{0,2,2};
    assert(fqvk::BeginWorldProbe(left,left,above,above));
    fqgl::state.depthWrite.Change(true);fqgl::state.texture2d[0].Change(5);
    fqvk::hooks.draw(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
    fqvk::WaterSnapshot();fqgl::state.depthWrite.Change(false);fqgl::state.texture2d[0].Change(14);
    fqvk::hooks.draw(3,13,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);fqvk::EndWorldProbe();
    const auto occluded=fqvk::renderer->Readback();
    Near(Pixel(occluded,0,32,32,0),1);Near(Pixel(occluded,0,32,32,1),0);
    assert(Pixel(occluded,0,5,32,1)>.1f);
}

float DisplayLinear(float value) {
    const float code=std::floor(std::clamp(value,0.0f,1.0f)*255+.5f)/255;
    const float gamma=std::pow(code,.76f);
    return gamma<=.04045f?gamma/12.92f:std::pow((gamma+.055f)/1.055f,2.4f);
}
void Post() {
    Scene(false);
    const uint8_t red[]{102,0,0,255};fqvk::Texture(5,1,1,red);
    const float left[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const float right[]{1,0,0,0,0,1,0,0,0,0,1,0,.8f,0,0,1};const float eye[]{0,0,2};
    fqvk::PostSettings settings;settings.interior=1;settings.flags=4;settings.tintValue=1;
    settings.tint[0]=settings.tint[2]=0;settings.tint[1]=1;settings.brightThreshold=.1f;
    for(int frame=0;frame<3;++frame) {
        assert(fqvk::BeginWorldProbe(left,right,eye,eye));
        fqgl::state.texture2d[0].Change(5);fqvk::hooks.draw(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
        settings.brightScale=frame==0?0.0f:1.0f;
        if(frame==2)fqvk::ResetExposure();
        fqvk::PostProcess(settings);assert(fqvk::renderer->postDone);fqvk::EndWorldProbe();
        const auto pixels=fqvk::renderer->Readback();
        const float redCombined=.4f+(frame==0?0.0f:.3f*.5f/settings.targetLum);
        const float expected=DisplayLinear(redCombined*.298999995f);
        Near(Pixel(pixels,0,32,32,0),0);Near(Pixel(pixels,0,32,32,1),expected);
        Near(Pixel(pixels,1,32,32,1),0);Near(Pixel(pixels,1,55,32,1),expected);
        if(frame)assert(Pixel(pixels,0,32,32,1)>DisplayLinear(.4f*.298999995f)+.01f);
    }
}

void Ui(bool animated) {
    Scene(false);Program(15,animated?10:9);
    std::vector<float> geometry;
    for(const auto& xy:std::array<std::array<float,2>,4>{{{{-.6f,-.6f}},{{.6f,-.6f}},{{.6f,.6f}},{{-.6f,.6f}}}}) {
        geometry.insert(geometry.end(),{xy[0],xy[1],-.2f,0,0});if(animated)geometry.push_back(0);
    }
    const uint32_t indices[]{0,1,2,0,2,3};fqvk::Mesh(16,geometry.data(),4,animated?6:5,indices,6);
    uint8_t white[]{255,255,255,255};fqvk::Texture(17,1,1,white,false,false);
    Set(15,"uMvp",{1,0,0,0,0,1,0,0,0,0,1,0,.2f,0,0,1});
    Set(15,"uTint",{0,1,0,animated?1.0f:.5f});if(animated)Set(15,"uAge",{1});
    const float left[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const float right[]{1,0,0,0,0,1,0,0,0,0,1,0,.8f,0,0,1};const float eye[]{0,0,2};
    assert(fqvk::BeginWorldProbe(left,right,eye,eye));
    fqgl::state.texture2d[0].Change(5);fqvk::hooks.draw(1,9,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);
    fqgl::state.texture2d[0].Change(17);fqgl::state.depthWrite.Change(false);fqgl::state.blend.Change(true);
    fqgl::state.srcRgb.Change(GL_SRC_ALPHA);fqgl::state.dstRgb.Change(GL_ONE_MINUS_SRC_ALPHA);
    fqgl::state.srcAlpha.Change(GL_ONE);fqgl::state.dstAlpha.Change(GL_ONE_MINUS_SRC_ALPHA);
    fqvk::hooks.draw(15,16,GL_TRIANGLES,0,6,GL_UNSIGNED_INT,0,1);fqvk::EndWorldProbe();
    const auto pixels=fqvk::renderer->Readback();
    Near(Pixel(pixels,0,32,32,0),.5f);Near(Pixel(pixels,0,32,32,1),.5f);
    Near(Pixel(pixels,0,17,32,1),0); // object translation is retained
    Near(Pixel(pixels,1,32,32,1),0);Near(Pixel(pixels,1,55,32,1),.5f);
}

int main() {
    for(int mode=0;mode<4;++mode) {
        assert(fqvk::InitializeWorldProbe(reinterpret_cast<XrInstance>(uintptr_t(1)),1,64,64));
        Scene(mode==1,mode==2,mode==3);fqvk::ShutdownWorldProbe();assert(!fqvk::WorldProbeActive());
    }
    assert(fqvk::InitializeWorldProbe(reinterpret_cast<XrInstance>(uintptr_t(1)),1,64,64));Sky();fqvk::ShutdownWorldProbe();
    assert(fqvk::InitializeWorldProbe(reinterpret_cast<XrInstance>(uintptr_t(1)),1,64,64));Water();fqvk::ShutdownWorldProbe();
    assert(fqvk::InitializeWorldProbe(reinterpret_cast<XrInstance>(uintptr_t(1)),1,64,64));Post();fqvk::ShutdownWorldProbe();
    for(bool animated:{false,true}){assert(fqvk::InitializeWorldProbe(reinterpret_cast<XrInstance>(uintptr_t(1)),1,64,64));Ui(animated);fqvk::ShutdownWorldProbe();}
    std::cout<<"Vulkan native multiview: original world/LAND shaders, indexed NIF layout, per-eye cameras, depth equality, ordered paint blending, layered water refraction, retained opaque depth, original exposure/bloom/film/gamma, transformed UI/notification alpha and recreation passed\n";
}
