# Sleeping corpses and player seating — v302

## Settled ragdolls

The shared Bullet world already slept supported, quiet ragdolls after two seconds. The surrounding engine still copied every body's pose/velocities before checking sleep, recalculated corpse bounds, and rebuilt each skin palette and invoked its upload/check path each frame. GPU mode already skipped identical palette bytes; the CPU fallback still skinned and uploaded vertices.

Sleeping corpses now retain their existing pose, bounds and GPU palettes. Activation is checked before pose-vector allocation/copy. Sleep-support manifold scanning and adaptive ragdoll substeps only run for active ragdolls. Bodies/constraints remain in the world, so a physical collision can wake them; the next capture invalidates the renderer's palette cache. Appearance/transform dirtiness still forces an update. Looting and saved corpse poses use the same canonical player state.

The production recording-renderer test verifies 180 sleeping frames with no additional skin-update submissions, an explicit dirty update, and an awake update. The NPC ragdoll test verifies frozen pose identity and no save revision churn over 180 frames, followed by restored velocity/motion. Existing original human ragdoll physics and corpse save/return tests remain applicable. No headset FPS gain is claimed without a device profile.

## Chairs

Resident FURN markers and their original enter/loop/exit IDLE KFs are prepared with scene assets. Player interaction adds an A) Sit action to supported chairs, using the actual marker and player eye anchor transformed by the original sit pose. The standing target and facing come from the authored exit pose. No guessed chair centre/seat height or chair FormID list is used.

A 120 ms black fade out hides the camera relocation; a 120 ms fade in follows. This timing and the tracked-body leg solve are Quest VR policies. Sit/stand camera moves update the application transform used by hands, carried objects and SpaceWarp. Locomotion/gravity do not pull the seated camera through the chair. Moving the left stick, B outside another menu, or the Stand action gets up. B closes an open terminal/Pip-Boy first; the next press gets up. Terminal aiming/interaction remains live while seated. B used for standing cannot leak into debug button reload.

Player and NPC seating share reservations; occupied seats cannot be selected. Dynamically arriving NPCs join that same reservation pool. Explicit furniture ownership and enabled state are checked. Furniture scripts retain OnActivate priority rather than being bypassed. The original transient GetSitting states (2 entering, 3 seated, 4 leaving, 0 none), GetFurnitureMarkerID and script GetCurrentFurnitureRef read the shared player state. Script previews copy this state without committing effects.

Current support is the already decoded upright, unit-scale adult chair marker programs (11, 12, 14). Beds, floor-sit markers, multi-seat benches, scripted furniture default activation, independently scheduled exterior chair streaming and a serialized seated state are separate work; this change does not claim those systems. Scene changes clear the temporary seat lease. The local original-data test reads the original office chair NIF, adult skeleton and Chair_LeftEnter / DynamicIdle_ChairSit / Chair_LeftExit KFs, verifies lower seated eye height and placement rotation, then exercises reservation and blackout commit rules. The actual interaction fixture verifies FURN targeting, Sit/Stand prompts, A dispatch and blocked/occupied seats.

## Pip-Boy STATS investigation

The supplied original stats_menu.xml resolves its stats_title to STATS using font 4. The original Monofonto_VeryLarge02_Dialogs2 FNT and full atlas name parse correctly; the corresponding original 512×512 TEX atlas produces all five letters correctly in a local asset check. A current headset screenshot and, if needed, the asset-loading log are required to reproduce the remaining reported display fault. No speculative font substitution or layout change is presented as a fix.

## Headset checks

- Let several Super-Duper Mart corpses settle. Compare CPU/animated-actor counts before/after settling; drop a physical object on one and verify wake-up, then loot/save/re-enter the area.
- Aim at an unoccupied office chair: A) Sit, short fade, correct facing/height. Access the nearby terminal normally. B closes it; B again or left-stick movement stands with a fade. Test occupied/owned chairs and scene travel.
- Confirm manual/physical sneaking, Megaton child proportions and both house stair orientations remain correct. Capture the STATS title fault for the remaining UI diagnosis.
