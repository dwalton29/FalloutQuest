# M1B3x — canonical dialogue INFO program identity

Continues canonical main 29928e386e172a5124787450023351e07a83fe2d (v212).
M1B3w Android CI succeeded (run 38914405926).

## Canonical lookup, not caller claims

PrepareDialogueInfoTransaction accepts a topic ID, INFO ID and explicit Begin,
End or Both phase. It resolves the original bodies from the Player's immutable
catalogue. Callers cannot attach an INFO identity to arbitrary result bodies.
The legacy raw-body factory remains explicitly UnboundResult.

Player catalogue construction rebuilds a derived ID-to-topic/ordinal index.
It stores values, not pointers into copyable/sortable INFO vectors. Supplied or
copied indexes are discarded. Duplicate INFO IDs anywhere in the catalogue,
zero identities and mismatched topic membership fail closed. Lookup does not
scan all dialogue records during every eligibility preview.

Captured entries identify DialogueInfo, canonical INFO record, topic and fragment
ordinal (begin 0, end 1), with exact source/header/compiled/reference/raw-record
bytes. Nested QUST results keep their own identities and actual execution order.
INFO decoding now preserves fragment-local SCHR/SCDA/SCTX/SCRO/SCRV/SLSD/SCVR
subrecords across NEXT, including empty/malformed raw fields. Previously INFO
kept only decoded fields. Existing decoded-reference/execution semantics are
unchanged; preserving malformed metadata does not admit it.
Plans retain independent program copies through allocation-free commit/rollback.
The factory proves identity, not dialogue eligibility or dependency ownership.

## Production dialogue path

Session eligibility resolves Both phases without capture, retaining bounded
transactional preflight without full program copies on every preview. Actual
begin/end publication resolves only the corresponding canonical phase and
captures its entered programs. End scripts still wait for the last response;
the begin transaction does not execute the end body or consume its budget.
The existing reference preview/publication callbacks and rejection policy remain.

The index and provenance are not saved gameplay state or delivered effects.
General resident Enable/Disable dispatch remains disabled. This checkpoint does
not establish strict original-master program matching, complete SCRO/condition/
event ownership, bounded dispatch cost or device validation.

## Verification

The canonical transaction tests cover separate/combined phases, nested stage
ordering, exact fragment metadata, immutable copies, capture-off behavior,
allocation-free commit/rollback, changed/reordered catalogue copies, invalid
phases, wrong/missing IDs, same/cross-topic duplicate IDs, malformed membership
and atomic rejection after a successful begin and failed end. Original MS11
dialogue provenance is checked against the verified original master alongside
the existing stage/bomb provenance fixtures. Existing dialogue lifecycle,
production bridge, barter and MS11 reward/save regressions protect phase timing.
The Session publication callback test observes exactly one correctly bound
fragment per phase, checks no early end effect, side-effect-free combined preview,
and rejection of stale/repeated audio completions.

All 52 core host cases and the production actor runtime test pass. The affected
canonical transaction target also passes ASan/UBSan against synthetic and
original ESM fixtures with LeakSanitizer disabled. Native/source and relevant
renderer/body/Pip-Boy checks pass. The INFO parser fixture verifies exact ordered
raw metadata on each side of NEXT, including empty/malformed fields, without
changing decoded SCRO-versus-SCRV behavior.
