#pragma once

#include "../data/fo3-texture-bsa.h"
#include <algorithm>
#include <atomic>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>

namespace fo3scene {
inline std::string ResidentTextureKey(std::string path) {
    for (char& c:path) {
        if(c=='/')c='\\';
        if(c>='A'&&c<='Z')c=char(c-'A'+'a');
    }
    while(!path.empty()&&path.front()=='\\')path.erase(path.begin());
    if(path.rfind("data\\",0)==0)path=path.substr(5);
    if(path.rfind("textures\\",0)==0)path=path.substr(9);
    return "textures\\"+path;
}
struct ResidentTextureRequest { std::string path; bool cube=false; };
struct ResidentTextures {
    std::unordered_map<std::string,Fo3RgbaTexture> images;
    std::unordered_map<std::string,Fo3RgbaCubeTexture> cubes;
    size_t bytes=0;
};
inline bool ResidentRgbaBytes(int width,int height,size_t& bytes) {
    if(width<=0||height<=0)return false;
    const size_t w=size_t(width),h=size_t(height),max=std::numeric_limits<size_t>::max();
    if(w>max/h||w*h>max/4)return false;
    bytes=w*h*4;return true;
}
inline bool ValidResidentImage(const Fo3RgbaTexture& image,size_t& bytes) {
    return ResidentRgbaBytes(image.width,image.height,bytes)&&image.rgba.size()==bytes;
}
inline bool ValidResidentCube(const Fo3RgbaCubeTexture& cube,size_t& bytes) {
    if(cube.width<=0||cube.width!=cube.height||cube.mipLevels<=0)return false;
    int maximumLevels=1;
    for(int extent=cube.width;extent>1;extent>>=1)++maximumLevels;
    if(cube.mipLevels>maximumLevels)return false;
    size_t total=0;
    for(const auto& face:cube.rgbaLevels) {
        if(face.size()!=size_t(cube.mipLevels))return false;
        int w=cube.width;
        for(const auto& mip:face) {
            size_t count=0;
            if(!ResidentRgbaBytes(w,w,count)||mip.size()!=count||count>SIZE_MAX-total)return false;
            total+=count;w=std::max(1,w>>1);
        }
    }
    bytes=total;return true;
}

// Worker-owned decode packet. Named assets never become invented fallback
// images. Empty paths deliberately request the renderer's existing neutral
// material defaults and consume no decode budget. Failure leaves out unchanged.
template<class DecodeImage,class DecodeCube>
bool PrepareResidentTextures(const std::vector<ResidentTextureRequest>& requests,
        ResidentTextures& out,DecodeImage image,DecodeCube cube,
        const std::atomic<bool>& cancelled,size_t byteLimit,std::string& error) {
    error.clear();ResidentTextures next;
    for(const auto& request:requests) {
        if(cancelled.load(std::memory_order_acquire)){error="resident texture preparation cancelled";return false;}
        if(request.path.empty())continue;
        const auto key=ResidentTextureKey(request.path);
        size_t bytes=0;
        if(request.cube) {
            if(next.cubes.count(key))continue;
            Fo3RgbaCubeTexture decoded;
            if(!cube(request.path,decoded)||!ValidResidentCube(decoded,bytes)) {
                error="resident cubemap unavailable: "+request.path;return false;
            }
            if(bytes>byteLimit-next.bytes){error="resident texture byte budget exceeded";return false;}
            next.cubes.emplace(key,std::move(decoded));
        } else {
            if(next.images.count(key))continue;
            Fo3RgbaTexture decoded;
            if(!image(request.path,decoded)||!ValidResidentImage(decoded,bytes)) {
                error="resident texture unavailable: "+request.path;return false;
            }
            if(bytes>byteLimit-next.bytes){error="resident texture byte budget exceeded";return false;}
            next.images.emplace(key,std::move(decoded));
        }
        next.bytes+=bytes;
    }
    if(cancelled.load(std::memory_order_acquire)){error="resident texture preparation cancelled";return false;}
    out=std::move(next);return true;
}

// Render-thread lease for privately uploaded textures. GpuObjects retain a const
// shared lease after publication. Reserve before generating any GL handle;
// adoption never allocates, and destruction runs before GL context teardown.
class ResidentTextureOwnership {
public:
    using Delete=void(*)(uint32_t) noexcept;
    explicit ResidentTextureOwnership(Delete destroy):destroy_(destroy){}
    ResidentTextureOwnership(const ResidentTextureOwnership&)=delete;
    ResidentTextureOwnership& operator=(const ResidentTextureOwnership&)=delete;
    ~ResidentTextureOwnership(){if(destroy_)for(auto id:handles_)destroy_(id);}
    void Reserve(size_t count){handles_.reserve(count);}
    bool Adopt(uint32_t id) noexcept {
        if(!destroy_||!id||handles_.size()==handles_.capacity()||
            std::find(handles_.begin(),handles_.end(),id)!=handles_.end())return false;
        handles_.push_back(id);return true;
    }
    size_t Size()const noexcept{return handles_.size();}
private:
    Delete destroy_;
    std::vector<uint32_t> handles_;
};
} // namespace fo3scene
