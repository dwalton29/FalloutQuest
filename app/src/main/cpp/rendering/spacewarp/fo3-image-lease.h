#pragma once
#include <openxr/openxr.h>
namespace fqspacewarp {
// A failed wait must not be followed by release or another acquisition. The
// caller latches a producer failure and retains the session until shutdown.
class ImageLease {
    XrSwapchain swapchain_=XR_NULL_HANDLE;
    bool waited_=false;
public:
    uint32_t index=0;
    ImageLease()=default;ImageLease(const ImageLease&)=delete;ImageLease& operator=(const ImageLease&)=delete;
    bool Acquire(XrSwapchain swapchain){
        if(swapchain_!=XR_NULL_HANDLE)return false;
        XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        if(XR_FAILED(xrAcquireSwapchainImage(swapchain,&acquire,&index)))return false;
        swapchain_=swapchain;XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout=XR_INFINITE_DURATION;
        const auto result=xrWaitSwapchainImage(swapchain_,&wait);
        waited_=result==XR_SUCCESS;return waited_;
    }
    bool Release(){
        if(!waited_)return false;
        XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        const bool ok=XR_SUCCEEDED(xrReleaseSwapchainImage(swapchain_,&release));
        waited_=false;swapchain_=XR_NULL_HANDLE;return ok;
    }
    ~ImageLease(){if(waited_)Release();}
};
} // namespace fqspacewarp
