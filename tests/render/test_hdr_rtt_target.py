"""Exercise the production EXT target against capability and driver failures."""
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
class HdrTarget(unittest.TestCase):
    def test_driver_admission_and_resource_lifetime(self):
        source=(ROOT/'app/src/main/cpp/rendering/hdr-rtt-target.h').read_text().split('namespace fqhdr',1)[1]
        prefix=r'''
#include <cstring>
#include <cassert>
using GLuint=unsigned;using GLint=int;using GLsizei=int;using GLenum=unsigned;using GLboolean=bool;using GLfloat=float;
constexpr unsigned GL_RGBA16F=1,GL_EXTENSIONS=2,GL_NO_ERROR=0,GL_FRAMEBUFFER=3,GL_COLOR_ATTACHMENT0=4,GL_TEXTURE_2D=5,GL_RENDERBUFFER=6,GL_DEPTH_COMPONENT24=7,GL_DEPTH_ATTACHMENT=8,GL_RENDERBUFFER_SAMPLES=9,GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT=10,GL_FRAMEBUFFER_COMPLETE=11,GL_DEPTH_WRITEMASK=12,GL_DEPTH_CLEAR_VALUE=13,GL_DEPTH_BUFFER_BIT=14,GL_READ_FRAMEBUFFER=15,GL_DRAW_FRAMEBUFFER=16,GL_NEAREST=17,GL_RENDERBUFFER_BINDING=18;
constexpr bool GL_TRUE=true;
using PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC=void(*)(GLenum,GLenum,GLenum,GLuint,GLint,GLsizei);
using PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC=void(*)(GLenum,GLsizei,GLenum,GLsizei,GLsizei);
const char* extensions="GL_EXT_multisampled_render_to_texture";
bool procs=true,complete=true,depthCopy=true,depthMask=false;
float clearValue=.25f;int samples=2,colourSamples=2,generated=0,deleted=0,blits=0;unsigned error=0;GLint renderbuffer=77;
void attach(GLenum,GLenum,GLenum,GLuint,GLint,GLsizei s){assert(s==2);}
void storage(GLenum,GLsizei s,GLenum,GLsizei,GLsizei){assert(s==2);}
void* eglGetProcAddress(const char* name){if(!procs)return nullptr;return std::strstr(name,"Texture")?reinterpret_cast<void*>(&attach):reinterpret_cast<void*>(&storage);}
const unsigned char* glGetString(GLenum){return reinterpret_cast<const unsigned char*>(extensions);}
unsigned glGetError(){auto e=error;error=0;return e;}
void glGenFramebuffers(int,GLuint* p){*p=++generated;}
void glGenRenderbuffers(int,GLuint* p){*p=++generated;}
void glDeleteFramebuffers(int,const GLuint*){++deleted;}
void glDeleteRenderbuffers(int,const GLuint*){++deleted;}
void glBindFramebuffer(GLenum,GLuint){}
void glBindRenderbuffer(GLenum,GLuint value){renderbuffer=value;}
void glGetIntegerv(GLenum,GLint* value){*value=renderbuffer;}
void glFramebufferRenderbuffer(GLenum,GLenum,GLenum,GLuint){}
void glGetRenderbufferParameteriv(GLenum,GLenum,GLint* p){*p=samples;}
void glGetFramebufferAttachmentParameteriv(GLenum,GLenum,GLenum,GLint* p){*p=colourSamples;}
GLenum glCheckFramebufferStatus(GLenum){return complete?GL_FRAMEBUFFER_COMPLETE:0;}
void glGetBooleanv(GLenum,GLboolean* p){*p=depthMask;}
void glGetFloatv(GLenum,GLfloat* p){*p=clearValue;}
void glDepthMask(GLboolean v){depthMask=v;}
void glClearDepthf(GLfloat v){clearValue=v;}
void glClear(GLenum){}
void glBlitFramebuffer(int,int,int,int,int,int,int,int,GLenum mask,GLenum){assert(mask==GL_DEPTH_BUFFER_BIT);++blits;if(!depthCopy)error=99;}
'''
        main=r'''
int main(){
    assert(!fqhdr::HasExtension(nullptr,"GL_EXT_multisampled_render_to_texture"));
    assert(!fqhdr::HasExtension("GL_EXT_multisampled_render_to_texture2","GL_EXT_multisampled_render_to_texture"));
    assert(fqhdr::HasExtension("other GL_EXT_multisampled_render_to_texture last","GL_EXT_multisampled_render_to_texture"));
    fqhdr::RttTarget t;
    for(int bad=0;bad<8;++bad){
        extensions=bad==0?"":bad==1?"GL_EXT_multisampled_render_to_texture2":"GL_EXT_multisampled_render_to_texture";
        procs=bad!=2;samples=bad==3?4:2;colourSamples=bad==4?4:2;complete=bad!=5;depthCopy=bad!=6;
        assert(!t.Ensure(10,20,31,bad==7?0:GL_RGBA16F,32));
        assert(!t.fbo&&!t.depth&&!t.ready);
        assert(!depthMask && clearValue==.25f && renderbuffer==77);
        const auto allocations=generated;
        assert(!t.Ensure(10,20,31,bad==7?0:GL_RGBA16F,32));assert(generated==allocations);
        t.Reset();assert(generated==deleted);
    }
    extensions="GL_EXT_multisampled_render_to_texture";procs=complete=depthCopy=true;samples=colourSamples=2;
    assert(t.Ensure(10,20,31,GL_RGBA16F,32));assert(t.fbo&&t.depth&&t.colour==31);
    const auto allocations=generated,copies=blits;
    assert(t.Ensure(10,20,31,GL_RGBA16F,32));assert(generated==allocations&&blits==copies);
    assert(t.Ensure(11,20,31,GL_RGBA16F,32));assert(generated==allocations+2&&blits==copies+1);
    t.Reset();assert(generated==deleted&&!depthMask&&clearValue==.25f&&renderbuffer==77);
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            cpp=Path(tmp)/'test.cpp';cpp.write_text(prefix+'namespace fqhdr'+source+main)
            binary=Path(tmp)/'test'
            subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(cpp),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
    def test_runtime_water_gate_and_default_off(self):
        core=(ROOT/'app/src/main/cpp/core/fo3-runtime.cpp').read_text()
        options=(ROOT/'app/src/main/cpp/rendering/opaque-telemetry.h').read_text()
        self.assertIn('bool hdrRtt=false',options)
        self.assertIn('Q238SelectMainTarget(!q209bWaterInFrustum);',core)
        self.assertIn('!fqopaque::Options().hdrRtt||!allowRtt||!q1280PostActive||!q2060MsaaActive||q2060MsaaSamples!=2',core)
        self.assertIn('if(q238RttActive)mask&=~GL_COLOR_BUFFER_BIT;',core)
        post=core.split('void Q1350CompositeEyePostQ1350(',1)[1].split('void Q1280ShutdownPost',1)[0]
        self.assertLess(post.index('Q2060ResolveEyeMsaaQ2060(q234FinalResolveMask)'),post.index('Q1280CompositeEyePostQ1280('))
if __name__=='__main__':unittest.main()
