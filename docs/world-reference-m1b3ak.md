# M1B3ak — unambiguous local declaration names

Continues M1B3aj (`7b615e10d1d51b3c2cd6a159bb256bd8ea8db416`), whose Android
build passed at https://github.com/dwalton29/FalloutQuest/actions/runs/39064993401.
Workspace maintenance removed the checkout/build caches; canonical main was
cloned again before this work. The newly available ESM matches the previously
verified SHA256 d9fb0a33af495ddb43992b96ea74f2741b123fefdb1fcdcea28096f7649b0d06.

Exact original declaration identity does not by itself make name lookup
unambiguous. NumericLocalKey and qualified variable lookup ignore name case and
scan a decoded map. Two different indexes with equal case-folded names could
therefore select an index according to map iteration order.

Captured preparation and master matching now share UnambiguousScriptLocals.
It rejects empty effective names and equal names after the same Lower helper
used by numeric local lookup. Names are normalized and sorted in a temporary
private vector; the retained declaration vector still has its existing exact
names and canonical index order. Distinct names such as Count and Counter remain
accepted. No new source identifier grammar, variable type or lookup fallback is
inferred, and capture-off execution keeps its existing behavior.

The policy runs after retained local-evidence charging, before capture copies
or master file access. Oversized declarations retain the existing byte-limit
failure. Temporary normalization/sort allocations are private preparation work,
not part of the retained-value accounting or frame-time report comparison.
The original 16 MiB evidence limit bounds the input charged before that work;
this is not a whole-preparation memory or Quest timing claim. Matches remains
unchanged and allocation-free. Failed preparation preserves live state/effects;
failed matching preserves the prior verified report.

An independent scan of the supplied ESM found 1,257 SCPT records, 867 with
nonempty decoded variable tables, and 4,032 SLSD subrecords, all 24 bytes. No
effective empty names or case-insensitive name collisions were found. Three
repeated SLSD indexes occur across two scripts. The new policy deliberately
operates on the effective decoded table and retains the decoder's existing
last-assignment behavior for those indexes; exact raw SCPT comparison is still
required by M1B3aj. This is not blanket rejection of repeated raw declarations.

Tests cover exact and case-only collisions, empty names, distinct prefixes,
both stage/event local scopes, compression and XXXX, canonical capture order,
failure before opening a missing master and original-style repeated indexes.
Capture tests compare captured and uncaptured behavior and verify unchanged
live revisions, state and effect queues after rejection.

Validation passes all 53 core host tests, the actor test, native/render source
guards and matcher ASan/UBSan (LeakSanitizer disabled). Against the supplied
ESM, MS11 provenance, Lucas acceptance/save-reload and the production bridge,
merchant barter and all three MS11 completion/reward/save-reload branches pass.
Existing unsupported showmap diagnostics remain unchanged.

This closes effective local-name ambiguity for admitted captured tables. It
does not prove source/table consistency for every declaration, quest/form-name
aliases, external variable bindings, runtime values, condition outcomes or
complete call/event causality. There is no dependency exemption or general
resident Enable/Disable caller. Remaining ownership, bounded dispatch and
device validation still gate that path. Version 212 remains unchanged.
