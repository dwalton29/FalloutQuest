#include "audio/fo3-audio-assets.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <unistd.h>
int main() {
    namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/("fq-audio-paths-"+std::to_string(getpid()));
    fs::create_directories(root/"Data");fs::create_directories(root/"Sound/FX/UI");
    fs::create_directories(root/"Music/Explore");
    std::ofstream(root/"Sound/FX/UI/Initial.WAV")<<"fixture";
    std::ofstream(root/"Data/SOUNDS.BSA")<<"fixture";
    const auto data=(root/"Data").string();
    assert(fo3audio::FindAudioFile(data,"sound/fx/ui/initial.wav")== (root/"Sound/FX/UI/Initial.WAV").string());
    assert(!fo3audio::FindAudioFile(data,"music/explore/").empty());
    assert(fo3audio::SoundArchives(data).size()==1);
    assert(fo3audio::FindAudioFile(data,"../Sound/FX/UI/Initial.WAV").empty());
    assert(fo3audio::FindAudioFile(data,"sound/../../Sound/FX/UI/Initial.WAV").empty());
    assert(fo3audio::FindAudioFile(data,"sound/missing.wav").empty());
    fs::create_directories(root/"Data/sound/fx/ui");
    std::ofstream(root/"Data/sound/fx/ui/initial.wav")<<"preferred";
    assert(fo3audio::FindAudioFile(data,"sound/fx/ui/initial.wav")== (root/"Data/sound/fx/ui/initial.wav").string());
    fs::remove_all(root);
}
