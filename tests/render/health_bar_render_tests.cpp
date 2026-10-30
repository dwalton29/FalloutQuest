#include <GLES3/gl3.h>
// This test exercises cold asset creation as well as the warm render path.
inline GLuint nextHealthObject=100;
inline void glGenTextures(GLsizei n,GLuint* ids){++driverCalls["glGenTextures"];while(n--)*ids++=nextHealthObject++;}
inline void glGenBuffers(GLsizei n,GLuint* ids){++driverCalls["glGenBuffers"];while(n--)*ids++=nextHealthObject++;}
inline void glGenVertexArrays(GLsizei n,GLuint* ids){++driverCalls["glGenVertexArrays"];while(n--)*ids++=nextHealthObject++;}
#include "ui/interaction/fo3-health-bar-hud.h"
#include "ui/interaction/fo3-compass-hud.h"
#include "ui/interaction/fo3-sneak-hud.h"
#include "ui/interaction/fo3-blink-hud.h"
#include <cassert>
#include <iostream>
#include <limits>

unsigned assetReads=0;
bool LoadFalloutTextureRgba(const std::string& path,Fo3RgbaTexture& out){
  ++assetReads;
  assert(path.find("solid.dds")==std::string::npos);
  out.width=8;out.height=32;out.rgba.assign(8*32*4,255);return true;
}
namespace fo3assets {
bool LoadTextureFile(const std::string&,std::vector<uint8_t>& out,BsaFileInfo*,size_t){
  ++assetReads;
  const std::string tai=
    "glow_hud_left_seperatorglow.dds health-atlas.dds,0,0,0.1,0.2,0,0.5,0.25\n"
    "glow_enemy_health_seperator.dds health-atlas.dds,0,0,0.6,0.4,0,0.3,0.1\n";
  out.assign(tai.begin(),tai.end());return true;
}
}
bool Close(float a,float b){return std::abs(a-b)<.000001f;}
int main(){
  using namespace fo3healthui;
  // Executable helper floors HP to ticks, centers enemy HP in tick pairs,
  // and preserves one visible tick for a living actor with critically low HP.
  assert(FillWidth(Style::Player,1)==296&&FillWidth(Style::Player,.5f)==144);
  assert(FillWidth(Style::Enemy,1)==240&&FillWidth(Style::Enemy,.5f)==112);
  for(auto style:{Style::Player,Style::Enemy}){
    assert(FillWidth(style,0)==0&&FillWidth(style,.0001f)==8);
    assert(FillWidth(style,-1)==0&&FillWidth(style,2)==FillWidth(style,1));
    assert(FillWidth(style,std::numeric_limits<float>::quiet_NaN())==0);
  }
  auto& hud=fo3hudrenderer::State();hud.attempted=hud.ready=true;
  hud.program=30;hud.interfaceTexture=31;hud.button.atlasPath="other.dds";
  {
    fo3hudrenderer::CachedStateGuard guard;
    assert(Ensure());
  }
  auto& resources=Get();
  assert(resources.tick.ownsTexture&&resources.playerBracket.ownsTexture);
  assert(!resources.enemyBracket.ownsTexture);
  assert(resources.playerBracket.texture==resources.enemyBracket.texture);
  assert(resources.tick.uv.w==1&&Close(resources.enemyBracket.uv.u,.6f));
  const auto reads=assetReads;
  Vertices vertices{};
  const std::array<float,3> center{0,0,0},right{1,0,0};
  const size_t full=BuildVertices(vertices,Style::Enemy,1,.24f,center,right,resources.enemyBracket.uv,resources.tick.uv);
  assert(full==186);assert(Close(vertices[6].x,-.12f)&&Close(vertices[full-1].x,.12f));
  assert(Close(vertices[6].y,.01f)&&Close(vertices[8].y,-.01f));
  // Bracket UV subrect is retained; every tick repeats full original art.
  assert(Close(vertices[0].u,.6f)&&Close(vertices[1].u,.9f));
  assert(vertices[6].u==0&&vertices[7].u==1&&vertices[12].u==0);
  const size_t half=BuildVertices(vertices,Style::Enemy,.5f,.24f,center,right,resources.enemyBracket.uv,resources.tick.uv);
  assert(half==90&&Close(vertices[6].x,-.056f)&&Close(vertices[half-1].x,.056f));
  assert(BuildVertices(vertices,Style::Player,0,.18f,center,right,resources.playerBracket.uv,resources.tick.uv)==6);
  const size_t player=BuildVertices(vertices,Style::Player,.5f,.30f,center,right,resources.playerBracket.uv,resources.tick.uv);
  assert(Close(vertices[6].x,-.15f)&&Close(vertices[player-1].x,-.006f));
  fqgl::Invalidate();glUseProgram(91);glBindVertexArray(92);glBindBuffer(GL_ARRAY_BUFFER,93);
  glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,94);
  glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glDisable(GL_BLEND);glDepthMask(GL_TRUE);
  glBlendFuncSeparate(1,2,3,4);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
  const auto queries=fqgl::counters.queries;const auto allocations=driverCalls["glBufferData"];
  const float mvp[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  fo3health::Bar bar;bar.Observe(100,100,0);bar.Observe(50,100,1);
  for(auto style:{Style::Player,Style::Enemy})for(int eye=0;eye<2;++eye){
    const auto draws=driverCalls["glDrawArrays"];
    RenderBar(mvp,center,{0,0,2},.28f,bar,1,style==Style::Enemy,style);
    assert(driverCalls["glDrawArrays"]==draws+2);
    assert(fqgl::state.program.value==91&&fqgl::state.vao.value==92);
    assert(fqgl::state.arrayBuffer.value==93&&fqgl::state.texture2d[0].value==94);
    assert(fqgl::state.depth.value&&fqgl::state.cull.value&&!fqgl::state.blend.value&&fqgl::state.depthWrite.value);
    assert(fqgl::state.srcRgb.value==1&&fqgl::state.dstRgb.value==2);
  }
  const auto draws=driverCalls["glDrawArrays"];
  RenderBar(mvp,center,{0,0,2},.28f,bar,5,true,Style::Enemy);
  assert(driverCalls["glDrawArrays"]==draws);
  bar.Observe(0,100,6);RenderBar(mvp,center,{0,0,2},.28f,bar,6,true,Style::Enemy);
  assert(driverCalls["glDrawArrays"]==draws+1); // empty bracket only
  assert(assetReads==reads&&fqgl::counters.queries==queries&&driverCalls["glBufferData"]==allocations);
  Shutdown();assert(!Get().ready);
  fo3compass::Frame compass;compass.bearing=0;compass.icons={{fo3compass::Kind::Quest,0},{fo3compass::Kind::Waypoint,.49f}};
  fo3compassui::Prepare(compass);const auto count=fo3compassui::Get().draws.size();assert(count==4); // two strip segments at North
  const auto compassReads=assetReads,compassDraws=driverCalls["glDrawArrays"];
  const auto compassQueries=fqgl::counters.queries;
  fo3compassui::Render(mvp);fo3compassui::Render(mvp);
  assert(driverCalls["glDrawArrays"]==compassDraws+2*count&&assetReads==compassReads);
  assert(fqgl::counters.queries==compassQueries);fo3compassui::Shutdown();

  hud.textureCount=1;for(auto& g:hud.glyphs){g.width=10;g.height=20;g.spacing=10;g.uv={0,0,1,0,0,1,1,1};}
  fo3sneakui::PrepareLabel(true,fo3sneak::Label(0));assert(fo3sneakui::Get().count>6);
  const auto sneakQueries=fqgl::counters.queries; const auto sneakReads=assetReads; const auto sneakUploads=driverCalls["glBufferData"];
  for(int eye=0;eye<2;++eye){fo3sneakui::PrepareLabel(true,fo3sneak::Label(0));fo3sneakui::Render(mvp);}
  assert(fqgl::counters.queries==sneakQueries&&assetReads==sneakReads&&driverCalls["glBufferData"]==sneakUploads);
  const auto sneakDraws=driverCalls["glDrawArrays"];fo3sneakui::PrepareLabel(false,"");fo3sneakui::Render(mvp);assert(driverCalls["glDrawArrays"]==sneakDraws);
  for(unsigned label=1;label<4;++label){fo3sneakui::PrepareLabel(true,fo3sneak::Label(label));assert(fo3sneakui::Get().count>6);}
  fo3sneakui::Shutdown();
  const auto beforeBlink=fqgl::state.program.value;fo3blinkui::Render(1);const auto blinkQueries=fqgl::counters.queries;const auto blinkUploads=driverCalls["glBufferData"];fo3blinkui::Render(.5f);assert(fqgl::counters.queries==blinkQueries&&driverCalls["glBufferData"]==blinkUploads&&fqgl::state.program.value==beforeBlink);fo3blinkui::Shutdown();
  std::cout<<"Authored HP: tick quantization, centered enemy drain, atlas UVs/sharing, empty HP, stereo state restoration and warm reuse passed\n";
}
