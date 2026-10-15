# M1B3r — interior settlement finalization and parked work

Continues canonical main dc815bbb29d0d02b9fae9c64916ace16f23a57f5 (v212).
M1B3q's Android build exposed libc++'s potentially throwing unique_ptr
dereference annotation in a noexcept assertion. The preceding fix checks the
adapter's Packet/Script reference signature with declval instead. Its isolated
original workflow host test passes; Android revalidation runs in CI. New
finalizer assertions use the same signature-only form.

## Settlement proof

The composite executor records the exact reference publication, visual, light,
door and collision epochs at its returned outcome. Explicit finalization accepts
only a matching settled ticket/packet/script, ready idle interior context,
quiescent asset families and a releasable resident lane. It checks result masks,
canonical lifetime/revision/effect/context, the expected candidate/before/rollback
snapshot and each component's actual owner, phase and publication epoch.

Player, visual, coupled resource and reference plans expose read-only CanFinalize
checks. They distinguish applied state from prepared or rolled-back state and
allocate nothing. Canonical rollback updates its expected guard revisions;
visual rollback captures its restored storage and new epoch. Neither change
allows a rolled-back plan to commit or roll back again. Queued effect delivery
or unrelated owner/publication changes invalidate settlement finalization.

## Reconciliation and retirement

Finalizing blocks cancellation, reentry, gameplay, effects and saving. The
workflow checks settlement, reconciles parked work, checks settlement again,
retires displaced/private resource owners, releases the lane and destroys its
packet/logical owner before returning to ordinary observers. A failed guard,
busy queue or allocation failure preserves settled owners and can be retried.
An unexpected release failure after retirement latches fatal scene/save/effect
exclusion rather than claiming that gameplay resumed. RecoveryFailed executor
outcomes cannot finalize.

For an unchanged Rejected outcome, existing queues still belong to the original
publication and are preserved. Committed and RolledBack outcomes both have a new
reference epoch. Their interior-only bridge drops completed CELL workers, CELL
metadata/private uploads, prepared collision output, LOD workers/private uploads
and completed legacy-worker tracking. It resets pending LOD requests while
preserving loaded assets and invalidates the old CELL scheduler context. Already
retired geometry is disposed through its existing shared-buffer deletion path.
It never publishes an old result into live geometry, collision or actors.

NPC arrival IDs are merged into a new pending set before any queue is changed.
After finished private arrival resources are discarded, the ordinary scheduler
will rebuild still-needed arrivals against current canonical state; its existing
resident/enabled/already-visible filters remain authoritative. Allocation
failure while preparing that union leaves all queues unchanged.

The terrain bridge checks CPU quiescence and joins only the finished worker
before reclaiming private texture/VBO/current-cell staging and retired geometry.
It clears pending target work without touching published LAND, resident window
keys or grounding. All cleanup runs on the render thread with the original GL
context and ownership caches alive. It may reclaim multiple resources in one
call; no maximum frame-time guarantee is claimed.

## Validation and limits

All 17 world, 31 player and one physics CTest cases pass (49 core host cases).
The new runtime test compiles the actual interior and terrain cleanup includes
with tracked private handles and mock worker/thread slots. It verifies every
busy-family guard, exterior/unreserved/legacy rejection, allocation failure
before disposal, exactly-once cleanup/retry, NPC requeue and valid rejection
preservation. Published scene/actor/LAND registries are absent from that harness,
so accidentally accessing them in these bridges fails host compilation.

Workflow tests cover retained owners on guard/pending/throwing reconciliation,
post-reconciliation invalidation, reentry, successful finalization and fatal
release failure. Real canonical script and reference-publication tests finalize
successful and rolled-back composite outcomes with allocations forbidden and
verify message delivery once after success. Real resource exchanges check
settlement after every apply failure and late verification rollback, as well as
wrong owners, stale epochs and revision exhaustion.

The six affected world targets and canonical script target pass ASan/UBSan with
LeakSanitizer disabled. Native source guards cover the full finalizer adapter,
epoch pinning and retirement/release ordering. Original ESM Lucas/nonresident/
barter dialogue, merchant trading, MS11 reward/save and load-door regressions
pass. Existing unsupported showmap diagnostics remain. Relevant body/skin/
Pip-Boy/shoulder/opaque/render-dispatch checks pass. This is not an on-device
OpenXR/GL/collision fault-injection or performance result; Android builds run in
CI separately.

No gameplay command starts, commits or finalizes this general resident workflow
yet. Complete authored/navigation/LOD admission, timer rebasing and remaining
lifecycle handling must precede Enable/Disable dispatch. Exterior reconciliation
and broader actor/held-object/local-door/UI ownership remain unadmitted. v212
gameplay and bounded nonresident actor Disable remain preserved.
