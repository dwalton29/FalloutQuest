# v180: resident package continuation

Version 180 / `0.47.1-resident-package-continuation`, based on main
`26d19debe30f7d328681352922870291cdee5307` (v179).

This is a further implementation checkpoint, not completion of the full
settlement-life milestone. Headset acceptance remains unverified. Original-data
host integration has rendering and physical-collision adapters; it does not
establish visible Quest movement, speech quality or population parity.

## Resident activation and statistics

The production actor simulator previously entered the death branch for both
zero health and the negative unresolved-health sentinel. Unresolved actors now
remain alive, with AI suspended and an explicit once-only diagnostic. Genuine
zero-health death retains the existing lifecycle. Initial resident activation
logs reference/base, effective package count, NAVM triangle count, health and
whether persistent state was restored.

The supplied ESM's Stockholm statistics chain is Stockholm `00003B1C` ->
LvlHunter `0003A586` -> VarWastelander LVLN `0002E2A4`. All 27 LVLO entries
resolve their Statistics category to Wastelander `0002E29B`. Its authored
base health is 10, Endurance 4, auto-calculated level 1, and both original
NPC health GMST multipliers are 5, giving 35 health.

LVLN decoding uses the actual LVLD/LVLF/LVLG and 12-byte LVLO fields. An
immutable statistics source is established only when every entry has level 1,
count 1, no chance-none/global dependency and supported flags, and every
recursive statistics source agrees in health, Endurance, skills, level limits
and health-calculation flags. Missing entries, cycles, depth/budget overflow,
malformed fields and disagreement reject the entire consensus. Traits, sex,
appearance and inventory are not selected from a random list. This resolves
an invariant without implementing or pretending to implement Bethesda's
canonical levelled spawn resolver. Non-invariant lists still suspend AI.

Factions, AI Data and AI Packages also follow a levelled template only when
every eligible entry inherits that category from the exact same original
NPC_ source record. Each category is proven independently: shared Statistics
alone never authorizes a variant's AI. Traits/voice/appearance/inventory are
outside this resolution. This closes the production AI-template gap exposed
by re-running the original combat inventory test with newly resolved health.

## Continuous package execution

Failures now have per-actor/per-package retry deadlines. A failed priority
entry is skipped temporarily so the selector can consider the actor's other
authored entries. Initial route, Escort and Flee failures back off two seconds;
a route blocked for three seconds backs off one second. Expiry returns the
package to normal authored priority/condition selection. This recovery policy
is a Quest runtime policy, not a decoded Bethesda failure timer. No destination
or package is fabricated. Route/surface/index/door state is cleared together.

Travel/Guard completion requires an exhausted route whose endpoint remains
within the executor's arrival tolerance. A progress counter without its
ephemeral route, or an actor displaced from that endpoint, rebuilds a route.
Dialogue at the destination remains completed; combat displacement and save
restoration can no longer make a stale counter imply permanent arrival.

Stockholm's original `MegStockholmGuardInterior0x24` package `00019539` is
Wander with PLDT reference `00019538` and radius zero. The prior implementation
reached that point once, then failed to find a different random point within
zero radius. Zero-radius Wander now reaches its authored point and stays
there while package conditions continue to be re-evaluated. Positive-radius
Wander and Sandbox retain their continuing reachable-point selection.

Pure persisted actor position/yaw/procedure/wait progress still increments the
normal save revision, but is excluded from the package invalidation revision.
The existing 0.25-second cadence samples spatial conditions. Package, scene,
hostility, equipment, death and other player/quest mutations still invalidate
selection immediately. Neither revision nor retry deadlines are persisted;
restoration naturally re-evaluates AI. Save schema and legacy catalog
fingerprints are unchanged; LVLN is an additional immutable record family.

## Dialogue and facial assets

The general reusable dialogue lifecycle from v179 is retained. Its original
Lucas handshake, three subsequent re-entry sessions, result-variable checks,
actor release, save restoration and stale-token rejection are re-exercised.
The headset-only inability to re-enter has not been conclusively reproduced;
a headset retest is required before calling it fixed.

