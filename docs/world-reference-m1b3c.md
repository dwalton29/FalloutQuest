# M1B3c — detached reference mutation plans

Continues canonical main 30e18f9219748b2d9133b0824b8d9a1f097fd99e (v209).
This checkpoint implements reference-state planning and commit ownership. It
neither stages complete script result transactions nor admits Enable/Disable.

## Implemented

PrepareReferenceEnablePlan validates an ordered batch into a private root overlay
without changing Player state, revisions, queued effects, inventory or save data.
The shared enable-parent resolver is used by live queries, existing staging and
candidate projection, preserving the same graph validation, inverted links,
parent-controlled no-ops, root override rules and bounds.

Each owned plan contains immutable candidate and rollback reference snapshots
and a sorted list of all effectively changed references. This includes original
XESP descendants outside the resident scene, root changes and removed overrides.
Workers retain the shared const snapshots instead of reading mutable Player or
retaining the render-thread plan. No immutable ESM catalogue is copied for this.

Invalid requests reject the whole batch. Cancelling/discarding a prepared plan
changes nothing live. Batches which return to the original root state are no-ops
and do not advance revisions. Override/batch bounds and revision exhaustion are
checked during preparation, before anything can publish.

CommitReferenceEnablePlan swaps only the prepared root map into canonical saved
state after validating player/session identity and the captured full-Player and
reference revisions. Rollback restores the previous root values only if no newer
Player mutation occurred. Both operations allocate nothing; snapshots and map
storage are prepared in advance. Plans cannot be committed twice, used after
rollback or transferred to another Player, including an address-reused session.
Destroying an applied plan finalizes that overlay; rollback is explicit.

Rollback advances revisions instead of rewinding them, preventing older plans
or scene workers from being admitted again. The plan provides the matching
rollback projection. Preparing an unrelated later plan does not invalidate an
older one; successful publication or another live Player mutation does.

## Coordinator contract and remaining boundary

The full-Player revision guard is deliberately conservative: clock, inventory,
health, actor or other changes can make a pending plan stale. The coordinator
must pause/revalidate the affected outer transaction during preparation; it must
not overwrite newer state or retry indefinitely against a continuously changing
simulation. This is a reference-only payload, not a saved copy of quest/inventory
changes or message/sound/Karma queues from a whole script.

The runtime must prepare visuals, collision (using M1B3b), door poses, actors,
held/loose objects and navigation from the candidate projection, validate the
scene/collision/player stamps, and exchange everything in one render-thread turn.
Commit the canonical reference map only after the other exchanges can succeed.
If resource preflight fails, discard the plan without any Player commit. If an
already committed map must be rolled back, publish the plan's rollback projection
with the restored resources and keep its advanced revision. No physics/AI/save
flush or eye rendering belongs between those coordinated changes.

No production script or render path invokes these new methods yet. Existing
StageReferenceEnable remains a lower-level persistence/staging API, not an
admitted source command. Existing compiler rejection of Enable/Disable, MoveTo,
spawning and deletion is retained. Empty-world collision semantics and the
resident outer-transaction/resource/actor coordinator remain unfinished.

## Validation

- Dedicated production-Player tests verify preparation leaves live state and
  saved bytes unchanged; complete affected-reference lists; transitive/inverted
  parents; atomic malformed/unknown/deleted/cyclic batch rejection; batch limits;
  no-op/cancelling batches; revision exhaustion; stale/competing plans; wrong
  sessions and address reuse; snapshot lifetime; commit/save/reload; and stale
  rollback preserving newer health/state. Allocation failure is injected during
  successful commit and rollback.
- Original master event tests prepare MS11DetonatorRef and an authored enable
  parent without live staging, compare all 5,502 original XESP children with the
  committed candidate projection, then roll back. Original bomb/cancel/disarm/
  Karma/notices/persistence regressions pass. Source command-compilation coverage
  remains 597/2,053 indexed blocks; this is not execution coverage.
- Full rebuilt player suite: 28 passed; world: six passed; collision: one passed
  (35 host cases total). Original Lucas/MS11 acceptance/save regressions also
  pass. Scene/epoch/NPC publication ordering source checks pass.
- AddressSanitizer/UndefinedBehaviorSanitizer pass for the new ownership/plan
  tests. As in M1B3b, LeakSanitizer is disabled because it cannot inspect traced
  task threads in this environment (ASAN_OPTIONS=detect_leaks=0).
- Android build and physical Quest checks remain separate and outstanding.
- Save schema and version remain at v209. Runtime command admission is unchanged.
