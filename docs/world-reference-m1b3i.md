# M1B3i — reserved resident asset preparation lane

Continues canonical main 25a7be3006e5eb2b16eaf6d48ffb9319b4f95f71 (v209).
M1B3h Android CI succeeded (run 38628279870), confirming the previous include-order
compile failure is resolved. This connects CPU/GPU preparation to an explicit scheduler reservation. No
production script starts a resident transaction; world commands remain rejected.

## Reservation and worker ownership

AcquireFo3ResidentAssetLane reserves a monotonically numbered render-thread
owner ticket and captures the published scene/reference/visual identities, CELL,
world and fixed GPU origin. It rejects loading, an active legacy window change,
a visual ownership plan, invalid publication and an already reserved lane.
Ticket exhaustion fails closed; an old ticket cannot cancel a newer owner.

While reserved, the ordinary exterior/CELL, collision, terrain, LOD and NPC
arrival scheduler entry points pause consumers as well as launches. Existing
CPU workers may finish. PollFo3ResidentAssetLane performs acquire-load readiness
probes across CELL, collision-snapshot, LOD, terrain, NPC-arrival and legacy
metadata jobs. Only after every running job is quiescent does the lane become
exclusive. No completed output is consumed, no collision token is published or
discarded, and no live GPU layout or terrain target changes during this probe.
The old queues and their partially uploaded resources remain owned in place.

Terrain exposes a read-only readiness probe rather than consuming its result.
Legacy detached metadata jobs are weakly tracked even after their normal pending
stream is cancelled, so losing that pending owner cannot hide an active reader.
Registry entries are pruned on later legacy launches. The normal idle scheduling
policy is otherwise unchanged. Audio's independent streaming system is outside
this geometry/texture/collision preparation lane.

StartFo3ResidentCpuPreparation requires exclusive ownership and an unchanged
scene context. The renderer captures value-only live visual IDs, original request,
explicit exterior scope, immutable published/candidate reference snapshots and
affected IDs before launching the existing strict CPU worker. The worker never
reads Player, gObjects or the mutable publication objects. Its packet carries the
owner ticket. TakeFo3ResidentCpuPreparation transfers a complete successful
packet, joins its finished worker, and keeps the reservation for GPU/collision/
outer-transaction preparation. Allocation failure leaves both the existing
caller output and worker result owned and available for retry.

GPU admission and every GPU step now also require that packet's exclusive ticket
and the captured context. ReleaseFo3ResidentAssetLane refuses an owned CPU job,
GPU stage or visual plan. The outer caller must finalize/discard the visual plan
before releasing the scheduler. Cancellation discards private GPU/prepared visual
resources, requests CPU cancellation and keeps the scheduler reserved until the
worker has completed. Poll cancellation with the same ticket; it does not join
unfinished decode work during an ordinary frame. A finish race cannot publish a
cancelled result. Failed CPU work also remains reserved until explicit cancellation.

Cancellation refuses an already applied visual exchange. The outer transaction
must first roll it back or finalize it; silently treating a published resource
change as cancelled would violate logical/resource atomicity. The visual plan
exposes only its applied phase for this guard. Explicit scene/context teardown
requests cancellation and joins owned resident CPU work before changing context
or deleting staged/context resources. Teardown can wait for a decoder to return;
no hard frame-time bound is claimed.

## Validation

- Ten world, 29 player and one physics CTest cases pass (40 core host cases).
- New lane tests cover all six busy families, drain-before-start, stale and
  overlapping owners, packet allocation failure/retry, exactly one preparation
  per owner, successful transfer while retaining the GPU reservation, failed/
  throwing work, parked-result preservation, cancellation while decode remains
  deliberately blocked, a cancellation/finish race, teardown joins and ticket
  exhaustion. Synchronization uses atomic handshakes rather than timed sleeps.
- Lane, scene preparation, visual ownership and private GPU tests pass ASan/UBSan
  with LeakSanitizer disabled in this traced runtime. Visual-plan tests check
  prepared/applied/rolled-back phase reporting.
- Production source guards check scheduler consumer/launch gates, all activity
  probes, immutable worker capture, ticketed GPU admission, release/cancellation
  restrictions and join-before-context teardown. Existing body/Pip-Boy/inventory/
  skin/opaque/render-dispatch/LOD source checks pass.
- These host checks do not execute the complete Android renderer or prove
  physical Quest scheduling/performance. Android compilation is checked in CI.
  Save schema and app version remain v209.

## Remaining coordinator

The caller still needs the complete script/Player/collision/resource stamps,
actual current residency scope and candidate collision model policy/door poses.
A parked old collision preparation must be retired safely before reserving a
candidate collision token. Successful publication must invalidate/reconcile old
CELL/NPC/terrain/LOD queues under the new canonical projection; releasing this
lane alone does not publish a new reference scene or prove those caches valid.

Lights, interaction/door prompts, actor residency, held/loose objects, navigation
and unsupported baked-LOD dependencies still need preflight and coordinated
publication. Simulation, saves and queued effects must be excluded from the final
logical/resource exchange and rollback. This reservation pauses asset scheduling,
not game simulation. No CELL reload substitutes for resident mutation, and no
Enable/Disable handler is admitted by this checkpoint.
