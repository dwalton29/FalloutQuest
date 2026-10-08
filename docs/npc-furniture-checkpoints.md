# Original furniture implementation checkpoints

Based on v181 `24fdb42f10fb5f7b74e507146c187a687979d355`.

## Checkpoint 1

FURN MODL/MNAM is decoded in the canonical ESM catalog. Furniture REFRs
retain DATA XYZ/rotation, XSCL, XOWN and enable-parent metadata instead of
being pruned as non-quest targets. BSFurnitureMarker FRN decoding uses the
existing bounds-checked NIF parser: name index, count, 16-byte XYZ/ushort
orientation/two position-reference bytes. Malformed and duplicate blocks
are rejected. Original BedTwin01 and Chair01 verified, including all marker
positions, references 1/2 and 11/12/14, and 1570/3141/4712 orientations.
MNAM low bits enable slots by index; high bits distinguish sleep and sit.
Marker rotation uses clockwise milliradians, REFR uses the existing Rz*Ry*Rx
radian convention. No model-name-based correction or placement offset.

Reservations use reference/slot keys and per-actor leases. Destruction and
explicit release clear only the owning actor's reservation. No occupation is
persisted: restored actors must reacquire and reapproach.

Animation Sample has an explicit Furniture policy. It samples the original
accumulation-root translation, rotation and scale. A supplied retained root
holds the entry result for loops with absent accumulation tracks. Default
Locomotion sampling remains unchanged. Original left/right bed entry ends
and exit starts are continuous within two game units; exit returns to near
zero. The authored residual is retained rather than replaced with an offset.

Host tests verify original assets, malformed data, transforms, mask filtering,
conflicting reservations, independent slots and lease cleanup. Package
activation and renderer integration are the next checkpoint; no standing
substitute for Sleep or Eat is enabled by this commit.

## Checkpoint 2 — Sleep

Sleep PACK type 4 is executable when the normal ordered condition/schedule
selector resolves a resident, available supported bed. PLDT references, local
CELL searches and near-editor locations use original placement data. Ownership
permits the original NPC base or one of its original factions. MNAM selects
slots by index; IDLE GetFurnitureMarkerID (function 160) selects bed left/right
entry and exit via references 1/2. The loop resolves original BedDynamicIdle.
No actor-ID routine or model-name offset is involved.

The runtime reserves, builds a NAVM route, checks the projected endpoint is
within eight game units of the marker, aligns with the original transform,
enters, retains accumulation for the sleep loop, exits and bakes the original
exit residual exactly once. Each new cycle reanchors to FRN, preventing drift.
The eight-unit arrival tolerance and 60-second approach timeout are Quest
navigation/recovery policies, not animation offsets. Route failure and
contention yield to the v181 bounded retry/selection path.

Furniture skin sampling bypasses locomotion root stripping and pose blending.
Normal locomotion, dialogue face deformation and other clip sampling remain
unchanged. Schedule/condition selection continues every 0.25 seconds; changes
exit before adopting another procedure. Dialogue requests an exit and may
speak during it; combat movement/fire waits until the authored exit completes.
Entry interruptions finish the short entry before playing its matching exit.
Death and object destruction immediately release leases. Furniture occupancy
is ephemeral; saved actor roots remain at the marker so restored actors can
reacquire and restart the authored cycle. No save schema change.

A correction to checkpoint 1: raw ESM rotations require Bethesda's inverse
Rz*Ry*Rx convention, as in Q230ConvertBethesdaRotation. Alignment and its test
now use that convention. Only level, unscaled resident furniture is admitted
by this first renderer bridge; tilted/scaled furniture and enable-parent
references are explicitly excluded rather than misaligned. RACE DATA bit 4
at offset 32 identifies children; child IDLE programs, floor-bed marker
references 3/4 and nonresident/cross-cell travel remain unsupported.

Original Moira 00004156/bed 00003D75 and Billy Creel 00003FF9/bed 00003CBF
resolve through their actual schedules and perform three entry/sleep/exit
cycles with stable marker roots. Production package executor host tests cover
dialogue and combat exit, death release and changing to ordinary Travel.
These are original-data host simulations, not headset validation.
