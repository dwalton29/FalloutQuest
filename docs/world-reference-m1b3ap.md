# M1B3ap — bound-name namespace closure

Continues M1B3ao (`c4cc1e0e2e096f7f37fbb7ba381b726e286228ae`). M1B3an's
Android build passed (run 39079810706); ao's was running at the initial recheck.

Matching an EDID on a claimed target ID did not exclude another live record
with that same normalized name. Master proof now rejects conflicting supplied
name-to-ID claims across either binding phase before opening the file. It then
checks all nondeleted record classes for competing EDIDs during the existing
first scan, not just requested IDs or catalog-selected types. A requested name
on any other live ID rejects regardless of file order, class or compression.

Deleted aliases do not participate. Requested targets still require one matching
complete EDID and cannot be deleted or duplicate identities. Other live records
may have no EDID, including ordinary zero-length payloads; unrelated name reuse
is not a bound-name conflict. Generic subrecord/XXXX framing and at-most-one
complete EDID are required across this scan. Malformed live payloads, duplicate
EDID fields, missing terminators or hidden tails reject rather than allowing a
potential competing name to be skipped. Empty compressed records are malformed.

This is a conservative all-class ownership-proof policy, not an assertion about
the original engine's per-type namespaces or load-order overrides. The ordinary
catalog loader and gameplay lookup behavior are unchanged. If no bindings are
supplied, unrelated payloads retain the previous header-only scan behavior;
empty supplied vectors still do not establish that the source used no names.

There is no additional master pass, retained evidence field, frame-time report
work, authored-dependency exemption or resident gameplay caller. Name indexes
are private preparation values built from already charged binding input. The
first scan now reads/decompresses all live payloads when bindings are present,
increasing private CPU/I/O work. Cancellation remains checked per record and the
reader's 64 MiB per-payload cap is unchanged. Payload buffers, index storage and
allocator capacity are outside the 16 MiB retained-evidence accounting; this
is not a whole-preparation memory, cancellation-latency or Quest timing claim.

Reproduce the metadata audit with:

```sh
python3 -B tools/scripting/audit_form_namespace.py <Fallout3.esm>
python3 -B tests/player/test_form_namespace_audit.py
```

The supplied ESM has 718,952 nondeleted records and 40,981 EDID fields. None are
empty, malformed, duplicated within a record or repeated after case folding.
Its SHA256 remains
`d9fb0a33af495ddb43992b96ea74f2741b123fefdb1fcdcea28096f7649b0d06`.
The audit reports metadata only, never script execution or engine semantics;
the production matcher independently validates framing in its broader scan.
No game source, bytecode or assets are committed.

Tests cover both binding phases, conflicting supplied claims, case-only and
exact competing names, either file order, unknown record classes, compression,
XXXX, prefixes, unrelated duplicate names, deleted aliases, empty/no-EDID
records, unreadable live records and no-binding scope. Failures leave the prior
verified report unchanged. Five synthetic metadata-audit tests pass alongside
all 53 core host tests, the actor test and native/render source guards. Matcher
and transaction fixtures pass ASan/UBSan (LeakSanitizer disabled). Original
captured MS11 stage/event/INFO evidence passes the broader namespace scan;
Lucas acceptance/production bridge, merchant barter and all three MS11 completion/
reward/save-reload branches pass. Existing unsupported showmap diagnostics remain.

Independent trace completeness/phase derivation, target type suitability,
external variables, implicit receivers, condition/read ownership, runtime
outcomes and call/event causality remain open. Full ownership, bounded dispatch
and device validation still gate general resident Enable/Disable. Version 212
and current gameplay/render/VR behavior are unchanged.
