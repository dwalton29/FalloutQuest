#include "rendering/mesh/fo3-static-nif.cpp"
#include "npc/fo3-actor-animation.h"
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
