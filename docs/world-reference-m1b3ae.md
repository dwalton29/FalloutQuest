# M1B3ae — original INFO speaker identity

Continues M1B3ad (`ae28ce5d3efe411821daaa3af2fe8a707c3f6003`), whose Android
build passed at https://github.com/dwalton29/FalloutQuest/actions/runs/39051165598.

Master-matched INFO result evidence now requires a nonzero captured speaker
that resolves uniquely to an original nondeleted ACHR or ACRE. Exactly one
nonzero four-byte NAME must identify a unique nondeleted NPC_ or CREA base,
respectively. An object REFR, quest, missing/deleted/duplicate identity,
malformed NAME or placement/base-type mismatch cannot authenticate the speaker
even when the INFO body, phase and topic match original data.

The existing owner/base scans now handle INFO speakers alongside SCPT owners.
INFO alone does not require a base SCRI: dialogue fragments belong to INFO,
and an actor may have no script or an unrelated script. If the same actor also
owns a captured SCPT event, that event still requires its original base SCRI.
Mixed evidence cannot relax either role's checks. Base payload framing remains
strict. The matcher does not decode a complete actor schema or resolve templates.

This reuses the two header passes for evidence containing events. INFO-only
evidence now also needs the second pass to resolve bases independently of record
order. Only requested payloads are decoded, under the existing size, count,
byte and cancellation limits. The immutable report already binds each exact
owner ID, so worker/commit/settlement interfaces do not change.

Tests cover both actor kinds, either record order, compressed and XXXX fields,
absent/unrelated SCRI, shared INFO/event identities, malformed or missing NAME,
zero/missing/deleted/duplicate speakers and bases, and mismatched types.
The real MS11 stage/bomb/INFO regression authenticates Lucas Simms' original
speaker; substituting the bomb's genuine object REFR or a zero speaker rejects
without replacing the report or publishing Player state.

This establishes original actor identity, not dialogue condition eligibility,
voice selection, runtime actor presence/liveness, arbitrary activator identity,
dynamic binding, complete dependency/read ownership or whole-master identity.
The canonical detached INFO API still permits a zero speaker for ordinary
preparation, but that evidence cannot pass reserved master admission.
General resident Enable/Disable remains gated by remaining ownership, bounded
dispatch and device validation. There is no Quest performance or version claim.
