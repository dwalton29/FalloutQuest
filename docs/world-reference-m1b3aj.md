# M1B3aj — original local-script declarations

Continues M1B3ai (`76cc8d85dafe1d1ade6470c2998c642a2c2d38c8`), whose Android
build passed at https://github.com/dwalton29/FalloutQuest/actions/runs/39064089108.

Captured entries now retain a canonical index-sorted vector of decoded local
variable index/name pairs from their actual catalog local script. Quest stage
entries with nonzero SCRI also retain the full SCPT container, as events already
did. This matters because NumericLocalKey reads source declarations to establish
numeric type support before resolving a decoded variable name/index. Matching
only the stage result body and QUST SCRI ID did not authenticate that context.

The existing second master scan now resolves each nonzero local-script ID to a
unique nondeleted SCPT with strict payload framing. Its full decoded ResultScript
and variable map must match every entry that cites it. Quest-only evidence with
no placed owner now performs this second scan when it has a local script. Shared
IDs are deduplicated; all entry comparisons and duplicate identity checks finish
before report publication. Record order and compression do not affect matching.
No third scan, whole-master catalog or execution of the local SCPT is introduced.

Both pieces of evidence matter: the full container preserves raw SLSD/SCVR and
source, while the explicit vector authenticates the normalized map actually used
by the catalog. Altering only that map now rejects even with an unchanged raw
container. A variable vector without a local-script ID rejects; vectors must
have strictly increasing indexes. Report comparison includes the vector and
remains allocation-free and linear in retained entries/string bytes. Unordered
map iteration order is never used for the retained representation.

The source map is charged before copying/sorting its entries. The shared helper
also charges the retained vector before master file access. Accounting includes
pair objects, names and terminators under the existing 16 MiB evidence bound;
the fixed vector object is part of ScriptExecution's entry size. Stage-local
containers are charged by the existing script-container budget. Failed local
charges preserve their input counter; failed captures leave live state/effects
unchanged and failed matches preserve the previous report. Missing catalog local
definitions reject captured preparation; capture-off behavior remains unchanged.
Allocator overhead/spare capacity and unrelated catalog/decoder allocations are
still outside the retained-value budget.

Tests cover names/indexes, missing/additional/reordered/duplicate indexes,
changed local-script IDs and full stage containers, unchanged raw event data
with altered decoded names, quest-only scans, compression, XXXX, either record
order, and missing/deleted/duplicate/wrong-type/malformed local definitions.
Capture tests cover canonical ordering, nested stage scopes, missing definitions,
and oversized variable names. Budget checks cover map/vector accounting parity,
exact fit, failure atomicity and rejection before opening a missing master.

Validation passes all 53 core host tests, the actor test, native/render source
guards and matcher ASan/UBSan (LeakSanitizer disabled). Against the retained
original ESM, MS11 stage/bomb/INFO provenance, Lucas dialogue acceptance and
production bridge, merchant barter and all three MS11 completion branches with
rewards/save-reload pass. Existing unsupported showmap diagnostics are unchanged.

This authenticates declaration metadata, not individual lookup resolution or
read ownership. Quest/form-name aliases, duplicate-name ambiguity, external
variable references, runtime values, condition outcomes and complete call/event
causality remain separate proof requirements. Existing decoder declaration
semantics are retained; no new variable layout/type execution is inferred.
There is no dependency exemption or general resident Enable/Disable caller.
Remaining ownership, bounded dispatch and device validation still gate that
path. Version 212 and existing gameplay/render behavior remain unchanged.
