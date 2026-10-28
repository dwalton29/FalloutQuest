#pragma once
#include <openxr/openxr.h>
#include "fo3-frame-history.h"
#include <cstring>
#include <vector>
namespace fqspacewarp {
enum class Api { None,EXT,FB };
// ToVirtualPose uses only a rigid yaw + translation for artificial locomotion.
// Head/controller tracking belongs in VP matrices, never in this delta.
inline bool ArtificialYawDelta(const Matrix& previous,const Matrix& current,Api api,XrPosef& delta){
    delta={};delta.orientation.w=1;
    if(api==Api::None)return false;
    for(const auto* m:{&previous,&current}){
        if(!Finite(*m))return false;
        for(unsigned i:{1u,3u,4u,6u,7u,9u,11u})if(std::abs((*m)[i])>1e-4f)return false;
        if(std::abs((*m)[5]-1)>1e-4f||std::abs((*m)[15]-1)>1e-4f||std::abs((*m)[0]-(*m)[10])>1e-4f||
           std::abs((*m)[2]+(*m)[8])>1e-4f||std::abs((*m)[0]*(*m)[0]+(*m)[8]*(*m)[8]-1)>1e-4f)return false;
    }
    const float yaw=std::remainder(std::atan2(current[8],current[0])-std::atan2(previous[8],previous[0]),6.28318530718f);
    delta.orientation.y=std::sin(yaw*.5f);delta.orientation.w=std::cos(yaw*.5f);
    // EXT explicitly requires translation in current application coordinates.
    // Meta's FB guide specifies inverse(previousAppPose) * currentAppPose.
    const auto& basis=api==Api::EXT?current:previous;
    const float x=current[12]-previous[12],y=current[13]-previous[13],z=current[14]-previous[14];
    delta.position={basis[0]*x+basis[1]*y+basis[2]*z,basis[4]*x+basis[5]*y+basis[6]*z,basis[8]*x+basis[9]*y+basis[10]*z};
    return std::isfinite(delta.position.x)&&std::isfinite(delta.position.y)&&std::isfinite(delta.position.z);
}
struct Target {XrSwapchain motion=XR_NULL_HANDLE,depth=XR_NULL_HANDLE;uint32_t width=0,height=0;};
// Owns only runtime swapchains. Graphics backends must acquire/render/release
// these before supplying matching Coverage; allocation alone never enables ASW.
class Session {
  Api api_=Api::None;
  std::array<Target,2> targets_{};
  std::array<XrFrameSynthesisInfoEXT,2> ext_{};
  std::array<XrCompositionLayerSpaceWarpInfoFB,2> fb_{};
  bool requested_=false,allocated_=false;
public:
  Session()=default;Session(const Session&)=delete;Session& operator=(const Session&)=delete;
  Api Backend()const{return api_;}
  bool Requested()const{return requested_;}
  bool Allocated()const{return allocated_;}
  const Target& Eye(unsigned eye)const{return targets_.at(eye);}
  const char* Negotiate(bool requested,const std::vector<XrExtensionProperties>& available){
    requested_=requested;api_=Api::None;if(!requested)return nullptr;
    bool ext=false,fb=false;
    for(const auto& e:available){ext|=!std::strcmp(e.extensionName,XR_EXT_FRAME_SYNTHESIS_EXTENSION_NAME);fb|=!std::strcmp(e.extensionName,XR_FB_SPACE_WARP_EXTENSION_NAME);}
    if(ext){api_=Api::EXT;return XR_EXT_FRAME_SYNTHESIS_EXTENSION_NAME;}
    if(fb){api_=Api::FB;return XR_FB_SPACE_WARP_EXTENSION_NAME;}return nullptr;
  }
  void Destroy(){for(auto& t:targets_){if(t.motion!=XR_NULL_HANDLE)xrDestroySwapchain(t.motion);if(t.depth!=XR_NULL_HANDLE)xrDestroySwapchain(t.depth);t={};}allocated_=false;}
  bool Create(XrInstance instance,XrSystemId system,XrSession session,int64_t motionFormat,int64_t depthFormat){
    Destroy();if(api_==Api::None)return false;
    std::array<uint32_t,2> width{},height{};
    if(api_==Api::EXT){
      std::array<XrFrameSynthesisConfigViewEXT,2> config{{{XR_TYPE_FRAME_SYNTHESIS_CONFIG_VIEW_EXT},{XR_TYPE_FRAME_SYNTHESIS_CONFIG_VIEW_EXT}}};
      std::array<XrViewConfigurationView,2> views{{{XR_TYPE_VIEW_CONFIGURATION_VIEW},{XR_TYPE_VIEW_CONFIGURATION_VIEW}}};
      for(unsigned i=0;i<2;++i)views[i].next=&config[i];
      uint32_t count=0;
      if(XR_FAILED(xrEnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,2,&count,views.data()))||count!=2)return false;
      for(unsigned i=0;i<2;++i){width[i]=config[i].recommendedMotionVectorImageRectWidth;height[i]=config[i].recommendedMotionVectorImageRectHeight;}
    }else{
      XrSystemSpaceWarpPropertiesFB warp{XR_TYPE_SYSTEM_SPACE_WARP_PROPERTIES_FB};XrSystemProperties properties{XR_TYPE_SYSTEM_PROPERTIES};properties.next=&warp;
      if(XR_FAILED(xrGetSystemProperties(instance,system,&properties)))return false;
      width.fill(warp.recommendedMotionVectorImageRectWidth);height.fill(warp.recommendedMotionVectorImageRectHeight);
    }
    for(unsigned i=0;i<2;++i){
      if(!width[i]||!height[i]||width[i]>8192||height[i]>8192){Destroy();return false;}
      auto& target=targets_[i];target.width=width[i];target.height=height[i];
      XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};info.width=width[i];info.height=height[i];info.sampleCount=1;info.faceCount=1;info.arraySize=1;info.mipCount=1;
      info.format=motionFormat;info.usageFlags=XR_SWAPCHAIN_USAGE_SAMPLED_BIT|XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
      if(XR_FAILED(xrCreateSwapchain(session,&info,&target.motion))){Destroy();return false;}
      info.format=depthFormat;info.usageFlags=XR_SWAPCHAIN_USAGE_SAMPLED_BIT|XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      if(XR_FAILED(xrCreateSwapchain(session,&info,&target.depth))){Destroy();return false;}
    }
    allocated_=true;return true;
  }
  bool Attach(std::array<XrCompositionLayerProjectionView,2>& views,const History& history,
              const std::array<Coverage,2>& coverage,const XrPosef& appDelta,float nearZ,float farZ){
    if(!allocated_||!history.CanSynthesize(coverage)||!std::isfinite(nearZ)||!std::isfinite(farZ)||nearZ<=0||farZ<=nearZ)return false;
    const float norm=appDelta.orientation.x*appDelta.orientation.x+appDelta.orientation.y*appDelta.orientation.y+appDelta.orientation.z*appDelta.orientation.z+appDelta.orientation.w*appDelta.orientation.w;
    if(!std::isfinite(norm)||std::abs(norm-1)>1e-3f||!std::isfinite(appDelta.position.x)||!std::isfinite(appDelta.position.y)||!std::isfinite(appDelta.position.z))return false;
    for(unsigned eye=0;eye<2;++eye)if(views[eye].next==&ext_[eye]||views[eye].next==&fb_[eye])return false;
    for(unsigned eye=0;eye<2;++eye){
      const auto& t=targets_[eye];XrSwapchainSubImage motion{t.motion,{{0,0},{int32_t(t.width),int32_t(t.height)}},0};auto depth=motion;depth.swapchain=t.depth;
      if(api_==Api::EXT){auto& info=ext_[eye];info={XR_TYPE_FRAME_SYNTHESIS_INFO_EXT};info.next=views[eye].next;
        info.layerFlags=XR_FRAME_SYNTHESIS_INFO_REQUEST_RELAXED_FRAME_INTERVAL_BIT_EXT;info.motionVectorSubImage=motion;info.depthSubImage=depth;info.appSpaceDeltaPose=appDelta;
        info.motionVectorScale={1,1,1,1};info.motionVectorOffset={0,0,0,0};info.minDepth=0;info.maxDepth=1;info.nearZ=nearZ;info.farZ=farZ;views[eye].next=&info;
      }else{auto& info=fb_[eye];info={XR_TYPE_COMPOSITION_LAYER_SPACE_WARP_INFO_FB};info.next=views[eye].next;info.motionVectorSubImage=motion;info.depthSubImage=depth;info.appSpaceDeltaPose=appDelta;
        info.minDepth=0;info.maxDepth=1;info.nearZ=nearZ;info.farZ=farZ;views[eye].next=&info;}
    }
    return true;
  }
};
} // namespace fqspacewarp
