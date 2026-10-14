#pragma once
#include <cstddef>
#include <vector>

namespace fo3scene {
enum class GpuStageResult { Pending, Ready, Failed };
// Render-thread staging only. Default Object owns no handles. Upload writes
// directly into preallocated private slots so exceptions/failure can reclaim
// partially initialized resources with the same disposer as complete objects.
template<class Object>
class ResidentGpuStage {
public:
    using Objects=std::vector<Object>;
    using Dispose=void(*)(Objects&) noexcept;
    ResidentGpuStage(size_t count,Dispose dispose):dispose_(dispose),objects_(count) {
        if(!dispose_)phase_=GpuStageResult::Failed;
        else if(!count)phase_=GpuStageResult::Ready;
    }
    ResidentGpuStage(const ResidentGpuStage&)=delete;
    ResidentGpuStage& operator=(const ResidentGpuStage&)=delete;
    ~ResidentGpuStage(){if(dispose_)dispose_(objects_);}
    template<class Upload>
    GpuStageResult Advance(Upload upload) {
        if(phase_!=GpuStageResult::Pending)return phase_;
        try {
            if(!upload(cursor_,objects_[cursor_]))return Fail();
        }catch(...){return Fail();}
        if(++cursor_==objects_.size())phase_=GpuStageResult::Ready;
        return phase_;
    }
    Objects* ReadyObjects() noexcept {return phase_==GpuStageResult::Ready?&objects_:nullptr;}
    size_t Cursor()const noexcept{return cursor_;}
    GpuStageResult Fail()noexcept {
        if(dispose_)dispose_(objects_);
        phase_=GpuStageResult::Failed;return phase_;
    }
private:
    Dispose dispose_;
    Objects objects_;
    size_t cursor_=0;
    GpuStageResult phase_=GpuStageResult::Pending;
};
} // namespace fo3scene