The existing Fallout 3 byte-compressed 30 Hz LIP decoder, token-specific audio
position sampling, FRTRI003 differential/sparse morphs and FaceGen layering
are retained. Four original Lucas lines are tested: greeting `0003DA20_3`,
greet1a `00003B82_1`, greet3c `0003D9F6_1`, bomb `00018AF4_1` (165/262/208/55
frames, first frames -8/-5/-6/-11). Head NIF/TRI geometry matches 1,211 vertices
and 46 targets; lower teeth match 14 vertices, and the left eye matches 49.
Original NIF/TRI/LIP integration deforms each part, restores its bind geometry
and preserves actor position/yaw. It is not a visual attachment test in Quest.

Loose original asset lookup now follows complete case-insensitive paths when
an extracted install has both `Sound` and `sound` directories. Previously the
first matching directory could hide a voice/LIP file under the other spelling.
Exact spelling has deterministic priority; ambiguous branches are bounded.
This fixes the existing audio-assets regression without weakening its assertion.

## Original resident audit and limits

The placement/package census remains [the v179 original inventory](npc-resident-audit-v179.md):
3 exterior and 34 separate interior NPC references. Relevant assignments:
Travel 57, Sandbox 90, Eat 33, Sleep 36, Dialogue 30, Wander 11, Patrol 4,
Follow 4, Guard 2, Accompany 2, Escort 1, Use Item At 1. These are package-list
entries, not simultaneously active packages.

The 120-second simultaneous exterior integration now has **3 executable actors
and 3 changed positions**, including Stockholm reaching his original guard
point. Burke's selected stay-at-current-location package remains authored.
Lucas switches WaitForGreeting to PatrolBomb after the original conversation.
The interior integration loads all **34** residents and simulates 57.6 seconds
with checks at 12:00, 20:00 and 07:00: **23** have an executable package at
07:00 and **24** move more than 16 planar game units during the run.

Major outstanding requirements: authored Eat/Sleep/furniture marker actions,
reservation and animations; cross-cell actor travel/unloaded schedule
simulation; NPC-to-NPC Dialogue, Accompany and Use Item At procedures; remaining
CTDA/scripts; non-invariant levelled spawn resolution. The existing furniture
audit identifies Moira's actual chair/bed and NIF markers but their use remains
unsupported. Interior NPCs are not teleported into the exterior.

Facial limitations remain: Eee/Ee correspondence, LIP head-rotation units and
composition, INFO expression layering, non-dialogue facial semantics, remaining
race/sex/head-part validation and deformed normals. Missing/invalid LIP gives
neutral speech geometry with a diagnostic, never fake amplitude mouth flapping.
Physical grounding, body rig, Pip-Boy, weapons and rendering policy are unchanged.

## Regression coverage

New host cases cover invariant/divergent/malformed/cyclic levelled statistics,
an unknown-health actor staying alive, movement-versus-semantic invalidation,
completed Travel after dialogue/combat displacement and missing restored paths,
per-actor route-failure fallback/retry and zero-radius Wander continuation.
The obsolete assertion that Stockholm has unresolved health is replaced by
exact original chain/entry-count/35-health assertions and three executable
resident assertions. Other existing assertions remain intact.

## Validation performed

The complete host workflow checks passed: native source layout; GL state cache;
nonblocking GPU timer; shoulder inventory/Pip-Boy/opaque/LOD/world shader/interior
lighting/font/GPU skin/actor scope/render dispatch source checks; production GLES
transform-feedback parity; actor skinning; stereo/reflection policy; Java install
discovery; and CTest assets (3), world (4), physics (1), NIF (11 registered,
2 optional asset cases skipped by default), player (15) and audio (2).
The two optional NIF cases were separately run successfully with original assets:
11 combat/death KFs and the original skeleton, hands, Pip-Boy arm and eyes.

With the supplied Fallout3.esm, these executables passed:
`player_state_tests`, `pipboy_data_tests`, `weapon_tests`, `dialogue_tests`,
`npc_package_runtime_tests`, `npc_combat_tests`, `npc_combat_runtime_tests`,
`npc_door_runtime_tests`, `npc_record_tests`, `interior_lighting_tests` and
`audio_catalog_tests`. Original facial decoding and head/lower-teeth/left-eye
mesh integration passed against the four LIPs and corresponding NIF/TRI files.
The original greeting Ogg passed `audio_assets_tests`. `git diff --check` passed.

These are host/original-data results. The Android CI build and Quest visual
acceptance are separate; the latter has not been performed in this environment.
