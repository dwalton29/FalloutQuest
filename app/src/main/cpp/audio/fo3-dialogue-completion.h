#pragma once
#include <cstdint>
#include <mutex>
namespace fo3audio {
// A stale platform callback must not overwrite a newer response's completion.
class DialogueCompletionMailbox {
    std::mutex mutex;
    uint32_t current=0;
    uint64_t completed=0;
public:
    void Start(uint32_t token) {std::lock_guard<std::mutex> lock(mutex);current=token;completed=0;}
    void Stop() {Start(0);}
    bool Done(uint32_t token,bool success) {
        std::lock_guard<std::mutex> lock(mutex);
        if(!token||token!=current||completed)return false;
        completed=(uint64_t(token)<<1)|(success?1u:0u);return true;
    }
    uint64_t Take() {
        std::lock_guard<std::mutex> lock(mutex);
        const auto value=completed;completed=0;if(value)current=0;return value;
    }
};
}
