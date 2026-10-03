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

The panel is 2.5 metres away; the 0.72-metre exhibit is 1.6 metres away, offset 0.58 metres left and 0.33 metres down and
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

### Loading hitch reduction (0.24.13)

The artwork quad reverses V to display DDS top-row artwork upright; exhibit
UVs retain the NIF convention. Fade and rotation clocks are separate: the
rotation phase stays continuous when newly uploaded assets finish their fade.
Both eyes use the same captured animation times.

While a loading presentation is visible, RenderScene advances the scene job
then returns before hidden world, terrain, actor, shadow and reflection draws.
The host also suppresses hidden sky draws. Initial Megaton/Wasteland collision
uses the existing placement-cache/snapshot path in a joined preparation worker.
The current collision remains available until the render thread publishes the
snapshot. Wasteland retains the exact authored arrival 3x3 filter; Megaton keeps
its original full placement set. Cancellation joins before restoring collision
policy and discards an unpublished token. A clean frame separates collision
publication from final scene/environment/terrain commit. Other worlds/interiors
retain their previous collision path.

This reduces known end-of-load work on the rendering thread, but is not a
measured guarantee of hitch-free loading: initial terrain generation/uploads,
interior collision and final environment setup still contain synchronous work.
Use SCENE COLLISION READY prepareUs/swapUs and SCENE LOAD READY finalizationUs
logs from the headset to identify any remaining pause.

Development parity against the supplied 224 Megaton NIFs compared the legacy
initializer with fresh-cache prewarm/snapshot publication: 172 collidable
placements and 32,889 triangles matched exactly, including transformed
vertices/normals, surface/layer/material identities and Havok welding keys.

### Original menu definition (0.24.14)

`fo3-loading-menu.h` reads the overlay filename, compass filename/animation name
and fade duration from original loading_menu.xml, plus MainMenu RGB from the
Interface section of Fallout.ini. The supplied values remain the defaults when
an installed INI/XML is unavailable. Catalogue publication also publishes this
definition; readers never access a config while its worker mutates it.

The artwork and compass inherit MainMenu colour (199,255,165 in the supplied
INI). LoadingAnim01.NIF explicitly opts out in XML and uses its decoded texture
and vertex colour. UI geometry is fit to the VR panel, preserving relative shape
placement and depth, then composited before the independent rotating exhibit.
Each shape preserves decoded NiAlphaProperty blend factors, alpha testing and
material alpha. Unresolved UI texture shapes are skipped with a diagnostic,
rather than drawn with the neutral fallback used for 3D exhibits. Transparent
UI geometry does not write panel depth. Blend equations are set to FUNC_ADD and restored with the rest of the caller's GL state.

Uploads retain one-shape-per-frame staging. Overlay/compass resources persist
across transitions and are released on shutdown. Presented-frame counting still
requires uploads to finish, and the minimum dwell cannot advance to WORK while
the authored fade is incomplete. Both eyes share one animation/fade clock.

This is the original-asset rendering path, not completed Gamebryo UI animation
support. The static NIF decoder supplies the bind/static geometry; LoadingAnim01
controllers and the compass Idle sequence are not evaluated. Compass rigid
rotation is the existing VR adaptation. Projector movement, animated texture
transforms and exact layering still need controller playback and texture
inspection. No artificial flicker or scanlines have been substituted. Tests validate the supplied XML/INI definition, section
selection, invalid values, incomplete XML and fade/completion gating.

### Loading UI textures (0.24.15)

The supplied original LoadingAnim01 and loading01 NIFs use TileShaderProperty,
whose BSShaderLightingProperty prefix is followed by a SizedString filename.
The static decoder now reads that material as unlit and rejects malformed tile
properties. Previously the unsupported property left the filename empty and
the UI renderer uploaded a white fallback, obscuring the art. UI shapes now
require a resolved DDS before upload, including when a material has no filename.
The originals decode into eight overlay shapes and two compass shapes, with
all ten texture references and blend factors recovered. Tests cover truncated
material blocks and optionally load both originals without redistributing them.


## Original NPC idle runtime (0.25.0)

`npc/fo3-npc.cpp` resolves explicit CELL ACHR references and appearance data.
Deleted/initially-disabled references are excluded. NPC templates inherit only
consumed appearance categories (traits, model/animation, base name, inventory);
stat-only templates do not force appearance resolution through their unrelated
levelled-template chain. Cycles, unresolved appearance templates and unsupported
levelled actors are logged/skipped. RACE tables select the actor's gender,
ARMO selects MODL/MOD3 wearable meshes, and NAM6 height combines with XSCL.
The display-equipment policy takes positive-count, non-overlapping armour in
inventory order. This is a provisional visual policy, not Bethesda's complete
AI equipment scoring, levelled inventory or weapon-equipping implementation.

