#pragma once
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>

namespace fo3notify {
// Vanilla executable uses "%s %s" and "%i %s%s %s", sPlural="(s)",
// sAddItemtoInventory="added"; pickup calls QueueMessage with 2.0 seconds.
inline std::string ItemText(const std::string& name,int32_t count,
                            const std::string& added="added",const std::string& plural="(s)") {
    return count>1 ? std::to_string(count)+" "+name+plural+" "+added : name+" "+added;
}
struct Entry {std::array<char,512> text{};std::string icon,sound;double enqueued=0;bool quest=false,sounded=false;};
class Queue {
public:
    static constexpr size_t Capacity=16;
    // Hold recovered from executable; fade/maximum backlog age are explicit
    // VR policies, not claimed to be vanilla executable timing.
    static constexpr double Hold=2,Fade=.5,MaxAge=40;
    void Clear() {head_=size_=0;started_=0; ++revision_;}
    void Enqueue(const std::string& text,const std::string& icon="") {
        if(size_==Capacity) {head_=(head_+1)%Capacity;--size_;started_=now_;++revision_;}
        auto& e=entries_[(head_+size_)%Capacity];
        std::snprintf(e.text.data(),e.text.size(),"%s",text.c_str());e.enqueued=now_;e.icon=icon;e.sound.clear();e.quest=false;e.sounded=false;
        if(!size_) {started_=now_;++revision_;}
        ++size_;
    }
    // Executable GMST defaults: character fade-in 2s, remain 4s, fade-out 2s.
    // The native character stagger is 0.1s; the GPU evaluates each glyph fade.
    void EnqueueQuest(const std::string& text,const std::string& sound) {
        Enqueue(text);auto& e=entries_[(head_+size_-1)%Capacity];e.quest=true;e.sound=sound;
    }
    double Age() const {return size_?now_-started_:0;}
    double Duration() const {
        if(!Quest())return Hold+Fade;
        // Upper bound includes whitespace and wrapped/truncated characters;
        // no notification can disappear before its final emitted glyph fades.
        return 8.0+.1*std::strlen(Text());
    }
    static float CharacterAlpha(double age,double delay) {
        return float(std::clamp(std::min((age-delay)/2.0,(delay+8.0-age)/2.0),0.0,1.0));
    }
    bool Quest() const {return size_&&entries_[head_].quest;}
    std::string TakeSound() {
        if(!size_||entries_[head_].sounded)return {};
        auto& e=entries_[head_];e.sounded=true;return e.sound;
    }
    // Called once per OpenXR frame. Rendering both eyes only reads this state.
    void Advance(double now,bool visible=true) {
        if(!visible){
            const double delta=std::max(0.0,now-now_);started_+=delta;
            for(size_t i=0;i<size_;++i)entries_[(head_+i)%Capacity].enqueued+=delta;
            now_=now;return;
        }
        now_=now;
        while(size_ && (now_-started_>=Duration() || now_-entries_[head_].enqueued>MaxAge)) {
            head_=(head_+1)%Capacity;--size_;started_=now_;++revision_;
        }
    }
    const char* Text() const {return size_ ? entries_[head_].text.data() : "";}
    const std::string& Icon() const {static const std::string empty;return size_?entries_[head_].icon:empty;}
    float Alpha() const {
        if(!size_)return 0;
        const double age=now_-started_;
        if(Quest())return static_cast<float>(std::clamp(std::min(age/2.0,(Duration()-age)/2.0),0.0,1.0));
        return static_cast<float>(std::clamp((Hold+Fade-age)/Fade,0.0,1.0));
    }
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
