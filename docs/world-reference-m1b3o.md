# M1B3o — composite commit/recovery executor

Continues canonical main 66ed57cf5764af7bbed863c8e28e8009f945ee10 (v212).
M1B3n Android CI passed (run 38795209693). This connects prepared ownership plans
to a reserved same-turn commit executor. No production world-command caller is
added; broader frame/navigation/queue integration remains unfinished.

## Binding and preflight

Successful GPU handoff records the exact completed CPU packet and its owned
collision token. Discarding any core resource plan invalidates this binding.
Release, cancellation and scene/context reset clear it. A stale packet or a
replacement token cannot silently be paired with the current resource plans.

The concrete adapter requires an idle ready interior, its exclusive ticket and
unchanged scene/origin context. Candidate and rollback snapshot identities and
the complete affected list must match the actual ScriptTransactionPlan. Player's
CanCommitScriptTransaction checks canonical lifetime, revision, effect delivery
and script-location context. All resource plans must pass their read-only
admission, including collision policy/collection/door state, current visual/light/
door ownership epochs and coupled reference/cache publication. This does not
replace full authored capability/navigation/LOD admission.

## Ordered commit and rollback

ExecuteResidentExchange uses a fixed allocation-free sequence: canonical Player,
collision, lights, visuals, doors, then reference/cache publication. A final
verification checks the canonical rollback guard, current resource ownership and
the exact candidate publication/cache. Returning true from each component alone
does not bypass that final verification.

Every adapter operation is noexcept, and a failed Apply must leave that component
unchanged. The concrete implementations use the existing guarded swap APIs.
This function never interprets scripts, pumps events, starts workers, draws a
frame, saves, flushes Player or delivers queued effects. The invocation is one
render-thread call; the future frame caller must keep all observers excluded
across it and manage the asynchronous preparation interval.

If any step fails after canonical commit, completed resource steps unwind in
reverse order: doors, visuals, lights, collision. Canonical rollback then restores
state and queues at its precomputed new reference revision. Reference/cache
recovery runs last, both when candidate publication already happened and when a
previous resource failure prevented it. It must not fabricate that new revision
when canonical rollback itself fails.

The result distinguishes Rejected, Committed, RolledBack and RecoveryFailed and
records the failed stage, applied mask and recovery-failure mask. A failed undo
does not masquerade as a successful rollback. Remaining resource undos are still
attempted. The concrete bridge latches recovery failure, marks the scene unready,
blocks saving and queued script-effect delivery, and refuses cancellation/release
of its retained resource owners. Teardown/restart must recover that state; no
automatic scene reload or success-only command is substituted.

The executor neither finalizes/discards successfully applied owners nor releases
the lane. The eventual outer workflow must reconcile parked work, retire displaced
resources in the correct context, release the lane and only then resume ordinary
simulation/effects/save policy. The executing flag rejects reentry, not arbitrary
cross-thread use; all concrete calls belong to the render thread.

## Validation and limits

- Fifteen world, 31 player and one physics CTest cases pass (47 core host cases).
- The pure executor test uses real RAII resource exchanges and coupled reference
  publication with a synthetic canonical adapter. It checks every apply failure,
  failure after publication, exact unwind order, old buffer restoration, stale
  preflight and each undo failure. All turns run with allocations disabled.
- Whole-script tests use real Player/ScriptTransactionPlan and reference exchange
  with synthetic resource adapters. They inject every commit-stage failure and
  late verification failure, restore inventory/quest/effect state and confirm
  only successful turns retain queued effects. No effects are delivered by the
  executor. These are not host OpenGL or actual collision-adapter failure tests.
- The pure executor and whole-script tests pass ASan/UBSan with LeakSanitizer
  disabled. Existing publication/scene/resource/lane/GPU/door sanitizer cases
  pass. The actual runtime effect-delivery bridge test confirms a recovery-failure
  latch leaves its queued sound untouched.
- Source guards cover the concrete native adapter, packet/script/token binding,
  read-only preflight, component apply/undo mapping, final verification, binding
  invalidation and recovery-failure save/effect/lane guards. Original Lucas/
  nonresident/barter production dialogue tests pass against the existing original
  ESM copy. Relevant body/Pip-Boy/inventory/skin/opaque/render-dispatch checks pass.

Native Android validation runs in CI; no physical Quest/performance claim is
made. Frame exclusion and stable script ownership through async preparation,
authored/nav/LOD admission, actor/held-object/local-door/UI and exterior ownership,
finalization, queue reconciliation and command dispatch remain unfinished.
General resident Enable/Disable is not admitted. v212's separate bounded
nonresident actor Disable policy and existing gameplay systems are preserved.
