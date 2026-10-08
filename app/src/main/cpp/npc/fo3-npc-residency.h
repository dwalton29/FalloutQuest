#pragma once
#include <cstdint>
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
} // namespace fo3npc
