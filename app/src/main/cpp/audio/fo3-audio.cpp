#include "fo3-audio.h"
#include "data/fo3-asset-store.h"
#include "fo3-audio-assets.h"
#include "fo3-audio-catalog.h"
#include <algorithm>
#include <android/log.h>
#include <atomic>
#include <cctype>
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
struct Event {
  Event() = default;
  Event(int k, uint32_t form, bool enabled, std::string editor = {})
      : kind(k), id(form), active(enabled), name(std::move(editor)) {}
  int kind;
  uint32_t id;
  bool active;
  std::string name{};
  const fo3pipdata::Definitions *definitions = nullptr;
  fo3pipdata::SessionState state;
  std::vector<std::string> paths;
};
struct Runtime {
  JavaVM *vm = nullptr;
  jobject activity = nullptr;
  std::thread worker;
  std::mutex mutex;
  std::condition_variable cv;
  std::deque<Event> events;
  bool stop = false;
  uint32_t lastCell = UINT32_MAX;
  bool lastActive = false;
  std::atomic<uint32_t> playingNote{0};
} runtime;
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
      (!runtime.lastActive && e.kind < 6))
    return;
  if (e.kind == 6 || e.kind == 7 || e.kind == 10)
    runtime.events.erase(
        std::remove_if(runtime.events.begin(), runtime.events.end(),
                       [&](const Event &old) { return old.kind == e.kind; }),
        runtime.events.end());
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
  auto music = env->GetMethodID(cls, "audioMusic", "(Ljava/lang/String;)V"),
       effect = env->GetMethodID(cls, "audioEffect", "(Ljava/lang/String;F)V"),
       ambient =
           env->GetMethodID(cls, "audioAmbient", "(Ljava/lang/String;F)V"),
       broadcast =
           env->GetMethodID(cls, "audioBroadcast", "(Ljava/lang/String;II)V"),
       active = env->GetMethodID(cls, "audioActive", "(Z)V");
  if (env->ExceptionCheck() || !music || !effect || !ambient || !active ||
      !broadcast) {
    __android_log_print(ANDROID_LOG_ERROR, "FalloutQuest",
                        "AUDIO JNI bridge unavailable");
    env->ExceptionClear();
    env->DeleteLocalRef(cls);
    runtime.vm->DetachCurrentThread();
    return;
  }
  auto call = [&](jmethodID method, const std::string &path, float gain,
                  bool hasGain) {
    if (method == effect) {
      std::lock_guard<std::mutex> lock(runtime.mutex);
      if (!runtime.lastActive || runtime.stop)
        return;
    }
    auto str = env->NewStringUTF(path.c_str());
    if (hasGain)
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
  Catalog catalog;
  std::string error;
  const bool ready =
      LoadCatalog(fo3assets::FalloutDataPath("Fallout3.esm"), catalog, error);
  __android_log_print(
      ready ? ANDROID_LOG_INFO : ANDROID_LOG_WARN, "FalloutQuest",
      "AUDIO: sounds=%zu music=%zu cells=%zu %s", catalog.sounds.size(),
      catalog.music.size(), catalog.cells.size(), error.c_str());
  mkdir("/data/user/0/com.falloutquest.app/cache", 0700);
  std::unordered_set<std::string> missing;
  std::unordered_map<std::string, std::string> extracted;
  uint64_t cacheSerial = 0;
  std::unordered_map<std::string, size_t> extractedSizes;
  std::deque<std::string> recentFiles;
  std::vector<std::string> broadcastFiles[2];
  std::string ambientFile;
  size_t cacheBytes = 0;
  const auto archives = SoundArchives(fo3assets::FalloutDataPath(""));
  __android_log_print(ANDROID_LOG_INFO, "FalloutQuest",
                      "AUDIO FILES: Sound=%s Music=%s archives=%zu dataRoot=%s",
                      Loose("sound/").empty() ? "missing" : "present",
                      Loose("music/").empty() ? "missing" : "present",
                      archives.size(), fo3assets::FalloutDataPath("").c_str());
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
  std::unordered_map<std::string, std::vector<std::string>> voiceDirectories;
  auto voicePath = [&](const std::string &request) {
    auto found = voiceCache.find(request);
    if (found != voiceCache.end())
      return found->second;
    auto colon = request.find(':', 7);
    if (colon == std::string::npos)
      return std::string{};
    std::string voice = request.substr(7, colon - 7),
                suffix = request.substr(colon + 1);
    std::transform(voice.begin(), voice.end(), voice.begin(), ::tolower);
    const std::string prefix = "sound/voice/fallout3.esm/" + voice + "/";
    std::string match;
    bool ambiguous = false;
    auto consider = [&](const std::string &path) {
      auto lower = path;
      std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
      auto dot = lower.rfind('.');
      if (dot == std::string::npos || !Playable(lower))
        return;
      auto stem = lower.substr(0, dot);
      if (stem.size() >= suffix.size() &&
          stem.compare(stem.size() - suffix.size(), suffix.size(), suffix) ==
              0) {
        if (!match.empty() && match != path)
          ambiguous = true;
        match = path;
      }
    };
    auto directory = voiceDirectories.find(prefix);
    if (directory == voiceDirectories.end()) {
      std::vector<std::string> paths;
      for (auto &root : AudioRoots(fo3assets::FalloutDataPath(""))) {
        auto folder = LooseAt(root, prefix);
        DIR *dir = folder.empty() ? nullptr : opendir(folder.c_str());
        if (dir) {
          while (auto *entry = readdir(dir))
            if (Playable(entry->d_name) && paths.size() < 65536)
              paths.push_back(prefix + entry->d_name);
          closedir(dir);
        }
        if (!paths.empty())
          break;
      }
      if (paths.empty())
        for (auto &archive : archives) {
          std::vector<fo3assets::BsaFileInfo> files;
          fo3assets::GetBsaArchive(archive)->List(
              prefix, files, fo3assets::BsaPathKind::Exact, 65536);
          for (auto &f : files)
            if (Playable(f.path))
              paths.push_back(f.path);
          if (!paths.empty())
            break;
        }
      directory = voiceDirectories.emplace(prefix, std::move(paths)).first;
    }
    for (auto &path : directory->second)
      consider(path);
    if (ambiguous)
      match.clear();
    voiceCache.emplace(request, match);
    return match;
  };
  auto remember = [&](const std::string &path) {
    recentFiles.push_back(path);
    if (recentFiles.size() > 8)
      recentFiles.pop_front();
    return path;
  };
  auto resolve = [&](std::string path) {
    if (path.rfind("@voice:", 0) == 0)
      path = voicePath(path);
    auto list = choices(path);
    if (!list.empty())
      path = list[std::uniform_int_distribution<size_t>(0, list.size() -
                                                               1)(random)];
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
    for (const auto &archive : archives)
      if (fo3assets::GetBsaArchive(archive)->Read(path, bytes, nullptr,
                                                  fo3assets::BsaPathKind::Exact,
                                                  8 * 1024 * 1024)) {
        loaded = true;
        break;
      }
    if (!loaded)
      return std::string{};
    auto protectedFile = [&](const std::string &target) {
      if (target == ambientFile ||
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
  auto sound = [&](uint32_t id, jmethodID method) {
    auto s = catalog.sounds.find(id);
    if (s == catalog.sounds.end() || s->second.path.empty()) {
      if (method == ambient)
        call(method, "", 1, true);
      return;
    }
    auto path = resolve(s->second.path);
    if (method == ambient)
      ambientFile = path;
    if (path.empty() && missing.insert(s->second.path).second)
      __android_log_print(ANDROID_LOG_WARN, "FalloutQuest",
                          "AUDIO missing effect: %s", s->second.path.c_str());
    call(method, path, s->second.gain, true);
  };
  uint32_t cell = UINT32_MAX;
  std::string previousMusic;
  fo3pipdata::Broadcast radio;
  const fo3pipdata::Definitions *definitions = nullptr;
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
    if (e.kind == 10) {
      broadcastState = std::move(e.state);
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
        auto relative = catalog.MusicForCell(cell);
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
          call(music, playlist, 1, false);
        }
        sound(catalog.AmbientForCell(cell), ambient);
      }
      continue;
    }
    uint32_t id = 0;
    if (e.kind == 5) {
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
    if (id)
      sound(id, effect);
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
void Context(uint32_t cell, bool active) {
  std::lock_guard<std::mutex> lock(runtime.mutex);
  if (!runtime.worker.joinable() || runtime.stop ||
      (runtime.lastCell == cell && runtime.lastActive == active))
    return;
  runtime.lastCell = cell;
  runtime.lastActive = active;
  if (!active)
    runtime.events.erase(
        std::remove_if(runtime.events.begin(), runtime.events.end(),
                       [](const Event &e) { return e.kind < 6; }),
        runtime.events.end());
  runtime.events.erase(
      std::remove_if(runtime.events.begin(), runtime.events.end(),
                     [](const Event &e) { return e.kind == 0; }),
      runtime.events.end());
  runtime.events.push_front({0, cell, active});
  runtime.cv.notify_one();
}
void Radio(uint32_t transmitter, const fo3pipdata::Definitions &definitions,
           const fo3pipdata::SessionState &state) {
  Event e{6, transmitter, true};
  e.definitions = &definitions;
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
uint32_t PlayingNote() { return runtime.playingNote.load(); }
void BroadcastDone(int channel, uint32_t generation) {
  Push({channel == 0 ? 8 : 9, generation, true});
}
void Pickup(uint32_t base) { Push({1, base, true}); }
void Open(uint32_t base) { Push({2, base, true}); }
void Close(uint32_t base) { Push({3, base, true}); }
void Scroll() { Push({4, 0, true}); }
void NamedSound(const std::string &editorId) {
  if (!editorId.empty() && editorId.size() <= 128)
    Push({5, 0, true, editorId});
}
} // namespace fo3audio

extern "C" JNIEXPORT void JNICALL
Java_com_falloutquest_app_FalloutNativeActivity_audioBroadcastDone(
    JNIEnv *, jobject, jint channel, jint generation) {
  fo3audio::BroadcastDone(channel, uint32_t(generation));
}
