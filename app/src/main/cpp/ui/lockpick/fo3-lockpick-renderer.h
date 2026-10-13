#pragma once
#include "ui/loading/fo3-loading-vr.h"
#include "ui/interaction/fo3-interaction-hud-renderer.h"
#include "player/fo3-lockpick.h"
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
inline float breakElapsed=-1,introElapsed=0;
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
      fo3anim::Sample(m.hierarchy,*clip,clip->stop,m.pose);
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
inline void Render(const float* vp,const fo3lockpick::Game& game,const fo3player::Player& player){
  if(!game.reference||!anchored||!vp)return;
  fo3hudrenderer::GlStateGuard guard;glActiveTexture(GL_TEXTURE0);glDisable(GL_CULL_FACE);
  fo3anim::Matrix view;std::copy_n(vp,16,view.begin());const auto panel=fo3anim::Multiply(view,anchor);
  // Authored NIF XZ plane, Y depth; shared mapping keeps both model proportions.
  auto map=fo3anim::Identity();map[0]=.025f;map[5]=map[10]=0;map[9]=.025f;map[6]=-.025f;
  const auto centre=fo3anim::Point(assets.lock.pose.global.empty()?fo3anim::Identity():assets.lock.pose.global[4],{0,0,0});
  map[12]=-.025f*centre[0];map[13]=-.025f*centre[2];map[14]=.025f*centre[1];
  const auto base=fo3anim::Multiply(panel,map);
  if(ready&&fo3loadingvr::EnsureProgram()){
    glUseProgram(fo3loadingvr::gProgram);glUniform1i(fo3loadingvr::gSampler,0);glUniform1f(fo3loadingvr::gFade,1);glUniform1f(fo3loadingvr::gClipUi,0);
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_TRUE);glClear(GL_DEPTH_BUFFER_BIT);
    for(auto* model:{&assets.lock,&assets.pin}) {
      auto pose=model->pose;
      auto enterClip=std::find_if(model->clips.begin(),model->clips.end(),[](const auto& c){return c.name=="Forward";});
      const float enterDuration=enterClip==model->clips.end()?0:enterClip->stop-enterClip->start;
      const char* sequence=introElapsed>=enterDuration&&(model==&assets.lock||game.durability<1)?"Backward":"Forward";
      auto motionClip=std::find_if(model->clips.begin(),model->clips.end(),[&](const auto& c){return c.name==sequence;});
      if(motionClip!=model->clips.end()){
        const float elapsed=sequence==std::string("Backward")?(motionClip->stop-motionClip->start)*(model==&assets.lock?game.cylinder/90.f:1-game.durability):std::min(introElapsed,motionClip->stop);
        fo3anim::Sample(model->hierarchy,*motionClip,elapsed,pose);
      }
      if(model==&assets.pin&&breakElapsed>=0){auto clip=std::find_if(model->clips.begin(),model->clips.end(),[](const auto& c){return c.name=="Left";});
        if(clip!=model->clips.end())fo3anim::Sample(model->hierarchy,*clip,breakElapsed,pose);}
      const char* node=model==&assets.pin?"bobbypin":"lock";
      const int bone=fo3anim::FindBone(model->hierarchy,node);
      if(bone>=0&&model==&assets.pin){const auto pivot=fo3anim::Point(pose.global[bone],{0,0,0});
        fo3anim::Transform turn;turn.translation=pivot;const float rad=(model==&assets.pin?game.pin:game.cylinder)*.01745329252f;
        turn.rotation={std::cos(rad*.5f),0,std::sin(rad*.5f),0};auto rotation=fo3anim::ToMatrix(turn);
        auto back=fo3anim::Identity();for(int k=0;k<3;++k)back[12+k]=-pivot[k];rotation=fo3anim::Multiply(rotation,back);
        for(size_t b=0;b<pose.delta.size();++b){bool rotate=model==&assets.pin;for(int parent=int(b);parent>=0&&!rotate;parent=model->hierarchy.bones[parent].parent)
          rotate=parent==bone||model->hierarchy.bones[parent].name=="screwdriver";
          if(rotate)pose.delta[b]=fo3anim::Multiply(rotation,pose.delta[b]);}
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
  glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),nullptr);
  glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),reinterpret_cast<void*>(3*sizeof(float)));
  glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(font.program);glUniformMatrix4fv(font.mvpLoc,1,GL_FALSE,panel.data());glUniform1i(font.texLoc,0);
  glUniform4f(font.tintLoc,26.f/255,1,128.f/255,1);glBindTexture(GL_TEXTURE_2D,font.fontTexture);glDrawArrays(GL_TRIANGLES,0,textCount);
  glBindTexture(GL_TEXTURE_2D,font.interfaceTexture);glDrawArrays(GL_TRIANGLES,textCount,GLsizei(vertices.size())-textCount);
}
inline void Shutdown(){preparation.Reset();for(auto* m:{&assets.lock,&assets.pin})fo3loadingvr::DeleteShapes(m->shapes);assets={};if(vao)glDeleteVertexArrays(1,&vao);if(vbo)glDeleteBuffers(1,&vbo);vao=vbo=0;started=ready=failed=anchored=spritesLoaded=false;}
} // namespace fo3lockui
