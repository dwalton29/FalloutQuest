# M1B3an — command form-name binding evidence

Continues M1B3am (`3164b3386477d13d57c852f7028a89175e119015`). General resident
Enable/Disable remains gated. This adds binding evidence, not a gameplay caller.

Captured preparation now records each successful formNames lookup performed by
the production command compiler: normalized source name and resolved FormID,
in lookup order, including repeated lookups and unreachable commands. Explicit
qualified command receivers and multi-form commands use the same lookup sink.
Implicit owners, built-in Player prefixes, expression operands and qualified
variable resolution do not pass through this sink and are not claimed here.
No compiler sink is supplied when provenance capture is off.

The sink checks pair objects and name bytes/terminators before retaining each
binding. Its value-byte limit is the transaction's remaining evidence allowance;
the final entry additionally charges program/container/conditions/local data and
all form bindings against the shared 16 MiB limit. Fixed vector storage is in
sizeof(ScriptExecution). The vector moves into the owned execution entry after
charging; failed preparation does not publish state/effects or partial evidence.
Collector capacity, allocator overhead, existing compiler intermediates and
matcher indexes remain private preparation work outside retained-value accounting.
This is not a whole-preparation memory or Quest timing guarantee.

Master matching validates binding shape after charging and before file access.
During the existing first scan, each supplied target ID must identify exactly
one nondeleted original record with exactly one complete EDID matching the
normalized name. Generic strict subrecord/XXXX framing is checked without
interpreting that record's gameplay schema. Names require one final NUL and no
embedded NUL/hidden tail. Missing/deleted/duplicate IDs, malformed EDIDs and
name mismatches reject before the previous verified report can be replaced.
Targets may overlap program/owner identities and may precede or follow them.
No new master pass is added. Exact frame-time report matching includes the
ordered binding vector and remains allocation/file free.

This establishes identity only for supplied command bindings. It does not
independently recompile the trace to prove completeness, exclude same-name EDIDs
on other IDs, prove target record-type suitability or interpret load-order
overrides. An empty caller-supplied vector is not evidence that a source body
has no form lookups. Expression reads, external variable chains, implicit
receivers, aliases, outcomes and call/event causality remain separate proof
requirements. There is no authored-dependency exemption or admission expansion.

Tests cover exact-fit and failed byte charges; bounded compiler collection;
case normalization; repeated, qualified, multi-form and unreachable lookups;
capture-off behavior; exact report ordering/content; missing/deleted/duplicate
target IDs; malformed/duplicate/extended EDIDs; shared program identities;
compression and record order; rejection before opening a missing master;
and unchanged live state/effects.

All 53 core host tests, the actor test, native/render source guards and matcher
ASan/UBSan pass (LeakSanitizer disabled). Original captured MS11 stage/event/INFO
evidence passes strict master matching with the new command bindings. The local
declaration policy still accepts all 1,257 SCPTs. Original Lucas acceptance and
production bridge, merchant barter and all three MS11 completion/reward/save-
reload branches pass; existing unsupported showmap diagnostics are unchanged.
The supplied ESM SHA256 was reverified as
`d9fb0a33af495ddb43992b96ea74f2741b123fefdb1fcdcea28096f7649b0d06`.

Version 212 / 0.74.1-moira-barter is unchanged. Original assets and current
gameplay/render/VR behavior are preserved. Device validation is still required.
