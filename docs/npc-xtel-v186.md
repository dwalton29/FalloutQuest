# v186 — NPC XTEL load-door approach and canonical CELL handoff

Baseline: v185 `9256b05cc12d2a5e14ac0735981ef7e7fb4605f6`.
Only `main` is used; the player scene transition queue is never invoked by NPCs.

## Implemented, local resident scope

- The original-data XTEL graph (v185) is created once on the scene
  preparation worker and atomically published as an immutable object. Live
  actor updates never scan Fallout3.esm.
- Location-directed PACKs with a **placed reference (PLDT type 0)** can select
  a directed first XTEL hop when that reference is in another CELL or WRLD.
  Covered procedure types: Travel (6/14), Wander (5), Sandbox (12), Eat (3),
  Sleep (4), and the v184 seated Use Item At subset (8). Actor-target-driven
  Follow/Accompany/Escort and scripted package execution remain separate.
- Candidate doors must have original DOOR/XTEL records, valid placement
  transforms and owning CELL, no initial disable/delete or enable parent,
  valid actor lock/key/script permissions via `CanActorOpenDoor`,
  a directed route to the package destination, and a reachable local authored
  NAVM approach. The first hop is chosen by fewest CELL links, then distance.
- Actor approaches the source door on NAVM, moves within 96 game units
  after an authored route has begun, and transfers through `Player::UpdateActor`
  into the actual XTEL destination CELL/WRLD and XTEL arrival position/yaw.
  The original package identity is carried; transient route/sequence state
  is discarded so the destination scene rebuilds its NAVM route.
- The source actor is marked off-scene after an accepted canonical transfer.
  The NPC package loop, combat loop, persistence writer and renderer cannot
  overwrite the destination state or draw/continue simulating the old root.
- Scene preparation includes original ACHR records that have been saved into
  its new CELL; actors moved elsewhere are excluded from their original CELL.
  Original NPC appearance, inventory and animation are rebuilt from the ESM
  records; no duplicated per-NPC routines or fabricated teleport meshes.
- Route/door failure uses existing bounded retry and lower-priority package
  fallback. Missing destination owner, inaccessible key/script door, missing
  NAVM route or failed state transfer never silently succeeds.

## Boundaries

**This is resident actor handoff, not continuous unloaded-cell scheduling.**
An actor arriving in an unloaded CELL is preserved in canonical save state;
it does not walk onward or evaluate the next package until that scene is
prepared. Movement across exterior worldspace grid cells without an XTEL is
not implemented. Cross-cell following of dynamic actor targets, locked door
unlock animations, initially disabled/enable-parent doors, levelled actor
templates and general script opcodes remain unsupported.

No changes were made to player transition/loading, stereo renderer, terrain
distance, Pip-Boy, body IK, combat weapons or furniture KFs.

## Validation

`npc_xtel_runtime_tests` compiles the production package
executor against a two-CELL authored-format ESM fixture. It tests remote
Travel selection, available source-door approach, transferred canonical
CELL/worldspace/XTEL position, absence of further source-scene movement and
rejection of a locked door without the NPC key.

The existing world XTEL tests validate the complete source/destination
record map. All normal player/NPC/world regressions and Android APK build
remain required.

Physical Quest headset validation is still necessary: try a resident with a
remote original PLDT destination, follow its route to the door, then visit
the target interior and confirm there is exactly one actor at the authored
XTEL arrival and that schedule selection resumes.

Next milestone: **unloaded actor scheduler** with bounded multi-cell progress,
respecting original time/conditions and without rendering remote interiors.
