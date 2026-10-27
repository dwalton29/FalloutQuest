"""Execute the production exterior CELL texture step, including native failure.

No original assets are embedded. Readiness, byte ownership and fallback behavior
are checked with decoded synthetic images and the actual context-free frontend.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ExteriorTextureUpload(unittest.TestCase):
    def test_streamed_pixels_reach_backend_before_cache_publication(self):
        source = (ROOT/'app/src/main/cpp/world/fo3-cell-streaming.inc').read_text()
        structs = source[source.index('struct Q1900TextureRequestQ19'):source.index('struct Q1960ShapeUploadQ19')]
        step = source[source.index('enum class Q1960UploadStepQ19'):source.index('Q1960UploadStepQ19 Q1960AdvanceShapeQ19')]
        harness = r'''
#define FQ_ENABLE_VULKAN 1
#include <GLES3/gl3.h>
// Give the recording GLES fallback real handles, as its driver would.
inline void glGenTextures(int n,GLuint* ids){for(int i=0;i<n;++i)ids[i]=900u+i;++driverCalls["glGenTextures"];}
#include "rendering/gl-state-cache.h"
#include "data/fo3-texture-bsa.h"
#include <chrono>
#include <array>
#include <cassert>
#include <iostream>
#include <unordered_map>
#define Q6H_LOGI(...) do {} while(0)
#define Q6H_LOGW(...) do {} while(0)
#define Q6H_LOGE(...) do {} while(0)
constexpr unsigned GL_TEXTURE_WRAP_S=0x2802,GL_TEXTURE_WRAP_T=0x2803,GL_REPEAT=0x2901;
constexpr unsigned GL_TEXTURE_MIN_FILTER=0x2801,GL_TEXTURE_MAG_FILTER=0x2800;
constexpr unsigned GL_LINEAR=0x2601,GL_LINEAR_MIPMAP_LINEAR=0x2703;
constexpr unsigned GL_SRGB8_ALPHA8=0x8c43,GL_RGBA8=0x8058,GL_RGBA=0x1908,GL_UNSIGNED_BYTE=0x1401,GL_NO_ERROR=0;
''' + structs + r'''
struct Q1900CellStateQ19 {Q1960TextureUploadQ19 q1960TextureUpload;std::vector<Q1900TextureRequestQ19> textures;size_t textureCursor=0;uint64_t gpuUs=0,maxTextureGpuUs=0;int gridX=0,gridY=0;};
struct CachedGpuTexture{GLuint id;bool real;};
std::unordered_map<std::string,CachedGpuTexture> gTextureCache;
constexpr size_t Q1960_TEXTURE_CHUNK_BYTES=16;
constexpr uint64_t Q1910_GPU_ITEM_WARN_US=100000;
static unsigned known=0,uploads=0;
static bool realImage=true;
static std::vector<uint8_t> expected;
std::string TextureCacheKey(const std::string& p,const char*){return p;}
void Q1900MarkGpuTextureKnownQ19(const std::string&){++known;}
void Q1070ApplyStaticAnisotropy(){}
bool Q1900TakePreparedTextureQ19(const std::string&,Fo3RgbaTexture& image,bool& real){image.width=image.height=4;image.rgba=expected;real=realImage;return true;}
bool UploadTexture(const std::string&,const std::vector<uint8_t>&,GLuint&,bool&,const char*,unsigned){throw std::runtime_error("unexpected cold fallback");}
''' + step + r'''
static Q1900CellStateQ19 NewCell(const char* label="DIFFUSE"){
    Q1900CellStateQ19 state;state.textures.push_back({"textures/synthetic.dds",label,{51,102,153,255}});return state;
}
static Q1960UploadStepQ19 Finish(Q1900CellStateQ19& state){
    size_t bytes=0;Q1960UploadStepQ19 result=Q1960UploadStepQ19::Pending;
    for(int steps=0;state.textureCursor==0&&steps<20;++steps){
        result=Q1960AdvanceTextureQ19(state,bytes);
        if(state.textureCursor==0)assert(gTextureCache.empty()&&known==0&&uploads==0);
    }
    assert(state.textureCursor==1&&!state.q1960TextureUpload.active);return result;
}
static void Reset(bool native){fqrender::nativeOnly=native;fqrender::driverCalls=0;fqgl::Reset();driverCalls.clear();fqvk::hooks={};gTextureCache.clear();known=uploads=0;realImage=true;}
int main(){
    expected.resize(64);for(size_t i=0;i<expected.size();++i)expected[i]=uint8_t(i*3);
    for(const auto* label:{"DIFFUSE","NORMAL","GLOW"}){
        Reset(true);auto state=NewCell(label);
        fqvk::hooks.texture=[](uint32_t id,int w,int h,const uint8_t* pixels,unsigned flags){
            assert(id&&w==4&&h==4&&flags==3&&gTextureCache.empty()&&known==0);
            assert(std::equal(expected.begin(),expected.end(),pixels));++uploads;
        };
        assert(Finish(state)==Q1960UploadStepQ19::Complete);
        assert(uploads==1&&known==1&&gTextureCache.at("textures/synthetic.dds").real);
        assert(driverCalls.empty()&&fqrender::driverCalls==0);
        // A second CELL borrowing the same native image must not re-upload it.
        auto warm=NewCell(label);size_t bytes=0;
        assert(Q1960AdvanceTextureQ19(warm,bytes)==Q1960UploadStepQ19::Complete&&uploads==1);
    }
    Reset(true);realImage=false;auto fallback=NewCell();
    fqvk::hooks.texture=[](uint32_t,int w,int h,const uint8_t* p,unsigned flags){assert(w==1&&h==1&&flags==3&&p[0]==51&&p[1]==102&&p[2]==153&&p[3]==255);++uploads;};
    assert(Finish(fallback)==Q1960UploadStepQ19::Complete&&uploads==1);
    assert(!gTextureCache.at("textures/synthetic.dds").real);
    // A missing backend and an upload failure must not poison the ready cache.
    Reset(true);auto absent=NewCell();assert(Finish(absent)==Q1960UploadStepQ19::Failed);
    assert(gTextureCache.empty()&&known==0&&driverCalls.empty());
    Reset(true);auto failed=NewCell();fqvk::hooks.texture=[](uint32_t,int,int,const uint8_t*,unsigned){fqvk::hooks={};};
    assert(Finish(failed)==Q1960UploadStepQ19::Failed&&gTextureCache.empty()&&known==0);
    // GLES keeps its sliced allocation/row/mipmap path without needing Vulkan.
    Reset(false);auto gles=NewCell();assert(Finish(gles)==Q1960UploadStepQ19::Complete&&uploads==0&&known==1);
    assert(driverCalls["glTexImage2D"]==1&&driverCalls["glTexSubImage2D"]==4&&driverCalls["glGenerateMipmap"]==1);
    // Optional GLES probe failure also cannot reject a valid GLES image.
    Reset(false);auto probe=NewCell();fqvk::hooks.texture=[](uint32_t,int,int,const uint8_t*,unsigned){fqvk::hooks={};};
    assert(Finish(probe)==Q1960UploadStepQ19::Complete&&known==1);
    std::cout<<"Exterior streaming: native pixel handoff precedes publication, cached reuse, material fallbacks, failure rejection and GLES fallback passed\n";
}
'''
        with tempfile.TemporaryDirectory() as temp:
            cpp = Path(temp)/'exterior.cpp'
            binary = Path(temp)/'exterior'
            cpp.write_text(harness)
            subprocess.run(['g++', '-O1', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            '-Wno-unused-variable', '-I'+str(ROOT/'tests/render/mock'),
                            '-I'+str(ROOT/'app/src/main/cpp'), str(cpp), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
