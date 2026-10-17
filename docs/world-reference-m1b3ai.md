# M1B3ai — decoded condition provenance

Continues M1B3ah (`8114392ccdebb991bfb303e0f4e24b1fe9845d92`), whose Android
build passed at https://github.com/dwalton29/FalloutQuest/actions/runs/39060759921.

Captured script entries now retain the decoded conditions attached to an entered
QUST stage item or INFO. INFOs additionally retain their catalog QSTI identity
and that owning quest's root conditions. These are owned value copies from the
immutable catalog; workers still receive no live Player. Capture remains opt-in.
Stage entries do not retain quest-root conditions: RunQuestStage evaluates the
item conditions, rather than the quest's dialogue eligibility conditions.

The master matcher compares item/INFO conditions in the first scan, alongside
the exact program. INFO owning-quest IDs must also match. The existing second
scan resolves parent quests and compares their root conditions for every INFO
phase that cites them. Shared parent IDs retain bounded entry-index lists;
there is no additional scan. Empty lists must match too. Events cannot claim
item or parent conditions; stage entries cannot claim INFO parent metadata.

Comparison includes condition order, flags, function, comparison value, both
parameters, run-on scope and reference. Float values compare by their bytes,
preserving signed zero and NaN payload identity without comparing struct padding.
This is decoded-field identity: CTDA framing still accepts the existing
20/24/28-byte layouts, and ignored padding/layout differences are not new raw
evidence. Metadata matching does not make nonfinite or unsupported conditions
executable.

Both condition vectors are charged against the shared 16 MiB retained-evidence
limit before condition copies into evidence and before master file access. The entry's fixed
size includes the new vector objects and parent ID. A failed charge leaves the
counter unchanged; a failed match preserves the previous report. Exact report
comparison includes every new field and remains allocation-free. Copying the
same parent conditions for multiple phases deliberately charges each copy.
Allocator overhead, spare capacity and other catalog/decoder work remain outside
this accounting.

Tests cover each decoded field, list deletion/addition/reordering, signed zero,
NaN payload changes, all three layouts, compressed records, either parent record
order, both INFO phases, incorrect parent IDs and inappropriate event/stage
metadata. Capture checks include nested stage calls, independent INFO phases,
commit/rollback, unchanged live state and both vectors exceeding the byte limit.

Validation passes all 53 core host tests, the actor test, native/render source
guards and matcher ASan/UBSan (LeakSanitizer disabled). Against the retained
original ESM (SHA256 d9fb0a33af495ddb43992b96ea74f2741b123fefdb1fcdcea28096f7649b0d06),
MS11 stage/bomb/INFO provenance, Lucas dialogue acceptance/save-reload and the
production bridge, merchant barter and all three MS11 completion branches pass.
Existing unsupported showmap diagnostics remain unchanged.

This authenticates gate metadata, not its truth or complete execution context.
It does not record evaluated values, condition operand bindings, runtime reads,
random samples, skipped stage items, event dispatch causality or complete call
edges. INFO transaction preparation still does not evaluate dialogue eligibility;
Session keeps its existing evaluation and begin/end timing. No dependency
exemption or general resident Enable/Disable dispatch is introduced. Remaining
ownership, bounded dispatch and device validation still gate that path. Version
212 and existing gameplay/render behavior remain unchanged.
