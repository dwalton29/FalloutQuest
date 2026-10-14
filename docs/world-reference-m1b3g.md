# M1B3g — candidate placement and CPU resource preparation

Continues canonical main 5fe5c76d72eb09869df8f479df6781a4f6bd83ba (v209).
This adds the detached placement/CPU preparation stage. It does not upload or
publish a resident mutation, schedule the asset lane, or admit world commands.

## Implemented

The resource selector compares original placements loaded under published and
candidate immutable reference projections for the same declared residency scope.
It produces ordered retained live slots, removed/incoming reference identities,
authored incoming placements and the full candidate collision placement set.
Every visual part of a removed reference leaves; unchanged references retain all
existing parts. Unrelated/out-of-scope visual slots and native LOD records remain
owned by the live layout. Incoming parts are prepared only for newly selected
references, preserving the original model, position, rotation, scale and CELL.
Collision selection remains independent of visual filtering and NIF part count.

All placement deltas must belong to the complete affected-reference list. Invalid
or duplicate identities, nonfinite transforms, invalid scales, relocation/model/
ownership changes, already-live incoming references, and affected live references
missing from both placement scopes reject. This prevents treating a mismatched
residency window as a successful enable change. The selector leaves its output
unchanged on failure. Nonresident affected references without live visuals need
no visual slot changes; their persistent state still belongs to the outer plan.

Explicit candidate selection permits an intentionally empty placement result and
requires resolution of every active base identity. Ordinary CELL-load behavior
keeps its existing defaults. Deleted references remain excluded. This option
does not bypass missing CELL/master or base-resolution errors.

PrepareFo3ResidentCpuResources is the renderer's CPU worker entry point. Its
inputs are value-only scene/scope metadata, shared const reference projections,
captured visual identities and affected IDs. It neither reads Player/live GPU
vectors nor changes terrain, collision policy, collision caches, GL ownership,
saved state or queued effects. The current exterior residency selection is
explicit and independent of the fixed scene/GPU origin. The returned packet owns
the original request, projections, selection and decoded incoming CpuObjects.

CPU orchestration rejects cancellation, unsupported incoming visual policy,
failed decode and empty part results. Partial work stays private and output
changes only after success. Incoming decode uses a separate complete-mode cache;
it cannot reuse an ordinary permissive parse that omitted geometry. Complete NIF
mode rejects scenegraph fallback and failures in selected known geometry blocks.
CpuObject construction also rejects size/vertex-stream omissions in this mode.
Existing ordinary NIF/render callers retain their permissive default. This is
validation of the current supported NIF path, not every Gamebryo geometry type,
controller, material or texture semantic.

## Remaining runtime boundary

The coordinator must capture the actual current resident scope and visual epoch,
isolate/drain background asset work, then invoke CPU preparation. It still needs
to stage all textures/GPU parts, prepare collision with the correct candidate
model policy/current door poses, and coordinate lights, actor residency, held/
loose objects, navigation and LOD behavior. Retaining native LOD here is not proof
that arbitrary reference mutations can update baked LOD. Unsupported visual
policy or asset semantics must not be silently admitted by a future command.

Before publication, revalidate scene/Player/visual/collision/context stamps and
the outer script plan. Exchange all resources and logical state, or roll back,
in one render-thread turn before simulation, saves, effect delivery or eyes.
No production script/render path starts this worker entry point yet. GPU staging,
the scheduler barrier and the outer coordinator remain unfinished. Source
Enable/Disable, MoveTo, spawning and deletion remain rejected. No CELL reload is
substituted for resident publication.

## Validation

- Dedicated selector/worker tests cover retained multi-part slots, deterministic
  deltas, complete collision inputs, empty/no-op/incoming cases, unchanged output
  on malformed/ambiguous scope or transform changes, partial/empty decode,
  unsupported policy, cancellation before/during decode and thrown allocation
  failure. Candidate loaders test empty selections, unknown CELLs and incomplete
  active base resolution while preserving ordinary defaults.
- Original MS11DetonatorRef 0001E99F: the production ESM selection/planner retains
  the existing live slots and prepares the newly enabled authored model/pose and
  collision input. Original Common House multi-part NIF passes complete decode
  and CPU orchestration. Corrupting one selected geometry data block in memory
  leaves ordinary partial decode available but makes candidate preparation reject
  atomically; restoring the source bytes passes again. Original files are never
  modified or committed.
- Eight world, 29 player and one physics cases pass (38 core host cases). The
  NIF suite has ten passing no-argument cases and three original-data skips;
  the new original NIF case is run separately with the supplied model. Relevant
  scene/epoch/CPU-source and opaque/render-dispatch checks pass.
- Selector/placement tests, original ESM placement planning and original strict
  NIF/CPU cases pass ASan/UBSan with LeakSanitizer disabled in this traced runtime.
- Native Android compilation and physical Quest/GPU publication verification
  remain separate. No headset, performance or complete-engine claim is made.
  Save schema and app version remain v209.
