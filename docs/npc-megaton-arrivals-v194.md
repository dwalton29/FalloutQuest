# v194 — Megaton NPC XTEL doors and live resident arrivals

Baseline: v193 main `7b31e9487e8a5ba4905f21a18d4523d79d948ff9`.
This milestone leaves rendering configuration, draw distance, player weapons,
Pip-Boy, body, combat, actor animation and existing furniture systems intact.

## Root causes addressed

1. The original ESM-derived XTEL graph knew the destination, but the
   `pipboy.targets` finalizer discarded most non-quest DOOR placements.
   Both the resident XTEL chooser and the unloaded scheduler separately
   required those placements, so doors could be unreachable despite valid
   links. Preserve every existing DOOR placement during finalization without
   changing the canonical XTEL graph or door lock/script/key checks.
2. Megaton is a fully rendered small WRLD with a persistent scene CELL plus
   exterior grid CELLS. The scheduler previously treated only one exact CELL
   as resident. New `fo3npc::SceneOwnsCell` preserves exact CELL matching
   for interiors, and treats only known original Megaton WRLD grid CELLS as
   resident, not the rest of the Wasteland.
3. Offscreen XTEL hops only updated saved ActorState. When a new resident
   arrived while the user was still in Megaton, there was no live actor mesh.
   Successful offscene-to-resident transfers now enqueue the original actor
   reference. A dedicated bounded worker reuses `Q230PrepareActors`,
   `Q230CacheDialogueAnimations` and `Q230CacheCombat` with an immutable
   snapshot of the requested actors; their GPU parts are uploaded at **one
   part per frame** on the GL thread and inserted into the active scene once.
   No scene reload or player teleport is used.

## Identity and safety

* Preserve original NPC_, ACHR, outfit, AI and XTEL source data.
* The live-arrival queue rechecks canonical cell/world/death and
  `gQ230NpcActors` before each part upload and publication.
* Actor CPU jobs are canceled and joined before any scene teardown/player
  session replacement, and partially uploaded GPU resources are freed.
* A successfully transferred NPC reuses the existing actor AI/state restore
  and ground navigation on its next frame.
* No remote NAVM step, offscreen mesh or fabricated actor schedule is added.
  Original schedules still gate each offscreen CELL hop by in-game minute.
* Background asset preparation is potentially expensive; large arrivals can
  become visible after multiple frames rather than instantaneously.

## Original records and validation

The original `Fallout3.esm` confirms:
- Common House interior DOOR `000043AD` -> exterior `0000435C`;
- both use `MetalScrapDoor01`, with no authored lock on either REFR;
- `MegSettler1PatrolMid10x4` (`0001944C`) is **type 12 Sandbox**
  (despite "Patrol" in its EDID), 10:00–14:00, anchored at `0001FB87`;
- `MegSettler6Brahmin8x12` (`0002594A`) is type 12 Sandbox,
  08:00–20:00, anchored at `00066F1E`.
Both anchors belong to Megaton's exterior WRLD, which means the existing
offscene location-procedure scheduler should be eligible at midday.

Added/extended host checks: DOOR retention and irrelevant REFR pruning,
original-master Common House doors, anchors and package windows; Megaton
scene-cell ownership; offscreen-to-resident callback exclusion.

## Headset acceptance

At midday, stand outside the Common House and check these exact log tags:
`NPC UNLOADED TICK`, `NPC LIVE ARRIVAL QUEUED`,
`NPC LIVE ARRIVAL PREPARE`, `NPC LIVE ARRIVAL READY`,
`NPC AI START`, `NPC ROUTE`, `NPC XTEL APPROACH` and
`NPC XTEL TRANSFER`. Residents should appear once at original XTEL arrival
points and navigate without reloading Megaton. Use a clock transition into
evening then verify the actor's return through original entry doors.
Build/host success is not a headset acceptance result.

Remaining: exact unloaded travel time and original NPC behaviours beyond
the supported location-directed package subset; not claimed complete.
