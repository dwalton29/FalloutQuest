#pragma once

#include "data/fo3-xtel-index.h"
#include "pipboy/fo3-pipboy-data.h"
#include "player/fo3-player-state.h"
#include "fo3-package-schedule.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// Bounded, deterministic off-scene package/CELL progression. No live meshes,
// NAVM, rendering, player teleport, audio, combat or furniture operations.
namespace fo3unloaded {
struct Callbacks {
  // Return authored priority-ordered effective PACKs for this actor.
  std::function<std::vector<uint32_t>(uint32_t base)> packages;
  // Canonical CTDA against the player/quest state. Unsupported gates fail shut.
  std::function<bool(uint32_t actor,uint32_t base,const fo3player::ActorState&,
                     const fo3pipdata::PackageDefinition&,float hour)> eligible;
  std::function<bool(uint32_t actor,uint32_t door)> canUseDoor;
  std::function<bool(uint32_t actor)> alive;
  // Commit atomically through Player::UpdateActor, not Snapshot mutation.
  std::function<bool(uint32_t actor,const fo3player::ActorState&)> persist;
};
struct Report {
  size_t visited=0,packageChanges=0,doorHops=0,blocked=0,unsupported=0;
  size_t residentSkipped=0,hostileSkipped=0;
};
bool ScheduleActive(const fo3pipdata::PackageSchedule&,float hour,fo3schedule::Calendar calendar={});
bool SupportedLocalProcedure(const fo3pipdata::PackageDefinition&);
class Scheduler {
public:
  void Reset();
  // Called from one game thread. A changed in-game minute begins an actor
  // batch; at most 64 tracked actors are evaluated per frame.
  Report Tick(float hour,uint32_t residentCell,uint32_t residentWorld,
              const std::unordered_map<uint32_t,fo3player::ActorState>& tracked,
              const fo3pipdata::Definitions& definitions,
              const fo3xtel::Index& graph,const Callbacks& callbacks,
              fo3schedule::Calendar calendar={});
private:
  int minute_=-1,previousMinute_=-1;
  // Freeze the game clock for each capped actor batch.
  float batchHour_=0.f;
  fo3schedule::Calendar batchCalendar_{};
  size_t cursor_=0;
  std::vector<uint32_t> pending_;
  // Stores cell of the first observed actor state at this game minute; the
  // public caller owns serialization of canonical ActorState.
  std::unordered_map<uint32_t,uint32_t> lastTransferredMinute_;
};
} // namespace fo3unloaded
