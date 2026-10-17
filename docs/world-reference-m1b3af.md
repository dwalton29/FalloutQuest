# M1B3af — original INFO restrictions and strict scope metadata

Continues M1B3ae (`0f9ac4fc15de7edd4674c25773d83c5ba6efbe8b`), whose Android
build passed at https://github.com/dwalton29/FalloutQuest/actions/runs/39053313356.

Master matching now applies the original INFO's explicit ANAM speaker
restriction to the authenticated actor base. ANAM names the base, not the
placed speaker reference; null/absent ANAM remains unrestricted. This uses the
same base comparison as Session eligibility. The worker records the original
restriction while decoding the INFO, then compares it on the existing base
pass, after all INFOs have been read. An actor appearing before its INFO cannot
escape the check, and no additional master pass is introduced.

The strict framing validator also requires four-byte, record-wide singleton
QUST SCRI and INFO QSTI/ANAM fields. Their counters do not reset at stage/item
or begin/end fragment boundaries. This prevents a tolerant first-field decoder
from authenticating ambiguous or truncated ownership metadata. These optional
fields may still be absent or explicitly null; captured quest local scope must
continue to equal the decoded original SCRI.

QUST/INFO CTDA payloads must use the existing supported 20/24/28-byte layouts
before tolerant decoding. This is a framing check; it does not evaluate a
condition or establish that its function/context is supported at execution.
Other record classes' similarly named fields are not reinterpreted.

Validation covers scalar lengths, duplicates across fragments/stage boundaries,
null/nonzero quest local scope, all supported condition widths, malformed
condition widths, compressed records and extended fields. INFO restriction
tests cover null, matching base, wrong base and reference-vs-base confusion,
with both actor/INFO record orders. The original test discovers a restricted
INFO and an authored matching actor from Fallout3.esm, accepts that identity,
then rejects Lucas as a genuine actor with the wrong base. Original MS11
stage/bomb/INFO provenance still matches without Player publication. All 53
core host tests, the actor test and matcher ASan/UBSan checks pass.

These checks establish scalar framing and one original static speaker
restriction. They do not authenticate the entire owning quest/eligibility
context, replay conditions, prove runtime liveness or complete read/dependency
ownership, or add dispatch authority. Existing byte/count limits and report
publication remain unchanged. General resident Enable/Disable still awaits
remaining ownership, bounded dispatch and device validation. No Quest timing
or version change is claimed.
