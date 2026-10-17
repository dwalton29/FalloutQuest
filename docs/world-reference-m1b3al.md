# M1B3al — declared local-slot consistency

Continues M1B3ak (`b2b195ecc8dd91c66fcb6bd4c75fd5fffbb350ad`), whose Android
build passed at https://github.com/dwalton29/FalloutQuest/actions/runs/39069861040.

Bound captured preparation and master matching now check recognized source
declarations against the effective decoded local table, after byte charging
and name uniqueness. A recognized declaration must have an operand and a
case-insensitive table slot. One source name cannot be classified as both
numeric and reference before the first Begin. Exact original script/table
identity alone did not establish these properties.

ConsistentScriptLocalDeclarations follows the existing NumericLocalKey scope:
strip semicolon comments, compare case-insensitively, read declaration keywords
and their first operand, and stop at the first Begin. Short/int/long/float remain
the existing numeric category; ref is recognized only to reject conflicts and
is not enabled for execution. Duplicate declarations of the same category are
accepted. Other preamble operations and remaining declaration tokens are not
given new execution semantics. Event bodies are not scanned as declarations.

An independent original-data scan found no recognized declaration without a
decoded slot, no numeric/reference conflict and no missing declaration operand
across 1,257 SCPTs. Two scripts have extra decoded names relative to recognized
header declarations; two have repeated source names. The policy therefore
preserves extra effective slots and repeated declarations of the same category.
It does not invent a blanket one-to-one source/table rule. A new original-data
regression checks the charged local policy against every decoded SCPT.

The matcher uses its charged retained container source and table before opening
the master; original full-container/table equality still follows in the existing
scans. Typed capture checks the already charged full local container before
copying evidence. Raw unbound bodies have no authenticated container and remain
unadmitted by master matching. No extra master scan or retained field is added.
Failures preserve the previous report and live state/effects. Capture-off
execution and allocation-free frame report comparison remain unchanged.

The check uses temporary normalized name/category pairs, source streams and a
line buffer during private preparation. Their input source/table bytes are
charged first under the existing 16 MiB retained-value limit; those temporaries,
allocator overhead and other preparation work are not included in that retained
accounting. This adds preparation CPU/allocation cost, not a Quest timing or
whole-preparation memory claim.

Tests cover missing slots/operands, either numeric/reference declaration order,
case/comments/CRLF, duplicate numeric and reference declarations, extra slots,
empty source, first-Begin scope, stage/event origins, compressed records and
rejection before opening a missing master. Captured rejection leaves live
revisions, state and effects unchanged; uncaptured preparation still works.

Validation passes all 53 core host tests, the actor test, native/render source
guards and matcher ASan/UBSan (LeakSanitizer disabled). The charged local policy
accepts all 1,257 decoded SCPTs from the verified supplied ESM. Original MS11
provenance, Lucas acceptance/save-reload and the production dialogue bridge,
merchant barter and all three MS11 completion/reward/save-reload branches pass.
Existing unsupported showmap diagnostics remain unchanged.

This establishes consistency only for the recognized declaration subset. It
does not infer full identifier/type grammar, classify every extra decoded slot,
validate every source statement or prove individual lookup resolution, aliases,
external variable bindings, runtime reads/outcomes or call/event causality.
There is no dependency exemption or general resident Enable/Disable caller.
Remaining ownership, bounded dispatch and device validation still gate that
path; version 212 remains unchanged.
