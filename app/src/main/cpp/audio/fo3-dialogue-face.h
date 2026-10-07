#pragma once
#include "npc/fo3-facial-data.h"
#include <mutex>
#include <memory>
namespace fo3audio {
struct DialogueFaceSample {std::shared_ptr<const fo3face::Lip> lip;double seconds=-1;};
class DialogueFaceMailbox {
  std::mutex mutex;uint32_t current=0;DialogueFaceSample sample;
public:
  void Start(uint32_t token){std::lock_guard<std::mutex> lock(mutex);current=token;sample={};}
  void Asset(uint32_t token,std::shared_ptr<const fo3face::Lip> lip){std::lock_guard<std::mutex> lock(mutex);if(token&&token==current)sample.lip=std::move(lip);}
  void Position(uint32_t token,int milliseconds){std::lock_guard<std::mutex> lock(mutex);if(token&&token==current&&milliseconds>=0)sample.seconds=milliseconds/1000.;}
  DialogueFaceSample Read(uint32_t token){std::lock_guard<std::mutex> lock(mutex);return token&&token==current?sample:DialogueFaceSample{};}
};
}
