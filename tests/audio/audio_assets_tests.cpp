#include "audio/fo3-audio-assets.h"
#include "audio/fo3-dialogue-completion.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <unistd.h>

static void U32(std::vector<uint8_t>& b,uint32_t x){for(int i=0;i<4;++i)b.push_back(uint8_t(x>>(8*i)));}
static void Set(std::vector<uint8_t>& b,size_t at,uint32_t x){for(int i=0;i<4;++i)b[at+i]=uint8_t(x>>(8*i));}
static void Bsa(const std::filesystem::path& target,const std::string& name,const std::vector<uint8_t>& audio) {
    const std::string folder="sound\\voice\\fallout3.esm\\maleuniquesimms";
    std::vector<uint8_t> b{'B','S','A',0};
    for(uint32_t x:{104u,36u,3u,1u,1u,uint32_t(folder.size()+1),uint32_t(name.size()+1),0u,0u,0u,1u,uint32_t(52+name.size()+1)})U32(b,x);
    b.push_back(uint8_t(folder.size()+1));b.insert(b.end(),folder.begin(),folder.end());b.push_back(0);
    const auto record=b.size();b.resize(record+16);b.insert(b.end(),name.begin(),name.end());b.push_back(0);
    Set(b,record+8,audio.size());Set(b,record+12,b.size());b.insert(b.end(),audio.begin(),audio.end());
    std::ofstream f(target,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());assert(f);
}
int main(int argc,char** argv) {
    fo3audio::DialogueCompletionMailbox mailbox;
    mailbox.Start(10);assert(mailbox.Take()==0); // Active playback cannot complete itself.
    mailbox.Start(11);assert(!mailbox.Done(10,true));assert(mailbox.Take()==0);
    assert(mailbox.Done(11,false));assert(!mailbox.Done(10,true));assert(!mailbox.Done(11,true));
    assert(mailbox.Take()==22);assert(mailbox.Take()==0); // Failure is not success.
    mailbox.Start(12);assert(mailbox.Done(12,true));assert(!mailbox.Done(11,false));assert(mailbox.Take()==25);
    mailbox.Stop();assert(!mailbox.Done(12,true));
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

    const auto voice=root/"Data/Sound/Voice/Fallout3.esm/MaleUniqueSimms";fs::create_directories(voice);
    std::ofstream(voice/"unrelated_00011111_1.ogg")<<"unrelated loose file";
    std::vector<uint8_t> original{'O','g','g','S',0,1,2};
    if(argc>1) {std::ifstream f(argv[1],std::ios::binary);assert(f);original.assign(std::istreambuf_iterator<char>(f),{});}
    const std::string greeting="ms11_greeting_0003da20_3.ogg";
    Bsa(root/"Data/Fallout - Voices.bsa",greeting,original);
    fo3audio::VoiceResolver resolver;
    const auto archives=fo3audio::SoundArchives(data);
    const auto resolved=resolver.Resolve(data,archives,"@voice:MaleUniqueSimms:_0003da20_3");
    assert(!resolved.ambiguous&&resolved.path=="sound/voice/fallout3.esm/maleuniquesimms/"+greeting);
    std::vector<uint8_t> extracted;
    assert(fo3assets::GetBsaArchive((root/"Data/Fallout - Voices.bsa").string())->Read(resolved.path,extracted));
    assert(extracted==original); // Actual supplied Vorbis may be exercised without committing it.
    std::ofstream(voice/greeting)<<"loose override";
    assert(!fo3audio::FindAudioFile(data,"sound\\voice\\fallout3.esm\\maleuniquesimms\\"+greeting).empty());
    fo3audio::VoiceResolver duplicates;
    assert(!duplicates.Resolve(data,archives,"@voice:MaleUniqueSimms:_0003da20_3").ambiguous);
    std::ofstream(voice/"distinct_prefix_0003da20_3.ogg")<<"ambiguous";
    fo3audio::VoiceResolver ambiguous;
    const auto bad=ambiguous.Resolve(data,archives,"@voice:MaleUniqueSimms:_0003da20_3");assert(bad.ambiguous&&bad.path.empty());
    assert(resolver.Resolve(data,archives,"@voice:../maleuniquesimms:_0003da20_3").path.empty());
    assert(fo3audio::FindAudioFile(data,"sound\\..\\..\\etc\\passwd").empty());
    fs::remove_all(root);
}
