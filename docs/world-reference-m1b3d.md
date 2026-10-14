# M1B3d — whole-script candidate transactions

Continues canonical main 0e9dd258701f9aca48de66acae3994129d20c6d3 (v209).
Supported dialogue results, nested quest stages and source event blocks now
evaluate against a private Player candidate before live publication. This is
the logical-state portion of the resident outer transaction; Enable/Disable
and other unsupported world commands remain rejected by the source compiler.

## Implemented

Preparation owns saved Player state, actor pose revision, message-button inputs,
and pending messages, sounds, quest notices, Karma and theft events. Failure or
discard leaves live state, save bytes and effect queues unchanged. Dialogue
begin/end fragments and nested stages share one candidate and instruction
budget. Const previews use this same path without mutating the live Player.

Candidates share the immutable original-data catalogue, avoiding catalogue
copies on every preview/event. They do not copy diagnostic callbacks, reported
error history or scheduler accumulators. Scheduler cadence and diagnostic
delivery remain owned by the live Player; rejected events retain the existing
consume-once message-button policy.

Commit and explicit rollback exchange prepared storage without allocation.
Admission validates owner/session identity, the full Player revision, an effect
and input epoch, and script location/activator/reference context. Consuming an
effect or submitting input invalidates pending plans even when saved state did
not change. Delivered effects cannot be restored or replayed by rollback.
Rollback advances revisions; it never re-admits an older competing plan.
Destroying an applied plan finalizes its effects. CandidateState is available
only before publication, because the applied plan holds the displaced payload.

If a future admitted operation changes root enable state, preparation composes
the M1B3c immutable candidate/rollback reference projections and affected list.
No reference projections are built for the current non-world script path.
Immediate production wrappers reject any reference-changing plan until the
resident coordinator exists. No new world-command handlers are introduced.

## Remaining publication boundary

Plans and Player access belong to the render thread. Resource workers must
receive owned immutable snapshots, never a candidate Player or mutable plan.
The full revision/context guard is conservative: the coordinator must pause or
revalidate the outer transaction while resources prepare. It cannot overwrite
intervening simulation changes or rely on retries against continuously moving
state.

The next checkpoint must coordinate resident geometry/GPU state, interactions,
collision, door poses, actors, held/loose objects and navigation with the logical
transaction. Preflight every resource, validate scene/collision/player stamps,
then exchange them in one render-thread turn before physics, AI, saves, effect
delivery or eye rendering. Failure must leave the prior world intact; rollback
must publish the matching advanced reference projection. Empty-world collision
semantics also remain unfinished. This checkpoint does not claim resident-world
publication, broader script coverage or frame-time parity.

## Validation

- Dedicated transaction tests cover pure preparation/save bytes, shared immutable
  catalogue, nested stages/journal, dialogue fragment failure, truly const
  previews, unsupported nested commands, effect ordering and delivery-once,
  message-button input, event diagnostics, save/reload, stale/wrong-owner plans,
  context changes and competing plans. Allocation failure is injected during
  successful commit and rollback.
- Original ESM event regressions retain bomb activation/cancel/disarm, Karma,
  notices, persistence and all 5,502 enable-parent children. Production source
  command-compilation remains 597/2,053 indexed blocks, not execution coverage.
- Full rebuilt player suite: 29 passed; world: six passed; collision: one
  passed (36 host cases). Original Lucas/MS11 acceptance and catalogue/census/
  save regressions pass. Scene/epoch/NPC source checks and four scripting
  audit tests pass. Both reference-plan and script-transaction tests pass
  AddressSanitizer/UndefinedBehaviorSanitizer.
- AddressSanitizer/UndefinedBehaviorSanitizer run with detect_leaks=0 because
  LeakSanitizer cannot inspect traced task threads in this environment.
- Android CI and physical Quest checks are separate from host validation.
  No physical Quest or CPU/GPU performance result is claimed.
- Save schema and app version remain unchanged at v209.
