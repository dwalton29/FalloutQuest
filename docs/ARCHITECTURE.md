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
- rendering/mesh/fo3-static-nif.cpp: static NIF render-mesh/material loading.
- fo3-collision-runtime.cpp: authored collision world and collision entry points.
- player/fo3-player-controller.inc: consolidated exterior player controller.
- world/fo3-worldspace-runtime.cpp: immutable worldspace metadata/index and request-local placement selection.
- world/fo3-world-streaming.cpp: portable CELL residency and motion-lookahead planner.
- world/fo3-cell-streaming.inc and rendering/terrain/fo3-terrain-*.inc: render-thread CELL publication and terrain implementation.
- Existing BSA, ESM, NIF and texture helpers remain normal source files.

## Shared assets: 0.24.9

`data/fo3-bsa-archive.cpp` is the single BSA v104 index/extraction implementation.
It has no Android/GL dependency. Index publication uses `std::call_once`; reads
use independent file handles, and failed reads return empty output buffers.

`data/fo3-asset-store.cpp` owns the archive registry and the existing data root
and texture archive fallback order. Mesh loading, DDS/cubemap loading and raw
HUD font/atlas loading use the same reader and shared indexes. DDS decoding
remains in `fo3-texture-bsa.cpp`; mesh/NIF and HUD format interpretation remain
with their existing consumers. No loose-file or plugin precedence is added.

Archives (including unavailable archives) are cached for the process lifetime,
as with the previous loaders. Restart after changing the supplied archive set.
Mesh prefix probes now limit the sorted result rather than an arbitrary hash
table iteration; full lists and rendering asset lookup are unchanged.

Host tests in `tests/assets` validate compression, aliases, bounds and parallel
reads without game assets. During this extraction, the adapters were compared
with the previous loaders using the supplied 224 Megaton NIFs and 78 DDS files
in temporary test archives, and the original supplied Misc BSA. The temporary
archives and copyrighted data are not repository contents.

## World streaming: 0.24.10

`world/fo3-worldspace-runtime.cpp` owns the immutable CELL/REFR/base and
Wasteland door-teleport index. The previous textual worldspace implementation
is removed. LAND decoding consumes explicit CPU helper declarations from
`fo3-worldspace-data.h`, without access to the placement index internals.
Initial arrivals keep the existing neighborhood API; background CELL and
legacy window workers pass `Fo3WorldspaceSelection` directly. Radius selection
is request-local, replacing the thread-local override and its selection mutex.
Index construction/publication remains locked; immutable reads remain safe
across workers.

`world/fo3-world-streaming.cpp` owns the residency planner's motion history and
returns a fixed-capacity plan without per-frame allocation. It preserves the
5x5 normal resident set, the complete 7x7 warm buffer and one hidden seven-CELL
lookahead strip per motion axis. Active collision/drawing gates, terrain runway,
LOD handoff and GPU upload/deletion budgets are unchanged.

`Fo3WorldStreamingState` groups live CELLs, worker slots, context generations,
collision task, terrain/collision centres, deferred GL deletion and planner
state. Render-thread publication remains in `fo3-cell-streaming.inc` while it
still shares renderer-owned GPU types/caches. This is an incremental extraction,
not yet a standalone GPU/world runtime. Detached collision workers now capture
their submission origin instead of reading mutable scene coordinates.

Portable tests in `tests/world` cover residency, directional prefetch, negative
grids, transition resets and independent planner state. Development comparisons
matched 20,000 planner frames against the previous implementation, plus 26
concurrent placement requests in the supplied Wasteland/Megaton ESM and all 240
indexed door teleports. No game data is committed.

## Background scene preparation: 0.24.11

Direct Megaton startup and authored door transitions share one preparation job.
The worker resolves the startup XTEL (when needed), loads request-local placement
metadata and builds CPU mesh objects. It uses the existing thread-safe model
and texture caches. Texture predecoding stops once the shared prepared-image
cache reaches 64 MiB; one image may cross that threshold. Remaining textures
use the existing upload-time decoder without changing rendered content.
Startup CPU preparation overlaps render-thread shader compilation.

