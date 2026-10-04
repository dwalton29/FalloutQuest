#include "../../app/src/main/cpp/rendering/multiview-shader-source.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
std::string Read(const char* path){std::ifstream file(path);assert(file);return {std::istreambuf_iterator<char>(file),{}};}
std::string Source(const std::string& text,const std::string& name){
    const auto begin=text.find(name);assert(begin!=std::string::npos);
    const auto start=text.find("R\"(",begin)+3,end=text.find(")\";",start);
    assert(end!=std::string::npos);return text.substr(start,end-start);
}
int main(int argc,char** argv){
    assert(argc==4);
    const auto runtime=Read(argv[1]);
    auto vertex=Source(runtime,"static const char* vertexSource");
    auto fragment=Source(runtime,"static const char* fragmentSource");
    questrender::TrimShaderPreamble(vertex);questrender::TrimShaderPreamble(fragment);
    const auto originalVertex=vertex,originalFragment=fragment;
    assert(questrender::WorldMultiviewSources(vertex,fragment));
    // Both view matrices and view-dependent lighting are indexed, and the
    // fragment shader receives the selected view with a flat integer varying.
    assert(vertex.find("uStereoMvp[gl_ViewID_OVR]")!=std::string::npos);
    assert(vertex.find("uStereoEye[gl_ViewID_OVR]")!=std::string::npos);
    assert(fragment.find("uStereoEye[vStereoView]")!=std::string::npos);
    assert(vertex.find("layout(num_views=2) in;")!=std::string::npos);
    auto bad=originalVertex;bad.replace(bad.find("uniform mat4 uMvp;"),18,"uniform mat4 changed;");
    auto fallbackFragment=originalFragment;
    assert(!questrender::WorldMultiviewSources(bad,fallbackFragment));
    auto repeated=originalVertex;repeated+="\nuniform mat4 uMvp;\n";
    fallbackFragment=originalFragment;
    assert(!questrender::WorldMultiviewSources(repeated,fallbackFragment));
    const std::string directory=argv[2];
    std::ofstream(directory+"/world.vert")<<vertex;
    std::ofstream(directory+"/world.frag")<<fragment;
    std::ofstream(directory+"/mono.vert")<<originalVertex;
    std::ofstream(directory+"/mono.frag")<<originalFragment;
    const auto pipeline=Read(argv[3]);
    std::ofstream(directory+"/import.vert")<<Source(pipeline,"const char* vertex=");
    std::ofstream(directory+"/import.frag")<<Source(pipeline,"const char* fragment=");
    std::cout<<"Multiview shader generation and fallback tests passed\n";
}
