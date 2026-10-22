"""Exercise recovery program admission, stale properties and compiler failures."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
RENDER = (ROOT / 'app/src/main/cpp/rendering/terrain/fo3-terrain-render.inc').read_text()


class TerrainProgramFallback(unittest.TestCase):
    def test_recovery_keeps_original_program_and_geometry_despite_stale_properties(self):
        # Compile the actual production factory, including its real shader texts.
        factory = 'GLuint Q76BCreateProgram() {' + RENDER.split('GLuint Q76BCreateProgram() {', 1)[1].split('Q76BV3 Q76BToVr(', 1)[0]
        prefix = r'''
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include "rendering/terrain/terrain-layer-options.h"
#include "rendering/terrain/fo3-terrain-v237-shaders.h"
using GLuint=unsigned; using GLenum=unsigned; using GLint=int;
constexpr unsigned GL_VERTEX_SHADER=1,GL_FRAGMENT_SHADER=2,GL_LINK_STATUS=3,GL_RENDERER=4;
constexpr int GL_FALSE=0,GL_TRUE=1;
#define Q76B_LOGI(...) ((void)0)
#define Q76B_LOGE(...) ((void)0)
std::string failure;
unsigned nextId=10, deletedShaders=0, deletedPrograms=0, originalCompiles=0, fusedCompiles=0;
std::vector<bool> shaderFused(100),programFused(100);
void Check(bool ok){if(!ok)std::abort();}
GLuint Q76BCompile(GLenum kind,const char* text) {
    bool fused=std::strstr(text,"aPaintAlpha")||std::strstr(text,"uPaintCount");
    (fused?fusedCompiles:originalCompiles)++;
    if(!fused)Check(std::strcmp(text,kind==GL_VERTEX_SHADER ?
        fo3terrain::kV237VertexShader : fo3terrain::kV237FragmentShader)==0);
    if(failure=="both"||(fused&&((failure=="vertex"&&kind==GL_VERTEX_SHADER)||
        (failure=="fragment"&&kind==GL_FRAGMENT_SHADER)))||
        (!fused&&((failure=="original-vertex"&&kind==GL_VERTEX_SHADER)||
        (failure=="original-fragment"&&kind==GL_FRAGMENT_SHADER))))return 0;
    auto id=nextId++;shaderFused[id]=fused;return id;
}
GLuint glCreateProgram(){return nextId++;}
void glAttachShader(GLuint p,GLuint s){programFused[p]=programFused[p]||shaderFused[s];}
void glLinkProgram(GLuint){}
void glDeleteShader(GLuint){++deletedShaders;}
void glDeleteProgram(GLuint){++deletedPrograms;}
void glGetProgramiv(GLuint p,GLenum,GLint* ok){*ok=!((failure=="link"&&programFused[p])||failure=="original-link");}
void glGetProgramInfoLog(GLuint,int,void*,char*){}
const unsigned char* glGetString(GLenum){return reinterpret_cast<const unsigned char*>("mock-device");}
'''
        main = r'''
int main(int argc,char** argv){
    Check(argc==2);failure=argv[1];
    Check(fo3terrain::LegacyLayers()); // No cold/streamed triangle removal.
    const auto p=Q76BCreateProgram();
    Check(fo3terrain::LegacyLayers()&&fusedCompiles==0&&originalCompiles==2);
    if(failure=="both"||failure.find("original-")==0){
        Check(!p);
        if(failure=="original-link")Check(deletedPrograms==1);
    }else Check(p&&!programFused[p]);
    failure="none";auto subsequent=Q76BCreateProgram();
    Check(subsequent&&!programFused[subsequent]&&originalCompiles==4&&fusedCompiles==0);
    fo3terrain::DisableFusionAfterProgramFailure();
    Check(fo3terrain::LegacyLayers());
}
'''
        with tempfile.TemporaryDirectory() as temp:
            temp = Path(temp)
            (temp/'sys').mkdir()
            (temp/'sys/system_properties.h').write_text(r'''
#pragma once
#include <cstdlib>
#include <cstring>
#define PROP_VALUE_MAX 92
inline int __system_property_get(const char* key,char* value){
    const char* text=std::strstr(key,"terrain_legacy")?std::getenv("FQ_LEGACY"):"";
    if(!text)text="";std::strcpy(value,text);return std::strlen(text);
}
''')
            cpp = temp/'factory.cpp'
            cpp.write_text(prefix+factory+main)
            binary = temp/'factory'
            import os
            for defines in [[], ['-D__ANDROID__'], ['-D__ANDROID__', '-DNDEBUG']]:
                subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-misleading-indentation',
                                *defines, '-I'+str(temp), '-I'+str(ROOT/'app/src/main/cpp'), str(cpp), '-o', str(binary)], check=True)
                for setting in ['', '1', 'garbage', '0']:
                    for failure in ['none', 'vertex', 'fragment', 'link', 'both',
                                    'original-vertex', 'original-fragment', 'original-link']:
                        subprocess.run([str(binary), failure], env={**os.environ, 'FQ_LEGACY':setting}, check=True)


if __name__ == '__main__':
    unittest.main()
