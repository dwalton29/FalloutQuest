# FalloutQuest architecture

## Q17 baseline

Q17 is the consolidation boundary between the exploratory Q-series prototype
and the maintained engine.

Before Q17, CMake progressively read older C++ files as text and applied more
than one hundred Q-numbered string replacements to generate the source that was
actually compiled. That was effective for rapid reverse-engineering, but later
changes became dependent on brittle textual anchors.

Q17 captures the exact mature generated runtime and makes it canonical source.
Future engine changes should modify normal C++ directly. Do not reintroduce
generated-source patching for engine behaviour.

## Canonical native translation units

- fo3-runtime.cpp: OpenXR lifecycle, renderer, scene/runtime streaming and frame loop.
- fo3-runtime-loop.inc: mature runtime/frame implementation included by fo3-runtime.cpp.
- fo3-cell-world.cpp: CELL/WRLD/LAND scene construction and terrain integration.
- fo3-static-nif-runtime.cpp: static NIF render-mesh/material loading.
- fo3-collision-runtime.cpp: authored collision world and collision entry points.
- fo3-player-controller-runtime.inc: consolidated exterior player controller.
- fo3-worldspace-runtime.inc and fo3-terrain-*-runtime.inc: consolidated world/terrain implementation.
- Existing BSA, ESM, NIF and texture helpers remain normal source files.

## Source-of-truth rule

Fallout 3's supplied data and observable PC runtime behaviour are authoritative
wherever practical. Do not invent replacement world geometry, materials,
lighting values, placements, or game-authored content when those can reasonably
be recovered from the user's game files.

Quest/OpenXR-specific behaviour may be implemented where Fallout 3 has no native
equivalent, including stereo presentation, VR input, comfort behaviour and
mobile performance adaptations. Keep those adaptations separate from authored
Fallout data.

## Renderer verification

For renderer discrepancies, prefer measured comparison with the PC reference path
over screenshot tuning. tools/pc-d3d9-logger captures Fallout 3 D3D9 shader,
state and constants and maps them to Shader Package 17.

## Development rule

main is the working branch unless explicitly requested otherwise. Changes should
leave the cloud APK build passing before additional systems are layered on top.


## Q18 Wasteland residency

Capital Wasteland CELL, REFR and base/model metadata are indexed once from the
user's Fallout3.esm. Rolling 5x5 neighbourhood requests use indexed CELL lookup
rather than rescanning the ESM. Exterior placements preserve their authored CELL
owner/XCLC grid; persistent-CELL refs use their authored DATA position for
spatial residency.

The current hierarchy is intentionally unchanged while it is measured:

- 3x3 active full-detail draw set.
- 3x3 authored collision set.
- 5x5 resident/prefetched visual placement set.
- 7x7 LAND runway.
- Bethesda Level4 terrain/object LOD outside the near world.

Streaming logs include per-phase microsecond totals for metadata, CPU NIF work,
GPU upload, collision, terrain and commit. Use these measurements before changing
budgets or restructuring collision further.
