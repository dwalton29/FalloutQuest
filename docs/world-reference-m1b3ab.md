# M1B3ab — original SCPT event-owner associations

The detached master matcher no longer accepts a SCPT event solely because its
body and container match the original script. Each captured event owner must
resolve uniquely to a nondeleted original REFR, ACHR, ACRE or QUST.

Placed owners require exactly one nonzero four-byte NAME and a unique,
nondeleted supported base containing exactly one nonzero four-byte SCRI matching
the captured SCPT. ACHR requires NPC_, ACRE requires CREA, and REFR excludes
those actor bases. Quest-owned events use the original QUST SCRI directly.
Supported direct base classes follow the existing catalog's event associations;
template inheritance and runtime script reassignment are not inferred.

The initial complete header scan resolves requested owners alongside programs.
A second complete header scan resolves requested bases independently of record
order. Only requested payloads are decoded, with the existing 64 MiB bound,
strict group/subrecord/XXXX framing and cancellation checks. All occurrences of
requested identities count, including wrong-type and deleted duplicates. Failed
attempts preserve the previous report. Publication, exact allocation-free report
comparison, worker ownership and resident dependency rejection are unchanged.

Validation covers changed/missing/null/duplicate/malformed associations,
deleted/missing/wrong-type owner/base identities, placement/base mismatches,
both record orders, compressed and extended fields, and direct quest scope.
The original MS11 bomb event passes; substituting the Player reference as its
owner rejects without replacing the report or mutating Player state. Integration
fixtures keep the scripted owner separate from the affected static reference,
and still reject incoming SCRO and nested/cyclic FLST dependencies.

This authenticates an original direct association, not runtime event eligibility,
activator identity, INFO speaker eligibility, inherited or dynamic binding,
complete conditional/read-set ownership, cryptographic whole-master identity,
or concurrent external edits. Another worker header scan adds CPU/I/O cost;
there is no Quest timing claim. General resident Enable/Disable remains
unadmitted pending complete ownership, bounded dispatch and device validation.
