#include "rendering/spacewarp/fo3-openxr-spacewarp.h"
#include "rendering/spacewarp/fo3-image-lease.h"
#include <cassert>
#include <set>
#include <string>
#include <type_traits>
using namespace fqspacewarp;
static unsigned creates=0,failAt=0;static uint32_t viewsCount=2,width=368;
static std::set<XrSwapchain> live;
static XrResult acquireResult=XR_SUCCESS,waitResult=XR_SUCCESS,releaseResult=XR_SUCCESS;
static unsigned acquisitions=0,waits=0,releases=0;
extern "C" {
XRAPI_ATTR XrResult XRAPI_CALL xrAcquireSwapchainImage(XrSwapchain,const XrSwapchainImageAcquireInfo*,uint32_t* index){++acquisitions;*index=2;return acquireResult;}
XRAPI_ATTR XrResult XRAPI_CALL xrWaitSwapchainImage(XrSwapchain,const XrSwapchainImageWaitInfo* info){++waits;assert(info->timeout==XR_INFINITE_DURATION);return waitResult;}
XRAPI_ATTR XrResult XRAPI_CALL xrReleaseSwapchainImage(XrSwapchain,const XrSwapchainImageReleaseInfo*){++releases;return releaseResult;}
XRAPI_ATTR XrResult XRAPI_CALL xrDestroySwapchain(XrSwapchain handle){assert(live.erase(handle)==1);return XR_SUCCESS;}
XRAPI_ATTR XrResult XRAPI_CALL xrCreateSwapchain(XrSession,const XrSwapchainCreateInfo* info,XrSwapchain* handle){
 assert(info->sampleCount==1&&info->arraySize==1&&info->mipCount==1);
 assert(info->usageFlags&XR_SWAPCHAIN_USAGE_SAMPLED_BIT);
 assert(info->usageFlags&(info->format==1?XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT:XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT));
 ++creates;if(creates==failAt)return XR_ERROR_RUNTIME_FAILURE;
 *handle=reinterpret_cast<XrSwapchain>(uintptr_t(creates));live.insert(*handle);return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateViewConfigurationViews(XrInstance,XrSystemId,XrViewConfigurationType,uint32_t capacity,uint32_t* count,XrViewConfigurationView* views){
 assert(capacity==2);*count=viewsCount;
 for(unsigned i=0;i<2;++i){auto* config=static_cast<XrFrameSynthesisConfigViewEXT*>(views[i].next);config->recommendedMotionVectorImageRectWidth=width+i;config->recommendedMotionVectorImageRectHeight=400+i;}
 return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetSystemProperties(XrInstance,XrSystemId,XrSystemProperties* properties){
 auto* fb=static_cast<XrSystemSpaceWarpPropertiesFB*>(properties->next);fb->recommendedMotionVectorImageRectWidth=300;fb->recommendedMotionVectorImageRectHeight=320;return XR_SUCCESS;
}
}
Frame Input(uint64_t serial){Frame f;f.serial=serial;f.displayTime=int64_t(serial)*1000;f.world=0x3c;f.sceneReady=f.focused=true;return f;}
Matrix Yaw(float yaw,float x=0,float y=0,float z=0){auto m=fqskin::Identity();m[0]=m[10]=std::cos(yaw);m[2]=-std::sin(yaw);m[8]=std::sin(yaw);m[12]=x;m[13]=y;m[14]=z;return m;}
int main(){
 XrPosef motionDelta{};
 for(auto api:{Api::EXT,Api::FB}){
   assert(ArtificialYawDelta(Yaw(.4f,1,2,3),Yaw(.4f,1,2,3),api,motionDelta));
   assert(std::abs(motionDelta.orientation.y)<1e-6f&&std::abs(motionDelta.orientation.w-1)<1e-6f&&motionDelta.position.x==0&&motionDelta.position.y==0&&motionDelta.position.z==0);
   assert(ArtificialYawDelta(Yaw(1.5707963268f),Yaw(1.5707963268f,1,2,0),api,motionDelta));
   assert(std::abs(motionDelta.position.x)<1e-6f&&motionDelta.position.y==2&&std::abs(motionDelta.position.z-1)<1e-6f);
   assert(ArtificialYawDelta(Yaw(0),Yaw(1.5707963268f,1,0,0),api,motionDelta));
   assert(std::abs(motionDelta.orientation.y-.70710678f)<1e-6f&&std::abs(motionDelta.orientation.w-.70710678f)<1e-6f);
   if(api==Api::EXT)assert(std::abs(motionDelta.position.x)<1e-6f&&std::abs(motionDelta.position.z-1)<1e-6f);
   else assert(std::abs(motionDelta.position.x-1)<1e-6f&&std::abs(motionDelta.position.z)<1e-6f);
   assert(ArtificialYawDelta(Yaw(3.13f),Yaw(-3.13f),api,motionDelta));assert(motionDelta.orientation.w>.999f&&motionDelta.orientation.y>0);
 }
 auto invalidYaw=Yaw(0);invalidYaw[4]=.1f;assert(!ArtificialYawDelta(Yaw(0),invalidYaw,Api::EXT,motionDelta));
 assert(!ArtificialYawDelta(Yaw(0),Yaw(0),Api::None,motionDelta));
 static_assert(!std::is_copy_constructible_v<Session>);
 std::vector<XrExtensionProperties> extensions(2);std::strcpy(extensions[0].extensionName,XR_FB_SPACE_WARP_EXTENSION_NAME);std::strcpy(extensions[1].extensionName,XR_EXT_FRAME_SYNTHESIS_EXTENSION_NAME);
 Session session;assert(!session.Negotiate(false,extensions));assert(session.Negotiate(true,extensions)==std::string(XR_EXT_FRAME_SYNTHESIS_EXTENSION_NAME));
 assert(session.Create(XR_NULL_HANDLE,1,XR_NULL_HANDLE,1,2)&&live.size()==4);assert(session.Eye(0).width==368&&session.Eye(1).width==369);
 History history;
 for(unsigned i=1;i<=2;++i){assert(history.Begin(Input(i)));history.EyeRendered(0);history.EyeRendered(1);assert(history.Finish(true));}
 assert(history.Begin(Input(3)));history.EyeRendered(0);history.EyeRendered(1);
 std::array<XrCompositionLayerProjectionView,2> views{{{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}}};
 XrCompositionLayerDepthInfoKHR depth{XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR};for(auto& v:views)v.next=&depth;
 std::array<Coverage,2> coverage{};XrPosef delta{};delta.orientation.w=1;
 assert(!session.Attach(views,history,coverage,delta,.04f,1000)&&views[0].next==&depth);
 for(auto& c:coverage){c.serial=3;c.motion=c.depth=c.staticWorld=c.cutouts=c.hands=c.weapons=true;}
 assert(session.Attach(views,history,coverage,delta,.04f,1000));
 const auto* info=static_cast<const XrFrameSynthesisInfoEXT*>(views[0].next);assert(info->next==&depth&&info->motionVectorSubImage.swapchain==session.Eye(0).motion);
 assert(info->layerFlags==XR_FRAME_SYNTHESIS_INFO_REQUEST_RELAXED_FRAME_INTERVAL_BIT_EXT&&info->nearZ==.04f&&info->farZ==1000);
 assert(!session.Attach(views,history,coverage,delta,.04f,1000));
 for(auto& v:views)v.next=&depth;
 coverage[1].weapons=false;assert(!session.Attach(views,history,coverage,delta,.04f,1000)&&views[0].next==&depth);coverage[1].weapons=true;
 auto interior=Input(4);interior.world=0;assert(!history.Begin(interior));assert(!session.Attach(views,history,coverage,delta,.04f,1000));
 for(unsigned i=5;i<=6;++i){assert(history.Begin(Input(i)));history.EyeRendered(0);history.EyeRendered(1);assert(history.Finish(true));}
 assert(history.Begin(Input(7)));history.EyeRendered(0);history.EyeRendered(1);for(auto& c:coverage)c.serial=7;
 assert(ArtificialYawDelta(Yaw(0),Yaw(.1f,.2f,0,-.1f),Api::EXT,delta));
 assert(session.Attach(views,history,coverage,delta,.04f,1000));
 info=static_cast<const XrFrameSynthesisInfoEXT*>(views[0].next);assert(info->appSpaceDeltaPose.position.z==delta.position.z&&info->appSpaceDeltaPose.orientation.y==delta.orientation.y);
 session.Destroy();assert(live.empty());
 extensions.resize(1);assert(session.Negotiate(true,extensions)==std::string(XR_FB_SPACE_WARP_EXTENSION_NAME));assert(session.Create(XR_NULL_HANDLE,1,XR_NULL_HANDLE,1,2));
 for(auto& v:views)v.next=nullptr;
 assert(session.Attach(views,history,coverage,delta,.04f,1000));assert(static_cast<const XrCompositionLayerSpaceWarpInfoFB*>(views[0].next)->type==XR_TYPE_COMPOSITION_LAYER_SPACE_WARP_INFO_FB);
 session.Destroy();assert(live.empty());
 assert(session.Negotiate(true,extensions));failAt=creates+3;assert(!session.Create(XR_NULL_HANDLE,1,XR_NULL_HANDLE,1,2)&&live.empty());failAt=0;
 extensions.resize(2);std::strcpy(extensions[1].extensionName,XR_EXT_FRAME_SYNTHESIS_EXTENSION_NAME);session.Negotiate(true,extensions);width=0;
 assert(!session.Create(XR_NULL_HANDLE,1,XR_NULL_HANDLE,1,2)&&live.empty());
 session.Destroy();
 const auto handle=reinterpret_cast<XrSwapchain>(uintptr_t(1000));
 {ImageLease lease;assert(lease.Acquire(handle)&&lease.index==2);assert(!lease.Acquire(handle));assert(lease.Release());assert(!lease.Release());}
 assert(acquisitions==1&&waits==1&&releases==1);
 acquireResult=XR_ERROR_RUNTIME_FAILURE;{ImageLease lease;assert(!lease.Acquire(handle));}assert(waits==1&&releases==1);acquireResult=XR_SUCCESS;
 waitResult=XR_ERROR_RUNTIME_FAILURE;{ImageLease lease;assert(!lease.Acquire(handle));assert(!lease.Acquire(handle));}assert(waits==2&&releases==1);
 waitResult=XR_TIMEOUT_EXPIRED;{ImageLease lease;assert(!lease.Acquire(handle));}assert(releases==1);
 waitResult=XR_SUCCESS;{ImageLease lease;assert(lease.Acquire(handle));}assert(releases==2);
 releaseResult=XR_ERROR_RUNTIME_FAILURE;{ImageLease lease;assert(lease.Acquire(handle));assert(!lease.Release());}assert(releases==3);
}
