# M1B3f — reversible resident visual ownership

Continues canonical main fb8e4024a53cc3d265bed752929d9c9f22dbab62 (v209).
This implements the visual ownership exchange needed by the resident resource
coordinator. It does not select/upload a candidate world, coordinate all world
resources, or admit Enable/Disable.

## Implemented

The renderer's GpuObject records contain explicit GL ownership, including unique
VAO/VBO/skin resources and references to shared geometry. Copying live records
into a rollback vector would duplicate ownership without increasing shared
reference counts. The new VisualExchange prepares default slots and an ordered
list of retained live indices instead. Incoming staged records already own their
resources; successful preparation consumes those records into the plan only
after every index/slot allocation completes. Live resources remain untouched and
drawable throughout preparation. Invalid indices, duplicate/out-of-order slots,
aliasing input and revision exhaustion reject without transferring resources.

Commit swaps retained live records into the reserved slots, then exchanges the
vectors. The plan holds removed resources and empty holes for retained slots.
Explicit rollback restores the original vector storage and slot order, returning
incoming ownership to the plan. Neither operation allocates or adjusts shared
geometry reference counts. No retained resource handle is copied. Retained
records carry their current metadata through the swaps rather than restoring an
old copy of a transform or lighting cache.

Cancelling a prepared plan disposes only incoming resources. Finalizing an
applied plan disposes only removed resources. Finalizing after rollback disposes
only incoming resources. Renderer disposal uses Q74DeleteGpuObjects, including
the existing skin, shared-reference and unique-buffer cleanup paths. Texture
ownership stays with the existing texture caches.

A separate visual-layout publication epoch guards visual owner identity,
revision, backing storage and size. Commit/rollback advance it; wrong owners,
address-reused epoch objects, competing/stale plans and double use reject before
moving any live slot. Epoch exhaustion never wraps into an older valid revision.
Legacy initialization, scene replacements, Q18/Q19 composition/retirement and GL
teardown mark the epoch before changing live ownership. Opaque pointer order,
main/reflection batches and water-reflection reuse also compare this epoch, so
restoring an original pointer/size cannot revive stale frame caches. Exhausted
epochs force cache rebuilds. Visual exchanges invalidate sun/terrain shadow reuse.

The renderer owns at most one resident visual plan. A new scene load or GL
teardown disposes it while the original context and shared geometry/texture
caches still exist. No caller can retain its unique ownership past that teardown.
An empty candidate is allowed and does not reset the loaded-scene readiness flag.

## Remaining coordinator contract

Preparation, plan ownership, exchanges and disposal belong to the render thread.
Workers prepare CPU inputs, never live GpuObjects or this mutable plan. The caller
must select retained/incoming shapes from the complete candidate reference
projection and preflight/upload every incoming part before preparing this layer.

This epoch tracks structural visual ownership, not arbitrary per-frame gameplay
changes. The outer coordinator still must capture/revalidate scene, Player,
collision, actor and interaction inputs; drain/isolate background asset work;
prepare lights, doors, actor residency, held/loose objects and navigation; and
exclude simulation, saves, effect delivery and eye rendering between coordinated
exchanges and any rollback. The one-frame outer transaction must finish before a
subsequent scene load can discard/finalize its visual plan. Layout guards alone
do not make a script or an actor move transactional.

The production renderer has the ownership methods and lifetime hooks, but no
script/render path initiates resident mutation preparation yet. Source compiler
rejection of Enable/Disable, MoveTo, spawning and deletion is unchanged. Continue
with candidate resource preparation and the outer runtime coordinator; do not
implement success-only command handlers or substitute a CELL reload.

## Validation

- The production template is tested with noncopyable explicit-disposal resource
  records: retained ordering/storage/metadata; unique and shared handles; incoming
  sharing a live buffer; cancel/finalize/rollback deletion; empty/remove-all,
  empty-to-nonempty and all-retained cases; wrong owner, stale/competing plans,
  owner address reuse, reallocation and revision exhaustion. Every preparation
  allocation is failed in turn, and allocations are forbidden during successful
  commit, rollback and finalization. Reference counts return to zero without
  double release. These host records model ownership, not an actual GL driver.
- Seven rebuilt world, 29 player and one physics cases pass (37 host cases).
  Visual exchange tests pass ASan/UBSan with LeakSanitizer disabled for this
  traced runtime. Existing Player/physics sources are unchanged.
- Scene/epoch/NPC source checks now also verify visual epoch marking, real
  disposal routing, context teardown ordering and frame-cache invalidation.
  Opaque path, LOD diagnostics and render-dispatch source checks pass. C++ GL
  state-cache and stereo/reflection policy regressions pass.
- Native Android compilation and actual GL/physical Quest validation remain
  separate. No original-data visual upload, headset result or performance result
  is claimed. Save schema and app version remain v209.
