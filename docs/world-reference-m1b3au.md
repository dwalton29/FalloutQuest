# M1B3au — source global targets and expression bindings

Continues M1B3at (`f8b6eaf8b3c2d14660166653fac215d9ac3f88fc`).

Entered-program capture now retains commandGlobals and expressionGlobals using
the same (FormID, immutable catalog-default float bits) representation as the
condition global vectors. After command compilation, every command classified
as a global assignment contributes its resolved target, including unreachable
assignments. Successful global operands in all-expression preflight contribute
their resolved identities/defaults in lookup order, including repeated accesses
and unreachable expressions. Existing local/qualified-variable precedence is
unchanged. The actual runtime expression pass receives no sink.

Global defaults come from immutable Catalog.globals, while evaluation continues
to use Player::GlobalValue for mutable globals and canonical clock values. The
captured default therefore need not equal the value evaluated or assigned.
Capture-off behavior is unchanged. Opted-in provenance rejects missing/nonfinite
defaults and unresolved global destinations even in unreachable assignments;
those destinations cannot receive an authenticated global identity.

The existing bounded compiler/preflight collector now includes a fixed-pair
global sink. It charges before append, shares its allowance with form/local/
qualified-variable bindings, and resets all vectors together. Assignment targets
consume the command allowance before expression preflight receives the remainder.
Final evidence accounting charges both vectors alongside all prior fields under
the same 16 MiB retained-value limit; vector objects are in sizeof(ScriptExecution).
Failed private preparation publishes no state or effects.

Before file access, every supplied source global must be nonnull and finite and
have a successful form lookup in its own command/preflight phase. Defaults must
agree with every other source or condition claim for the same ID. Existing EDID
namespace proof authenticates the named resolution, and the existing first master
scan requires the unique live numeric GLOB and exact FLTV default bits. Strict
FNAM/FLTV framing, compression, XXXX, signed zero and failure atomicity remain as
in M1B3at. No additional master pass is added. Exact allocation/file-free report
comparison includes both ordered vectors.

Same-phase form presence is not independent source token association, execution
phase derivation or completeness. Empty/shortened vectors do not prove absence
of source accesses. These are original identity/default proofs for supplied
bindings, not runtime read/assignment values, outcomes, numeric coercion, all
implicit receivers or event/call causality. Temporary phase indexes, collector
capacity and decoding remain private preparation costs outside retained-value
accounting and whole-preparation/device latency claims.

Validation results are recorded in SCRIPTING_COMPLETION_STATUS.md. Version 212
is unchanged; no dependency exemption or general resident gameplay caller is
added. Full ownership, bounded dispatch and device validation still gate general
resident Enable/Disable.
