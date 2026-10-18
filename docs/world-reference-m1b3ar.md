# M1B3ar — bare local-variable bindings

Continues M1B3aq (`357e0b3240f62ed3e2dd9d1f8136c8c8a357444d`), whose
Android build passed (run 39081584682).

Captured command compilation and all-expression preflight now retain successful
bare NumericLocalKey resolutions in separate ordered commandLocals and
expressionLocals vectors. Each pair owns the resolved numeric index and
normalized token; the containing entry already owns its local-script and local
owner identity. Zero indexes, repeated accesses and unreachable commands are
retained. The existing quest-qualified expression fallback still takes precedence
and uses qualified-variable evidence when successful. Runtime expression
evaluation receives no sink; capture-off local resolution uses the same resolver.

The shared collector charges the pair object, normalized name and terminator
before retaining a value. Form, qualified-variable and bare-local collections
share one allowance. Preflight subtracts command collection, and final entry
accounting charges both new vectors under the existing 16 MiB retained-value
limit. Fixed vector objects are included in sizeof(ScriptExecution). Failed
collection cannot publish live state or effects, even if a global fallback exists.
Temporary resolver strings, collector capacity and declaration indexes remain
private preparation work outside this accounting.

Before opening the master, matching checks every supplied bare binding against
the canonical index-sorted local table and pre-Begin numeric declaration
classification. Empty, mixed-case, dotted, embedded-NUL, unknown, reference and
unclassified names reject. Numeric categories are classified once per entry.
Existing original-master proof then authenticates the exact SCPT container and
decoded table, QUST SCRI or placed reference/base SCRI chain, and original result
or event body. No additional master pass or EDID lookup is required for a bare
name. Missing/deleted/duplicate/wrong-type identities and changed associations
remain rejected by that proof. The old report survives any failure.

Exact report comparison includes both ordered vectors without file access or
allocation. This binds an accepted report to the exact captured values and phase;
the matcher does not independently derive phase, token association or completeness
from the source. Empty vectors do not establish absence of local access. Failed
lookups, runtime read values/outcomes, indexed condition reads, implicit receivers,
aliases, integer coercion and event/call causality remain open.

Validation results are recorded in SCRIPTING_COMPLETION_STATUS.md. No authored
dependency exemption or general resident gameplay caller is added. Version 212
is unchanged. Complete ownership, bounded dispatch and device validation still
gate general resident Enable/Disable.
