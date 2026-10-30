# Captured script speech, floor furniture and animation sound events (v294)

The normal frame pump captures original SCPT execution for transactional
publication. v293's Say/SayTo selection tried to preflight INFO results using a
nested detached transaction on that captured candidate. The transaction guard
correctly rejected it, but selection treated the failure as an empty topic and
committed the caller's waiting variables. Gob and Weld could therefore wait for
SayToDone even though no voice request existed. The earlier original-data test
invoked events without capture and missed this integration defect.

Say now binds its INFO against a separate read-only snapshot of the exact command
state. It does not execute INFO results inside the calling SCPT. The actual voice
adapter retains its result preflight and resident publication checks. Unsupported
selection preflight aborts the calling transaction; a legitimate empty selection
remains an original Say no-op. Candidate diagnostics are retained. Regression
coverage includes the normal captured frame pump, original gate proximity and
Weld speech, Gob speech and the complete Gob/Nova exchange, and rollback of a
synthetic unsupported INFO without committing waiting flags. Scripted actors also
carry the actual voice token into facial sampling, instead of reading the inactive
interactive-dialogue token; it advances between responses and clears on stop.

The original water beggar travels to his linked reference. It is a FURN using
FloorSitMarker.NIF, FRN entry 17, orientation 3141, with original FloorSitEnter,
FloorSitDynamicIdle and FloorExit IDLEs. Shared Travel resolution now accepts
linked references and enters resident furniture when it is the authored
reference destination. The furniture loader supports this adult male floor-sit
program through its existing approach, alignment, reservation, entry/loop/exit
and root accumulation handling. Female/skirt variants remain excluded pending
their original conditional program selection. A headless test uses Micky's real
PACK, original 3x3 resident NAVM window, linked marker and supplied original
NIF/KFs to reach the seated state. Assets are never committed to the repository.

Scripted NIF animation previously sampled geometry/collision but ignored text
sound keys. Shared timeline sound dispatch resolves only original SOUN editor
IDs. It supports case-insensitive `sound:` prefixes and bare original SOUN keys,
with loop/reverse/clamp timing and retained-publication deduplication. The
supplied Megaton gatehouse NIF has a lowercase prefixed opening cue at about
0.00001 seconds and a bare closing cue. Neither cue is invented or timed by a
location-specific rule. Host adapter tests exercise both forms and ensure mesh
republication cannot repeat the sounds.

Original evening saloon ingress is verified through the door's captured source
script, canonical realization/template choices and the actual cell actor reader:
Moriarty, Gob, Nova and all three authored customers resolve. Their schedule and
enable conditions remain original. The reported missing live actors have not
been reproduced in that test. Native preparation now reports canonical/resolved
counts and unresolved resident source identities; assembly logs already report
missing models and failed visual preparation. A headset log/save is needed if
actors remain absent.

Existing v293 saves may already contain waiting script variables without any
speech request. This change does not guess a lost topic/response or rewrite NPC
locals. The affected player-state.fqps is required to inspect such a checkpoint
and recover it through original dialogue/callbacks without resetting progress.
Normal in-flight speech continuation is still not serialized by this patch.

Validation distinguishes host original-data/asset tests, synthetic/native-adapter
regressions, Android compilation and physical headset observation. Headset checks:
Micky approaches and sits at his original marker; new original Gob/Weld sequences
produce voice and completion callbacks; gate opening/closing sound cues occur
once; evening saloon customers follow the door's authored enable schedule.
