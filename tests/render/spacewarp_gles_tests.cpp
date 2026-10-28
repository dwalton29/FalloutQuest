#include "rendering/spacewarp/fo3-motion-gles.h"
#include <EGL/egl.h>
#include <cassert>
#include <iostream>
#include <limits>
#include <dlfcn.h>
using namespace fqspacewarp;
static unsigned driverQueries=0;
// Observe a real driver entry point used at the beginning of every GL state
// scope. Forward to GLVND for the pixel tests; reject any native-mode entry.
extern "C" void GL_APIENTRY glGetIntegerv(GLenum name,GLint* value){
 assert(!fqrender::nativeOnly);++driverQueries;
 static auto original=reinterpret_cast<PFNGLGETINTEGERVPROC>(dlsym(RTLD_NEXT,"glGetIntegerv"));assert(original);original(name,value);
}
namespace {
constexpr int Size=16;
Frame FrameAt(int){static uint64_t serial=0;Frame f;f.serial=++serial;f.scene=1;f.world=0x3c;f.displayTime=serial*10000000;f.sceneReady=f.focused=true;return f;}
GLuint Texture(GLint format,GLenum external,GLenum type,const void* data,int width=Size,int height=Size){
 GLuint t=0;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
 glTexImage2D(GL_TEXTURE_2D,0,format,width,height,0,external,type,data);assert(glGetError()==GL_NO_ERROR);return t;
}
void Depth(GLuint texture,float z){
 std::vector<uint32_t> data(Size*Size,uint32_t(double(z)*std::numeric_limits<uint32_t>::max()));
 glBindTexture(GL_TEXTURE_2D,texture);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,Size,Size,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,data.data());
}
bool Close(float x,float y){return std::abs(x-y)<.002f;}
}
int main(){
 fqmotion::Gles disabled;fqrender::nativeOnly=true;
 assert(!disabled.Initialize()&&!disabled.Visualize(1,Size,Size,1));assert(driverQueries==0);fqrender::nativeOnly=false;
 EGLDisplay display=eglGetDisplay(EGL_DEFAULT_DISPLAY);assert(eglInitialize(display,nullptr,nullptr));assert(eglBindAPI(EGL_OPENGL_ES_API));
 const EGLint configAttrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_NONE};
 EGLConfig config;EGLint count=0;assert(eglChooseConfig(display,configAttrs,&config,1,&count)&&count);
 const EGLint surfaceAttrs[]={EGL_WIDTH,Size,EGL_HEIGHT,Size,EGL_NONE},contextAttrs[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
 EGLSurface surface=eglCreatePbufferSurface(display,config,surfaceAttrs);EGLContext context=eglCreateContext(display,config,EGL_NO_CONTEXT,contextAttrs);
 assert(context!=EGL_NO_CONTEXT&&eglMakeCurrent(display,surface,surface,context));
 fqmotion::Gles renderer;if(!renderer.Initialize()){std::cerr<<renderer.Error()<<'\n';return 1;}
 GLuint motion=Texture(GL_RGBA16F,GL_RGBA,GL_HALF_FLOAT,nullptr),depth=Texture(GL_DEPTH_COMPONENT24,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr),source=Texture(GL_DEPTH_COMPONENT24,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
 Depth(source,.5f);const unsigned char white[]={255,255,255,255};GLuint diffuse=Texture(GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE,white,1,1);
 GLuint vbo=0,skin=0,read=0;glGenBuffers(1,&vbo);glGenBuffers(1,&skin);glGenFramebuffers(1,&read);
 std::vector<float> vertices(3*18);
 for(int v=0;v<3;++v){vertices[v*18]=(v==1?.8f:v==2?0.f:-.8f);vertices[v*18+1]=v==2?.8f:-.8f;vertices[v*18+14]=vertices[v*18+15]=vertices[v*18+16]=vertices[v*18+17]=1;}
 glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(float),vertices.data(),GL_STATIC_DRAW);
 fqmotion::Draw draw;draw.identity=1;draw.vbo=vbo;draw.diffuse=diffuse;draw.count=draw.vertices=3;draw.cull=false;
 auto render=[&](Drawable previous,Drawable current,std::array<float,3> scale=std::array<float,3>{1,1,1},Matrix previousVP=fqskin::Identity(),Matrix currentVP=fqskin::Identity()){
   History history;previous.geometry=current.geometry=1;
   for(int frame=1;frame<=2;++frame){auto f=FrameAt(frame);f.viewProjection={previousVP,previousVP};assert(history.Begin(f));assert(history.Record(1,previous));history.EyeRendered(0);history.EyeRendered(1);assert(history.Finish(true));}
   auto f=FrameAt(3);f.viewProjection={currentVP,currentVP};assert(history.Begin(f));
   fqmotion::Capture capture;capture.Begin(history);capture.Main();assert(capture.Record(draw,current));
   glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(2,3,9,10);glEnable(GL_SCISSOR_TEST);glScissor(1,2,3,4);glDepthMask(GL_FALSE);glDepthFunc(GL_GREATER);glColorMask(GL_FALSE,GL_TRUE,GL_FALSE,GL_TRUE);
   glDepthRangef(.1f,.9f);glActiveTexture(GL_TEXTURE3);
   const auto beforeNative=driverQueries;fqrender::nativeOnly=true;
   assert(!renderer.Initialize()&&!renderer.Render(capture,0,source,motion,depth,Size,Size,scale)&&!renderer.Visualize(0,Size,Size,motion));
   assert(driverQueries==beforeNative);fqrender::nativeOnly=false;
   assert(renderer.Render(capture,0,source,motion,depth,Size,Size,scale));
   const auto uploaded=renderer.UploadedBytes(),allocated=renderer.Allocations();
   assert(renderer.Render(capture,1,source,motion,depth,Size,Size,scale));
   assert(renderer.UploadedBytes()==uploaded&&renderer.Allocations()==allocated);
   GLint viewport[4]{},framebuffer=1,active=0,depthFunc=0;GLboolean colour[4]{},mask=GL_TRUE;
   glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&framebuffer);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glGetIntegerv(GL_DEPTH_FUNC,&depthFunc);
   glGetBooleanv(GL_COLOR_WRITEMASK,colour);glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);
   assert(viewport[0]==2&&viewport[1]==3&&viewport[2]==9&&viewport[3]==10&&framebuffer==0&&active==GL_TEXTURE3&&depthFunc==GL_GREATER);
   assert(glIsEnabled(GL_SCISSOR_TEST)&&!mask&&!colour[0]&&colour[1]&&!colour[2]&&colour[3]);
   glBindFramebuffer(GL_FRAMEBUFFER,read);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,motion,0);glReadBuffer(GL_COLOR_ATTACHMENT0);
   std::array<float,4> result{};glReadPixels(8,7,1,1,GL_RGBA,GL_FLOAT,result.data());assert(glGetError()==GL_NO_ERROR);
   history.EyeRendered(0);history.EyeRendered(1);assert(history.Finish(true));
   auto next=FrameAt(4);next.viewProjection={currentVP,currentVP};assert(history.Begin(next));capture.Begin(history);capture.Main();assert(capture.Record(draw,current));
   assert(renderer.Render(capture,0,source,motion,depth,Size,Size,scale));assert(renderer.Allocations()==allocated);
   glBindFramebuffer(GL_FRAMEBUFFER,0);glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);glDepthRangef(0,1);glActiveTexture(GL_TEXTURE0);
   return result;
 };
 Drawable previous,current;previous.model[12]=-.2f;
 auto rigid=render(previous,current);assert(Close(rigid[0],.2f)&&Close(rigid[1],0)&&Close(rigid[2],0));
 auto vprevious=fqskin::Identity();vprevious[13]=-.15f;
 auto converted=render(previous,current,{1,-1,.5f},vprevious);assert(Close(converted[0],.2f)&&Close(converted[1],-.15f));
 // Static-world reprojection outside the dynamic triangle, with a camera move.
 draw.count=0;auto vp=fqskin::Identity();vp[12]=.1f;
 // No invalid dynamic command is emitted for this static-only case.
 {
  History history;auto f=FrameAt(1);assert(history.Begin(f));history.EyeRendered(0);history.EyeRendered(1);history.Finish(true);
  f=FrameAt(2);f.viewProjection={vp,vp};assert(history.Begin(f));fqmotion::Capture capture;capture.Begin(history);capture.Main();
  assert(renderer.Render(capture,1,source,motion,depth,Size,Size,{1,1,1}));
  glBindFramebuffer(GL_FRAMEBUFFER,read);std::array<float,4> pixel{};glReadPixels(8,7,1,1,GL_RGBA,GL_FLOAT,pixel.data());assert(Close(pixel[0],.1f));glBindFramebuffer(GL_FRAMEBUFFER,0);
 }
 draw.count=3;
 // Foreground depth blocks a moving object hidden behind it.
 Depth(source,.25f);auto hidden=render(previous,current);assert(Close(hidden[0],0));Depth(source,.5f);
 // A sloped surface sampled from a 4x larger depth image must still receive
 // its dynamic vectors. Naive LEQUAL against the nearest sample rejects it.
 const GLuint originalSource=source;constexpr int Large=64;
 std::vector<uint32_t> slope(Large*Large);
 for(int y=0;y<Large;++y)for(int x=0;x<Large;++x)slope[y*Large+x]=uint32_t((.5-.1*(2.0*(x+.5)/Large-1))*std::numeric_limits<uint32_t>::max());
 source=Texture(GL_DEPTH_COMPONENT24,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,slope.data(),Large,Large);
 for(int v=0;v<3;++v)vertices[v*18+2]=-.2f*vertices[v*18];
 glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferSubData(GL_ARRAY_BUFFER,0,vertices.size()*sizeof(float),vertices.data());
 auto sloped=render(previous,current);assert(Close(sloped[0],.2f));
 source=originalSource;for(int v=0;v<3;++v)vertices[v*18+2]=0;
 glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferSubData(GL_ARRAY_BUFFER,0,vertices.size()*sizeof(float),vertices.data());
 // Actual authored alpha-test silhouettes, with optional vertex/material alpha.
 const unsigned char transparent[]={255,255,255,0};glBindTexture(GL_TEXTURE_2D,diffuse);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,transparent);
 draw.alphaTest=true;auto cutout=render(previous,current);assert(Close(cutout[0],0));
 glBindTexture(GL_TEXTURE_2D,diffuse);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,white);
 auto solid=render(previous,current);assert(Close(solid[0],.2f));draw.alphaTest=false;
 // CPU/facial previous positions are independent of the current VBO.
 previous={};current={};fqmotion::Positions(previous,vertices);fqmotion::Positions(current,vertices);
 for(size_t v=0;v<3;++v)previous.deformedPositions[v*3]-=.15f;
 auto face=render(previous,current);assert(Close(face[0],.15f));
 // GPU palettes reproduce NPC normalisation and player residual weights.
 previous={};current={};previous.palette.resize(1);current.palette.resize(1);
 auto bone=fqskin::Identity();bone[12]=.2f;fqskin::Pack(previous.palette[0],fqskin::Identity(),fqskin::Identity());fqskin::Pack(current.palette[0],bone,fqskin::Identity());
 std::vector<float> weights(3*8);for(int v=0;v<3;++v)weights[v*8]=.5f;
 glBindBuffer(GL_ARRAY_BUFFER,skin);glBufferData(GL_ARRAY_BUFFER,weights.size()*sizeof(float),weights.data(),GL_STATIC_DRAW);
 GLuint palette=Texture(GL_RGBA32F,GL_RGBA,GL_FLOAT,current.palette.data(),8,1);
 draw.skinAttributes=skin;draw.paletteTexture=palette;draw.skinMode=2;
 auto npc=render(previous,current);assert(Close(npc[0],.2f));draw.skinMode=1;
 auto hand=render(previous,current);assert(Close(hand[0],.1f));
 GLuint preview=Texture(GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
 glBindFramebuffer(GL_FRAMEBUFFER,read);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,preview,0);
 assert(renderer.Visualize(read,Size,Size,motion));std::array<unsigned char,4> debug{};glReadPixels(8,7,1,1,GL_RGBA,GL_UNSIGNED_BYTE,debug.data());
 // The final repeated pose has zero velocity, visualised as neutral grey.
 assert(debug[0]>=126&&debug[0]<=129&&debug[1]>=126&&debug[1]<=129);
 renderer.Shutdown();assert(glGetError()==GL_NO_ERROR);
 eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(display,context);eglDestroySurface(display,surface);eglTerminate(display);
 std::cout<<"GLES motion pixels: static, rigid, EXT axes, depth occlusion, cutouts, facial positions, NPC/hand palettes and GL state restoration passed\n";
}
