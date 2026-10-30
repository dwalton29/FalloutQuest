# Furniture animation heading — v303

## Cause and source evidence

The original entry animations approach a shared idle from different directions. FalloutQuest retained the Bip01 translation when switching to a dynamic idle, but discarded the change of rotational frame between the entry endpoint and that idle's first Bip01 NonAccum orientation. Rotating the FRN marker itself would also rotate its translation and move the actor off the furniture.

The supplied original adult KFs show these planar differences (radians, rounded):

| Original entry | Shared idle | Continuation heading |
| --- | --- | ---: |
| Chair_LeftEnter | DynamicIdle_ChairSit | +1.57007 |
| Chair_RightEnter | DynamicIdle_ChairSit | -1.57154 |
| Chair_ForwardEnter | DynamicIdle_ChairSit | -3.14159 |
| BedLeft_Enter | DynamicIdle_Sleep | +1.57079 |
| BedRight_Enter | DynamicIdle_Sleep | -1.57079 |
| SitFloorEnter | SitFloorDynamicIdle | -3.14009 |

These are observations, not runtime constants. The resident furniture program derives its planar heading once from the original entry endpoint and loop-start rotations. The loop receives that rotation around the retained accumulation pivot, keeping the original accumulated position. Enter and exit sequences keep their original frames. Actor placement, marker orientation and skeleton coordinates are not globally changed. Missing rotation channels yield no invented correction. The decoded programs remain shared among resident furniture slots.

The production NPC pose-to-skin bridge uses this continuation root only for furniture loops. The player's chair camera uses the same corrected loop and heading. The original exit remains responsible for its standing target. No NPC, quest, furniture FormID or per-model angle exception is introduced.

## Validation

The host furniture orientation test covers a non-origin accumulation pivot, unchanged retained translation, missing-channel fallback and a different loop frame. With the supplied original assets it checks all six routes above: entry-end to idle-start and idle-start to exit-start orientation, head position, three rotated/translated placements, and agreement between alternate chair/bed entries. The recording production actor bridge checks that entry, loop and exit submit matching skinned vertices through Q230UpdateActor. The original player-chair test checks seated eye height, standing position and placement transforms.

Build v302 exposed two native compilation errors in chair scene sharing: assigning a const NPC scene to a mutable player scene pointer, and replacing a reservation pointer through a const scene. Player/preparation scene references now preserve constness; arriving actors join the shared reservation pool through their existing mutable residentReservations state. Scene publication/resource retirement guards remain intact.

Local verification: all 49 player tests completed (48 passed, the optional MQ01 data test skipped); the NIF suite completed with 18 passing and six optional asset tests skipped. The six-route original orientation check, original chair-camera check and original Micky floor-sit package/NAVM check passed separately. Scene collision/publication guards and the 14 loading-pipeline checks passed.

Android Debug/O2 compilation and physical headset verification are separate from host/original-data checks. Consult the matching Actions run for the final native build outcome. No headset orientation or performance result is claimed here.

## Headset checks

- Re-enter Moriarty's Saloon and Megaton houses. Watch NPCs enter, idle and leave chairs/beds; their orientation should stay consistent through both transitions.
- Visit the water beggar outside Megaton. His floor-sitting loop should retain the original entry orientation and position. Trigger dialogue/exit and let the package resume.
- Sit at an unoccupied terminal chair using A. Verify camera facing/height, terminal access, B to close the terminal and B again to stand. Repeat at differently rotated chairs.
- Check several settled Super-Duper Mart bodies, physical wake-up and looting. The earlier sleeping-corpse optimizations remain included.

The outstanding Pip-Boy STATS title fault still requires a current headset screenshot: original text/font/atlas checks do not reproduce it. This build does not claim that UI issue fixed.
