# M1B3aa — reject incoming original form-list membership

Continues main baaee75956b3e8a12c686f2b6edcf57f7b876db1 (v212).
M1B3z Android CI succeeded (run 38960311215).

## Potential list indirection

A script's SCRO table can identify a form list rather than an affected object.
Direct SCRO/CTDA operand matching therefore does not cover potential list-member
indirection on its own. The existing whole-master second pass now also checks
LNAM members of every nondeleted FLST record against the complete affected
reference/base set. A matching member rejects, and a malformed member size
rejects. Original FLST decoding already identifies these as four-byte FormIDs.
LNAM fields in other record types are not reinterpreted as list membership.

This deliberately rejects dormant/unconsumed lists too. It does not need to
discover every consumer, construct a reverse graph or decide whether a particular
Fallout function traverses nested lists. Any potential chain of original list
memberships ending at an affected ID has a final direct membership edge. The
whole-master scan rejects that edge regardless of list nesting, cycles, record
order or foreign CELL/worldspace context. Unrelated/self/cyclic memberships with
no affected reference/base are not rejected by this check alone.

Valid empty lists, repeated unrelated members, null members and deleted list
sources retain their existing admission policy. Record framing/inflation limits,
cancellation, failure-output preservation and prior navigation/link/package/
script/condition guards remain unchanged. Zero-length record payloads remain
fail-closed; an empty-list fixture still includes valid record metadata.

## No ownership exemption or new dispatch

The integration fixture uses a master-matched SCPT whose only SCRO target is a
list, then two nested/cyclic FLST records reaching the affected reference. The
independent authored guard rejects at the leaf membership while the provenance
report remains valid. Master matching is not dependency ownership.

This covers original FLST membership only. Runtime-mutated list contents,
dynamic/local references, other containers/aliases, implicit contexts and full
conditional ownership remain separate work. No bytecode is interpreted and no
general resident Enable/Disable gameplay dispatch is enabled. No renderer,
draw-distance, normal dialogue or nonresident publication policy changes occur.

The existing payload pass is reused; there is no extra full-master pass or
potentially unbounded graph traversal. There is still no Quest preparation-time
or memory-budget claim for that broader worker path.

## Verification

Host cases cover reference/base members, dormant lists, foreign scopes,
compressed/extended payloads, malformed members, deleted sources, unrelated
LNAM fields, empty lists, duplicate/null/self members, nested/cyclic metadata and
both record orders. The master-match integration verifies no list-indirection
exemption. Original Brass Lantern admission and the existing original door,
navigation, XLKR and SCRO rejection fixtures retain their expected outcomes.

All 53 core host tests plus the production actor test pass. Both affected
admission/master-match targets pass ASan/UBSan with LeakSanitizer disabled,
including the original-data admission suite. Relevant native/source and
renderer/body/Pip-Boy checks and original dialogue bridge/barter/MS11/door
regressions retain their expected results.
