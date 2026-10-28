#pragma once
#include <cstring>
namespace fqrender {
// Vulkan owns normal gameplay. Keep an explicit GLES comparison override.
inline bool SelectVulkan(const char* value){return !value||std::strcmp(value,"gles")!=0;}
}
