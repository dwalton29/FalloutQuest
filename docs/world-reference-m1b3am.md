# M1B3am — strict local-record framing

Continues M1B3al (`0bef93c45e4632885b0dc10f2367a40509249202`), whose Android
build passed at https://github.com/dwalton29/FalloutQuest/actions/runs/39076646316.

The gameplay SCPT decoder is intentionally tolerant: an orphan SCVR can use its
default/stale index, a short SLSD can leave that index unchanged, and ZString
can discard bytes after an embedded NUL. Exact raw-container and decoded-table
equality therefore did not establish a well-framed authored slot/name binding.

The strict proof path now requires each SLSD to be exactly 24 bytes and to have
one subsequent SCVR before another SLSD or the end of the container. SCVR must
have a preceding unmatched slot and one terminating NUL, with no embedded NUL
or hidden tail. Other records may intervene; no new field-order requirement is
invented. Empty effective names still fail the separate name policy. Repeated
indexes with complete pairs remain legal, preserving original last-assignment
decoding. Slot metadata beyond framing is not assigned new semantics.

A shared allocation-free framing state machine checks retained containers
after evidence charging and before copying or opening the master. The original
SCPT payload scanner also applies it before tolerant decoding, in both the
event identity scan and the quest-local SCRI scan. XXXX framing remains handled
by the existing payload scanner. There is no additional master pass, retained
field, dependency exemption or frame-time report work. Raw unbound bodies do
not acquire an authenticated container; capture-off execution is unchanged.

An independent direct scan of the supplied ESM found 1,257 SCPTs and 4,032
SLSD/SCVR pairs, all satisfying this framing. The original-data transaction
regression also checks every decoded SCPT with the production framing helper.
No game source, bytecode or assets are copied into the repository.

Tests exercise orphan/repeated names, overwritten/dangling slots, SLSD lengths
0/4/23/25, missing terminators, embedded NULs/hidden tails, both stage/event
origins and compressed original payloads. Malformed retained evidence rejects
before file access; valid evidence cannot hide a malformed original payload.
Failures preserve the previous report and live state/effects. Existing tests
retain complete repeated-index pairs and ordinary/XXXX name records.

All 53 core host tests, the actor test, native/render source guards and matcher
ASan/UBSan pass (LeakSanitizer disabled). Original MS11 provenance, Lucas
acceptance/save-reload and production bridge, merchant barter and all three
MS11 completion/reward/save-reload branches pass. Existing unsupported showmap
diagnostics are unchanged. The ESM SHA256 was reverified as
`d9fb0a33af495ddb43992b96ea74f2741b123fefdb1fcdcea28096f7649b0d06`.

This checkpoint does not prove arbitrary form-name resolution, external variable aliases, runtime
reads/outcomes, full source/type grammar or call/event causality. Complete
ownership/read proof, bounded dispatch and device validation still gate general
resident Enable/Disable. Version 212 and existing gameplay/render/VR behavior
remain unchanged.
