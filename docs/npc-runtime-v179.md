# v179: authored speech morphs, dialogue activation and route recovery

Version 179 / `0.47.0-authored-lip-recovery`, based on main
`81c671ef0915cab7e2b6bc41767d112321e0dee9` (v178).

This is an implementation checkpoint. It does **not** complete settlement-life
parity or establish headset acceptance. Eat/Sleep/furniture and cross-cell actor
travel remain unsupported. The precise headset-only repeat-dialogue failure has
not been reproduced; new activation/teardown fixes and diagnostics need a Quest
retest. Original-data host tests establish the results below, not visual parity.

## Dialogue lifecycle

A Talk eligibility query previously evaluated GREETING before the NPC's
OnActivate script. Supported activation commands can change the variables used
by GREETING. `CanActivate` now interprets those commands into a temporary
variable overlay, so the UI can expose an activation-enabled greeting without
mutating persistent state. Actual Start runs activation, then resolves fresh
conditions and INFO priority against the resulting player state. An active
session cannot be replaced. Unsupported scripts reject atomically.

Cleanup resets the published audio token, panel INFO/response/choice state and
eligibility cache, stops audio and calls the actor's EndDialogue. Persistent
said-INFO, talked-actor, quest and script variables survive. Session tokens remain
monotonic; delayed audio completion and position events cannot affect a different
line/session. Combat/death/loading/unload/distance interruption retains the
existing general cleanup path. An unavailable head pose is still not an unload.

The original Lucas handshake follows INFO `0003DA20` response 3, player topic
`00003B80`, INFO `00003B82`, then acknowledgement INFO `0003DA07`. Its result sets
DialogueMegaton.LucasGreet to -1. After save/restore, the regression enters and
terminates three more sessions, re-evaluates the current Patrol package context,
rejects old tokens, preserves authored variables, and permits another session
after nonlethal combat. The original session tests already passed repeated simple
contexts on v178, so these findings are not proof of the reported headset root
cause. `NPC DIALOGUE REENTER` includes package/activity/dialogue state on failure.

## Package lifecycle and resident actors

Effective AI lists use NPC template flag 16. Priority, supported CTDA and daily
schedule checks remain authored. Ordinary condition scans run at most every
0.25 seconds, immediately on player revision, game-minute boundary, package
owner reset, dialogue/combat return or route failure. Follow/Flee positions and
resident target availability update between scans. Existing combat perception
has its separate bounded think cadence.

Each resident owns its package, path, triangle surfaces, index, sequence and
retry timers. NAVM is immutable and shared. Travel/Guard arrival is explicitly
Completed while selection continues; it does not invent a new destination for
an authored stay/wait package. Patrol uses authored marker waits and existing
repeat/reversal rules. Wander/Sandbox continues choosing reachable points after
arrival. Explicit Procedure states distinguish executing, waiting, completed,
blocked, invalid target, route failed, unsupported and interrupted. These states
and evaluation caches are ephemeral; the save schema is unchanged.

An obstruction, including a door that never clears, stops motion. After three
seconds the route is invalidated, Travel/Guard procedure progress is reset and
retry is delayed one second. Initial route failures retry after two seconds.
Root grounding, collision surface selection and locomotion-root handling from
v178 are preserved. This does not add crowd avoidance or a new pathfinder.

Loading still covers all resolvable non-deleted/non-initially-disabled NPC_ ACHR
in the requested Megaton/interior scene, independent of dialogue and editor ID.
Visual preparation, equipment, state restoration and navigation attachment remain
the existing shared path. Wasteland actor residency and cross-cell NPC movement
are not added. No interior NPC is teleported into Megaton's exterior.

See [the complete resident inventory](npc-resident-audit-v179.md). There are 3
exterior placements and 34 interior placements. At the authored host baseline,
2 exterior actors execute packages; Stockholm is blocked by unresolved levelled
statistics (LVLN VarWastelander). It is now logged explicitly. Burke's selected
Travel is DefaultStayAtCurrentLocationSkipFallout, so his stationary destination
is authored. Lucas switches WaitForGreeting to PatrolBomb after the handshake.
The exterior integration runs all three simultaneously for 120 seconds.

The interior integration loads all 34 actors, shares each CELL's original NAVM,
simulates 57.6 seconds with schedule checks at 12:00, 20:00 and 07:00, and logs
every final selection. At 07:00, 23 have executable packages; 24 actors moved
more than 16 game units in the plane during the run. This test uses production
package code and NAVM, with rendering/world collision stubs. It is not a full
24-hour simulation, canonical unloaded actor simulation or headset population
acceptance.

## Original LIP decoding

Evidence comes from supplied Fallout3.exe reader `0x62D2D0`, frame reader
`0x62D510`, playback `0x62CFB0` and speech-name table `0x10FE230`, checked against
five original lines. No Skyrim layout or audio-amplitude phonemes are used.