`npc/fo3-actor-animation.*` owns portable transform/pose evaluation.
`rendering/mesh/fo3-actor-animation-decode.inc` shares the existing bounds-checked
NIF parser and exposes skeleton/KF decoding. The layouts are based on NifTools
nifxml and verified with the supplied 20.2.0.7 / user 11 / Bethesda 34 files.
Skeleton parents, rotations, translation and scale remain intact; complete
inverse skeleton bind matrices produce bone deformation matrices. Mesh bone
names map to this skeleton while the existing authored skin weights are retained.
All Lucas outfit/head bone names matched the supplied skeleton in a host probe.

KF playback supports ordinary transform channels, linear quaternion SLERP,
linear/Hermite scalar/vector keys, XYZ Euler rotation groups, float and signed
short compressed open uniform cubic B-splines. Compact controls expand with
`offset + short/32767 * halfRange`; quaternion curves normalize after evaluation.
The sequence's accumulation root starts at identity before controller evaluation,
avoiding a second application of the skeleton's root translation. Sequence cycle
and frequency are respected. TBC and quadratic quaternion tracks are explicitly
rejected rather than approximated. Non-transform cosmetic float/visibility
controllers are counted but not evaluated; facial expression and lip sync need
TRI/controller support. Walking KFs decode in tests; root-motion extraction and
locomotion scheduling are not connected to the actor runtime yet.

`npc/fo3-npc-runtime.inc` bridges CPU staging to renderer-owned actor instances.
Scene preparation builds meshes, FaceGen textures, skeleton and the original
skeleton-directory `locomotion/mtidle.kf` on the existing joined worker. Temporary
generated texture maps are request-local until GPU publication. Actor shapes
upload one per submitted frame while the loading presentation remains visible.
They publish atomically with the new scene; aborted/staged and retired resources
are freed on the rendering thread. Generated CPU textures are released after
upload, with decoded/uploaded texture caches retaining their usual ownership.

Pose updates happen once per stereo frame, skin each source vertex once, and
copy posed positions/normals/tangents into reusable expanded GPU buffers.
Rigid eye/mouth/head parts use the same head-bone deformation as the body.
The rig captures its scene origin explicitly. Scene replacement rebuilds actor
resources, avoiding reuse of VBOs relative to a previous door-arrival origin.

Current actor scope is Megaton plus explicitly loaded interiors; Wasteland
persistent actor residency, ACRE/CREA, enable-parent/quest state, schedules,
NAVM pathfinding, dialogue, scripts, actor collision and combat are not implemented.
Actors play original idle poses at authored initial ACHR placements. This milestone
provides a visual/animation foundation, not a claim of functioning Fallout AI.

Host tests in `tests/nif` cover gender/wearable slot selection, selective template
inheritance, deleted/disabled references, matrix inverses/hierarchy cycles,
accumulation-root handling, linear/cubic sampling, cycle modes and invalid files.
Optional integration arguments sample the original idle/walk for 360 frames each
and resolve the supplied Megaton ESM; no game data is shipped in the repository.
Headset stereo appearance, skin seams, door-return placement and performance
still require an on-device pass.

## Player state and inventory foundation (0.26.0)

