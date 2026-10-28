#pragma once
#include "fo3-motion-gles.h" // Must already be compiled before legacy GL aliases.
#include "rendering/gl-state-cache.h"
namespace fqmotion {
inline void (*reportBoundary)(const char*,uint32_t)=nullptr;
// Enclose the complete runtime handoff, not just our GL commands. In
// particular, invalidating the texture cache cannot undo a sampler binding
// left by image acquisition/release or a runtime compositor operation.
class DriverBoundary {
    fqgl::State cached_;
    std::optional<GlState> driver_;
    const char* label_=nullptr;
    bool report_=false;
public:
    explicit DriverBoundary(const char* label=nullptr,bool report=false):cached_(fqgl::state),label_(label),report_(report){if(!fqrender::nativeOnly)driver_.emplace();}
    DriverBoundary(const DriverBoundary&)=delete;
    DriverBoundary& operator=(const DriverBoundary&)=delete;
    ~DriverBoundary(){if(report_&&driver_&&reportBoundary)reportBoundary(label_,driver_->SamplerChanges());fqgl::state=cached_;} // driver_ restores the matching real state next.
};
} // namespace fqmotion
