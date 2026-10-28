#include "rendering/spacewarp/fo3-motion-gles.h"
#include <EGL/egl.h>
#include <cassert>
#include <iostream>
#include <dlfcn.h>
extern "C" void GL_APIENTRY glGetIntegerv(GLenum name,GLint* value){
 assert(!fqrender::nativeOnly);
 static auto original=reinterpret_cast<PFNGLGETINTEGERVPROC>(dlsym(RTLD_NEXT,"glGetIntegerv"));assert(original);original(name,value);
}
// These stand in for runtime work outside the original motion-draw guard.
namespace runtime {
void Samplers(GLuint sampler){for(unsigned unit:{0u,4u,6u,15u})glBindSampler(unit,sampler);}
void State(GLuint sampler,GLuint program,GLuint vao,GLuint fbo){
 Samplers(sampler);glUseProgram(program);glBindVertexArray(vao);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
 glActiveTexture(GL_TEXTURE15);glBindTexture(GL_TEXTURE_2D,0);glBindTexture(GL_TEXTURE_CUBE_MAP,0);
 glViewport(1,2,3,4);glScissor(2,2,2,2);glEnable(GL_SCISSOR_TEST);glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
 glDepthRangef(.25f,.75f);glClearColor(1,0,1,1);glClearDepthf(.3f);glBlendFuncSeparate(GL_ZERO,GL_ONE,GL_ZERO,GL_ONE);glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT,GL_FUNC_SUBTRACT);
}
}
#include "rendering/spacewarp/fo3-driver-boundary.h"
namespace {
constexpr int Size=16;
GLuint Program(){
 const char* vs=R"(#version 300 es
 precision highp float;
 layout(location=0) in vec3 position;
 uniform sampler2D bones;uniform bool skin;uniform mat4 model;
 out vec2 uv;
 void main(){vec4 p=vec4(position,1.0);if(skin)p=mat4(texelFetch(bones,ivec2(0,0),0),texelFetch(bones,ivec2(1,0),0),texelFetch(bones,ivec2(2,0),0),texelFetch(bones,ivec2(3,0),0))*p;gl_Position=model*p;uv=position.xy*.5+.5;})";
 const char* fs=R"(#version 300 es
 precision highp float;
 in vec2 uv;uniform sampler2D ground;uniform samplerCube sky;uniform bool skyMode;
 out vec4 colour;void main(){colour=skyMode?texture(sky,vec3(0,0,1)):texture(ground,uv);})";
 GLuint result=glCreateProgram();for(int i=0;i<2;++i){GLuint s=glCreateShader(i?GL_FRAGMENT_SHADER:GL_VERTEX_SHADER);const char* text=i?fs:vs;glShaderSource(s,1,&text,nullptr);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);assert(ok);glAttachShader(result,s);glDeleteShader(s);}glLinkProgram(result);GLint ok=0;glGetProgramiv(result,GL_LINK_STATUS,&ok);assert(ok);return result;
}
GLuint Texture(GLenum format,GLenum external,GLenum type,const void* data,int width,int height){GLuint id=0;glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexImage2D(GL_TEXTURE_2D,0,format,width,height,0,external,type,data);return id;}
}
int main(){
 fqrender::nativeOnly=true;{fqmotion::DriverBoundary noDriver;}fqrender::nativeOnly=false;
 EGLDisplay display=eglGetDisplay(EGL_DEFAULT_DISPLAY);assert(eglInitialize(display,nullptr,nullptr)&&eglBindAPI(EGL_OPENGL_ES_API));
 EGLint attributes[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_NONE};EGLConfig config;EGLint count=0;assert(eglChooseConfig(display,attributes,&config,1,&count)&&count);
 EGLint surfaceAttributes[]={EGL_WIDTH,Size,EGL_HEIGHT,Size,EGL_NONE},contextAttributes[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
 EGLSurface surface=eglCreatePbufferSurface(display,config,surfaceAttributes);EGLContext context=eglCreateContext(display,config,EGL_NO_CONTEXT,contextAttributes);assert(eglMakeCurrent(display,surface,surface,context));
 fqgl::Reset();fqmotion::Gles motion;{fqmotion::DriverBoundary boundary;assert(motion.Initialize(false));}
 GLuint program=Program(),otherProgram=Program(),vao=0,otherVao=0,vbo=0,fbo=0,otherFbo=0;
 glGenVertexArrays(1,&vao);glGenVertexArrays(1,&otherVao);glGenBuffers(1,&vbo);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
 float vertices[]={-1,-1,0,3,-1,0,-1,3,0};glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,nullptr);
 std::array<unsigned char,64> groundPixels{};for(unsigned i=0;i<16;++i){groundPixels[i*4+(i%2)]=255;groundPixels[i*4+3]=255;}
 GLuint ground=Texture(GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE,groundPixels.data(),4,4);
 auto identity=fqskin::Identity();GLuint palette=Texture(GL_RGBA32F,GL_RGBA,GL_FLOAT,identity.data(),4,1);
 GLuint cube=0;glGenTextures(1,&cube);glBindTexture(GL_TEXTURE_CUBE_MAP,cube);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 std::array<unsigned char,64> blue{};for(unsigned i=0;i<16;++i){blue[i*4+2]=255;blue[i*4+3]=255;}
 for(unsigned face=0;face<6;++face)glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,0,GL_RGBA8,4,4,0,GL_RGBA,GL_UNSIGNED_BYTE,blue.data());
 GLuint colour=Texture(GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE,nullptr,Size,Size),velocity=Texture(GL_RGBA16F,GL_RGBA,GL_HALF_FLOAT,nullptr,Size,Size),depth=Texture(GL_DEPTH_COMPONENT24,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr,Size,Size);
 std::vector<uint32_t> depthPixels(Size*Size,0x80000000u);GLuint sourceDepth=Texture(GL_DEPTH_COMPONENT24,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,depthPixels.data(),Size,Size);
 glGenFramebuffers(1,&fbo);glGenFramebuffers(1,&otherFbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,colour,0);
 GLuint badSampler=0;glGenSamplers(1,&badSampler);glSamplerParameteri(badSampler,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_NEAREST);
 auto draw=[&](int kind){
  glBindFramebuffer(GL_FRAMEBUFFER,fbo);glViewport(0,0,Size,Size);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);
  glUseProgram(program);glBindVertexArray(vao);glUniform1i(glGetUniformLocation(program,"ground"),0);glUniform1i(glGetUniformLocation(program,"sky"),4);glUniform1i(glGetUniformLocation(program,"bones"),6);
  glUniform1i(glGetUniformLocation(program,"skin"),kind==1);glUniform1i(glGetUniformLocation(program,"skyMode"),kind==3);
  auto model=identity;if(kind==2)model[0]=model[5]=.7f;glUniformMatrix4fv(glGetUniformLocation(program,"model"),1,GL_FALSE,model.data());
  fqgl::BindTextureUnit(GL_TEXTURE0,GL_TEXTURE_2D,ground);fqgl::BindTextureUnit(GL_TEXTURE4,GL_TEXTURE_CUBE_MAP,cube);fqgl::BindTextureUnit(GL_TEXTURE6,GL_TEXTURE_2D,palette);
  glDrawArrays(GL_TRIANGLES,0,3);std::array<unsigned char,4> pixel{};glReadPixels(8,8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());assert(glGetError()==GL_NO_ERROR);return pixel;
 };
 std::array<std::array<unsigned char,4>,4> expected{};for(int kind=0;kind<4;++kind){expected[kind]=draw(kind);assert(expected[kind][3]==255);}
 // Invalidation forgets the binding, not the driver's current program. A
 // uniform write before the next UseProgram must invalidate its old bank.
 assert(draw(0)==expected[0]);fqgl::Invalidate();glUniform1i(glGetUniformLocation(program,"skyMode"),1);
 assert(draw(0)==expected[0]);
 // The old draw-only guard plus cache invalidation leaves runtime sampler
 // bindings in place: textured land/LOD/sky go black and bone fetches collapse.
 runtime::Samplers(badSampler);fqgl::Invalidate();bool differs=false;for(int kind=0;kind<4;++kind)differs|=draw(kind)!=expected[kind];assert(differs);
 fqmotion::Gles::PrepareSceneSampling();fqgl::Invalidate();for(int kind=0;kind<4;++kind)assert(draw(kind)==expected[kind]);
 fqspacewarp::History history;fqspacewarp::Frame frame;frame.world=0;frame.scene=1;frame.serial=1;frame.displayTime=1000;frame.sceneReady=frame.focused=true;assert(history.Begin(frame));
 fqmotion::Capture capture;capture.Begin(history);capture.Main();
 static uint32_t samplerChanges=0;
 fqmotion::reportBoundary=[](const char*,uint32_t mask){samplerChanges=mask;};
 for(int pass=0;pass<4;++pass){
  {
   fqmotion::DriverBoundary boundary("test-handoff",true);
   runtime::State(badSampler,otherProgram,otherVao,otherFbo); // acquisition/wait
   assert(motion.Render(capture,0,sourceDepth,velocity,depth,Size,Size,{1,1,1},false));
   runtime::State(badSampler,otherProgram,otherVao,otherFbo); // release/endFrame
  }
  assert(samplerChanges==((1u<<0)|(1u<<4)|(1u<<6)|(1u<<15)));
  for(int kind=0;kind<4;++kind)assert(draw(kind)==expected[kind]);
 }
 motion.Shutdown();std::cout<<"Colour after XR handoff: textured terrain, skinning palette, distant mesh, cube sky and cached bindings preserved\n";
}
