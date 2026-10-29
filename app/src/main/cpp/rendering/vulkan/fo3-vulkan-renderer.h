#pragma once
#include <cstdint>
#include <cstddef>
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#ifndef XR_USE_GRAPHICS_API_VULKAN
#define XR_USE_GRAPHICS_API_VULKAN
#endif
#include <openxr/openxr_platform.h>
#include "rendering/spacewarp/fo3-motion-capture.h"

namespace fqvk {
// World validation runs alongside the usable GLES renderer until all passes
// have native equivalents. Never use its partial GPU time as application FPS.
bool InitializeWorldProbe(XrInstance instance,XrSystemId system,uint32_t width,uint32_t height,const char* outputDirectory=nullptr);
void ShutdownWorldProbe();
bool BeginWorldProbe(const float* leftMvp,const float* rightMvp,
                     const float* leftEye,const float* rightEye,bool loading=false,const float* horizon=nullptr,bool splitComposition=false);
bool StereoBoundsVisible(const float* vertices,size_t count,size_t stride);
void PrewarmPipelines();
void EndWorldProbe();
// Experimental compositor owner; resource submission still uses the shared bridge.
bool GameplayBinding(XrGraphicsBindingVulkan2KHR& binding);
bool CreateGameplaySwapchain(XrSession session,bool submitDepth=false);
void DestroyGameplaySwapchain();
bool PresentGameplay(const XrView* views,XrCompositionLayerProjectionView* output,float nearZ,float farZ);
bool WorldProbeActive();
struct MotionSwapchainTarget {XrSwapchain motion,depth;uint32_t width,height;};
bool InitializeSpacewarp(const std::array<MotionSwapchainTarget,2>& targets);
void ShutdownSpacewarp();
void QueueSpacewarp(const fqmotion::Capture& capture,std::array<fqspacewarp::Coverage,2>& coverage);
bool SpacewarpOverlay(const XrView* views,XrCompositionLayerProjectionView* output);
}
