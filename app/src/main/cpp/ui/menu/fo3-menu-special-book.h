#pragma once

#include "data/fo3-bsa-reader.h"
#include "fo3-menu-renderer.h"
#include "rendering/mesh/fo3-static-nif.h"
#include "ui/chargen/fo3-special-book.h"
#include "ui/lockpick/fo3-lockpick-overlay.h"

namespace fo3menuui {
struct SpecialBookFrame {
  static constexpr const char* ModelPath="Terminals\\Babybook02.NIF";
  static constexpr const char* Textures="textures\\terminals\\";
  fo3anim::Skeleton hierarchy;
  std::vector<fo3anim::Clip> clips;
  std::vector<int> blocks;
  std::vector<Fo3StaticNifMesh> meshes;
  fo3special::BookTiming timing;
  std::array<const fo3anim::Clip*,fo3special::ForwardClips.size()> forward{};
  std::array<const fo3anim::Clip*,fo3special::ReverseClips.size()> reverse{};
  Surface surface;
  fo3anim::Pose pose;
  std::array<float,3> centre{};
  bool attempted=false,ready=false;
  uint64_t revision=UINT64_MAX;
  float lastTime=-1;
  int lastRemaining=-1;
  std::array<uint8_t,7> lastValues{};

  static GLuint Image(const std::string& path) {
    auto id=Shared().Image(path,{}).id;
    const auto separator=path.find_last_of("\\/");
    if(!id&&separator!=std::string::npos)
      id=Shared().Image(Textures+path.substr(separator+1),{}).id;
    return id;
  }
  static std::string NumberTexture(int value) {
    return std::string(Textures)+"BBNumber"+std::to_string(value)+".dds";
  }
  static int Attribute(const std::string& name) {
    if(name.size()>2&&name[0]=='P'&&name[1]>='1'&&name[1]<='7'&&name[2]=='_')
      return name[1]-'1';
    if(name.rfind("Index_",0)!=0)return -1;
    constexpr const char* labels[]={"Strength","Perception","Endurance","Charisma","Intelligence","Agility","Luck"};
    for(int i=0;i<7;++i)if(name.compare(6,std::char_traits<char>::length(labels[i]),labels[i])==0)return i;
    return -1;
  }
  static std::string Material(const Fo3StaticNifMesh& mesh,const fo3special::Model& model,
                              const std::array<uint8_t,7>& values,int remaining) {
    const auto& name=mesh.shapeName;
    const int attribute=Attribute(name);
    if(attribute>=0&&name.find("PointVal")!=std::string::npos)return NumberTexture(values[attribute]);
    if(name.find("PointsRemainDigit")!=std::string::npos){
      if(remaining==0)return {};
      const bool tens=name.find("Digit2")!=std::string::npos;
      if(tens&&remaining<10)return {};
      return NumberTexture(tens?remaining/10:remaining%10);
    }
    if(name.find("_Message")!=std::string::npos)
      return std::string(Textures)+"english\\BBMessage0"+(remaining==0?"4":remaining==1?"3":"2")+".dds";
    if(name=="AllDone_Btn:0")
      return std::string(Textures)+(remaining==0?"BBXOn.dds":"BBXOff.dds");
    if(attribute>=0&&model.page==fo3special::IndexPage&&size_t(attribute)==model.cursor.selected&&
       name.rfind("Index_",0)==0){
      if(name.find("Increase_Btn")!=std::string::npos&&values[attribute]<10&&remaining>0)
        return std::string(Textures)+"BBIncreaseOn.dds";
      if(name.find("Decrease_Btn")!=std::string::npos&&values[attribute]>1)
        return std::string(Textures)+"BBDecreaseOn.dds";
    }
    return mesh.diffuseTexturePath;
  }

  bool Ensure() {
    if(attempted)return ready;
    attempted=true;
    fo3hudrenderer::CachedStateGuard guard;
    std::vector<uint8_t> bytes;
    if(!Shared().Ensure()||!LoadFalloutMeshFile(ModelPath,bytes,nullptr)||
       !fo3anim::DecodeUiAnimation(bytes,hierarchy,blocks,clips)||
       !LoadFo3StaticNifMeshes(ModelPath,meshes,false,false,true))return false;
    const auto bind=[&](const auto& names,auto& sequences,auto& durations){
      for(size_t i=0;i<names.size();++i){
        const auto found=std::find_if(clips.begin(),clips.end(),[&](const auto& clip){return clip.name==names[i];});
        if(found==clips.end()||found->frequency<=0||found->stop<=found->start)return false;
        sequences[i]=&*found;
        durations[i]=(found->stop-found->start)/found->frequency;
      }
      return true;
    };
    if(!bind(fo3special::ForwardClips,forward,timing.forward)||
       !bind(fo3special::ReverseClips,reverse,timing.reverse))return false;
    const auto cover=std::find_if(meshes.begin(),meshes.end(),[](const auto& mesh){return mesh.shapeName=="cover:0";});
    if(cover==meshes.end()||cover->positions.empty())return false;
    std::array<float,3> lo{INFINITY,INFINITY,INFINITY},hi{-INFINITY,-INFINITY,-INFINITY};
    for(size_t i=0;i<cover->positions.size();++i){
      const size_t axis=i%3;
      lo[axis]=std::min(lo[axis],cover->positions[i]);
      hi[axis]=std::max(hi[axis],cover->positions[i]);
    }
    for(size_t axis=0;axis<3;++axis)centre[axis]=(lo[axis]+hi[axis])*.5f;
    for(unsigned i=1;i<=5;++i)
      if(!Image(std::string(Textures)+"english\\BabyBook0"+std::to_string(i)+".dds"))return false;
    for(int i=0;i<=10;++i)if(!Image(NumberTexture(i)))return false;
    return ready=true;
  }

