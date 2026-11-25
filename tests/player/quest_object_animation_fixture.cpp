#include "rendering/mesh/fo3-static-nif.cpp"
#include "npc/fo3-actor-animation.h"
#include "player/fo3-player-seat.h"
#include "scripting/fo3-script-actions.h"
#include <fstream>
#include <iterator>

// The host receives loose NIFs extracted from the player's installation.
bool LoadFalloutMeshFile(const std::string& path,std::vector<uint8_t>& bytes,std::string* resolved){
  std::ifstream input(path,std::ios::binary);
  if(!input)return false;
  bytes.assign(std::istreambuf_iterator<char>(input),{});
  if(resolved)*resolved=path;
  return !bytes.empty();
}
bool LoadQuestObjectClips(const std::string& path,std::vector<fo3script::AnimationSequence>& definitions){
  std::vector<uint8_t> bytes;
  fo3anim::Skeleton hierarchy;std::vector<int> blocks;std::vector<fo3anim::Clip> clips;
  if(!LoadFalloutMeshFile(path,bytes,nullptr)||!fo3anim::DecodeObjectAnimation(bytes,hierarchy,blocks,clips,true))return false;
  for(const auto& clip:clips)
    definitions.push_back({clip.name,(clip.stop-clip.start)/clip.frequency,clip.cycle==0||clip.cycle==1});
  return !definitions.empty();
}

bool SitInQuestFurniture(const fo3pipdata::Definitions& data,uint32_t reference,const std::string& root,uint8_t& marker,std::array<float,3>& eyePosition,std::array<float,3>& actorOrigin){
  const auto& placement=data.targets.at(reference);
  const auto load=[&](std::string path,std::vector<uint8_t>& bytes){
    for(char& c:path)c=c=='\\'?'/' :char(std::tolower(static_cast<unsigned char>(c)));
    return LoadFalloutMeshFile(root+"/"+path,bytes,nullptr);
  };
  fo3seat::State player;player.scene=fo3furniture::BuildScene(data,placement.cell,placement.world,load,true);
  for(size_t i=0;i<player.scene->slots.size();++i){
    const auto& seat=player.scene->slots[i];
    if(seat.reference!=reference||!fo3seat::Usable(seat))continue;
    std::vector<uint8_t> bytes;fo3anim::Skeleton skeleton;
    if(!load("characters/_male/skeleton.nif",bytes)||!fo3anim::DecodeSkeleton(bytes,skeleton))return false;
    fo3seat::Pose sit,stand;
    const int head=fo3anim::FindBone(skeleton,"Bip01 Head");
    if(head<0)return false;
    const auto eye=fo3anim::Point(skeleton.bindGlobal[size_t(head)],{0,0,0});
    if(!fo3seat::Targets(seat,skeleton,eye,sit,stand)||!player.Begin(reference,i,sit,stand,0)||
       player.Step(1)!=fo3seat::State::Commit::Sit||!player.seated)return false;
    eyePosition=sit.eye;actorOrigin=sit.actorOrigin;marker=seat.marker.reference;return true;
  }
  return false;
}
