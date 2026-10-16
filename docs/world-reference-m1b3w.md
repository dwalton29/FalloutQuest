# M1B3w — detached script-program provenance

Continues canonical main 73510a4983f0bef0872dff672db99fc831170f5a (v212).
M1B3v Android CI succeeded (run 38912030225).

## Evidence before ownership exceptions

The detached script plan previously retained its projected state/effects and
reference changes, but not the exact programs that produced them. A future
dependency scan must not exempt a script merely because a caller supplied a
record number or source text. It first needs immutable evidence of the entered
program bodies and their actual execution context.

PrepareDialogueTransaction, PrepareQuestStageTransaction and
PrepareReferenceEventTransaction now accept optional EnteredPrograms capture.
It is off by default, preserving ordinary dialogue preview and GameMode paths
without copying full program records on every tick. HasProgramProvenance
distinguishes an explicitly captured no-op from an uncaptured plan.

The native resident workflow now requires captured provenance at Current and
commit preflight. This is only a necessary guard, not proof that every recorded
program owns its referenced world changes. Ordinary canonical/nonresident paths
retain their existing policy and default capture mode.

## Recorded identities

The plan exposes a const ordered list of entered programs:

- QUST stage results identify the quest record, stage number and authored item
  ordinal, including nested SetStage calls in their actual entry order.
- Reference events identify the SCPT record, normalized event and Begin source
  line. They retain the entered body and the original containing script's
  source, header, compiled bytes, reference values and raw subrecords.
- Dialogue begin/end results remain explicitly UnboundResult, with fragment
  ordinal 0/1. Their existing API accepts result bodies, not an authenticated INFO
  identity; no INFO form ID is invented from text equality.

Each entry preserves owner, activator, numeric-local script identity and the
exact ResultScript data supplied to the interpreter. Numeric-local scope is
separate from the authored QUST/SCPT origin. Raw records preserve SCRO versus SCRV
and other metadata; capturing them does not interpret compiled bytecode.

Only entered bodies are recorded, after compilation and block validation. An
already visited nonrepeatable stage enters no program. Skipped stage-item
conditions are not a complete read-set, and unexecuted branches are not asserted
to have run. The complete body is retained for later conservative analysis.

## Lifetime and publication

The collector is installed only on the private candidate while preparation runs.
A scope guard clears it on successful return, failure or exception; it is not
inherited from the live Player and is never swapped into canonical state.
Nested SetStage execution uses this same collector. A nested detached preparation
from an actively captured candidate fails explicitly instead of silently losing
provenance. Existing stage depth/instruction budgets are unchanged.

The immutable list remains owned by the exact script plan across asynchronous
preparation, commit, rollback and settlement. Publication/rollback still perform
only their existing allocation-free state/effect swaps. Provenance is neither
saved in the Player file nor delivered as gameplay effects. It is not a mutable
Player pointer or a worker callback.

## Validation and limits

All 52 core host cases pass, plus the production actor runtime test. The real
canonical transaction suite checks nested quest/event ordering, quest stage/item
and SCPT event/line identity, scoped activator/owner values, original container
metadata, raw reference records, distinct unbound fragments, independent deep
copies and captured-empty versus uncaptured results. Audit storage remains
stable through commit/rollback with allocations forbidden. The real-script
workflow harness rejects uncaptured plans without taking ownership and exercises
captured plans through its existing commit/recovery/finalization matrix.
The affected target passes ASan/UBSan with LeakSanitizer disabled.

Original MS11 is resolved by its authored EDID. Its stage-10 plan and the
existing original bomb activation fixture produce provenance matching the
catalogue's source/compiled/header/reference/raw records without changing live
state or delivering effects. Original Lucas/nonresident/barter dialogue,
merchant trading and MS11 reward/save regressions pass. Existing unsupported
showmap diagnostics remain. Native provenance guards and relevant body/skin/
Pip-Boy/shoulder/opaque/dispatch/notification checks pass.

This checkpoint does not yet match provenance against a strict master-wide
script/condition dependency index, bind dialogue fragments to canonical INFO
identity, or authorize an originating script to bypass dependency checks.
Those are the next admission tasks. General resident Enable/Disable still has
no gameplay caller. Remaining platform/device validation and broader
navigation/LOD/exterior/interactive ownership stay open. No device or performance
claim is made; existing v212 gameplay remains preserved.
