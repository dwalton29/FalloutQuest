# M1B3n — guarded reference/cache publication

Continues canonical main 1543fa28a4bd69c5fca72e4051598d93a3b8e6c2, preserving
v212 and its barter, Moira and MS11 work. This adds the reference-publication
piece of the unfinished outer coordinator, not a gameplay caller for resident
Enable/Disable. The separate v211 nonresident actor Disable path is unchanged.
M1B3m Android CI passed (run 38793178567).

## Why rollback needs a new projection

The canonical reference/script plans already precompute candidate and rollback
snapshots. Starting from revision N, candidate is N+1 and rollback restores the
old effective states at N+2. Republishing the old N snapshot after canonical
rollback would leave the renderer publication/cache inconsistent with Player
and invalidate all subsequent work for the wrong reason. Reusing the old scene
stamp would also let parked workers incorrectly appear current.

ReferenceExchange owns the original, candidate and rollback shared snapshots.
Preparation validates sequential revisions, valid reference/state entries and
exact old effective-state restoration in the rollback projection. It requires
the actual published snapshot and renderer cache to point at that same original
snapshot; equal values or matching revision numbers alone are insufficient.
No caller input or live publication changes on rejection/allocation failure.

The plan is tied to the publication object's lifetime identity, its current
scene/reference stamp and the stable cache-owner address. A legacy publication
invalidates it even when loading the same CELL/snapshot again. Reconstructing a
publisher at the same address cannot revive a stale plan. The cache slot must
outlive the plan; the renderer uses its stable global cache slot.

## Allocation-free publication and recovery

Before canonical commit, CanPublishBefore checks the old canonical revision and
all publication/cache identities. After canonical commit, Commit requires N+1
and assigns both the scene publication and fast cache to the precomputed
candidate snapshot. After canonical rollback, Rollback requires N+2 and assigns
both to the precomputed rollback snapshot. Neither operation allocates, performs
ESM access or rewrites the snapshot maps.

Rollback handles two paths: candidate reference publication already happened,
or a resource failure forced canonical rollback before it happened. Both must
publish N+2. In either case the scene epoch advances; before/candidate worker
stamps never become current again merely because effective states were restored.
Rollback is one-shot, not a toggle/reapply API. Abandoning a prepared plan leaves
publication untouched; finalizing an applied plan leaves the candidate live.

Preparation reserves room for two scene-epoch advances. ReferencePublication no
longer wraps its serial to a previously used nonzero identity. At exhaustion a
legacy Publish still replaces its snapshot, but marks its epoch invalid and all
worker admission fails closed. Normal scene-load/nonresident publication retains
its existing behavior; no counter exhaustion is silently reported as a valid
new worker epoch.

## Resident lane integration and contract

StartFo3ResidentCpuPreparation now requires the canonical plan's immutable
rollback snapshot as well as candidate. It validates projections before starting
the worker; the worker only retains that owned value. At GPU completion, the
reference/cache plan is prepared with the other resource plans, before final
visual handoff. Failure discards the private plan without publishing anything.

Core wrappers require the owning exclusive ticket, no active scene load and
the actual current Player reference revision. Preflight/commit also require the
interaction admission added in M1B3m; rollback deliberately does not require idle
UI. Lane release refuses an owned publication plan. Cancellation refuses an
applied publication; scene/context reset joins work before discarding the plan.

These wrappers publish references only. They do not commit/rollback canonical
Player state or prove the lifetime/context/effect stamps of a ScriptTransactionPlan.
The final coordinator must own that plan, verify candidate/rollback/affected-list
binding, exclude simulation/rendering/saves/effect delivery, apply or restore all
resources and publish the matching references in that same render-thread turn.
It must also handle a canonical rollback while reference publication is still
prepared. Dropping/canceling a prepared publication after changing canonical
state is not a substitute for that recovery. No production code starts this
resident workflow yet, and no normal CELL reload substitutes for it.

## Validation

- Fourteen world, 31 player and one physics CTest cases pass (46 core host cases).
- Publication tests cover malformed/overflowed revisions and states, rollback
  metadata mismatch, wrong owners, replacement cache pointers, stale/reconstructed
  publishers, allocation failure, no-allocation commit/rollback, finalization,
  both rollback paths and serial exhaustion.
- Real Player whole-script tests stage Disable plus inventory/quest/message
  effects, then commit and roll back canonical state and reference publication
  with allocations forbidden. They cover recovery before/after candidate
  publication, restore old effective states at N+2 and leave no staged effects.
- Publication/scene-preparation and existing admission/resource/lane/GPU/door
  tests pass ASan/UBSan with LeakSanitizer disabled in the traced runtime.
  Canonical whole-script publication/rollback tests pass under the sanitizers
  too. The original Lucas/nonresident/barter production dialogue bridge passes.
  Source guards verify worker isolation, handoff/failure cleanup and lifecycle
  guards; body/Pip-Boy/inventory/skin/opaque/render-dispatch checks pass.

Native Android validation runs in CI. No physical Quest/performance claim is
made. Actor/held-object/local-door/UI ownership, navigation and baked LOD,
exterior resource scope, the final atomic coordinator and parked queue
reconciliation remain unfinished. General resident Enable/Disable is not admitted.
