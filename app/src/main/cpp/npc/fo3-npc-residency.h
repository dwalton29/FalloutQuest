#pragma once
#include <cstdint>
#include <cmath>
#include <array>
#include <unordered_map>

// An original Megaton WRLD is a fully rendered small worldspace. Its exterior
// grid CELL records (unlike interior CELLs) belong to the active scene even
// when the loading cell is the WRLD persistent CELL. Do not generalize this
// to large streaming Wasteland worldspaces.
namespace fo3npc {
inline bool SceneOwnsCell(uint32_t sceneCell,uint32_t sceneWorld,
                          uint32_t actorCell,uint32_t actorWorld,
                          const std::unordered_map<uint32_t,uint32_t>& worlds){
  if(!actorCell||actorWorld!=sceneWorld)return false;
  if(sceneCell==actorCell)return true;
  if(sceneWorld!=0x00000A74u)return false;
  const auto entry=worlds.find(actorCell);
  return entry!=worlds.end()&&entry->second==sceneWorld;
}
// Match the three-by-three physical collision window; never instantiate a
// whole large WRLD. Small worlds remain fully resident, interiors exact CELL.
inline bool Resident(uint32_t sceneCell,uint32_t sceneWorld,bool smallWorld,
                     float centreX,float centreY,uint32_t actorCell,uint32_t actorWorld,
                     const std::array<float,3>& position) {
  if(actorWorld!=sceneWorld)return false;
  if(!sceneWorld)return actorCell==sceneCell;
  if(smallWorld)return true;
  return std::isfinite(position[0])&&std::isfinite(position[1])&&
    std::abs(std::floor(position[0]/4096.f)-std::floor(centreX/4096.f))<=1&&
    std::abs(std::floor(position[1]/4096.f)-std::floor(centreY/4096.f))<=1;
}
} // namespace fo3npc
