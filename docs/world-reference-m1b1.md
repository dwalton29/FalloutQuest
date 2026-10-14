# M1B1 — persistent enable overlays and owned snapshots

Starting canonical main: 6469dbb872889a79a0682501b75f6ac6577ab547 (v208).

Historical format note: this file describes the M1B1/v208 checkpoint. Newer
v209 writes RFE1 plus PST1 for attribute overrides while retaining baseline v14; see world-reference-m1b2.md and
player-attributes-save-v209.md for the current format.

This is a backend checkpoint of M1B, not a completed Enable/Disable command.
No source command handler is admitted yet. Scene publication and actor state
must be connected before scripts can claim real world effects.

## Canonical state

State owns sparse referenceEnabled overrides and a rollback-compatible world
revision. StageReferenceEnable validates the complete graph through the M1A
query, changes root state, propagates inversion through existing queries, and
normalizes a return to the original state by removing the override. Valid parent-
controlled children cannot acquire an independent override; original requests
on those children are no-ops. Missing/deleted/malformed/cyclic references reject.
No immutable ESM definitions are modified.

CaptureReferenceState produces an owned value snapshot, projecting changed
roots and all XESP children. Unresolvable children carry Unavailable and select
as disabled. Unlisted references use the loader's authored state. The snapshot
contains no Player, render, physics, GL, or callback pointers. It cannot change
when the originating Player later changes.

## Save compatibility

An optional versioned RFE1 extension follows the FQPS v14 lock tail: tag, bounded
count, then sorted (FormID, enabled byte) entries. Empty overlays leave existing
v14 save bytes unchanged. All previously supported v1–v14 saves remain readable.
The old application cannot read a nonempty new extension; it rejects the save
rather than discarding or replacing it. Unknown future extension tags reject.

Restore validates size, count, boolean encoding, root identity, deleted flags,
parent-controlled references and duplicates before committing any state. Its
world revision invalidates a prior snapshot after successful restore. Failed
restore leaves state and revisions unchanged. Save uses the existing checksum,
fsync, temporary-file and rename path. The catalogue fingerprint is unchanged.

## Validation

- All 27 player CTest cases pass, including legacy save migrations.
- New tests cover root changes, authored no-ops, parent control, query propagation,
  immutable snapshots, save/reload, legacy loading, failed dialogue transactions
  preserving the overlay, preview restoration, malformed booleans/counts/IDs,
  duplicate IDs, child overrides, truncation and unknown extension tags.
- Original master checks stage and save/reload MS11DetonatorRef and the original
  Angela/Diego home marker, including its inverted child. All 5,502 children
  resolve, and original bomb/disarm/Karma/notice regressions still pass.
- No script Enable/Disable execution, live scene mutation, actor enable, APK,
  headset correctness or performance improvement is claimed by this checkpoint.

Next: supply immutable snapshots to interior/exterior placement and light
selection before model/collision preparation, then implement live publication,
streaming revision invalidation and actor coordination before command admission.
