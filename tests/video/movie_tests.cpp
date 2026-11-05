#include "video/fo3-bink-decoder.h"
#include "video/fo3-movie-playback.h"
#include "video/fo3-movie-request.h"
#include "video/fo3-movie-controls.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <unistd.h>
using Bytes=std::vector<uint8_t>;
static void Put(Bytes& out,uint32_t value){for(unsigned n=0;n<4;++n)out.push_back(uint8_t(value>>(8*n)));}
// A self-authored BIKk fixture: three solid-colour YUV planes using the codec's
// fill-plane syntax. Contains no Bethesda or third-party movie content.
static Bytes SolidMovie(){
    Bytes out{'B','I','K','k'};constexpr unsigned frames=3,offset=48+4*frames;
    Put(out,offset+16*frames-8);Put(out,frames);Put(out,16);Put(out,0);Put(out,16);Put(out,16);Put(out,30);Put(out,1);Put(out,0);Put(out,0);Put(out,0);
    for(unsigned n=0;n<frames;++n)Put(out,(offset+16*n)|1u);
    for(unsigned y:{16u,235u,81u}){Put(out,0);Put(out,(y<<1)|1u);Put(out,(128<<1)|1u);Put(out,(128<<1)|1u);}return out;
}
static void Write(const std::string& path,const Bytes& bytes){std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());assert(out.good());}
int main(){
    fo3movie::Controls controls;
    assert(!controls.Step(1,true,false,true,true,false));
    assert(!controls.Step(1,true,false,true,true,false));
    assert(!controls.Step(1,false,false,true,true,false));
    assert(controls.Step(1,true,false,true,true,false));
    assert(!controls.Step(2,false,false,true,false,false));
    assert(!controls.Step(2,false,false,true,false,false));
    assert(!controls.Step(2,false,true,true,false,false));
    assert(!controls.Step(2,false,false,false,false,true));
    assert(!controls.Step(2,false,true,true,false,true));
    assert(!controls.Step(2,false,false,true,false,true));
    assert(controls.Step(2,false,true,true,false,true));
    const auto root="/tmp/fq-movies-"+std::to_string(getpid());std::filesystem::create_directories(root+"/ViDeO/SubFolder");
    const auto file=root+"/ViDeO/SubFolder/Fixture.BIK";Write(file,SolidMovie());
    assert(fo3movie::ResolvePath(root,"subfolder/fixture.bik")==file);
    for(const char* bad:{"../fixture.bik","/tmp/f.bik","bad//f.bik","f.mp4","C:\\f.bik"})assert(!fo3movie::ValidPath(bad)&&fo3movie::ResolvePath(root,bad).empty());
    fo3movie::Decoder decoder;std::string error;assert(decoder.Open(file,error));
    fo3movie::Decoded frame;unsigned count=0;
    while(decoder.Next(frame,error)){
        assert(frame.kind==fo3movie::Decoded::Kind::Video&&frame.width==16&&frame.height==16&&frame.rgba.size()==1024);
        assert(std::abs(frame.pts-double(count)/30)<.00001&&std::abs(frame.duration-1./30)<.00001);
        if(count==0)assert(frame.rgba[0]<30);
        if(count==1)assert(frame.rgba[0]>220);
        for(size_t i=3;i<frame.rgba.size();i+=4)assert(frame.rgba[i]==255);
        ++count;
    }
    assert(error.empty()&&count==3);
    Write(root+"/bad.bik",Bytes{'b','a','d'});assert(!decoder.Open(root+"/bad.bik",error)&&!error.empty());
    fo3movie::Playback player;player.Start(file);
    for(unsigned i=0;i<100;++i){player.Tick(10+i*.01,false,1);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    assert(!player.Frame()&&player.Status()==fo3movie::Playback::State::Opening);
    player.Tick(20,true,1);assert(player.Frame()&&player.Frame()->pts==0);
    player.Tick(21,false,1);player.Tick(50,true,1);assert(player.Frame()->pts==0); // Focus loss does not age the movie.
    for(unsigned i=1;i<100&&player.Status()!=fo3movie::Playback::State::Finished;++i){player.Tick(50+i*.01,true,1);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    assert(player.Status()==fo3movie::Playback::State::Finished);
    player.Start(root+"/missing.bik");
    for(unsigned i=0;i<100&&player.Status()!=fo3movie::Playback::State::Failed;++i){player.Tick(60+i*.01,true,1);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    assert(player.Status()==fo3movie::Playback::State::Failed&&!player.Error().empty());player.Stop();
    std::filesystem::remove_all(root);std::cout<<"Real Bink decoding, RGBA/PTS/EOF, asynchronous playback, focus pause and missing-file failures passed\n";
}
