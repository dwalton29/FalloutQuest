# M1B3aq — qualified-variable binding chains

Continues M1B3ap (`43e7c537c9fde5f8886f022e13c42a4229b9427b`). M1B3ap and
M1B3ao Android builds passed (runs 39080827516 and 39080442018).

Qualified variable resolution previously bypassed the form-name sink. Captured
command compilation and expression preflight now retain successful named
VariableKey resolutions as separate ordered commandVariables/expressionVariables
vectors. Each value owns the resolved owner/script/index, quest-or-actor kind,
actor base where applicable, and normalized owner/variable names. The existing
expression fallback that synthesizes a quest-qualified name is included. Bare
NumericLocalKey and indexed condition-system lookups are not new trace paths.
Runtime expression evaluation still receives no sink, avoiding duplicate evidence.

The shared binding collector charges fixed variable-binding objects and both
strings/terminators before retaining them. Form and variable bindings consume
the same collector allowance; preflight subtracts command collection first.
Final entry accounting charges every binding vector plus existing evidence
under the shared 16 MiB limit. Fixed vector objects are in sizeof(ScriptExecution).
Vectors move into entries after charging, and failed collection cannot publish
live state/effects. Capture-off resolution, float storage and execution semantics
are unchanged; no reference-variable or integer-coercion behavior is introduced.

Master proof authenticates supplied chains using the existing first scan:

| Owner kind | Authenticated original chain |
| --- | --- |
| Quest | Named QUST, exact direct SCRI, unique live SCPT, numeric slot index/name |
| NPC placement | Named ACHR, exact NAME base, NPC_ direct SCRI, SCPT numeric slot |
| Creature placement | Named ACRE, exact NAME base, CREA direct SCRI, SCPT numeric slot |

Owner names join the existing all-class live EDID collision proof even if no
ordinary form bindings are supplied. All chain IDs are captured up front, so
owner/base/script records can occur in any order without another master pass.
Missing/deleted/duplicate/wrong-type records, changed associations, placement/base
type mismatches, malformed script framing and changed slot indexes/names reject
before report replacement. Index zero and complete repeated raw index pairs
remain legal; the effective decoder table retains last-assignment semantics.

External SCPT tables must have unambiguous names and consistent declarations.
The existing pre-Begin/comment-aware declaration classifier now optionally emits
its sorted categories once per script; each bound name must be numeric (short,
int, long or float), not ref or an unclassified extra slot. This avoids reparsing
the script source for every binding. Source grammar and integer storage/coercion
remain incomplete. Direct associations only are proven: template inheritance,
arbitrary reference variable owners and external alias mechanisms are not inferred.

Exact frame-time report comparison includes both vectors and all binding fields
without allocations or file access. Worker indexes use pointers only into the
owned immutable evidence for the duration of matching, never live Player state;
the published report copies values and retains no such pointers. Master payloads
remain reader-bounded, with record-level cancellation and additional per-slot
checks. Decoded external schemas, classification tables, collector capacity and
indexes are private preparation work outside retained-evidence accounting; no
whole-preparation memory, cancellation-latency or Quest timing guarantee is made.

Tests cover both phases, quest/NPC/creature chains, record order, compression,
XXXX names, repeated raw slots, index zero, reference/unclassified/ambiguous
declarations, malformed slots, missing/deleted/duplicate/wrong-type identities,
association/placement mismatches, every retained field, pre-file rejection,
exact-fit/failed/shared byte charging, explicit and synthesized quest lookup,
capture-off behavior and allocation-free commit/rollback.

All 53 core host tests, the actor test, five namespace-audit tests and native/
render source guards pass. Matcher and transaction fixtures pass ASan/UBSan
(LeakSanitizer disabled). Original captured MS11 stage/event/INFO evidence
passes strict matching with the new binding vectors, and the local policy still
accepts all 1,257 SCPTs. Lucas acceptance/production bridge, merchant barter and
all three MS11 completion/reward/save-reload branches pass. Existing unsupported
showmap diagnostics remain unchanged. The supplied ESM SHA256 was reverified as
`d9fb0a33af495ddb43992b96ea74f2741b123fefdb1fcdcea28096f7649b0d06`.

These are original-schema proofs for supplied successful resolutions, not an
independent trace completeness/phase derivation or full catalog-equivalence
proof. Failed lookups, runtime values/read ownership, condition-system reads,
implicit receivers, aliases and call/event causality remain open. Empty vectors
do not prove absence of variable access. No authored-dependency exemption or
general resident caller is added. Version 212 and current gameplay/render/VR
behavior are unchanged; full ownership, bounded dispatch and device validation
still gate general resident Enable/Disable.
