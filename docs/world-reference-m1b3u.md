# M1B3u — cooperative interruption and explicit context teardown

Continues canonical main 1be0d0a64136c4e40d8242447c241755b2be2e82 (v212).
M1B3t Android CI succeeded (run 38908824693).

## Interruption is not execution

The workflow exposes a guarded Interrupt operation. Draining, CPU, GPU and Ready
candidates request cooperative cancellation and retain owners until the adapter
finishes cancelling. Ready never executes as a side effect of interruption.
Committed, rejected and rolled-back settled outcomes instead use the existing
settlement finalizer; a pending/failed guard retains their exclusion and owners.
Executing/finalizing reentry and fatal recovery refuse interruption.

The native focus-loss pump uses this operation, including repeated pumps while
the XR session is no longer running. STOPPING therefore does not depend on a
future rendered frame to complete cooperative cancellation. Settled finalization
still requires all of its original owner, epoch, activity, queue and resource
proofs; interruption does not relax them or invent a recovery.

The existing deferred-save service runs after this pump even on non-rendered
frames and in the non-running session loop. Its original frame/fatal/save guards
remain authoritative, so it cannot flush a still-owned or failed exchange and
does not require a later valid stereo view once cancellation has completed.

## Scene-change boundary

Scene loading no longer invokes the joining resident reset on an ordinary
frame. A read-only queue-presence probe prevents idle scene polling from
interrupting a resident workflow when no transition exists. When a request is
pending, the renderer first cooperatively quiesces the resident owner, including
an acquired lane whose logical factory did not become a workflow owner. Only
after successful quiescence does it consume the original request.

Pending worker cancellation or settlement preserves the queue and loading
generation. There is no request rewrite, synthetic destination or dropped
transition. Fatal recovery rejects new scene loading. A settlement whose
original finalization conditions cannot become valid remains fail-closed; this
does not force a scene swap past an invalid owner/loading-context guard.

## Explicit teardown and save safety

XR shutdown now explicitly retires resident work before flushing Player,
destroying UI, deleting framebuffers or releasing EGL. It runs even if renderer
initialization never created a framebuffer. The framebuffer deletion hook remains
an idempotent fallback, not the only way to drain resident resources.

The shutdown path first attempts normal cooperative cancellation/finalization.
Uncommitted preparations may then be joined at this explicit teardown boundary.
It latches teardown exclusion, joins the resident CPU worker, discards private
GPU staging, retires visual/light/door/reference plans and interaction admission,
then destroys the workflow packet/script and clears prepared bindings while the
old context and shared geometry/texture caches are alive.

A settled owner that cannot prove/finish finalization, or a fatal recovery,
blocks saving and marks the scene unavailable before these owners are destroyed.
Resource cleanup never clears the fatal latch: deleting resources is not proof
that a mixed canonical/resource world was repaired. Existing disk saves are not
overwritten through this path. The session's saveBlocked flag remains set.

Reentrant shutdown during execution/finalization refuses to tear down. If the
joining/disposal bridge throws, it retains teardown/fatal/save exclusion and
returns failure; the XR caller returns without destroying its UI/GL context.
No ordinary-frame join or bounded shutdown-time guarantee is claimed.

## Validation and limits

20 world, 31 player and one physics CTest cases pass (52 core host cases), plus
the production actor runtime test. The real generic workflow tests now interrupt
every preparation phase, all settled outcomes, a held asynchronous CPU worker,
pending finalization, fatal recovery and execution/finalizer reentry. They prove
that Ready interruption never calls Execute and cancellation retains ownership
until worker acknowledgement.

A new host test compiles the actual native lifecycle and reset includes against
tracked context/resource slots. It verifies cooperative scene-change retries,
orphan-lane cancellation and exceptions, no joining reset on that path,
preparation shutdown, successful settled finalization, fatal/unfinalizable save
exclusion, disposal order under the teardown gate, idempotent repeated cleanup,
missing Player, reentry refusal and simulated join failure retaining context.
Both affected targets pass ASan/UBSan with LeakSanitizer disabled.

Native source checks verify the real request-presence/quiesce/consume ordering,
focus/non-running pumps, explicit shutdown before UI/save/EGL destruction and
fatal-latch persistence. Original ESM authored guard, load-door, Lucas/nonresident/
barter dialogue, merchant trading and MS11 reward/save regressions pass. Existing
unsupported showmap diagnostics remain. Relevant body/skin/Pip-Boy/shoulder,
opaque, dispatch and notification-layering checks pass.

The native harness mocks platform/context/resource slots; it is not an on-device
OpenXR/EGL-loss test. Actual context-loss recovery, complete admitted dependency
ownership, a bounded gameplay command bridge and device fault injection remain
open. No command dispatches general resident Enable/Disable yet. Exterior,
general navigation/LOD and broader actor/interactive mutations stay unadmitted;
existing v212 gameplay and bounded nonresident actor Disable remain preserved.
