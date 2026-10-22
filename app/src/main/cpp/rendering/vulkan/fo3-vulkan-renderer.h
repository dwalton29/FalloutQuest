#pragma once
#include <cstdint>
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#ifndef XR_USE_GRAPHICS_API_VULKAN
#define XR_USE_GRAPHICS_API_VULKAN
#endif
#include <openxr/openxr_platform.h>

namespace fqvk {
// World validation runs alongside the usable GLES renderer until all passes
// have native equivalents. Never use its partial GPU time as application FPS.
bool InitializeWorldProbe(XrInstance instance,XrSystemId system,uint32_t width,uint32_t height,const char* outputDirectory=nullptr);
void ShutdownWorldProbe();
bool BeginWorldProbe(const float* leftMvp,const float* rightMvp,
                     const float* leftEye,const float* rightEye);
void EndWorldProbe();
bool WorldProbeActive();
}
