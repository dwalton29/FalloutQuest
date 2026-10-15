# M1B3q — owned asynchronous resident workflow

Continues canonical main ab592e1cb7cddcdb6e05d80abff04b753fcc1de1 (v212).
M1B3p Android CI passed (run 38799323481).
This connects script ownership to the existing reserved CPU/GPU preparation and
composite executor. The new workflow has no gameplay command-dispatch caller;
general resident Enable/Disable remains unadmitted.

## Ownership and progress

The native factory reserves the asset/frame lane before invoking an immediate
preparation callback with const Player access. It accepts the resulting whole-
script transaction only if its canonical owner/revision/effect/context guards
are current and it has candidate/rollback reference projections. Null, rejected
or throwing preparation cancels the unused reservation without applying state.
The callback is not retained, run by an asset worker or called during commit.

One render-thread workflow owns the exact ScriptTransactionPlan, request and
completed CPU packet. The existing asset lane owns its CPU job, and the private
GPU stage owns the packet while uploading. Only immutable projections, copied
affected IDs and other existing value inputs cross to the CPU worker. The
candidate Player and mutable script plan stay on the render thread.

Progress has distinct Draining, CPU, GPU and Ready phases. Each pump advances
at most one phase; GPU progress still uploads at most one complete part per
call. Canonical/context/interaction guards are checked on every pump, including
Ready. CPU failure, failed packet transfer, failed GPU setup/upload or a thrown
preparation operation moves to cancellation. Ready never auto-commits.

The real OpenXR loop pumps once after xrBeginFrame, before shouldRender/view
checks and outside the gameplay-exclusion branch. A reservation therefore does
not deadlock its own preparation, including frames without valid rendered views.
The pump returns immediately while the workflow is idle. A nonfocused XR state also pumps
cancellation immediately on the same render/event thread while its GL context
is current, including STOPPING before another submitted frame can occur.

## Cancellation, execution and teardown

Cooperative cancellation retains the script, packet and ticket until the CPU
job has finished and the existing native cancellation path has disposed private
GPU/resource plans. A cancellation/finish race cannot publish the result.
Ordinary direct release/cancellation is rejected while the workflow owns the
reservation; its scoped cleanup path alone can call native cancellation.
The frame gate also checks workflow ownership as a conservative backstop.

Explicit Execute requires the matching ticket and Ready state, marks Executing
before entering the no-throw composite executor, and rejects reentry or
cancellation. The native composite preflight now requires the exact packet and
script owned by that executing workflow in addition to all prior canonical and
resource guards. Raw unrelated or replaced script/packet pairs cannot commit.

Committed, RolledBack and Rejected outcomes become Settled; RecoveryFailed gets
a separate retained phase and the existing fatal scene/save/effect guards.
Every outcome retains its resource owners and reservation. Even a clean rollback
has a new publication revision, so ordinary cancellation is not a substitute for
parked-result reconciliation and finalization. Those boundaries are still to be
implemented. Effects/saves remain excluded until a future valid finalizer can
retire displaced ownership, reconcile queues and resume the frame.

Scene/context teardown joins the resident worker before clearing workflow
owners, then follows the existing staged/displaced-resource disposal sequence
while the original GL context and caches remain alive. ResetAfterDrain is a
teardown-only operation, not a way to finalize or resume a live transaction.

## Validation and limits

- Sixteen world, 31 player and one physics CTest cases pass (48 core host cases).
- The workflow test uses the real asynchronous preparation lane with synthetic
  script/packet resources. It covers busy worker draining, every preparation
  failure, exceptions before and after ownership transfer, incremental GPU
  progress, stale context in every phase, wrong tickets, repeated/reentrant
  execution, all four executor outcomes, held cancellation and teardown joining.
  Destruction counters verify retained and exactly-once reclaimed owners.
- The real Player/ScriptTransactionPlan test now runs all composite apply and
  post-publication failure cases through the workflow owner. Canonical state,
  publication and effects stay unchanged during preparation; commit/rollback
  execute with allocations forbidden. Separate tests invalidate the prepared
  plan through health revision, message input and script-location changes.
- Both changed host targets pass ASan/UBSan with LeakSanitizer disabled. Source
  guards verify native factory ordering, concrete adapter calls, owner binding,
  teardown ordering, focus cancellation and frame-pump placement. These do not
  execute a native OpenXR frame or real GL/collision exchange on the host.
- Original ESM regressions pass for Lucas/nonresident/barter dialogue, merchant
  trading, MS11 reward/save branches and load-door resources. The pre-existing
  unsupported showmap diagnostic remains. Relevant body, skin, Pip-Boy,
  shoulder, opaque-path and render-dispatch checks pass. Android compilation is
  delegated to CI; no physical Quest or performance claim is made.

Next: resource finalization and parked-result reconciliation, complete authored/
navigation/LOD admission, timer rebasing and remaining lifecycle handling, then
command dispatch. Exterior/actor/held-object/local-door/UI ownership restrictions
remain. Existing v212 gameplay and its bounded nonresident Disable are preserved.
