# M1B3ao — expression-preflight form bindings

Continues M1B3an (`844e6a2518d69d3e86e516acc4d815b4e5bee879`). M1B3am's
Android build passed (run 39079219216); the M1B3an build was still running at the
initial recheck. General resident Enable/Disable remains gated.

The existing all-expression preflight can resolve form names even when the
command compiler itself resolves none. Captured preparation now retains these
successful lookups separately in ScriptExecution::expressionForms, in evaluator
lookup order. The shared evaluator's form resolver covers quest queries, player
item counts, named reference queries and the global-value fallback. Case folding
is still the evaluator's existing token normalization. No new expression or
gameplay semantics are enabled.

Collection occurs only during the existing preflight, including expressions in
unreachable commands and both operands of the current non-short-circuit logical
operators. The execution pass supplies no sink and does not duplicate this
evidence. The vector records binding identities, not returned values, branch
outcomes, a runtime read sequence, or proof that a branch was executed. Local
and qualified-variable lookup paths, implicit receivers, Player/actor-value
built-ins and condition-system reads remain outside this form-name sink.

The shared bounded sink checks pair objects plus name bytes/terminators before
copying. Its allowance is the transaction's remaining evidence bytes minus
already collected command bindings. Final entry accounting charges both vectors
alongside program/container/conditions/local data under the common 16 MiB limit;
fixed vector storage is in sizeof(ScriptExecution). Both vectors move into the
entry after charging. A failed preflight, including an exhausted collector,
rejects without publishing live state/effects or partial evidence.

Master matching charges and validates both supplied vectors before opening the
file, then authenticates their targets together in the existing first scan.
Every supplied ID must identify one nondeleted record with one complete matching
EDID under the existing strict generic framing rules. Targets shared across
phases are scanned once for this purpose. Exact report comparison retains each
phase and ordering independently, without allocations or file access. No extra
master scan, dependency exemption or gameplay caller is introduced.

Tests extend the binding fixture matrix to both phases, shared targets, record
order, compression, XXXX, missing/deleted/duplicate/misnamed records, malformed
EDIDs, oversized evidence and pre-file rejection. Preparation tests cover
stage/event origins, unreachable queries, logical operands, repeated quest
queries, named receivers, globals and item counts; capture-off behavior and
allocation-free commit/rollback are preserved. A deliberately oversized global
name rejects bounded collection while uncaptured evaluation still prepares.

All 53 core host tests, the actor test and native/render source guards pass.
Matcher and transaction fixtures also pass ASan/UBSan (LeakSanitizer disabled),
including the new preflight collector and oversized-name rejection. Original
captured MS11 stage/event/INFO evidence passes strict master matching with both
binding phases. All 1,257 SCPTs still satisfy the local declaration policy.
Original Lucas acceptance/production bridge, merchant barter and all three MS11
completion/reward/save-reload branches pass; existing unsupported showmap
diagnostics are unchanged. The ESM SHA256 was reverified as
`d9fb0a33af495ddb43992b96ea74f2741b123fefdb1fcdcea28096f7649b0d06`.

These are authenticated supplied bindings, not an independent completeness or
phase-recompilation proof. Same-name EDIDs on other IDs, target type suitability,
external variable/read ownership, aliases, runtime values/outcomes and call/event
causality remain open. Empty supplied vectors do not prove that no names were
resolved. Collector capacity, normalization/tokenization, matcher indexes and
other private preparation work remain outside retained-value accounting; no
whole-preparation memory or Quest timing guarantee is made.

Version 212 / 0.74.1-moira-barter and current gameplay/render/VR behavior are
unchanged. Full ownership, bounded dispatch and device validation still gate
general resident Enable/Disable.
