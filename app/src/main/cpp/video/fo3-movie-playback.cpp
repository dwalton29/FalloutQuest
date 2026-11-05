#include "fo3-movie-playback.h"
#include "fo3-movie-request.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cmath>
#include <deque>
#include <dirent.h>
#include <mutex>
#include <strings.h>
#include <sys/stat.h>
#include <thread>
#ifdef __ANDROID__
#include <aaudio/AAudio.h>
#endif
namespace fo3movie {
std::string ResolvePath(const std::string& root,const std::string& relative){
    if(!ValidPath(relative))return {};
    std::string path=root;std::string parts="Video/"+relative;std::replace(parts.begin(),parts.end(),'\\','/');
    size_t from=0;
    while(from<parts.size()){
        const auto end=parts.find('/',from);const auto part=parts.substr(from,end==std::string::npos?end:end-from);
        if(path.empty()||path.back()!='/')path+='/';
        DIR* folder=opendir(path.c_str());if(!folder)return {};
        std::string found;while(auto* entry=readdir(folder))if(strcasecmp(entry->d_name,part.c_str())==0){found=entry->d_name;break;}
        closedir(folder);if(found.empty())return {};path+=found;
        if(end==std::string::npos)break;
        from=end+1;
    }
    struct stat file{};return stat(path.c_str(),&file)==0&&S_ISREG(file.st_mode)&&file.st_size>0?path:std::string{};
}
struct Playback::Impl {
    mutable std::mutex mutex;std::condition_variable cv;std::thread worker;
    std::deque<std::shared_ptr<const Decoded>> frames;
    std::shared_ptr<const Decoded> displayed;
    std::atomic<bool> cancel{false},focused{false};
    std::atomic<float> gain{1};
    std::string error;bool eof=false,started=false,audioRunning=false;
    State state=State::Opening;double clock=0,lastNow=0,end=0;
    static constexpr uint64_t Capacity=1u<<19;
    std::vector<float> ring=std::vector<float>(Capacity);
    std::atomic<uint64_t> read{0},written{0};int rate=0,channels=0;
#ifdef __ANDROID__
    std::atomic<AAudioStream*> stream{nullptr};
    static aaudio_data_callback_result_t Audio(AAudioStream*,void* owner,void* output,int32_t frameCount){
        auto& p=*static_cast<Impl*>(owner);auto* dst=static_cast<float*>(output);
        const auto count=uint64_t(frameCount)*p.channels;const auto r=p.read.load(std::memory_order_relaxed),w=p.written.load(std::memory_order_acquire);
        const auto supplied=std::min(count,w-r);const float gain=p.gain.load();
        for(uint64_t i=0;i<supplied;++i)dst[i]=p.ring[(r+i)&(Capacity-1)]*gain;
        std::fill(dst+supplied,dst+count,0.f);p.read.store(r+supplied,std::memory_order_release);return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }
    bool AudioOpen(int sampleRate,int channelCount){
        if(stream.load())return rate==sampleRate&&channels==channelCount;
        rate=sampleRate;channels=channelCount;AAudioStreamBuilder* builder=nullptr;
        if(AAudio_createStreamBuilder(&builder)!=AAUDIO_OK)return false;
        AAudioStreamBuilder_setDirection(builder,AAUDIO_DIRECTION_OUTPUT);
        AAudioStreamBuilder_setFormat(builder,AAUDIO_FORMAT_PCM_FLOAT);
        AAudioStreamBuilder_setSampleRate(builder,rate);AAudioStreamBuilder_setChannelCount(builder,channels);
        AAudioStreamBuilder_setSharingMode(builder,AAUDIO_SHARING_MODE_SHARED);
        AAudioStreamBuilder_setPerformanceMode(builder,AAUDIO_PERFORMANCE_MODE_NONE);
        AAudioStreamBuilder_setDataCallback(builder,Audio,this);
        AAudioStream* opened=nullptr;const auto result=AAudioStreamBuilder_openStream(builder,&opened);AAudioStreamBuilder_delete(builder);
        if(result!=AAUDIO_OK)return false;
        if(AAudioStream_getSampleRate(opened)!=rate||AAudioStream_getChannelCount(opened)!=channels||AAudioStream_getFormat(opened)!=AAUDIO_FORMAT_PCM_FLOAT){AAudioStream_close(opened);return false;}
        stream.store(opened);return true;
    }
#endif
    bool AudioPush(const Decoded& audio){
#ifdef __ANDROID__
        if(!AudioOpen(audio.sampleRate,audio.channels))return false;
        size_t at=0;
        while(at<audio.samples.size()&&!cancel){
            const auto w=written.load(std::memory_order_relaxed),r=read.load(std::memory_order_acquire);
            const auto count=std::min<uint64_t>(audio.samples.size()-at,Capacity-(w-r));
            if(!count){std::unique_lock<std::mutex> lock(mutex);cv.wait_for(lock,std::chrono::milliseconds(5));continue;}
            for(uint64_t i=0;i<count;++i)ring[(w+i)&(Capacity-1)]=audio.samples[at+i];
            written.store(w+count,std::memory_order_release);at+=size_t(count);
        }
#else
        (void)audio;
#endif
        return !cancel;
    }
    void Run(std::string path){
        Decoder decoder;std::string failure;
        if(!decoder.Open(path,failure)){std::lock_guard<std::mutex> lock(mutex);error=failure;return;}
        Decoded decoded;bool seenVideo=false;
        while(!cancel){
            if(!decoder.Next(decoded,failure))break;
            {std::lock_guard<std::mutex> lock(mutex);end=std::max(end,decoded.pts+decoded.duration);}
            if(decoded.kind==Decoded::Kind::Audio){if(!AudioPush(decoded)){if(!cancel)failure="Movie audio output unavailable";break;}}
            else{
                seenVideo=true;auto frame=std::make_shared<Decoded>(std::move(decoded));
                std::unique_lock<std::mutex> lock(mutex);cv.wait(lock,[&]{return cancel||frames.size()<3;});
                if(cancel)break;
                frames.push_back(std::move(frame));
            }
        }
        std::lock_guard<std::mutex> lock(mutex);eof=true;
        if(!failure.empty())error=failure;else if(!seenVideo&&!cancel)error="Movie contained no video frames";
    }
    ~Impl(){
        cancel=true;cv.notify_all();if(worker.joinable())worker.join();
#ifdef __ANDROID__
        if(auto* audio=stream.load()){AAudioStream_requestStop(audio);AAudioStream_close(audio);}
#endif
    }
};
Playback::Playback()=default;Playback::~Playback()=default;
void Playback::Start(const std::string& path){Stop();impl_=std::make_unique<Impl>();auto* p=impl_.get();p->worker=std::thread([p,path]{p->Run(path);});}
void Playback::Stop(){impl_.reset();}
void Playback::Tick(double now,bool focused,float volume){
    if(!impl_)return;
    auto& p=*impl_;const bool wasFocused=p.focused.exchange(focused);p.gain=std::clamp(volume,0.f,1.f);
    {
        std::lock_guard<std::mutex> lock(p.mutex);
        if(!p.error.empty())p.state=State::Failed;
        else if(focused){
            if(!p.started&&!p.frames.empty()){p.started=true;p.clock=p.frames.front()->pts;p.lastNow=now;}
            else if(p.started&&wasFocused&&p.lastNow>0&&std::isfinite(now))p.clock+=std::clamp(now-p.lastNow,0.,.25);
            while(!p.frames.empty()&&p.frames.front()->pts<=p.clock+.001){p.displayed=p.frames.front();p.frames.pop_front();}
            if(p.started)p.state=p.eof&&p.frames.empty()&&p.clock>=p.end?State::Finished:State::Playing;
        }
        p.lastNow=now;
    }
#ifdef __ANDROID__
    if(auto* stream=p.stream.load()){
        const bool play=focused&&p.started&&p.state==State::Playing;
        if(play!=p.audioRunning){const auto result=play?AAudioStream_requestStart(stream):AAudioStream_requestPause(stream);
            p.audioRunning=play;
            if(result!=AAUDIO_OK){std::lock_guard<std::mutex> lock(p.mutex);p.error="Movie audio stream stopped";p.state=State::Failed;}
        }
    }
#endif
    p.cv.notify_all();
}
Playback::State Playback::Status()const{return impl_?impl_->state:State::Finished;}
std::string Playback::Error()const{if(!impl_)return {};std::lock_guard<std::mutex> lock(impl_->mutex);return impl_->error;}
std::shared_ptr<const Decoded> Playback::Frame()const{if(!impl_)return {};std::lock_guard<std::mutex> lock(impl_->mutex);return impl_->displayed;}
}