  void Build(const fo3special::Model& model,const fo3player::Player& player) {
    const auto& values=player.Snapshot().specialDraft;
    const int remaining=player.SpecialBookRemaining();
    if(revision==model.revision&&lastTime==model.turnTime&&lastRemaining==remaining&&lastValues==values)return;
    revision=model.revision;lastTime=model.turnTime;lastRemaining=remaining;lastValues=values;
    const auto* clip=model.turnDuration>0?(model.forward?forward[model.page-1]:reverse[model.page]):nullptr;
    if(clip){
      fo3anim::BindClip(hierarchy,*clip,pose);
      if(!fo3anim::Sample(hierarchy,*clip,model.turnTime,pose,nullptr,
         fo3anim::RootPolicy::Furniture,nullptr,fo3anim::Playback::Clamp))return;
    }
    struct MaterialDraw {
      const Fo3StaticNifMesh* mesh;
      fo3anim::Matrix transform;
      GLuint texture;
      float depth;
    };
    std::vector<MaterialDraw> draws;
    for(const auto& mesh:meshes){
      const auto path=Material(mesh,model,values,remaining);
      if(path.empty()||mesh.positions.empty()||mesh.texcoords.size()!=mesh.positions.size()/3*2)continue;
      const auto texture=Image(path);
      if(!texture)continue;
      const auto bone=mesh.shapeBlock<blocks.size()?blocks[mesh.shapeBlock]:-1;
      const auto transform=clip&&bone>=0&&size_t(bone)<pose.delta.size()?pose.delta[bone]:fo3anim::Identity();
      const auto p=fo3anim::Point(transform,{mesh.positions[0],mesh.positions[1],mesh.positions[2]});
      draws.push_back({&mesh,transform,texture,p[1]});
    }
    std::stable_sort(draws.begin(),draws.end(),[](const auto& a,const auto& b){
      if(a.mesh->alphaBlend!=b.mesh->alphaBlend)return !a.mesh->alphaBlend;
      return a.mesh->alphaBlend?a.depth>b.depth:a.texture<b.texture;
    });
    surface.vertices.clear();surface.batches.clear();
    // VR placement in metres. NIF X/Z is the page, with the reader facing -Y.
    constexpr float scale=.0045f;
    for(const auto& draw:draws){
      const auto& mesh=*draw.mesh;
      const bool depthWrite=!mesh.alphaBlend;
      if(surface.batches.empty()||surface.batches.back().texture!=draw.texture||
         surface.batches.back().depthWrite!=depthWrite||surface.batches.back().alpha!=mesh.alpha){
        Batch batch;
        batch.texture=draw.texture;batch.first=GLsizei(surface.vertices.size());
        std::fill_n(batch.rgb,3,1.f);
        batch.alpha=mesh.alpha;batch.depthWrite=depthWrite;
        surface.batches.push_back(batch);
      }
      for(const auto index:mesh.indices){
        const auto p=fo3anim::Point(draw.transform,{mesh.positions[index*3],mesh.positions[index*3+1],mesh.positions[index*3+2]});
        surface.vertices.push_back({(p[0]-centre[0])*scale,(p[2]-centre[2])*scale,
            (centre[1]-p[1])*scale,mesh.texcoords[index*2],mesh.texcoords[index*2+1]});
      }
      surface.batches.back().count+=GLsizei(mesh.indices.size());
    }
    if(!surface.vertices.empty())surface.Upload();
  }

  void Draw(const float* mvp,const fo3special::Model& model,const fo3player::Player& player,
            GLsizei width,GLsizei height) {
    if(!ready)return;
    fo3hudrenderer::CachedStateGuard guard;
    Build(model,player);
    fo3lockoverlay::DepthScope depth(width,height);
    if(depth.valid)surface.Render(mvp,true);
  }

  void Shutdown() {surface.Shutdown();*this={};}
};
inline SpecialBookFrame& BookFrame() {
  static SpecialBookFrame book;
  return book;
}
}
