# v181: NPC interruption recovery and facial deformation validation

Version 181 / `0.47.2-npc-interruption-recovery`, based on latest main
`15ddfce41c1ef15301d9923fb3c8a002c52139f2` (v180). Main was fetched again
before delivery. This is a tested implementation checkpoint; the complete
settlement-life milestone and headset acceptance remain outstanding.

## Runtime changes

Escort completion now requires the actor to remain at its reached route
endpoint. A persisted completed phase without its ephemeral route, or combat
displacement from that endpoint, rebuilds the original destination route.
Patrol similarly rebuilds its current leg after displacement instead of
advancing a stale exhausted index and skipping the marker's wait.

Follow stopping inside the authored target radius clears route, NAVM surfaces,
index, door and blockage state together. Target movement resumes routing at
the existing bounded repath cadence. Combat entry/exit now also clear these
coupled route fields together.

Dialogue and combat use the same Patrol-wait suspension/resumption helpers.
The remaining authored wait is captured on entry and rebased on return; time
spent in dialogue does not silently consume it. Return invalidates package
selection and resets its simulation clock. No save schema changes were made.
Existing player-state tests retain old-save and actor-progress coverage.

Dialogue teardown now invalidates the actor skin frame and logs
`NPC DIALOGUE END` with the actor, package and reason. Existing teardown still
clears token, panel, cursor, choices, eligibility and talking ownership while
preserving quest/local/result state. Inactive facial decay now covers all 33
authored speech/modifier channels; blink/brow/look channels no longer snap to
zero when a line ends. A missing resident NAVM produces a once-only explicit
diagnostic and Unsupported procedure instead of silently retaining Walk.

The resident activation, effective template resolution, canonical actor
restoration, continuous 0.25-second selection/event invalidation and original
NAVM grounding paths remain those documented in [v180](npc-runtime-v180.md).

## Facial mesh directions

Authored LIP/TRI offsets previously changed positions while leaving the
original shading normals and tangent frame fixed. The facial upload now
transports each NIF normal with the inverse transpose of its deformed
triangle differential, and transports tangent/bitangent directions forward.
Area-weighted contributions are joined by original NIF vertex index, retaining
authored mesh seams. Degenerate triangles retain safe bind directions.
Neutral output restores the exact FaceGen-derived bind stream.

This changes facial deformation only. It retains the existing GPU/CPU skinning
paths, body pose, eye/teeth attachments, UV/color attributes and world root.
It does not add phonemes or synthetic animation. The original LIP decoder,
audio-token playback clock, FRTRI003 differential/sparse decoding and FaceGen
offset layering are described in [v179](npc-runtime-v179.md).

The four original Lucas LIPs were re-exercised:

| Original line | Frames | First frame |
| --- | ---: | ---: |
| `ms11_greeting_0003da20_3.lip` | 165 | -8 |
| `ms11_ms11lucasgreet1a_00003b82_1.lip` | 262 | -5 |
| `ms11_ms11lucasgreet3c_0003d9f6_1.lip` | 208 | -6 |
| `ms11_ms11lucasbomb_00018af4_1.lip` | 55 | -11 |

Speech channels remain the original executable's order: Aah, BigAah, BMP,
ChJSh, DST, Eee, Eh, FV, I, K, N, Oh, OohQ, R, Th, W. Exact-name targets
map directly; Eee remains unresolved because the head TRI names that target
Ee. Original executable comparisons use exact strings; no alias was invented.
The head TRI has 1,211 vertices, 38 differential and 8 sparse targets.
Original `mouthhuman.nif`/TRI has 27 vertices and 114 expanded indices.
Both head and mouth tests sample deformation, changed finite normals, exact
neutral restoration and unchanged actor root. These are host geometry tests,
not a Quest visual attachment test.

The supplied executable's expression-name table at `0x10FE1A8` contains
Anger, Fear, Happy, Sad, Surprise, MoodNeutral, MoodAfraid, MoodAnnoyed,
MoodCocky, MoodDrugged, MoodPleasant, MoodAngry, MoodSad, Pained, CombatAnger.
INFO response emotion and intensity are separate fields. Their weighting and
layering with FaceGen/LIP have not been established; presence of a similarly
named TRI target does not establish the engine's blend semantics. INFO emotion,
Eee/Ee, head rotation units/composition and non-dialogue facial life remain
explicit limitations.

## Original settlement audit and furniture investigation

The re-run [original resident inventory](npc-resident-audit-v179.md) has
**3 exterior** and **34 separate interior** eligible placements. Exterior
simulation runs the three actual actors together for 120 seconds: all three
receive executable packages and change position. Interior integration runs
all 34 residents in their original separate CELLs for 57.6 seconds with
12:00, 20:00 and 07:00 schedule checks: **23 executable at 07:00**, **24 with
more than 16 game units of planar movement**. Tests now resolve actor targets
against the actual simulated population instead of stale copied placements.
These placement counts do not imply that interior NPCs are exterior residents.

