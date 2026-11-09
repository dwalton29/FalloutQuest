#include "fo3-audio.h"
#include "fo3-dialogue-completion.h"
#include "data/fo3-asset-store.h"
#include "fo3-audio-assets.h"
#include "fo3-audio-catalog.h"
#include "fo3-footsteps.h"
#include "ui/loading/fo3-loading-state.h"
#include "data/fo3-bsa-reader.h"
#include <algorithm>
#include <android/log.h>
#include <atomic>
#include <cctype>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <dirent.h>
#include <fstream>
#include <mutex>
#include <random>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <unordered_set>
namespace fo3audio {
namespace {
constexpr int RadioConversationEvent=19;
constexpr int LoadingEffectEvent=20;
struct Event {
  Event() = default;
  Event(int k, uint32_t form, bool enabled, std::string editor = {})
      : kind(k), id(form), active(enabled), name(std::move(editor)) {}
  int kind;
  uint32_t id;
  bool active;
  std::string name{};
  std::shared_ptr<const fo3pipdata::Definitions> definitions;
  fo3pipdata::SessionState state;
  std::vector<std::string> paths;
  std::array<float,4> volumes{1,.3f,1,1};
  unsigned dialogueLane=0;
  uint32_t radioTopic=0;
  bool intercom=false;
  float distance=-1,pan=0;
  uint64_t loadingGeneration=0;
  uint64_t loadingCueUs=0;
};
struct Runtime {
  JavaVM *vm = nullptr;
  jobject activity = nullptr;
  std::thread worker;
  std::mutex mutex;
  std::condition_variable cv;
  std::deque<Event> events;
  FootstepGait walk, run, sneak;
  bool wasSneaking=false;
  FootstepClock footsteps;
  bool stop = false;
  uint32_t lastCell = UINT32_MAX;
  bool lastActive = false;
  std::atomic<uint32_t> playingNote{0};
  std::array<DialogueCompletionMailbox,2> dialogueCompletion;
  std::array<DialogueFaceMailbox,2> dialogueFace;
} runtime;

void JNICALL DialogueDoneNative(JNIEnv*, jobject, jint token, jboolean success) {
  DialogueDone(uint32_t(token), success == JNI_TRUE);
}
void JNICALL BroadcastDoneNative(JNIEnv*, jobject, jint channel, jint generation) {
  BroadcastDone(channel, uint32_t(generation));
}
void JNICALL DialoguePositionNative(JNIEnv*, jobject, jint token,jint milliseconds) {
  for(auto& lane:runtime.dialogueFace)lane.Position(uint32_t(token),milliseconds);
}
jboolean JNICALL LoadingCurrentNative(JNIEnv*,jobject,jlong generation){
  return IsFo3LoadingAudioCurrent(uint64_t(generation))?JNI_TRUE:JNI_FALSE;
}
bool RegisterActivityNatives(JNIEnv* env, jobject activity) {
  if (!env || !activity) return false;
  jclass cls = env->GetObjectClass(activity);
  if (!cls) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    __android_log_print(ANDROID_LOG_ERROR, "FalloutQuest",
                        "AUDIO JNI GetObjectClass failed");
    return false;
  }
  JNINativeMethod methods[] = {
      {const_cast<char*>("audioDialoguePosition"), const_cast<char*>("(II)V"),
       reinterpret_cast<void*>(DialoguePositionNative)},
      {const_cast<char*>("audioDialogueDone"), const_cast<char*>("(IZ)V"),
       reinterpret_cast<void*>(DialogueDoneNative)},
      {const_cast<char*>("audioBroadcastDone"), const_cast<char*>("(II)V"),
       reinterpret_cast<void*>(BroadcastDoneNative)},
      {const_cast<char*>("audioLoadingCurrent"), const_cast<char*>("(J)Z"),
       reinterpret_cast<void*>(LoadingCurrentNative)},
  };
  const jint status = env->RegisterNatives(
      cls, methods, static_cast<jint>(sizeof(methods) / sizeof(methods[0])));
  env->DeleteLocalRef(cls);
  if (status != JNI_OK || env->ExceptionCheck()) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    __android_log_print(ANDROID_LOG_ERROR, "FalloutQuest",
                        "AUDIO JNI RegisterNatives failed status=%d", int(status));
    return false;
  }
  __android_log_print(ANDROID_LOG_INFO, "FalloutQuest",
                      "AUDIO JNI callbacks registered");
  return true;
}

