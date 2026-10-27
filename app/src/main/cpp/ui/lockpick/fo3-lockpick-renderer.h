#pragma once
#include "ui/loading/fo3-loading-vr.h"
#include "ui/interaction/fo3-interaction-hud-renderer.h"
#include "player/fo3-lockpick.h"
#include "fo3-lockpick-pose.h"
#include "fo3-lockpick-overlay.h"
namespace fo3lockui {
struct Model {
  fo3anim::Skeleton hierarchy;std::vector<fo3anim::Clip> clips;std::vector<int> blocks;
  std::vector<fo3loadingvr::CpuShape> cpu;std::vector<fo3loadingvr::Shape> shapes;fo3anim::Pose pose;
};
struct Assets {Model lock,pin;};
inline fo3scene::Preparation<Assets> preparation;
inline Assets assets;
inline bool started=false,ready=false,failed=false;
inline GLuint vao=0,vbo=0;
inline fo3anim::Matrix anchor=fo3anim::Identity();
inline bool anchored=false;
inline float breakElapsed=-1,introElapsed=0,pinIntroElapsed=0;
inline bool spritesLoaded=false;
inline std::array<fo3hudassets::TaiSprite,5> sprites;
inline void Prepare(){
  if(started)return;started=true;
  preparation.Start([](Assets& a,const std::atomic<bool>& cancel){
    for(auto pair:{std::pair<Model*,const char*>{&a.lock,"terminals/lockinterface01.nif"},{&a.pin,"terminals/bobbypin01.nif"}}){
      auto& m=*pair.first;std::vector<uint8_t> bytes;
      if(!LoadFalloutMeshFile(pair.second,bytes,nullptr)||!fo3anim::DecodeUiAnimation(bytes,m.hierarchy,m.blocks,m.clips)||
        !fo3loadingvr::ReadShapes(pair.second,m.cpu,cancel,true))return false;
      const auto clip=std::find_if(m.clips.begin(),m.clips.end(),[](const auto& c){return c.name=="Forward";});
      if(clip==m.clips.end())return false;
      fo3lockpose::Seek(m.hierarchy,&*clip,fo3lockpose::Duration(&*clip),m.pose);
      for(auto& s:m.cpu){
        if(s.mesh.shapeBlock<m.blocks.size())s.bone=m.blocks[s.mesh.shapeBlock];
        const auto& mesh=s.mesh;s.vertices.resize(mesh.positions.size()/3);
        for(size_t i=0;i<s.vertices.size();++i){auto& v=s.vertices[i];
          for(int k=0;k<3;++k){v.p[k]=mesh.positions[i*3+k];v.n[k]=mesh.normals.size()==mesh.positions.size()?mesh.normals[i*3+k]:(k==1?-1.f:0.f);}
          for(int k=0;k<2;++k)v.uv[k]=mesh.texcoords.size()==s.vertices.size()*2?mesh.texcoords[i*2+k]:0;
          for(int k=0;k<4;++k)v.colour[k]=mesh.vertexColors.size()==s.vertices.size()*4?mesh.vertexColors[i*4+k]:1;
        }
      }
    }
    return !cancel.load();
  });
}
inline void Advance(){Prepare();if(ready||failed)return;
  if(preparation.Ready()&&!preparation.Successful()){failed=true;__android_log_print(ANDROID_LOG_ERROR,"FalloutQuest","LOCKPICK ASSET MISS: terminals/lockinterface01.nif, terminals/bobbypin01.nif or textures/terminals/lockinterface01.dds");return;}
  if(!preparation.Ready())return;
  assets=std::move(preparation.Get());
  for(auto* m:{&assets.lock,&assets.pin})for(auto& s:m->cpu)fo3loadingvr::UploadShape(s,m->shapes,false);
  ready=!assets.lock.shapes.empty()&&!assets.pin.shapes.empty();preparation.Reset();
}
inline void SetHead(const float* head){if(anchored||!head)return;std::copy_n(head,16,anchor.begin());
  // Capture a stationary panel 1.4 m in front of the entry pose (VR adaptation).
  for(int k=0;k<3;++k)anchor[12+k]-=1.4f*head[8+k];anchored=true;}
inline std::string String(const fo3player::Player& p,const char* key,const char* original){auto i=p.Definitions().strings.find(key);return i==p.Definitions().strings.end()?original:i->second;}
inline void Render(const float* vp,const fo3lockpick::Game& game,const fo3player::Player& player,GLsizei width,GLsizei height){
  if(!game.reference||!anchored||!vp)return;
  fo3hudrenderer::GlStateGuard guard;glActiveTexture(GL_TEXTURE0);glDisable(GL_CULL_FACE);
  fo3anim::Matrix view;std::copy_n(vp,16,view.begin());const auto panel=fo3anim::Multiply(view,anchor);
  // Authored NIF XZ plane, Y depth; shared mapping keeps both model proportions.
  auto map=fo3anim::Identity();map[0]=.025f;map[5]=map[10]=0;map[9]=.025f;map[6]=-.025f;
  const int lockBone=fo3anim::FindBone(assets.lock.hierarchy,"lock");
  const auto centre=fo3anim::Point(lockBone>=0?assets.lock.pose.global[lockBone]:fo3anim::Identity(),{0,0,0});
  map[12]=-.025f*centre[0];map[13]=-.025f*centre[2];map[14]=.025f*centre[1];
  const auto base=fo3anim::Multiply(panel,map);
  fo3lockoverlay::DepthScope modalDepth(width,height);
  if(ready&&fo3loadingvr::EnsureProgram()){
    glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
    glUseProgram(fo3loadingvr::gProgram);glUniform1i(fo3loadingvr::gSampler,0);glUniform1f(fo3loadingvr::gFade,1);glUniform1f(fo3loadingvr::gClipUi,0);
    if(modalDepth.valid)for(auto* model:{&assets.lock,&assets.pin}) {
      auto pose=model->pose;
      const bool pin=model==&assets.pin;
      if(!fo3lockpose::Sample(model->hierarchy,model->clips,pin,pin?pinIntroElapsed:introElapsed,
                            game.cylinder,game.durability,breakElapsed,pose))continue;
      if(pin) {
        const auto placement=fo3lockpose::PinPlacement(game.pin);
        for(auto& delta:pose.delta)delta=fo3anim::Multiply(placement,delta);
      }
      for(const auto& shape:model->shapes){fo3loadingpose::Matrix mvp;auto matrix=fo3anim::Multiply(base,shape.bone>=0?pose.delta[shape.bone]:fo3anim::Identity());
        std::copy(matrix.begin(),matrix.end(),mvp.m);fo3loadingvr::Draw(shape,mvp);}
    }
  }
  if(!fo3hudrenderer::EnsureResources())return;
  const auto& font=fo3hudrenderer::State();std::vector<fo3hudassets::Vertex> vertices;
  const float scale=.023f/std::max(1.f,font.baseLine);
  auto text=[&](const std::string& value,float x,float y){fo3font::AppendText(font,value,0,0,[&](uint8_t,const fo3font::Glyph& glyph,const fo3font::Quad& q){
    fo3hudrenderer::AppendPromptQuad(vertices,x+q.left*scale,y-q.top*scale,x+q.right*scale,y-q.bottom*scale,.35f,
      glyph.uv[0],glyph.uv[1],glyph.uv[2],glyph.uv[3],glyph.uv[4],glyph.uv[5],glyph.uv[6],glyph.uv[7]);});};
  const int tier=std::max(0,player.LockRequirement(game.reference)/25);static const char* tiers[]={"VeryEasy","Easy","Average","Hard","VeryHard"};static const char* labels[]={"Very Easy","Easy","Average","Hard","Very Hard"};
  text(String(player,"sLockpickSkillText","Lockpick Skill")+"  "+std::to_string(player.Snapshot().skills[4]),-.62f,-.185f);
  text(String(player,"sPicksRemainingText","Bobby Pins")+"  "+std::to_string(player.ItemCount(player.Definitions().bobbyPin)),-.62f,-.230f);
  text(String(player,"sLockLevelText","Lock Level")+"  "+String(player,(std::string("sLockLevelName")+tiers[tier]).c_str(),labels[tier]),-.62f,-.275f);
  text(String(player,"sForceLock","Force Lock")+" ["+std::to_string(player.ForceLockChance(game.reference))+"%]",.29f,-.230f);
  text(String(player,"sExit","Exit"),.29f,-.275f);
  const GLsizei textCount=GLsizei(vertices.size());
  if(!spritesLoaded){const char* names[]={"glow_general_button_a.dds","glow_general_button_b.dds","solid.dds","fade_to_top.dds","fade_to_right.dds"};
    for(size_t i=0;i<sprites.size();++i)fo3hudassets::ParseButtonTai(sprites[i],names[i]);spritesLoaded=true;}
  auto sprite=[&](size_t index,float x,float y,float w,float h){const auto& s=sprites[index];if(s.w>0)fo3hudrenderer::AppendPromptQuad(vertices,x,y,x+w,y-h,.35f,
    s.u,s.v,s.u+s.w,s.v,s.u+s.w,s.v+s.h,s.u,s.v+s.h);};
  sprite(0,.25f,-.230f,.028f,.028f);sprite(1,.25f,-.275f,.028f,.028f);
  sprite(3,-.64f,-.150f,.002f,.030f);sprite(2,-.64f,-.178f,.002f,.145f);
  sprite(2,-.64f,-.323f,.370f,.002f);sprite(4,-.272f,-.323f,.030f,.002f);
  if(!vao){glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);}glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
  glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(fo3hudassets::Vertex),vertices.data(),GL_DYNAMIC_DRAW);
    fo3hudassets::UploadVertices(vao,vertices.data(),vertices.size());
  glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),nullptr);
  glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),reinterpret_cast<void*>(3*sizeof(float)));
  glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(font.program);glUniformMatrix4fv(font.mvpLoc,1,GL_FALSE,panel.data());glUniform1i(font.texLoc,0);
  glUniform4f(font.tintLoc,26.f/255,1,128.f/255,1);glBindTexture(GL_TEXTURE_2D,font.fontTexture);glDrawArrays(GL_TRIANGLES,0,textCount);
  glBindTexture(GL_TEXTURE_2D,font.interfaceTexture);glDrawArrays(GL_TRIANGLES,textCount,GLsizei(vertices.size())-textCount);
}
inline void Shutdown(){fo3lockoverlay::Shutdown();preparation.Reset();for(auto* m:{&assets.lock,&assets.pin})fo3loadingvr::DeleteShapes(m->shapes);assets={};if(vao)glDeleteVertexArrays(1,&vao);if(vbo)glDeleteBuffers(1,&vbo);vao=vbo=0;started=ready=failed=anchored=spritesLoaded=false;}
} // namespace fo3lockui
