# M1B3h — private resident texture and GPU preparation

Continues canonical main ecbd98b85fd3c8979022c2b563f8aa3b6663de5a (v209).
The new entry points prepare a reversible visual plan; no production caller
schedules them or admits Enable/Disable, MoveTo, spawning or deletion.

## Preparation and ownership

The asset worker expands incoming vertex streams and decodes all named diffuse,
normal, glow, environment-mask and cube images into an immutable packet. Texture
paths are normalized and deduplicated separately by texture kind. Missing named
assets, malformed RGBA dimensions/bytes, incomplete cube faces/mips, cancellation
and a decoded-packet budget above 64 MiB reject preparation without publishing
partial output. Empty paths retain the existing intentional neutral materials.
The byte limit bounds retained decoded data, not decoder peak memory or GPU use.

The caller supplies scene/reference and visual stamps captured together with the
live visual inventory, before worker work. GPU admission and every upload step
revalidate them against the current publication, Player reference revision,
CELL/world and fixed scene origin. Chunk-capture/nested upload contexts reject.
The current resident selection is still supplied independently of the origin.

BeginFo3ResidentGpuStage consumes a CPU packet only after admission and private
allocation succeed. AdvanceFo3ResidentGpuStage uploads one complete incoming part
per call. The renderer's normal upload functions use a scoped candidate texture
context: validated named images cannot fall back, and new textures never enter
the live texture caches or known-texture bookkeeping. Existing real cached
handles may be borrowed while the GL context remains valid. Uploads preserve
raw RGBA8, ordinary material defaults and existing filtering. Cubes upload all
validated faces/mips and restrict the maximum mip to the supplied chain.
Bindings, VAO, array-buffer and unpack alignment are restored after each part.
Door prompt publication is suppressed during staging.

New texture handles belong to an explicit render-thread lease shared by incoming
GpuObjects. Lease adoption reserves storage before generating GL handles and
cannot allocate. Objects use the existing unique/shared geometry deletion path;
failed or thrown partial uploads release the entire staged batch. A batch's
textures remain allocated until its final owning shape is discarded. This is
aggregate batch ownership, not per-texture eviction. Borrowed cache handles are
never deleted by this lease. Deferred shape retirement also drops its lease.

After all parts succeed, staging transfers them to the existing reversible
visual ownership plan. The completed CPU packet returns to the outer caller,
retaining candidate/published reference snapshots and collision placements.
Nothing commits the visual plan or changes canonical state here. Empty incoming
batches can prepare removal-only plans. Cancellation, stale stamps and context
teardown dispose private work before global geometry/texture caches disappear.

The checkpoint also fixes the previous Android compile failure: the resident
resource selector no longer includes the transition collision-policy macro
before the original collision-parser definition. The unchanged pure authored
collision selection helper now lives in its own header. The real host collision
translation unit tests that renderer include order and rejects macro leakage.

## Validation and limits

- Nine world, 29 player and one physics CTest cases pass (39 core host cases).
- The new test runs production texture-upload helpers against a recording GL
  mock. It injects failure at every image/cube GL step, checks cube face/mip
  dimensions and filtering, zero handles, full/duplicate lease adoption,
  partial/throwing GPU uploads, cancellation, empty batches, ownership transfer,
  prepared-plan abandonment, commit/rollback and exact-once texture deletion.
- CPU texture tests cover normalization/deduplication, kind separation, missing
  and malformed images/cubes, byte limits, cancellation, throwing decode and
  unchanged output on failure. The new case passes ASan/UBSan with LeakSanitizer
  disabled for the traced runtime.
- Scene/worker/texture-cache/stamp/teardown source guards and body/Pip-Boy/
  inventory/actor-skin/opaque/render-dispatch regression checks pass.
- Mock GL checks are not actual driver/headset validation. One whole part may
  exceed a frame budget; no latency, throughput or peak-memory claim is made.
  Native Android compilation is checked separately in CI. v209 is unchanged.

The scheduler barrier and outer transaction still need candidate collision
policy/current door poses, lights and interactions, actor residency, held/loose
objects, navigation and LOD admission. Revalidate all script/Player/resource
stamps and exchange logical state plus every resource in one excluded render
turn, rolling back before simulation, saves or effects on any failure. Native
LOD retention alone cannot prove arbitrary reference mutations are supported.