std::string Loose(const std::string &relative) {
  return FindAudioFile(fo3assets::FalloutDataPath(""), relative);
}
bool Playable(const std::string &p) {
  auto dot = p.rfind('.');
  if (dot == std::string::npos)
    return false;
  auto ext = p.substr(dot);
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return ext == ".wav" || ext == ".mp3" || ext == ".ogg";
}
void Push(Event e) {
  std::lock_guard<std::mutex> lock(runtime.mutex);
  if (!runtime.worker.joinable() || runtime.stop ||
      (!runtime.lastActive && (e.kind < 6 || e.kind==11 || e.kind==16)))
    return;
  if(e.kind==LoadingEffectEvent){
    if(e.name.empty()){
      runtime.events.erase(std::remove_if(runtime.events.begin(),runtime.events.end(),
          [](const Event& pending){return pending.kind==LoadingEffectEvent;}),runtime.events.end());
      runtime.events.push_front(std::move(e));
    }else{
      const auto queued=std::find_if(runtime.events.begin(),runtime.events.end(),
          [](const Event& pending){return pending.kind!=LoadingEffectEvent;});
      runtime.events.insert(queued,std::move(e));
    }
    runtime.cv.notify_one();
    return;
  }
  if (e.kind == 6 || e.kind == 7 || e.kind == 10 || e.kind == 14 || e.kind==16 || e.kind==17 || e.kind==18)
    runtime.events.erase(
        std::remove_if(runtime.events.begin(), runtime.events.end(),
                       [&](const Event &old) { return old.kind == e.kind; }),
        runtime.events.end());
  if(e.kind==13) {
    // A skipped response queues a stop followed by the next authored voice.
    // Coalesce obsolete voice starts but retain the stop as a barrier, so
    // Android releases the old MediaPlayer BEFORE starting its successor.
    runtime.events.erase(
        std::remove_if(runtime.events.begin(),runtime.events.end(),
                       [&](const Event& old){
                         return old.kind==13 && old.dialogueLane==e.dialogueLane && (e.id==0 || old.id!=0);
                       }),runtime.events.end());
    if(runtime.events.size()>=32)runtime.events.pop_front(); // preserve voice dispatch
  }
  if (runtime.events.size() < 32) {
    runtime.events.push_back(e);
    runtime.cv.notify_one();
  }
}
void Worker() {
  JNIEnv *env = nullptr;
  if (runtime.vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
    __android_log_print(ANDROID_LOG_ERROR, "FalloutQuest",
                        "AUDIO JNI attach failed");
    return;
  }
  auto cls = env->GetObjectClass(runtime.activity);
  auto volumes=env->GetMethodID(cls,"audioVolumes","(FFFF)V");
  auto movie=env->GetMethodID(cls,"audioMovie","(ZZ)V");
  auto loadingEffect = env->GetMethodID(cls,"audioLoadingEffect","(Ljava/lang/String;FJ)V");
  auto menuLoop = env->GetMethodID(cls,"audioMenuLoop","(Ljava/lang/String;F)V");
  auto music = env->GetMethodID(cls, "audioMusic", "(Ljava/lang/String;F)V"),
       effect = env->GetMethodID(cls, "audioEffect", "(Ljava/lang/String;FF)V"),
       ambient =
           env->GetMethodID(cls, "audioAmbient", "(Ljava/lang/String;F)V"),
       broadcast =
           env->GetMethodID(cls, "audioBroadcast", "(Ljava/lang/String;II)V"),
       active = env->GetMethodID(cls, "audioActive", "(Z)V"),
       dialogue=env->GetMethodID(cls,"audioDialogue","(Ljava/lang/String;IIZ)V"),
       dialogueGain=env->GetMethodID(cls,"audioDialogueGain","(F)V");
  if (env->ExceptionCheck() || !music || !effect || !ambient || !active ||
      !broadcast || !dialogue || !dialogueGain || !menuLoop || !loadingEffect || !volumes || !movie) {
    __android_log_print(ANDROID_LOG_ERROR, "FalloutQuest",
                        "AUDIO JNI bridge unavailable");
    env->ExceptionClear();
    env->DeleteLocalRef(cls);
    runtime.vm->DetachCurrentThread();
    return;
  }
  auto call = [&](jmethodID method, const std::string &path, float gain,
                  bool hasGain,float pan=0) {
    if (method == effect) {
      std::lock_guard<std::mutex> lock(runtime.mutex);
      if (!runtime.lastActive || runtime.stop)
        return;
    }
    auto str = env->NewStringUTF(path.c_str());
    if(method==effect)
      env->CallVoidMethod(runtime.activity,method,str,gain,pan);
    else if (hasGain)
      env->CallVoidMethod(runtime.activity, method, str, gain);
    else
      env->CallVoidMethod(runtime.activity, method, str);
    env->DeleteLocalRef(str);
    if (env->ExceptionCheck()) {
      __android_log_print(ANDROID_LOG_ERROR, "FalloutQuest",
                          "AUDIO JNI playback call failed");
      env->ExceptionDescribe();
      env->ExceptionClear();
    }
  };
  auto playLoading = [&](const std::string& path,float gain,uint64_t generation){
    auto str=env->NewStringUTF(path.c_str());
    env->CallVoidMethod(runtime.activity,loadingEffect,str,gain,jlong(generation));
    env->DeleteLocalRef(str);
    if(env->ExceptionCheck())env->ExceptionClear();
  };
  Catalog catalog;
  std::string error;
  const bool ready =
      LoadCatalog(fo3assets::FalloutDataPath("Fallout3.esm"), catalog, error);
  __android_log_print(
      ready ? ANDROID_LOG_INFO : ANDROID_LOG_WARN, "FalloutQuest",
      "AUDIO: sounds=%zu music=%zu cells=%zu %s", catalog.sounds.size(),
      catalog.music.size(), catalog.cells.size(), error.c_str());
  FootstepGait walk,run,sneak;
  auto loadGait=[](const char* path,FootstepGait& gait) {
    std::vector<uint8_t> bytes;fo3anim::Clip clip;
    return LoadFalloutMeshFile(path,bytes) && fo3anim::DecodeClip(bytes,clip) && gait.Load(clip);
  };
  const bool walkReady=loadGait("characters/_1stperson/locomotion/mtforward.kf",walk);
  const bool runReady=loadGait("characters/_1stperson/locomotion/mtfastforward.kf",run);
  loadGait("characters/_1stperson/sneakmtforward.kf",sneak);
  {std::lock_guard<std::mutex> lock(runtime.mutex);runtime.sneak=sneak;runtime.walk=walk;runtime.run=run;runtime.footsteps.Reset();}
  __android_log_print(ANDROID_LOG_INFO,"FalloutQuest",
      "FOOTSTEPS: originalKF walk=%d stride=%.3f duration=%.3f run=%d stride=%.3f duration=%.3f",
      walkReady,walk.stride,walk.duration,runReady,run.stride,run.duration);
  mkdir("/data/user/0/com.falloutquest.app/cache", 0700);
  std::unordered_set<std::string> missing;
  std::unordered_map<std::string, std::string> extracted;
  uint64_t cacheSerial = 0;
  std::unordered_map<std::string, size_t> extractedSizes;
  std::deque<std::string> recentFiles;
  std::vector<std::string> broadcastFiles[2];
  std::string ambientFile;
  std::array<std::string,2> dialogueFiles;
  size_t cacheBytes = 0;
  const auto archives = SoundArchives(fo3assets::FalloutDataPath(""));
  const auto voiceArchive=FindAudioFile(fo3assets::FalloutDataPath(""),"Fallout - Voices.bsa");
  __android_log_print(ANDROID_LOG_INFO, "FalloutQuest",
                      "AUDIO FILES: Sound=%s Music=%s archives=%zu voices=%s dataRoot=%s",
                      Loose("sound/").empty() ? "missing" : "present",
                      Loose("music/").empty() ? "missing" : "present",
                      archives.size(), voiceArchive.empty() ? "missing" : voiceArchive.c_str(),
                      fo3assets::FalloutDataPath("").c_str());
  if (Loose("sound/").empty() && archives.empty())
    __android_log_print(
        ANDROID_LOG_WARN, "FalloutQuest",
        "AUDIO INSTALL REQUIRED: original Sound folder or Fallout - Sound.bsa "
        "is absent; APK contains no game audio");
  if (Loose("music/").empty())
    __android_log_print(ANDROID_LOG_WARN, "FalloutQuest",
                        "AUDIO INSTALL REQUIRED: original Music folder is "
                        "absent; APK contains no game music");
  std::mt19937 random(std::random_device{}());
  auto choices = [&](const std::string &relative) {
    std::vector<std::string> result;
    for (const auto &root : AudioRoots(fo3assets::FalloutDataPath(""))) {
      auto loose = LooseAt(root, relative);
      struct stat st{};
      if (!loose.empty() && !stat(loose.c_str(), &st)) {
        if (S_ISREG(st.st_mode) && Playable(loose))
          result.push_back(relative);
        else if (S_ISDIR(st.st_mode)) {
          DIR *d = opendir(loose.c_str());
          if (d) {
            while (auto *entry = readdir(d))
              if (Playable(entry->d_name) && result.size() < 128)
                result.push_back(relative +
                                 (relative.back() == '/' ? "" : "/") +
                                 entry->d_name);
            closedir(d);
          }
        }
      }
      if (!result.empty())
        break;
    }
    if (result.empty() && relative.rfind("sound/", 0) == 0 &&
        !Playable(relative)) {
      for (const auto &archive : archives) {
        std::vector<fo3assets::BsaFileInfo> files;
        const auto prefix = relative.back() == '/' ? relative : relative + '/';
        fo3assets::GetBsaArchive(archive)->List(
            prefix, files, fo3assets::BsaPathKind::Exact, 128);
        for (auto &f : files)
          if (Playable(f.path))
            result.push_back(f.path);
        if (!result.empty())
          break;
      }
    }
    std::sort(result.begin(), result.end());
    return result;
  };
  std::unordered_map<std::string, std::string> voiceCache;
  VoiceResolver voiceResolver;
  auto voicePath = [&](const std::string &request) {
    auto cached=voiceCache.find(request);if(cached!=voiceCache.end())return cached->second;
    const auto resolved=voiceResolver.Resolve(fo3assets::FalloutDataPath(""),archives,request);
    __android_log_print(resolved.path.empty()?ANDROID_LOG_WARN:ANDROID_LOG_INFO,"FalloutQuest",
      "DIALOGUE VOICE LOOKUP request=%s prefix=%s path=%s candidates=%zu ambiguous=%d archives=%zu",
      request.c_str(),resolved.prefix.c_str(),resolved.path.c_str(),resolved.candidates,resolved.ambiguous,archives.size());
    voiceCache.emplace(request,resolved.path);return resolved.path;
  };
  auto remember = [&](const std::string &path) {
    recentFiles.push_back(path);
    if (recentFiles.size() > 8)
      recentFiles.pop_front();
    return path;
  };
  auto resolve = [&](std::string path,bool forLoading=false) {
    if (path.rfind("@voice:", 0) == 0)
      path = voicePath(path);
    if(IsLoadingScreenEffect(path)!=forLoading)return std::string{};
    auto list = choices(path);
    if (!list.empty())
      path = list[std::uniform_int_distribution<size_t>(0, list.size() -
                                                               1)(random)];
    if(IsLoadingScreenEffect(path)!=forLoading)return std::string{};
    auto loose = Loose(path);
    if (!loose.empty() && Playable(loose))
      return loose;
    auto cached = extracted.find(path);
    if (cached != extracted.end())
      return remember(cached->second);
    if (!Playable(path))
      return std::string{};
    std::vector<uint8_t> bytes;
    bool loaded = false;
    std::string sourceArchive;
    for (const auto &archive : archives)
      if (fo3assets::GetBsaArchive(archive)->Read(path, bytes, nullptr,
                                                  fo3assets::BsaPathKind::Exact,
                                                  8 * 1024 * 1024)) {
        loaded = true;sourceArchive=archive;
        break;
      }
    if (!loaded) {
      __android_log_print(ANDROID_LOG_WARN,"FalloutQuest","AUDIO extraction failed path=%s archives=%zu",path.c_str(),archives.size());
      return std::string{};
    }
    __android_log_print(ANDROID_LOG_INFO,"FalloutQuest","AUDIO extracted path=%s archive=%s bytes=%zu ogg=%d",
      path.c_str(),sourceArchive.c_str(),bytes.size(),bytes.size()>=4&&std::memcmp(bytes.data(),"OggS",4)==0);

    auto protectedFile = [&](const std::string &target) {
      if (target == dialogueFiles[0] || target == dialogueFiles[1] || target == ambientFile ||
          std::find(recentFiles.begin(), recentFiles.end(), target) !=
              recentFiles.end())
        return true;
      for (auto &files : broadcastFiles)
        if (std::find(files.begin(), files.end(), target) != files.end())
          return true;
      return false;
    };
    while (extracted.size() >= 128 ||
           cacheBytes + bytes.size() > 64 * 1024 * 1024) {
      auto victim =
          std::find_if(extracted.begin(), extracted.end(),
                       [&](const auto &v) { return !protectedFile(v.second); });
      if (victim == extracted.end())
        return std::string{};
      cacheBytes -= extractedSizes.at(victim->first);
      unlink(victim->second.c_str());
      extractedSizes.erase(victim->first);
      extracted.erase(victim);
    }

    auto target = "/data/user/0/com.falloutquest.app/cache/fq-audio-" +
                  std::to_string(getpid()) + "-" +
                  std::to_string(cacheSerial++) + path.substr(path.rfind('.'));
    std::ofstream f(target, std::ios::binary);
    f.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    f.close();
    if (!f) {
      unlink(target.c_str());
      return std::string{};
    }
    cacheBytes += bytes.size();
    extractedSizes[path] = bytes.size();
    extracted[path] = target;
    return remember(target);
  };
  for(const auto& entry:catalog.sounds)
    if(IsLoadingScreenEffect(entry.second.path))resolve(entry.second.path,true);
  auto sound = [&](uint32_t id, jmethodID method,float distance=-1,float pan=0) {
    auto s = catalog.sounds.find(id);
    if (s == catalog.sounds.end() || s->second.path.empty() || IsLoadingScreenEffect(s->second.path)) {
      if (method == ambient)
        call(method, "", 1, true);
      return;
    }
    const float gain=s->second.GainAt(distance);
    if(method==effect&&gain<=0)return;
    auto path = resolve(s->second.path);
    if (method == ambient)
      ambientFile = path;
    if (path.empty() && missing.insert(s->second.path).second)
      __android_log_print(ANDROID_LOG_WARN, "FalloutQuest",
                          "AUDIO missing effect: %s", s->second.path.c_str());
    call(method,path,gain,true,pan);
  };
  uint32_t cell = UINT32_MAX;
  std::string previousMusic;
  fo3pipdata::Broadcast radio;
  std::shared_ptr<const fo3pipdata::Definitions> definitions;
  fo3pipdata::SessionState broadcastState;
  uint32_t broadcastGeneration[2]{};
  auto playBroadcast = [&](const std::vector<std::string> &paths, int channel) {
    std::string playlist;
    broadcastFiles[channel].clear();
    for (auto &relative : paths) {
      auto path = resolve(relative);
      if (path.empty()) {
        __android_log_print(ANDROID_LOG_WARN, "FalloutQuest",
                            "PIPBOY BROADCAST asset-resolution failure: %s",
                            relative.c_str());
        playlist.clear();
        break;
      }
      broadcastFiles[channel].push_back(path);
      playlist += path + '\n';
    }
    auto str = env->NewStringUTF(playlist.c_str());
    env->CallVoidMethod(runtime.activity, broadcast, str, channel,
                        ++broadcastGeneration[channel]);
    env->DeleteLocalRef(str);
    if (env->ExceptionCheck())
      env->ExceptionClear();
    return !playlist.empty();
  };
  auto advanceRadio = [&]() {
    if (!definitions)
      return;
    std::string error;
    auto paths = radio.Advance(*definitions, broadcastState, error);
    if (!error.empty())
      __android_log_print(ANDROID_LOG_WARN, "FalloutQuest", "PIPBOY RADIO %s",
                          error.c_str());
    playBroadcast(paths, 0);
  };
  while (true) {
    Event e;
    {
      std::unique_lock<std::mutex> lock(runtime.mutex);
      runtime.cv.wait(lock,
                      [] { return runtime.stop || !runtime.events.empty(); });
      if (runtime.stop)
        break;
      e = runtime.events.front();
      runtime.events.pop_front();
    }
    if(e.kind==18){env->CallVoidMethod(runtime.activity,movie,jboolean((e.id&2)!=0),jboolean((e.id&4)!=0));if(env->ExceptionCheck())env->ExceptionClear();continue;}
    if(e.kind==17){env->CallVoidMethod(runtime.activity,volumes,e.volumes[0],e.volumes[1],e.volumes[2],e.volumes[3]);if(env->ExceptionCheck())env->ExceptionClear();continue;}
    if(e.kind==13) {
      if(e.id&&!e.name.empty()) {
        auto path=e.name.rfind("@voice:",0)==0?voicePath(e.name):e.name;
        const auto dot=path.rfind('.');
        if(dot!=std::string::npos)path.replace(dot,std::string::npos,".lip");else path.clear();
        std::vector<uint8_t> bytes;const auto loose=Loose(path);
        if(!loose.empty()) {
          std::ifstream file(loose,std::ios::binary|std::ios::ate);
          const auto size=file.tellg();
          if(size>0&&size<=8*1024*1024){bytes.resize(size);file.seekg(0);if(!file.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))bytes.clear();}
        }
        if(bytes.empty()&&!path.empty())for(const auto& archive:archives)
          if(fo3assets::GetBsaArchive(archive)->Read(path,bytes,nullptr,fo3assets::BsaPathKind::Exact,8*1024*1024))break;
        auto lip=std::make_shared<fo3face::Lip>();std::string error;
        if(fo3face::DecodeLip(bytes,*lip,error)) {
          __android_log_print(ANDROID_LOG_INFO,"FalloutQuest","NPC LIP START token=%u frames=%zu firstFrame=%d path=%s",e.id,lip->frames.size(),lip->firstFrame,path.c_str());
          runtime.dialogueFace[e.dialogueLane].Asset(e.id,std::move(lip));
        }else __android_log_print(ANDROID_LOG_WARN,"FalloutQuest","NPC LIP MISSING token=%u path=%s reason=%s",e.id,path.c_str(),error.c_str());
      }
      auto& dialogueFile=dialogueFiles[e.dialogueLane];
      dialogueFile=e.name.empty()?"":resolve(e.name);
      if(!e.name.empty()&&dialogueFile.empty())DialogueDone(e.id,false);
      auto str=env->NewStringUTF(dialogueFile.c_str());
      env->CallVoidMethod(runtime.activity,dialogue,str,jint(e.id),jint(e.dialogueLane),jboolean(e.intercom));env->DeleteLocalRef(str);
      if(env->ExceptionCheck()){env->ExceptionClear();DialogueDone(e.id,false);}
      continue;
    }
    if(e.kind==14) {float gain=0;std::memcpy(&gain,&e.id,4);env->CallVoidMethod(runtime.activity,dialogueGain,gain);if(env->ExceptionCheck())env->ExceptionClear();continue;}
    if (e.kind == 10) {
      const auto transmitter=radio.transmitter;
      const bool modeChanged=transmitter&&broadcastState.silentRadios.count(transmitter)!=e.state.silentRadios.count(transmitter);
      broadcastState = std::move(e.state);
      if(modeChanged&&!radio.scripted)advanceRadio();
      continue;
    }
    if(e.kind==RadioConversationEvent){
      if(definitions&&radio.transmitter==e.id&&radio.StartConversation(*definitions,e.state,e.radioTopic)){
        broadcastState=std::move(e.state);
        advanceRadio();
      }
      continue;
    }
    if (e.kind == 6) {
      definitions = e.definitions;
      broadcastState = std::move(e.state);
      bool ok = definitions && radio.Tune(*definitions, e.id);
      __android_log_print(ANDROID_LOG_INFO, "FalloutQuest",
                          "PIPBOY RADIO tune=%08X supported=%d", e.id, ok);
      if (e.id && ok)
        advanceRadio();
      else
        playBroadcast({}, 0);
      continue;
    }
    if (e.kind == 7) {
      runtime.playingNote = e.id;
      if (!playBroadcast(e.paths, 1))
        runtime.playingNote = 0;
      __android_log_print(ANDROID_LOG_INFO, "FalloutQuest",
                          "PIPBOY NOTE play=%08X", runtime.playingNote.load());
      continue;
    }
    if (e.kind == 8) {
      if (e.id == broadcastGeneration[0]) advanceRadio();
      continue;
    }
    if (e.kind == 9) {
      if (e.id == broadcastGeneration[1]) runtime.playingNote = 0;
      continue;
    }
    if (e.kind == 0) {
      __android_log_print(ANDROID_LOG_INFO, "FalloutQuest",
                          "AUDIO CONTEXT: cell=%08X focused=%d", e.id,
                          e.active);
      env->CallVoidMethod(runtime.activity, active, jboolean(e.active));
      if (env->ExceptionCheck())
        env->ExceptionClear();
      if (cell != e.id) {
        cell = e.id;
        const bool dead=cell==DeathCell;
        auto relative = dead?std::string{}:cell==TitleCell?std::string("music/special/maintitle.mp3"):catalog.MusicForCell(cell);
        if (relative != previousMusic) {
          previousMusic = relative;
          std::string playlist;
          for (auto &choice : choices(relative)) {
            auto path = Loose(choice);
            if (!path.empty())
              playlist += path + '\n';
          }
          if (!relative.empty() && playlist.empty())
            __android_log_print(
                ANDROID_LOG_WARN, "FalloutQuest",
                "AUDIO missing music: %s (install original Data/Music)",
                relative.c_str());
          __android_log_print(ANDROID_LOG_INFO, "FalloutQuest",
                              "AUDIO MUSIC: original=%s tracks=%zu",
                              relative.c_str(),
                              static_cast<size_t>(std::count(
                                  playlist.begin(), playlist.end(), '\n')));
          call(music, playlist, cell==TitleCell?2.f:1.f, true);
        }
        sound(dead||cell==TitleCell?0:catalog.AmbientForCell(cell), ambient);
        if(dead){
          definitions.reset();
          radio={};
          playBroadcast({},0);
          runtime.playingNote=0;
          playBroadcast({},1);
        }
      }
      continue;
    }
    if(e.kind==LoadingEffectEvent){
      const auto currentCue=[&]{
        const auto now=Fo3LoadingClockUs();
        return now-e.loadingCueUs<=Fo3LoadingAudioFrameTimeoutUs&&IsFo3LoadingAudioCurrent(e.loadingGeneration,now);
      };
      if(e.name.empty()){
        playLoading("",1,0);
      }else if(currentCue()){
        const auto named=catalog.names.find(e.name);
        const auto asset=named==catalog.names.end()?catalog.sounds.end():catalog.sounds.find(named->second);
        if(asset!=catalog.sounds.end()){
          const auto path=resolve(asset->second.path,true);
          if(!path.empty()&&currentCue()){
            __android_log_print(ANDROID_LOG_INFO,"FalloutQuest","LOADING AUDIO cue=%s generation=%llu",
                e.name.c_str(),static_cast<unsigned long long>(e.loadingGeneration));
            playLoading(path,asset->second.gain,e.loadingGeneration);
          }
        }
      }
      continue;
    }
    if(e.kind==15){
      auto named=catalog.names.find(e.name);auto asset=named==catalog.names.end()?catalog.sounds.end():catalog.sounds.find(named->second);
      std::string path;float gain=1;if(asset!=catalog.sounds.end()&&!IsLoadingScreenEffect(asset->second.path)){path=resolve(asset->second.path);gain=asset->second.gain;
        if(path.empty()&&missing.insert(asset->second.path).second)__android_log_print(ANDROID_LOG_WARN,"FalloutQuest","AUDIO missing menu loop: %s",asset->second.path.c_str());}
      call(menuLoop,path,gain,true);continue;
    }
    uint32_t id = 0;
    if(e.kind==16){id=catalog.Footstep(e.id&255u,e.id>>8u);} else if(e.kind==11||(e.kind==12&&e.name.empty())){id=e.id;} else if (e.kind == 5||e.kind==12) {
      auto n = catalog.names.find(e.name);
      if (n != catalog.names.end())
        id = n->second;
    } else if (e.kind == 4) {
      auto n = catalog.names.find("UIMenuFocus");
      if (n != catalog.names.end())
        id = n->second;
    } else {
      auto o = catalog.objects.find(e.id);
      if (o != catalog.objects.end())
        id = e.kind == 1   ? o->second.pickup
             : e.kind == 2 ? o->second.open
                           : o->second.close;
    }
    if(e.kind==12){
      const auto asset=catalog.sounds.find(id);if(asset!=catalog.sounds.end()){
        const auto variants=choices(asset->second.path);
        if(variants.empty())resolve(asset->second.path);else for(const auto&path:variants)resolve(path);
      }
    }else if (id)
      sound(id,effect,e.distance,e.pan);
  }
  playBroadcast({}, 0);
  playBroadcast({}, 1);
  env->CallVoidMethod(runtime.activity, active, jboolean(false));
  if (env->ExceptionCheck())
    env->ExceptionClear();
  for (auto &entry : extracted)
    unlink(entry.second.c_str());
  env->DeleteLocalRef(cls);
  runtime.vm->DetachCurrentThread();
}
} // namespace
void Start(JavaVM *vm, jobject activity) {
  if (runtime.worker.joinable())
    return;
  JNIEnv *env = nullptr;
  bool attach =
      vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK;
  if (attach && vm->AttachCurrentThread(&env, nullptr) != JNI_OK)
    return;
  // NativeActivity loads the .so for android_main, but that does not guarantee
  // ART associates it with this Java subclass for name-based native lookup.
  // Register the callbacks explicitly against the live activity class.
  if (!RegisterActivityNatives(env, activity)) {
    if (attach) vm->DetachCurrentThread();
    return;
  }
  runtime.vm = vm;
  runtime.activity = env->NewGlobalRef(activity);
  runtime.stop = false;
  runtime.lastCell = UINT32_MAX;
  runtime.lastActive = false;
  runtime.worker = std::thread(Worker);
  if (attach)
    vm->DetachCurrentThread();
}
void Shutdown() {
  if (!runtime.worker.joinable())
    return;
  {
    std::lock_guard<std::mutex> lock(runtime.mutex);
    runtime.stop = true;
    runtime.events.clear();
  }
  runtime.cv.notify_one();
  runtime.worker.join();
  JNIEnv *env = nullptr;
  bool attach = runtime.vm->GetEnv(reinterpret_cast<void **>(&env),
                                   JNI_VERSION_1_6) != JNI_OK;
  if (!attach || runtime.vm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
    env->DeleteGlobalRef(runtime.activity);
    if (attach)
      runtime.vm->DetachCurrentThread();
  }
  runtime.activity = nullptr;
}
void Movie(uint8_t flags){Push({18,flags,true});}
void Volumes(const std::array<float,4>& values){Event e{17,0,true};e.volumes=values;Push(std::move(e));}
void Context(uint32_t cell, bool active) {
  std::lock_guard<std::mutex> lock(runtime.mutex);
  if (!runtime.worker.joinable() || runtime.stop ||
      (runtime.lastCell == cell && runtime.lastActive == active))
    return;
  runtime.footsteps.Reset();
  runtime.lastCell = cell;
  runtime.lastActive = active;
  runtime.events.erase(std::remove_if(runtime.events.begin(),runtime.events.end(),
      [](const Event& e){return e.kind==16;}),runtime.events.end());
  if (!active)
    runtime.events.erase(
        std::remove_if(runtime.events.begin(), runtime.events.end(),
                       [](const Event &e) { return e.kind < 6 || e.kind==11; }),
        runtime.events.end());
  runtime.events.erase(
      std::remove_if(runtime.events.begin(), runtime.events.end(),
                     [](const Event &e) { return e.kind == 0; }),
      runtime.events.end());
  runtime.events.push_front({0, cell, active});
  runtime.cv.notify_one();
}
void ScriptDialogue(const std::string& request,uint32_t token,unsigned lane,bool intercom) {
  if(lane>=runtime.dialogueFace.size())return;
  runtime.dialogueCompletion[lane].Start(token);
  runtime.dialogueFace[lane].Start(token);
  Event event{13,token,true,request};
  event.dialogueLane=lane;event.intercom=intercom;
  Push(std::move(event));
}
void Dialogue(const std::string& request,uint32_t token){ScriptDialogue(request,token,0,false);}
void StopDialogueLane(unsigned lane){ScriptDialogue({},0,lane,false);}
void DialogueStop(){StopDialogueLane(0);}
DialogueFaceSample DialogueFace(uint32_t token){
  auto primary=runtime.dialogueFace[0].Read(token);
  return primary.lip||primary.seconds>=0?primary:runtime.dialogueFace[1].Read(token);
}
void DialogueGain(float gain){gain=std::clamp(gain,0.f,1.f);uint32_t bits;std::memcpy(&bits,&gain,4);Push({14,bits,true});}
uint64_t TakeDialogueLane(unsigned lane){return lane<runtime.dialogueCompletion.size()?runtime.dialogueCompletion[lane].Take():0;}
uint64_t DialogueCompletion(){return TakeDialogueLane(0);}
void DialogueDone(uint32_t token,bool success){
  __android_log_print(ANDROID_LOG_INFO,"FalloutQuest","NPC LIP END token=%u audioSuccess=%d",token,int(success));
  for(auto& lane:runtime.dialogueCompletion)if(lane.Done(token,success))break;
}
void Radio(uint32_t transmitter, std::shared_ptr<const fo3pipdata::Definitions> definitions,
           const fo3pipdata::SessionState &state) {
  Event e{6, transmitter, true};
  e.definitions = std::move(definitions);
  e.state = state;
  Push(std::move(e));
}
void Note(uint32_t id, const std::vector<std::string> &paths) {
  runtime.playingNote = id;
  Event e{7, id, true};
  e.paths = paths;
  Push(std::move(e));
}
void RadioState(const fo3pipdata::SessionState &state) {
  Event e{10, 0, true};
  e.state = state;
  Push(std::move(e));
}
void RadioConversation(uint32_t transmitter,uint32_t topic,const fo3pipdata::SessionState& state) {
  Event e{RadioConversationEvent,transmitter,true};
  e.radioTopic=topic;e.state=state;
  Push(std::move(e));
}
uint32_t PlayingNote() { return runtime.playingNote.load(); }
void BroadcastDone(int channel, uint32_t generation) {
  Push({channel == 0 ? 8 : 9, generation, true});
}
void PreloadSound(uint32_t soundForm){if(soundForm)Push({12,soundForm,false});}
void PreloadSound(const std::string&editorId){if(!editorId.empty())Push({12,0,false,editorId});}
void SoundEvent(uint32_t soundForm) { if(soundForm) Push({11,soundForm,true}); }
void SoundAt(uint32_t form,float distance,float pan) {
  if(!form||!std::isfinite(distance)||distance<0||!std::isfinite(pan))return;
  Event event{11,form,true};event.distance=distance;event.pan=std::clamp(pan,-1.f,1.f);
  Push(std::move(event));
}
void Pickup(uint32_t base) { Push({1, base, true}); }
void Open(uint32_t base) { Push({2, base, true}); }
void Close(uint32_t base) { Push({3, base, true}); }
void Scroll() { Push({4, 0, true}); }
void MenuLoop(const std::string& editorId){Push({15,0,false,editorId});}
int AdvancePlayerFootsteps(float distance,float dt,bool enabled,bool sneaking) {
  std::lock_guard<std::mutex> lock(runtime.mutex);
  if(sneaking!=runtime.wasSneaking){runtime.footsteps.Reset();runtime.wasSneaking=sneaking;}
  const int step=runtime.footsteps.Advance(distance,dt,enabled&&runtime.lastActive&&!runtime.stop,
      sneaking?runtime.sneak:runtime.walk,sneaking?FootstepGait{}:runtime.run);
  return step<0?-1:sneaking?step+4:step;
}
void Footstep(uint32_t material,unsigned action) {
  if(action<FootstepActions.size())Push({16,(material&255u)|(action<<8u),true});
}
void NamedSound(const std::string &editorId) {
  if (!editorId.empty() && editorId.size() <= 128)
    Push({5, 0, true, editorId});
}
void LoadingSound(const std::string& editorId) {
  const auto generation=GetFo3LoadingGeneration();
  if(editorId.empty()||editorId.size()>128||!IsFo3LoadingAudioCurrent(generation))return;
  Event event{LoadingEffectEvent,0,true,editorId};
  event.loadingGeneration=generation;
  event.loadingCueUs=Fo3LoadingClockUs();
  Push(std::move(event));
}
void StopLoadingSound(){Push({LoadingEffectEvent,0,false});}
} // namespace fo3audio

// Keep conventional exports as a fallback/debug surface. Production binding
// is explicit RegisterNatives above, which is independent of class-loader
// library association.
extern "C" JNIEXPORT void JNICALL
Java_com_falloutquest_app_FalloutNativeActivity_audioBroadcastDone(
    JNIEnv *env, jobject activity, jint channel, jint generation) {
  (void)env;(void)activity;
  fo3audio::BroadcastDone(channel, uint32_t(generation));
}

extern "C" JNIEXPORT void JNICALL
Java_com_falloutquest_app_FalloutNativeActivity_audioDialogueDone(
    JNIEnv *env, jobject activity, jint token, jboolean success) {
  (void)env;(void)activity;
  fo3audio::DialogueDone(uint32_t(token), success == JNI_TRUE);
}
