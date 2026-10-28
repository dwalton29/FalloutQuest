"""Exercise production cache teardown across repeated graphics sessions."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2] / 'app/src/main/cpp'


def body(path, signature):
    text = (ROOT / path).read_text()
    start = text.index('{', text.index(signature))
    depth = 1
    end = start + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


class SessionLifetime(unittest.TestCase):
    def test_real_owner_shutdown_is_idempotent_and_expires_cached_names(self):
        sky = body('rendering/environment/fo3-environment.h', 'inline void ShutdownSkyGpu()')
        pc = body('rendering/environment/fo3-pc-sky.h', 'inline void ShutdownGpu()')
        clouds = body('rendering/environment/fo3-pc-sky.h', 'inline void DeleteClouds()')
        terrain = body('rendering/terrain/fo3-terrain-render.inc', 'void ShutdownFo3TerrainGpuSession()')
        # Compile the actual owner functions, with tracked resource deletion.
        source = r'''
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <cassert>
using GLuint=unsigned;
std::unordered_set<unsigned> live;
unsigned next=1;
unsigned Make(){live.insert(next);return next++;}
void Delete(int n,const unsigned* ids){for(int i=0;i<n;++i)assert(live.erase(ids[i])==1);}
void glDeleteTextures(int n,const unsigned* ids){Delete(n,ids);}
void glDeleteBuffers(int n,const unsigned* ids){Delete(n,ids);}
void glDeleteVertexArrays(int n,const unsigned* ids){Delete(n,ids);}
void glDeleteProgram(unsigned id){Delete(1,&id);}
#define Q76B_LOGI(...) ((void)0)
namespace fo3env {
unsigned skyProgram=0,skyVao=0,skyVbo=0;int skyVertexCount=0;
int skyMvpLocation=0,skyUpperLocation=0,skyLowerLocation=0,skyHorizonLocation=0;
bool skyLogged=false;
void ShutdownSkyGpu() SKY
}
namespace fo3pcsky {
struct CloudGpu {unsigned texture=0;};std::array<CloudGpu,4> gClouds{};
unsigned gWeatherForm=0,gSunTexture=0,gProgram=0;
bool gSunTextureAttempted=false,gLogged=false;
int gMvpLoc=0,gModeLoc=0,gTextureLoc=0,gUpperLoc=0,gLowerLoc=0,gHorizonLoc=0,gColourLoc=0,gOffsetLoc=0,gSunDirectionLoc=0;
void DeleteClouds() CLOUDS
void ShutdownGpu() PC
}
std::unordered_map<std::string,unsigned> q1910TerrainTextureCache,q1810TerrainNormalTextureCache;
std::unordered_set<unsigned> terrainGeometry;
std::vector<int> q229TerrainBaseBatches,q229TerrainPaintBatches;
// Production calls streaming shutdown before walking the cached textures.
void ShutdownFo3TerrainRenderQ76(){for(auto id:terrainGeometry)Delete(1,&id);terrainGeometry.clear();}
void ShutdownFo3TerrainGpuSession() TERRAIN
int main(){
 for(int session=0;session<3;++session){
  fo3env::skyProgram=Make();fo3env::skyVao=Make();fo3env::skyVbo=Make();fo3env::skyVertexCount=120;
  fo3pcsky::gProgram=Make();fo3pcsky::gSunTexture=Make();fo3pcsky::gSunTextureAttempted=true;
  for(auto& c:fo3pcsky::gClouds)c.texture=Make();
  q1910TerrainTextureCache["land"]=Make();q1810TerrainNormalTextureCache["normal"]=Make();terrainGeometry.insert(Make());
  fo3pcsky::ShutdownGpu();ShutdownFo3TerrainGpuSession();
  assert(live.empty()&&!fo3env::skyProgram&&!fo3env::skyVao&&!fo3env::skyVertexCount);
  assert(!fo3pcsky::gProgram&&!fo3pcsky::gSunTexture&&!fo3pcsky::gSunTextureAttempted);
  assert(q1910TerrainTextureCache.empty()&&q1810TerrainNormalTextureCache.empty());
  fo3pcsky::ShutdownGpu();ShutdownFo3TerrainGpuSession();assert(live.empty());
 }
}
'''
        source = source.replace(' SKY', ' ' + sky).replace(' CLOUDS', ' ' + clouds).replace(' PC', ' ' + pc).replace(' TERRAIN', ' ' + terrain)
        with tempfile.TemporaryDirectory() as tmp:
            cpp = Path(tmp) / 'session.cpp'
            cpp.write_text(source)
            binary = Path(tmp) / 'session'
            subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', str(cpp), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)

    def test_session_ends_all_owners_before_destroying_backend(self):
        loop = body('core/fo3-runtime-loop.inc', 'void Shutdown()')
        self.assertLess(loop.index('ShutdownFo3SceneGpu()'), loop.index('fqvk::ShutdownWorldProbe()'))
        self.assertLess(loop.index('ShutdownFo3SceneGpu()'), loop.index('eglDestroyContext'))
        self.assertLess(loop.index('ShutdownFo3SceneGpu()'), loop.index('if (framebuffer_)'))
        scene = body('core/fo3-runtime.cpp', 'bool ShutdownFo3SceneGpu()')
        for call in ['Q210DeletePlayerBody()', 'Q1990ClearNativeLodGeometry()', 'ShutdownFo3TerrainGpuSession()', 'fo3pcsky::ShutdownGpu()', 'Q230DeleteNpcActors()']:
            self.assertLess(scene.index(call), scene.index('Q2017ForceClearSharedGeometry()'))
        self.assertIn('gQ210PlayerBodyAttempted=false', scene)
        self.assertIn('gQ1990NativeLodOriginValid=false', scene)
        self.assertIn('&gCubeTextureCacheQ2050', scene)
        wrapper = body('core/fo3-runtime.cpp', 'void Q6HDeleteFramebuffers(')
        self.assertNotIn('Shutdown', wrapper)


if __name__ == '__main__':
    unittest.main()