CPU placement loading no longer invokes terrain teardown, mutates the transition
queue or changes collision policy. The render thread applies those effects.
Rolling exterior work is paused during preparation/publication, and old CELL,
collision and terrain workers are drained before changing collision policy.
The preparation job supports cancellation and is joined before renderer teardown.

Uploads advance once per stereo frame, targeting a 6 ms slice after each whole
shape. One shape/texture upload can exceed that budget; progressive byte/row
uploads remain a later improvement. Collision construction, terrain/environment
setup and scene commitment still run on the render thread, and are timed.
CPU upload buffers are released after each shape; the old scene remains until
the destination finishes. The original renderable-placement collision inputs
(including per-shape entries), authored origin and readiness rules are retained.
The Wasteland 49-CELL/LOD warmup gate and loading presentation dwell are unchanged.

Diagnostics: `SCENE STARTUP SHADERS`, `SCENE CPU READY` and `SCENE LOAD READY`
report shader, gate-resolution, metadata, CPU, texture, upload, collision and
finalization costs. `LOADING PRESENTATION COMPLETE` reports elapsed time from
loading begin through the submitted finished frame, including exterior warmup.
These are measurement hooks; no on-headset wall-clock speedup is claimed yet.
Portable tests cover preparation publication, failure, cancellation and lifetime.
The extracted startup resolver also matched the previous authored gate/XTEL
result exactly against the supplied Fallout3.esm.

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

## Immersive loading presentation

`ui/loading/fo3-loading-catalog.h` parses original LSCR pictures and WEAP/MISC
MODL paths without Android/GL dependencies. A worker publishes the immutable
catalogue; another worker prepares each random display, including NIF geometry,
DDS textures and interleaved vertices. GL uploads occur one shape per frame.
`fo3-loading-pose.h` defines tracking-space placement and generation selection.
`fo3-loading-vr.h` draws into a reusable private colour/depth target and replaces
the entire final eye image after world post-processing. The shared anchor is
captured once per generation from the midpoint of the located eye poses. Each
eye uses its physical tracking-space view/projection; door arrival yaw and
locomotion do not move the loading panel. Prior art/model resources remain
visible during later preparation, then staged meshes replace the old display.

The panel is 2.5 metres away; the 0.72-metre exhibit is 2.1 metres away and
rotates at 0.20 radians/second. Original LSCR selection retains CELL/worldspace
priority with a per-session/per-generation random seed. Fallout 3 LSCR records
have no model association: exhibits are original weapons and ordinary props,
not a claim that Bethesda authored a Fallout 4 loading scene for Fallout 3.
The compass loads `Interface\Circular Loading\loading01.nif`, confirmed by
the supplied Misc.bsa loading_menu.xml. Geometry/textures use the existing NIF
and DDS decoders. It rotates as a rigid emblem; Gamebryo's Idle controllers are
not played. Missing/unsupported assets produce explicit logs and the opaque
background remains; no synthetic compass substitutes for the original asset.
The full UI mesh/texture archives are needed; those assets are not in the
supplied Megaton subset and cannot be visually verified from that subset.

Presented-frame dwell starts only after display uploads finish and a render
layer is submitted. The existing WORK/POST gate covers scene upload and final
commit; Wasteland release requires its complete detail, near-LOD and horizon
warmup. The old 25-second forced reveal is now a diagnostic warning while
coverage remains. Missing required world assets can therefore hold the loader
until resolved. Background prefetch during ordinary seamless outdoor movement
retains its existing behaviour; loading scenes cover explicit CELL transitions.
Headset stereo comfort, compass orientation and frame timing still require an
on-device pass with the full installed archives. Host tests cover anchor/eye
separation, randomized location priority, malformed records and release gates.
