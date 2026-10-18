# M1B3at — condition global identities and authored defaults

Continues M1B3as (`059dc2c78a54f5918dd4914b0efb6de0e08bd5d3`).

Entered-program evidence now includes conditionGlobals and questConditionGlobals.
For each retained condition, function 74 (GetGlobalValue) contributes its a
operand first, and flags&4 contributes the comparison FormID encoded in value
second. Repeated accesses remain repeated; each list preserves authored order.
These dependencies cover entered item/INFO gates and retained INFO-parent gates,
not skipped stage items or every possible condition operand.

Each owned pair holds a FormID and the bit-exact finite default from immutable
Catalog.globals. It does not call Player::GlobalValue, which may return a saved
override or canonical clock value. Runtime condition evaluation remains unchanged.
Capture does not evaluate INFO eligibility or condition outcomes. Signed zero is
preserved, while missing/null/nonfinite catalog defaults reject opted-in capture.

The collector charges each fixed pair under the existing shared 16 MiB evidence
limit before appending. Counts use division-before-multiplication and failed
charges preserve the running total. Existing entry, body, condition, local and
variable evidence consumes the same allowance; fixed vector objects are already
included in sizeof(ScriptExecution). Rejection cannot publish private state or
effects. Allocator capacity, temporary maps and reader payloads remain separate
preparation costs, with no total-memory or device-latency guarantee.

Before file access, matching derives global access order directly from retained
CTDA fields. Missing, extra, reordered, null or nonfinite bindings reject, as do
conflicting defaults for the same ID across either list or multiple entries.
The existing first master scan then requires a unique nondeleted GLOB for every
claimed ID. Payload framing must be complete, including XXXX handling. Exactly
one one-byte FNAM with a supported numeric type (s/l/f) and exactly one four-byte
finite FLTV are required. Captured default bits must match FLTV, including the
sign of zero. Compression and record order do not change the proof. Numeric-ID
access does not require EDID name resolution or another master scan.

The original condition fields and INFO-parent fields remain authenticated by
their existing record proof. Exact allocation/file-free report comparison now
includes both global vectors. Failure preserves the previous verified report.
This establishes potential dependency identity and original defaults; it does
not establish the current runtime value, numeric storage/coercion behavior,
gate eligibility, event/call causality or full catalog equivalence. Source lookup
completeness, other condition operands, activation overlays, implicit receivers
and runtime read ownership remain open.

A read-only metadata audit using tools/dialogue/audit_esm.py's record iterator
found 155 live globals in the supplied ESM: 139 short, 14 float and 2 long. All
had exactly one supported FNAM and one finite four-byte FLTV. Across all record
classes, 887 CTDA query/comparison global accesses referenced existing global
IDs. This metadata audit does not prove runtime reads or extend admitted gates.

Validation results are recorded in SCRIPTING_COMPLETION_STATUS.md. Version 212
is unchanged. No dependency exemption or resident gameplay caller is added;
complete ownership, bounded dispatch and device validation still gate general
resident Enable/Disable.