| Relevant PACK type | Assigned list entries |
| --- | ---: |
| Sandbox | 90 |
| Travel | 57 |
| Sleep | 36 |
| Eat | 33 |
| Dialogue | 30 |
| Wander | 11 |
| Patrol / Follow | 4 / 4 |
| Guard / Accompany | 2 / 2 |
| Escort / Use Item At | 1 / 1 |

The linked audit contains ordered chains for Lucas, Moira, Gob, Moriarty,
Walter and other residents. Lucas's original handshake changes selection from
WaitForGreeting `0003DBCE` to PatrolBomb `00055471`, without a runtime actor-ID
special case.

Eat/Sleep account for 69 assigned entries and remain high-priority missing
procedures. The original ESM and loose furniture/KFs were inspected further:

* Moira Sleep `00004156` targets REFR `00003D75`, FURN `00015838`
  BedTwin01L, `MNAM=80000002`, `Furniture/BedTwin01.NIF`.
* Moira Eat `00004155` targets REFR `00015882`, FURN `00015840`
  Chair01R, `MNAM=40000002`, `Furniture/Chair01.NIF`.
* BedTwin01's BSFurnitureMarker FRN contains two 16-byte markers; each has
  XYZ floats, a 16-bit orientation and two position-reference bytes. Its
  references are 1/2; Chair01 has three markers with references 11/12/14.
* `bedleft_enter.kf` is SpecialIdle_BedLeft_Enter, Bip01 root, non-looping,
  3 seconds, with NPCHumanBedEnter at 0.666667 seconds. Its accumulated
  root translation ends near (-10.54, 68.3868, 0.00092) game units.
* `dynamicidle_sleep.kf` is a 4-second looping DynamicIdle_Sleep and has no
  Bip01 translation track. It cannot simply reset the preceding accumulator.
* `bedleft_exit.kf` begins near that accumulated entry translation and ends
  near (-0.1073, -0.8291, 0.00095); NPCHumanBedExit occurs at 0.033333 seconds.

The existing locomotion skin path removes root locomotion. Reusing it for
these clips would misplace the body relative to the bed. Marker/KF accumulator
composition, IDLE selection, occupation/reservation and interruption/exit
semantics are not yet implemented. This build continues to reject Eat/Sleep
explicitly and consider lower-priority supported original packages. No in-place
bed animation, invented chair behavior or substitute furniture destination
was enabled. Full Sandbox furniture/idle-marker use also remains outstanding.

Other outstanding behavior: cross-cell/unloaded actor scheduling and travel,
NPC-to-NPC Dialogue, Accompany, Use Item At, remaining CTDA/result/package
scripts and non-invariant levelled actor spawning. Supported local door use is
covered by the production door bridge tests; XTEL traversal is still rejected.

## Tests and acceptance

New `dialogue_runtime_tests` includes the actual production dialogue bridge
and package executor with recording input/audio/skin adapters. Synthetic
coverage runs four fresh goodbye sessions, rejects late audio tokens, tests
speech/modifier neutral decay, verifies clean panels and released actors,
and interrupts dialogue through loading, Pip-Boy, walk-away, B and combat.
Original integration traverses Lucas's actual `0003DA20 -> 00003B82 ->
0003DA07` path, closes it, checks persistent result variables, performs three
fresh condition-selected reentries, then verifies package movement and a
post-nonlethal-combat conversation. It does not run Android MediaPlayer or
simulate headset input.

Additional regressions cover Escort completion after dialogue/combat/restore,
Patrol displacement and retained waits, and Follow stop/resume with complete
route-state cleanup. Existing Travel completion, repeated Wander/Sandbox,
Patrol reversal/repeat, schedule changes, route retries, per-actor state,
original residents, persistence, combat and door assertions are retained.

Validation passed:

* CTest assets 3, world 4, physics 1, NIF 11 registered (2 optional originals
  skipped in default invocation), player 16, audio 2.
* All workflow rendering/source checks: GL state cache, GPU timer, shoulder
  inventory, Pip-Boy, opaque hot path, LOD diagnostics, world shader, interior
  lighting, fonts, GLES transform-feedback skin parity, actor skin scope,
  actor skinning, scene dispatch and stereo/reflection policy; collision/source
  layout guards and Java GameInstall discovery.
* With original ESM: player_state_tests, pipboy_data_tests, weapon_tests,
  dialogue_tests, dialogue_runtime_tests, npc_package_runtime_tests,
  npc_combat_tests, npc_combat_runtime_tests, npc_door_runtime_tests,
  npc_record_tests, interior_lighting_tests, audio_catalog_tests.
* Four original LIPs in facial_tests; head and mouth NIF/TRI/LIP samples for
  all four lines in facial_mesh_tests; original greeting Ogg in audio_assets_tests.
* The two optional NIF cases separately with originals: npc_combat_asset_tests
  (11 combat/death KFs) and vr_body_asset_tests (skeleton, hands, Pip-Boy arm
  and both eyes).
* git diff --check.

Android build results and the downloadable APK are reported with the delivery.
Quest visual acceptance has not been performed. Verify reentry, face/teeth
attachment and independent movement on the headset before treating these
observations as resolved. This build does not satisfy Eat/Sleep/furniture or
full ordinary-settlement-life acceptance.
