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
