# M1A — original reference enable-state queries

Built on M0 `2310bbe9028c583053d846bf762039a8ce1d1a1a`, version remains v208.
This checkpoint implements a read/query prerequisite of M1, not world mutation.

The player catalogue retains a sparse immutable XESP map for REFR/ACHR/ACRE,
including parents outside the current cell and children discarded by Pip-Boy
objective-target pruning. This avoids enlarging every placed Reference.
Malformed XESP remains a failing graph entry rather than becoming unparented.
XESP's 8-byte layout, opposite bit 0 and Pop In bit 1 are corroborated by
https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.5/Core/wbDefinitionsFO3.pas
(the wbXESP definition). Pop In does not invert the queried state.
The existing catalogue fingerprint, save version and mutable State are unchanged.

`Player::ReferenceDisabled` follows the complete parent chain, combining opposite
flags. The terminal parent's initial-disabled bit controls the result; an enable
child does not independently use its own initial-disabled bit. A 128-entry stack
bounds traversal without heap allocations. Missing/deleted parents, cycles,
unsupported flags and malformed records fail explicitly. This function is shared
by CTDA function 35 and source `[reference.]GetDisabled` expressions. It does not
cache state in a separate script-only store.

Tests cover unparented/parented/inverted/multilevel graphs, both observed flag
bits, child-own flags, missing/deleted/null references, malformed records, cycles,
excessive depth, nonresident reference contexts and failed-script rollback.
Original-data tests resolve all 5,502 children; check disabled MS11DetonatorRef,
enabled Burke, and Angela/Diego's inverted home-marker child. Existing original
bomb and Lucas acceptance/save-reload regressions remain required.

## M1B implementation boundary

Enable/Disable remain unsupported. No success is fabricated and no reference is
moved or materialized by this checkpoint. FQPS overlays and scene publication are
next. In particular:

- `fo3-cell-interior.inc`, `fo3-worldspace-runtime.cpp`, and the house placement
  loader currently discard initially disabled objects before base/model loading.
  Keep authored placement identity and transforms in immutable worker inputs.
- `Fo3SceneCpuPreparation` and `Fo3SceneCollisionPreparation` in fo3-runtime.cpp
  publish replacements together. Thread a committed mutation revision/snapshot
  into preparation; never read mutable Player state on the worker.
- `SetFo3CollectedCollisionRefs` destructively removes collision triangles. It
  cannot serve as a reversible disabled-reference set: enabling must restore
  authored collision and invalidate the proper acceleration structures.
- Preserve nonresident mutations and parent-descendant propagation. Existing
  actor registration deliberately rejects initially disabled/conditional ACHRs;
  actor enabling must coordinate saved ActorState and visual/AI arrival.
- Do not publish external effects during RunScript or dialogue preview. Stage
  changes can recurse and fail; callbacks that mutate the scene before the outer
  transaction commits would bypass the existing rollback system.
- Test visible/physical/interactive state, reload, streaming and failed nested
  stages before adding the mutation command compiler handlers.

No render/AI/collision settings, draw distance, animation quality, actor counts
or original binary assets change here. No headset performance claim is made.
