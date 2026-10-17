# M1B3ah — shared INFO structural admission

Continues M1B3ag (`b48a32e08cdd4681965b16fb321add46ed3e9ffb`), whose Android
build passed at https://github.com/dwalton29/FalloutQuest/actions/runs/39057444197.

Original-master INFO evidence now uses the same static supported-structure
predicate as live Session selection: type 0 or 3, no deleted flag, no next
speaker, no secondary flags, no primary flag 0x80, and at least one response.
Session's previous predicate is extracted without changing its behavior.
An exact begin/end body, original actor association and owning quest cannot
authenticate an INFO outside that supported structure.

Strict framing requires exactly one record-wide INFO DATA of 3 or 4 bytes,
including across NEXT. Every TRDT must meet the existing decoder's 16-byte
minimum; a later valid response cannot hide a short one. Extended subrecords
and compressed payloads obey the same checks. An independent scan of the
supplied Fallout3.esm found 22,327 INFOs: 21,693 DATA fields of 4 bytes and
634 of 3 bytes. TRDT sizes were 24 (25,644), 20 (1,589) and 16 (2,230).
No new exact TRDT width or response-text requirement is inferred.

Tests exercise all 256 type values, every bit in the three mode/flag bytes,
DATA absence, width and duplicates on either side of the result fragments,
XXXX/compression and short/missing/multiple responses. Both script phases
are included and failed matching preserves the existing verified report.
An original unsupported INFO with a matching explicit-speaker actor and exact
body is now correctly rejected; the previous checkpoint admitted that metadata.
Synthetic supported INFOs retain positive and negative ANAM association tests.

Validation passes all 53 core host tests, the actor test, native/render source
guards and matcher ASan/UBSan (LeakSanitizer disabled). Against the supplied
ESM, stage/bomb/INFO provenance, Lucas dialogue acceptance and production
bridge, merchant barter, all three MS11 completion branches and save/reload,
load-door resources and authored dependency admission pass. Existing showmap
rejections remain unchanged; this checkpoint does not implement that command.

This is a static structure check. It does not replay INFO/quest conditions,
said-once state, quest running state, voice availability, random selection,
Previous INFO ordering or complete execution/read ownership. Master matching
still grants no dispatch authority and no dependency exemption. General
resident Enable/Disable remains gated by ownership, bounded dispatch and
device validation. Version 212 and gameplay/render behavior are preserved.
