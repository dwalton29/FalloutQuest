# M1B3y — match captured programs against the installed master

Continues main 71ca0af09e42a9fde21e93514f477d81a797e647 (v212).
M1B3x Android CI succeeded (run 38916458849).

## Evidence checked

MatchScriptProgramsToMaster accepts owned execution evidence, not a live Player.
It streams the complete master record/group headers, decoding only the records
claimed by that evidence. Every claimed FormID must occur exactly once, have
the expected QUST/INFO/SCPT type, and not be deleted. Scanning continues after
the first match so a later duplicate or broken record/group boundary rejects.
Missing records, unbound bodies and malformed origin fields fail closed.

- QUST: match the canonical stage and item ordinal and exact result fields.
  Numeric-local scope must equal that QUST's authored SCRI association (which
  need not be zero). Decoder diagnostics reject malformed/orphan quest data.
- INFO: match canonical topic, INFO ID and begin/end ordinal. A missing TPIC
  may use its actual enclosing topic group, following the gameplay decoder.
- SCPT: match the full original containing script and the normalized event plus
  Begin line. Reparse that container and match the exact extracted body; a copy
  of unrelated source text is not accepted as that event.

All ResultScript source, header, compiled bytes, decoded reference values and
ordered raw records are compared, including SCRO versus SCRV. SCPT container
metadata is compared independently from the entered subsection. Empty metadata
is not synthesized or ignored. Unknown compiled instructions are not executed.

The scanner bounds group nesting at 128 and inflated records at 64 MiB. It
validates complete relevant subrecord/XXXX framing, script scalar uniqueness,
SCHR/reference field sizes and INFO phase/topic delimiters before the tolerant
gameplay decoder. Cancellation is checked between records and before output.
Failure leaves the caller's prior report unchanged; native preparation uses a
fresh private report and does not consume a failed result.

## Native integration and binding

The reserved worker receives a value copy of the exact plan's execution list.
Original-master matching precedes authored world-reference admission, placement
selection, NIF, texture and collision work. The packet owns the successful match
report. Private GPU admission requires a verified report; commit preflight and
settlement require it to match the exact ordered plan evidence, including
owner/activator/local-script context. Report comparison does not allocate or
read files; it does compare the captured bytes. Commit/rollback still do not
perform filesystem work or swap provenance into saved state/effect queues.

This adds a complete header scan, relevant record inflation and evidence copies
on the worker path. It is not a measured Quest time/memory budget. Render-thread
evidence comparison is linear in the captured bytes, not a constant-time claim.
General resident Enable/Disable is still not dispatched by gameplay callers.

## Deliberate limits

This proves that captured program bytes match their claimed installed-master
origins. It does not cryptographically certify the entire installed file, lock
it against external edits, prove condition/read-set ownership, authenticate an
event owner's SCRI association or activator against the ESM, interpret compiled
references, or exempt any script from the authored dependency guard. Only the
claimed program metadata is matched for QUST/INFO, not every condition, response
or other record field. Unrelated record payloads are not fully decoded here.
Complete SCRO/condition/event ownership and bounded dispatch/device validation
remain separate, required work.

## Validation

The new host test covers exact matching and mismatch of each program/container
field, wrong origins, event lines, phases and topics, missing/deleted/wrong-type
and duplicate identities, repeated legitimate entries, report order/context
binding, captured input limits, cancellation and unchanged output on failure.
Compressed and extended records and enclosing INFO topic fallback are accepted;
truncated records/groups, corrupt compression, malformed XXXX/scalar/reference
fields and excess nesting reject. The production transaction suite checks the
real MS11 stage, bomb event and Lucas INFO fragments together against the verified
original master, checks allocation-free report comparison and rejects altered
source without publishing canonical state or effects.

All 53 core host tests and the production actor runtime test pass. Both affected
matching/transaction targets pass ASan/UBSan (LeakSanitizer disabled), including
original-master transaction matching. Native source guards, relevant renderer,
body/Pip-Boy checks and original dialogue/bridge/barter/MS11/door regressions pass.
