#pragma once
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <string>

namespace fo3notify {
// Vanilla executable uses "%s %s" and "%i %s%s %s", sPlural="(s)",
// sAddItemtoInventory="added"; pickup calls QueueMessage with 2.0 seconds.
inline std::string ItemText(const std::string& name,int32_t count,
                            const std::string& added="added",const std::string& plural="(s)") {
    return count>1 ? std::to_string(count)+" "+name+plural+" "+added : name+" "+added;
}
struct Entry {std::array<char,512> text{};double enqueued=0;};
class Queue {
public:
    static constexpr size_t Capacity=16;
    // Hold recovered from executable; fade/maximum backlog age are explicit
    // VR policies, not claimed to be vanilla executable timing.
    static constexpr double Hold=2,Fade=.5,MaxAge=40;
    void Clear() {head_=size_=0;started_=0; ++revision_;}
    void Enqueue(const std::string& text) {
        if(size_==Capacity) {head_=(head_+1)%Capacity;--size_;started_=now_;++revision_;}
        auto& e=entries_[(head_+size_)%Capacity];
        std::snprintf(e.text.data(),e.text.size(),"%s",text.c_str());e.enqueued=now_;
        if(!size_) {started_=now_;++revision_;}
        ++size_;
    }
    // Called once per OpenXR frame. Rendering both eyes only reads this state.
    void Advance(double now) {
        now_=now;
        while(size_ && (now_-started_>=Hold+Fade || now_-entries_[head_].enqueued>MaxAge)) {
            head_=(head_+1)%Capacity;--size_;started_=now_;++revision_;
        }
    }
    const char* Text() const {return size_ ? entries_[head_].text.data() : "";}
    float Alpha() const {return size_ ? static_cast<float>(std::clamp((Hold+Fade-(now_-started_))/Fade,0.0,1.0)) : 0;}
    size_t Size() const {return size_;}
    uint64_t Revision() const {return revision_;}
private:
    std::array<Entry,Capacity> entries_{};
    size_t head_=0,size_=0;
    double started_=0,now_=0;
    uint64_t revision_=0;
};
inline Queue& Notifications() {static Queue queue;return queue;}
} // namespace fo3notify
