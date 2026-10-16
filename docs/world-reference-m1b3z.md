# M1B3z — reject direct incoming script/condition dependencies

Continues main 6c9f22a543f26e4fab6eabc97b7a81a7eb867708 (v212).
M1B3y Android CI succeeded (run 38958841210).

## Conservative dependency census

Resident authored admission's second streaming pass now reads every nondeleted
record payload, not only affected STAT/LIGH bases. It uses the existing strict
record/group/subrecord/XXXX boundaries, 64 MiB inflation limit and cancellation
checks. This includes quests, dialogue, packages, effects, unknown record types
and sources outside the resident CELL/worldspace. A record-type allowlist must
not hide an incoming dependency.

SCRO FormIDs that match any affected reference or its resolved base reject.
SCRV values remain local indexes, not FormIDs, even when the number happens to
equal an affected ID. Malformed SCRO/SCRV sizes fail closed. Repeated unrelated
SCRO entries are valid reference-table entries, not singleton-field errors.

CTDA must have the existing supported 20/24/28-byte layout. The guard compares
raw operands at +12/+16 and, when present, +24 against the same reference/base
set. With comparison-global flag bit 2 set, +4 is checked too; ordinary literal
comparison bytes are not treated as a global FormID. There is deliberately no
function-arity inference or supported-function exception. Numeric, unused or
stale raw operands can therefore false-positive. This narrows admission rather
than guessing that a potential dependency is irrelevant.

The first hit identifies field, source type/FormID, target and reference/base
classification. Malformed payloads identify the nearby source. Failure or
cancellation leaves the previous output unchanged. Deleted dependency sources
are skipped, but deleted affected references/bases remain rejected by the
existing identity guard. Existing package, incoming-link, XESP closure, base,
navigation and interior-only restrictions remain in force.

## No program-ownership exception

Master matching and this guard are independent necessary checks. A SCPT whose
captured body/container exactly match its original record still cannot exempt
its own SCRO edge. The integration regression verifies that case explicitly.
No gameplay caller dispatches general resident Enable/Disable, and no normal
dialogue/nonresident publication policy is changed.

This is a direct raw-metadata guard, not complete dependency ownership. It does
not resolve dynamic/local references, source-only names, implicit run-on/owner
contexts, transitive form lists, complete condition read-sets or event ownership.
It does not interpret bytecode or establish that matching numeric operands are
semantically FormIDs. Indirect/conditional ownership, bounded dispatch and
Quest validation remain required before general resident command admission.

The worker now inflates and walks all nondeleted payloads in its second pass.
There is no whole-master payload cache, but CPU/I/O cost has increased; this is
not a Quest timing or memory-budget claim. No renderer/draw-distance changes
are included.

## Validation

Synthetic cases cover reference/base hits across twelve source classes,
foreign scopes, compressed/extended data, every supported CTDA operand/layout,
comparison globals, unknown function arities, malformed sizes/framing,
deleted/unrelated sources and SCRV-versus-SCRO distinction. Outputs remain
unchanged on rejection. Test masters use per-process paths for parallel runs.
The master-match suite verifies that authentic provenance grants no exception.

The original-data test resolves an interior STAT/LIGH reference from real SCRO
targets and confirms its rejection with original source/target IDs, without
inventing a production placement. The original Brass Lantern light remains
admitted, while load doors, navmeshed geometry and incoming XLKR remain rejected.

All 53 core host tests plus the production actor runtime test pass. The affected
authored-admission and master-match targets pass ASan/UBSan with LeakSanitizer
disabled, including the original dependency fixture. Native/source and relevant
renderer/body/Pip-Boy checks pass; original dialogue bridge, barter and MS11
reward/save regressions retain their existing outcomes.
