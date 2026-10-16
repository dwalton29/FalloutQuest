# M1B3s — authored-reference and dependency rejection

Continues canonical main d5e97daecb85eae0c9f342a4c7fec43cd1f84498 (v212).
Both M1B3r and the preceding libc++ assertion fix passed Android CI.

## Early guard

The immutable CPU worker now checks every ID in the logical plan's sorted,
unique affected set before placement selection, NIF decoding, texture work or
collision preparation. A reference absent from live geometry or lacking MODL
cannot disappear from admission. Successful output pins the interior CELL and
the complete affected list; GPU admission, commit preflight and settled
finalization recheck that identity.

The reader streams the master twice, retaining only affected reference/base
identities and package target values rather than indexing every master reference.
It checks record/group bounds, nesting depth, complete subrecord framing
(including XXXX), decompression success, duplicate selected identities,
single-instance reference fields, finite transforms and positive scale.
Cancellation/failure leaves the caller's previous admission report untouched.

Only interior REFRs with STAT or LIGH bases can clear this necessary guard.
STAT requires a real model path. Deleted, visible-when-distant, cross-CELL,
worldspace, missing and unknown identities are rejected. The narrow reference
field whitelist rejects XTEL, room/portal, emittance, linked and unknown attached
metadata. The narrow base whitelist rejects scripts, destructible or other
unowned metadata and nonzero light sound dependencies. Actors, creatures,
items, containers, furniture, idle markers, doors and static collections remain
unadmitted here. Existing interaction and strict lighting/mesh/resource guards
still apply after this check; this is not a replacement for them.

## Navigation boundary

Nondeleted NAVM ownership comes from its DATA CELL field, not merely its group
label. Any model-bearing affected base (including modeled lights) in a CELL
with authored navigation is rejected until route/grounding reconciliation has an
owner. Nondeleted PACK PLDT/PLD2/PTDT/PTD2 value fields matching affected reference
or base IDs are conservatively rejected without assuming the package target type.
All occurrences are inspected, including repeated package fields.

The original master contains 12-byte locations and both 12/16-byte targets.
Both target layouts expose the existing package loader's common type/value/radius
prefix. Other lengths fail closed. Deleted navigation/package records do not
create live dependencies. This guard does not alter NAVM, routes, actor packages,
door graphs, published geometry or ordinary door behavior. It does not claim
complete dependency closure, dynamic navigation support or general LOD ownership.

## Validation

18 world, 31 player and one physics CTest cases pass (50 core host cases).
New synthetic coverage includes unsupported base/reference kinds, no-mesh IDs,
complete-list mismatch, unknown attachments, duplicate/missing identities,
cross-CELL/worldspace scope, deleted/VWD flags, invalid transforms/scale,
malformed/extended framing, cancellation, package reference/base targets,
12/16-byte targets, modeled-light navigation and NAVM DATA ownership independent
of group label. Deleted NAVM does not falsely block a supported static fixture.
Failure preserves previous output; successful report identity is checked.
The new test passes ASan/UBSan with LeakSanitizer disabled.

The failed attachment had left a truncated local ESM. Validation uses the
recovered 288,771,262-byte Dropbox master with SHA-256
d9fb0a33af495ddb43992b96ea74f2741b123fefdb1fcdcea28096f7649b0d06,
matching the existing inventory. Original Brass Lantern fixtures are resolved
from authored EDID/base/reference records rather than invented form IDs: a light
clears this guard, its load door fails closed, and static geometry is blocked by
authored navigation. Existing load-door resource exchange, Lucas/nonresident/
barter dialogue, merchant trading and MS11 reward/save regressions pass.
Existing unsupported showmap diagnostics remain unchanged.

Native source checks prove the guard precedes placement/resource work and is
bound at all three consumption points. Relevant renderer, body/skin, shoulder
and Pip-Boy source checks pass. No OpenXR/GL/on-device or performance validation
is claimed; Android validation runs separately in CI.

No gameplay command starts or dispatches this general resident workflow.
Remaining prerequisites include complete admitted dependency ownership,
timer/lifecycle handling and a bounded command integration with device testing.
Exterior, general navigation/LOD changes and broader actor/interactive mutations
remain unadmitted. Existing v212 gameplay and bounded nonresident actor Disable
are preserved.