`player/fo3-player-state.*` is a portable gameplay data/state layer, independent
of Android, OpenXR and GL. Its bounded ESM scan reads the original Player NPC_
FormID 7 (ACBS, DATA, DNAM, CNTO), the required seven GMSTs, and original WEAP,
ARMO, AMMO, ALCH, INGR, MISC, KEYM, BOOK and NOTE definitions. Record layouts
follow [xEdit's FO3 definitions](https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.6/Core/wbDefinitionsFO3.pas).
Deleted records are excluded; compressed records and extended subrecords are
supported. Missing/invalid player data, inherited/auto-calculated player stats,
plugins/master chains and levelled starting inventory are explicitly rejected.
Starting CNTO/COED entries retain authored condition; unresolved owner/rank
metadata is rejected rather than discarded.
The destination catalog remains unchanged on failure.

The current player stats use the manually authored Player template. Derived
health is `DATA baseHealth + (END + fAVDHealthEnduranceOffset) *
fAVDHealthEnduranceMult + (level - 1) * fAVDHealthLevelMult`; AP is
`fAVDActionPointsBase + AGI * fAVDActionPointsMult`; capacity is
`fAVDCarryWeightsBase + STR * fAVDCarryWeightMult`. These baseline relationships
are documented in the original GECK settings reference, mirrored at
[health](https://geck.uesp.net/wiki/FAVDHealthEnduranceMult),
[AP](https://geck.uesp.net/wiki/FAVDActionPointsBase) and
[carry](https://geck.uesp.net/wiki/FAVDCarryWeightsBase).
Constants come from the installed ESM, not those documentation pages (which may
also describe New Vegas). Skills and skill offsets are retained in original
DNAM order, including the unused Throwing slot. They are not re-derived from
SPECIAL or tagged by a simulated character-creation script.

Inventory stores stable per-stack IDs, positive signed quantities and normalized
condition. Identical unequipped FormID/condition stacks merge. Equipped items
split out as one instance retaining the selected stack ID. Equipping a weapon
unequips the previous weapon; armour only displaces overlapping authored biped
slots. Non-playable and broken items cannot be user-equipped. Authored quest
flags and weapon cannot-drop flags block removal. Counts, stack limits and numeric inputs are bounded; invalid
operations leave state/revision unchanged. Weight currently sums authored item
weights, including equipped stacks; ownership/quest runtime exceptions and
encumbrance movement effects are not implemented. FO3 ammunition has no weight
field. Aid/ingredient auto-calculated values are flagged unknown rather than
claimed to be complete without the magic-effect runtime. Script/enchantment
FormIDs are retained without executing them.

The startup scene worker creates a request-local Session, restored from
`files/player-state.fqps` if present. It publishes on scene commitment; the one
render-thread-owned session survives CELL changes and GL resource recreation.
`GetFo3PlayerSession()` returns null before publication. UI/gameplay callers must
mutate through Player operations on that same thread. Catalog failure leaves
the existing world prototype usable and emits `PLAYER STATE UNAVAILABLE`.
`PLAYER STATE READY` reports HP/AP, capacity, weight, catalog size and save status.
No game data is loaded or save file read every frame.

The custom FQPS v1 save contains mutable resources and inventory, with explicit
little-endian fields, catalog fingerprint, payload length and CRC. Restore
validates IDs, quantities, conditions, equipment conflicts and resource bounds
before replacing live state. The catalog fingerprint covers decoded consumed
ESM records/headers, binding saves to the supplied definitions. Saves write a
temporary sibling file, flush/fsync it, then rename it over the destination and
fsync its parent directory. Dirty state flushes at scene commitment, loss of
OpenXR focus, session STOPPING and renderer shutdown. A rejected/unreadable
existing save blocks writes and is preserved;
its baseline session can still be inspected. No automatic reset or silent save
migration occurs. This is not Bethesda .fos compatibility, and it does not save
world/quest state or player position. Unexpected process termination before a
lifecycle flush can lose unflushed changes.

This milestone provides state and APIs for the next original Pip-Boy/pickup
slice. There is no stats/inventory UI or new controller action yet. Equipment
state is not connected to the visible body rig. Consumption, ownership/theft,
container transfer, scripts, perks, effects, radiation/limb damage, combat,
level progression and character-creation allocation remain unimplemented.
Portable checks in `tests/player` are run by the APK workflow.

## Item and load-door interaction (0.27.0)

The player catalog also indexes REFR NAME, XCNT, XHLP, XOWN and XLOC, CELL
ownership and scripted DOOR bases. Counts and normalized condition come from
original records. The CELL GRUP hierarchy supplies inherited ownership. Explicit
Player ownership is accepted; other ownership, malformed references, non-playable
items and scripted base activation are blocked until those runtimes exist. Any
XLOC is treated conservatively as locked, including key-only locks; a matching
inventory key permits travel. This does not execute OnActivate or OnAdd scripts,
faction privileges, public-cell ownership exceptions or leveled lock rules.

The right-controller virtual aim ray chooses the closest rendered loose-item
shape or existing authored XTEL door within 3 m. Item AABBs transform all eight
corners with the current loose-body matrix. A 1.5 cm item targeting tolerance is
an explicit VR adaptation. The authored collision grid performs two-sided ray
triangle occlusion, retaining large surfaces outside the dynamic grid. References
without rendered item geometry cannot be picked up. Existing no-model load-door
anchors remain available. Local door animation and container looting are pending.

Right A activates once per input edge. Pickup records the source REFR exactly
once, adds its authored count/condition, releases either hand and its dynamic body,
removes its collision, and flushes the player save. It never resets the VR player
origin. Door travel retains the existing destination resolution and origin/facing
handoff. The original HUD font/widget renders FULL item names with the vanilla
English Take label, verified in the supplied Fallout3.exe. No Bethesda asset is
committed. Unsupported items show their name but activation remains blocked.

Rendering (including instancing/reflection), grab selection and targeting skip
collected IDs. The collision TU owns a render-thread exclusion set, filters the
active triangle vector and invalidates derived caches/manifolds after removal.
It reapplies exclusions to initial worlds and every rolling snapshot publication;
background snapshot workers never read mutable Player state. Small removal events
can trigger a one-time derived collision rebuild; headset frame time needs checking.

FQPS v2 adds a separately fingerprinted world definition set and sorted collected
IDs. v1 saves retain their existing catalog identity and inventory/resources.
Restoration rejects duplicate/unknown collected IDs, mismatched world records,
truncated lists and invalid CRCs before publishing anything. World removals and
inventory share one atomic save, preventing restart duplication. Save-blocked
sessions reject pickup. Failed writes retain live state and retry on lifecycle
flush; loss of the process before a successful flush can lose a recent pickup.
This does not implement object respawning, drop-to-world, quests or general savegames.