The 12-byte header contains version 1, allocation size and compression flag.
Allocation includes 16 bytes of engine object overhead. The raw payload contains
u32 frame count and signed first-frame index, then 132 bytes per frame:
16 speech floats followed by 17 modifier floats. Compression is **byte-wise**:
nonzero bytes are literal, zero introduces a little-endian u16 zero-byte count.
Zero bytes inside a float use the same encoding. Playback is 30 frames/second;
negative first-frame values describe preroll. Sampling interpolates neighboring
speech frames using the matching MediaPlayer playback position.

Input, decoded size, frame counts and channels are bounded; bad versions,
truncation, inconsistent sizes, zero-run overflow and nonfinite values reject.
The 17 modifier names are established by executable table `0x10FE1E8`, the
17-entry TRI matching loops at `0x5FE222`/`0x5FE652` and the playback modifier
submission at `0x62D0AA`: BlinkLeft, BlinkRight, BrowDownLeft, BrowDownRight,
BrowInLeft, BrowInRight, BrowUpLeft, BrowUpRight, LookDown, LookLeft, LookRight,
LookUp, SquintLeft, SquintRight, HeadPitch, HeadRoll, HeadYaw. Exact matching TRI
targets layer on the bind geometry. Tested lines have authored blink/brow tracks;
no random facial behavior is introduced. Head rotation channels have no matching
head TRI target and remain unapplied; their rotation units/composition are not
established here.

| Original line | Frames | First frame |
| --- | ---: | ---: |
| ms11_greeting_0003da20_3.lip | 165 | -8 |
| ms11_ms11lucasgreet1a_00003b82_1.lip | 262 | -5 |
| ms11_ms11lucasgreet3c_0003d9f6_1.lip | 208 | -6 |
| ms11_ms11lucasbomb_00018af4_1.lip | 55 | -11 |
| ms11_greeting_0007e113_2.lip (Burke) | 275 | 0 |

The exact speech order is Aah, BigAah, BMP, ChJSh, DST, Eee, Eh, FV, I, K, N,
Oh, OohQ, R, Th, W. Exact matching TRI names are used. The human TRI contains
**Ee**, while the engine speech-name table says **Eee**. That channel is not
silently aliased; its correspondence still needs engine-side confirmation.

## TRI and runtime deformation

The original human head is FRTRI003 with 1,211 vertices, 2,294 triangles,
38 differential targets and 8 sparse absolute modifiers referencing 238 extra
vertices. The actual NIF matches the 1,211 vertices and expands to 6,882 indices.
The parser validates named scaled i16 differential vectors, sparse modifier
indices and end-of-file. Sparse modifiers are not applied to speech geometry.

Targets include the speech names above (with Ee), Anger, Disgust, Fear, Happy,
Sad, Surprise, brow targets, MoodNeutral/MoodAfraid/MoodAnnoyed/Cocky/MoodDrugged/
MoodPleasant/MoodAngry/MoodSad, Pained and CombatAnger. Head TRI has no blink
morph. INFO emotion fields already decode, but their application, blink/eye
semantics and non-dialogue facial tracks remain unsupported rather than random.

Scene preparation loads matching TRI files for the actor's actual head parts.
Only vertex-count-matching shapes receive differential offsets. The offsets
follow NIF geometry transforms, placement and existing rigid head attachment,
then scene axis/unit conversion. They add to the current FaceGen bind result.
Existing bone palettes, body KF talking/listening and head-look remain active.
Only TRI-bearing face parts update bind vertices; body skinning retains its
palette-only path. Normal/tangent vectors remain the bind vectors, so speech
lighting is approximate. Race/sex variants use their own resolved model/TRI;
asset validation here covers the adult human head, not every race or head part.

Audio resolves the original voice filename, loads its same-stem LIP from loose
files/BSA on the audio worker, and publishes an immutable token-specific asset.
Java sends MediaPlayer position every 33ms only while the exact player/token is
current. Render preparation samples that asset; stale token events are ignored.
Missing/invalid LIP gives a neutral speech face and logs its reason. Line end
returns speech weights toward zero within about 100ms. There is no invented
mouth flapping or clock-based substitution when playback position is unavailable.

## Verification and remaining work

New regressions cover activation preview purity, reusable original conversations,
actor release, stale tokens, obstruction recovery, independent original residents,
schedule transitions, real LIP/TRI/NIF samples, malformed assets and neutral bind
restoration. Existing assertions are retained. The original mesh integration
checks vertex deformation without altering actor position/yaw or non-position
attributes; it does not replace a rendered Quest check.

Remaining major work: canonical LVLN actor statistics/spawn resolution; Eat/Sleep
and authored furniture marker decoding/reservation/animation; Accompany, Use Item
At and dialogue package procedures; cross-cell NPC travel; remaining CTDA/scripts;
LIP head rotation channels and Eee/Ee correspondence; complete teeth/eyes/variant validation,
normal deformation and original idle/blink/emotion semantics. Unsupported package
entries continue down the selector and log their reason. These limitations mean
the full requested settlement-life milestone is not complete in v179.
