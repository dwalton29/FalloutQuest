# M1B3v — incoming authored reference edges

Continues canonical main 321502777d182dfea5404ffcf89e07d363872639 (v212).
This extends the necessary M1B3s admission guard, not gameplay command dispatch.

## Reverse ownership check

Checking only attachments on affected references missed incoming dependencies.
An actor outside the resident CELL can have an XLKR to a changed reference;
an XESP child outside the supplied change set can depend on a changed parent.
Neither necessarily appears in selected or live model placements.

The existing first worker scan now reads every nondeleted REFR, ACHR and ACRE
payload, regardless of CELL/worldspace, enabled state or mesh residency. It
validates complete subrecord framing/decompression before reading the known
incoming XLKR, XESP, XTEL and XEMI fields. Known-link duplicate fields and
unsupported lengths fail closed, including on otherwise unaffected references.
The affected-record parser reuses that payload rather than reading it twice.

XLKR/XTEL/XEMI targeting an affected reference is rejected as an unowned incoming
edge. This does not assume that a placed LIGH legitimately drives XEMI:
the existing renderer resolves XEMI through base LIGH/REGN data. An XEMI targeting
a changed placed reference remains unadmitted rather than being silently ignored.
No links or door prompts/routes are rewritten.

Every nondeleted XESP child whose direct parent is affected must itself be in
the complete affected set. Children within that set still undergo the original
strict CELL, reference/base-kind, metadata, navigation and resource guards.
This is intentionally conservative: an independent child override is not used
to bypass the authored closure check. Initially disabled sources remain checked;
deleted sources do not create live edges.

Failure diagnostics identify the incoming field, source record/type and target.
Cancellation or any rejection preserves the caller's previous admission report.
Successful reports retain the existing CELL/complete-list binding at worker,
GPU, commit and finalization boundaries.

## Original evidence and cost

The verified original master contains 4-byte XLKR/XEMI, 8-byte XESP and 32-byte
XTEL subrecords. The existing 28/32-byte XTEL reader remains supported.
An original incoming XLKR target in an interior STAT/LIGH reference is resolved
from the records and rejected by the new check; no fixture form IDs are invented.
The existing Brass Lantern light still clears the necessary guard, while load
doors and navigation-dependent static geometry remain rejected.

The first scan now decodes all nondeleted placed-reference payloads, not just
affected references plus NAVM/PACK. It still streams one bounded record at a
time and does not build a whole-master reference catalogue. Cancellation is
checked between records. This is private CPU work under the reservation, not
frame-thread I/O, but its increased Quest preparation cost has not been measured.
No latency or memory-performance improvement is claimed.

## Validation and remaining boundary

All 52 core host cases pass, plus the production NPC runtime test. The authored
suite adds REFR/ACHR/ACRE incoming links in the same CELL, another interior and
another worldspace; every known link type; malformed lengths; duplicate fields;
compressed payloads and XXXX extended framing. Unrelated links and deleted
sources are checked as nonblocking cases. A complete same-CELL XESP pair clears
the guard; omitting its child or placing that child in another CELL fails closed.
Reports remain unchanged on rejection. The affected target passes ASan/UBSan
with LeakSanitizer disabled.

Verified-original authored link and light/door/navigation fixtures pass, as do
load-door exchange, Lucas/nonresident/barter dialogue, merchant trading and MS11
reward/save regressions. Existing unsupported showmap diagnostics remain.
Native guard ordering and relevant Pip-Boy, shoulder, body/skin, opaque,
render-dispatch and notification-layering checks pass. Android validation is
separate; no device/OpenXR or performance result is claimed.

These are known placed-reference edges, not a complete script/condition/event
dependency graph. Script-reference ownership, other record-family dependencies,
general navigation/LOD mutation, remaining platform handling, bounded command
dispatch and device validation remain open. No general resident Enable/Disable
caller is enabled. Existing v212 gameplay and bounded nonresident actor Disable
remain preserved.
