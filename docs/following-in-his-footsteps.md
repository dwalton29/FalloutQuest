# Following in His Footsteps: implementation checkpoint

Baseline: main 09d5a3c (v288). Foundation: 853deb8 (v289).
Original quest MQ01 / 00014E87; movement checkpoint v290; native arrival follow-up v291.
This is an incomplete quest implementation, not an end-to-end certification.

## Original progression and alternatives

- Valid entry is the post-Escape MQ01 state, with the search-for-Dad objective.
  An initial-state fixture is not a production command to force a stage.
- Megaton conversations can direct the player to Colin Moriarty. His initial
  100-cap offer, later 300-cap demand and other conditional replies are separate
  original INFOs. Silver's related branch must preserve its own conditions and
  consequences rather than grant information unconditionally.
- Gob can reveal Moriarty's terminal; Nova has an authored password-note branch.
  Moriarty's terminal visitors/main menu results also execute MQ01 stage 40.
  The terminal route requires real password/hacking access and menu conditions.
- Stage 40 completes prior search objectives, displays the GNR objective and
  reveals its map marker. Getting information is not quest completion.
- MQ01 and MQ01LD coordinate the Lyons' Pride approach, checkpoint/schoolhouse
  scenes, GNR fight, Behemoth entrance/death, Fat Man interactions and aftermath.
  Actor SCPTs use package callbacks, SayToDone and weapon-filtered hit events.
- Three Dog has distinct authored outcomes. Stage 175's route invokes MQ03 and
  its stage results; stage 200 has the MQ02/dish-dependent continuation. Their
  original results include 300 XP, radio state, objectives and achievement data.
  Early MQ03 progression and Three Dog's death have separate MQ01 stage results
  (210/250), and must not be treated as the normal reward path.
- Sarah's script includes later main-quest branches. Those bodies are not a
  reason to implement MQ11 wholesale or execute unentered branches, but current
  eager command compilation previously rejected a whole event on an unknown
  command. Unsupported leaf commands inside conditionals now remain explicit
  failing commands: skipping the branch succeeds, entering it rejects the entire
  transaction. Malformed control flow and evidence limits remain fatal. Unknown
  expressions and typed reference operations still need implementation.

## Current evidence

Original-data preflight confirms the 100-cap result (INFO 000151BC under DIAL
000151B6) and its nested stage-40 effects with sufficient inventory. The 300-cap
result rejects insufficient funds. These are not headset conversation proofs.
The optional original-data weapon test starts the normal Moriarty greeting,
selects the original father/location/payment choices through the shared dialogue
session, advances original responses by completion tokens,
checks caps/objective/stage effects and saves/restores state. Its starting
snapshot is explicitly MQ01 stage 10; it does not demonstrate the Escape quest.

The supplied executable's SetQuestDelay command entry (opcode 11DC,
handler 007A6510) writes the float quest override at quest +40h and marks change
flag 4. The supplied FALLOUT.INI has fQuestScriptDelayTime=5.0000. QUST DATA
retains its flags and processing-delay float; these now drive quest scheduling.

Shared ResetHealth support is added to the command compiler and canonical Player
runtime. It clears damage on living actors through the same state consumed by
NPC combat and health UI, supports the player receiver, preserves death, and
participates in script preview/rollback. The original handler (007A0FF0) also restores seven condition actor values;
this engine currently has no limb-condition damage state. Adding that state must
connect those restorations rather than preserve damaged limbs. ResetHealth does
not implement Resurrect. Generic tests cover preview isolation, healing and dead-actor retention.
This command occurs in the original Lyons' Pride stage results. Only the
explicitly listed whole result bodies below have been tested.

## Remaining reusable dependencies

| Area | Required shared support / verification |
|---|---|
| Named AI values | Added SetAV/GetAV aggression and assistance with persistent overrides and live combat/fast-travel consumers; the complete original stage-80 result executes and its AI values survive reload. |
| Quest cadence | Added original QUST DATA cadence and saved SetQuestDelay overrides. Zero uses the supplied INI default of 5 seconds. Fast quests run at most once per frame; reference scripts retain their bounded cadence. Timing, reset-to-default and reload tests pass. |
| Factions | Added saved SetEnemy/SetAlly directional relations and live combat/fast-travel consumers. Original executable handlers 007A3FF0 and 007A2AF0 confirm neutral/enemy and ally/friend flag semantics. |
| Essential actors | Added saved base-actor SetEssential overrides used by canonical hit/kill policy, retaining Traits inheritance and death state. Handler 007A40A0 sets base flag 2 for a positive integer argument. |
| Reference movement | Added actor MoveTo/MoveToMarker with authored destination, rotation and world-axis offsets, transactional canonical state, resident pose invalidation before simulation/persistence, navigation reset and fresh motion identity. Existing arrivals/retirement handles changes in residency. Player/non-actor moves and reference-variable destinations remain unsupported. |
| Positional effects | PlaySound3D and original explosion/weapon effects, using authored locations and ownership. Stage 104 currently rejects marker.PlaySound3D. |
| Events | OnHitWith must carry the weapon filter separately from the attacker; preserve IsActionRef/OnHit/OnDeath identity. Do not substitute the weapon as activator. Verify OnPackageStart/Done and speech-completion filters. |
| Typed references | Durable reference locals and reference-valued expressions for MQ01's speaker/target scene logic. Numeric locals must not stand in for FormIDs. |
| Dialogue | Normal GREETING/choices, speech outcomes, Silver, Gob/Nova, Three Dog's alternatives and unavailable-condition diagnostics. A result-script preview alone does not establish access to a choice. |
| World route | Original encounters, triggers, doors, combat and navigation across the D.C. approach and GNR still need demonstration. Off-scene reference changes now publish logically when every enable-parent descendant is outside the current CELL/world and live resource inventory; resident changes still use the resource coordinator. |
| Completion | Original stage flags/results, nested MQ02/MQ03 execution, actual perk/item/XP/radio consequences and no duplicate rewards after reload. |

## Verification boundary and next checkpoints

1. Megaton information routes: normal talk/choice access, paid/speech/terminal
   alternatives, correct GNR marker and objective; leave/return and reload.
2. Approach scenes: named actor values, quest cadence, faction relations,
   package/speech events and movement through the original triggers.
3. GNR battle: actor flags, original effects and hit/death provenance.
4. Three Dog: every original completion/skip/death branch and nested quest effects,
   including save/reload and repeated-interaction reward protection.

The original result bodies for stages 55, 80, 142 and 150 execute in the host
fixture, with their timing/AI/faction/essential overrides retained across reload.
This does not demonstrate the actual approach, battle or world scene.

No complete world traversal or headset quest route has been demonstrated.
Android compilation and physical headset testing are separate evidence requirements.

Host verification: event_script_tests, weapon_tests with Fallout3.esm,
npc_combat_tests, original MS11 completion tests (all three branches plus
Swift Learner), and scene-collision/indexed-NIF source contracts pass. These do
not establish headset playback, navigation or frame time.

## Movement and early Rivet City checkpoint

Pip-Boy target pruning discarded the Citadel destination marker used by MQ01's
endings. The shared decoder now retains original SCRO dependencies from SCPT,
QUST, INFO, terminal and patrol records before pruning placements. This is not
an editor-name allowlist. It does not add marker meshes to rendering.

The shared AddNote path previously treated notes as stackable AddItem grants.
The original executable's AddNote entry (117C / 0078E480) delegates to the unique
player note list (008AF220). Repeated AddNote now retains one owned note and does
not replay its added notification. Existing DATA/Notes content/audio paths consume
that inventory; it persists through the existing item save format.

The original-data test `mq01_progression_tests Fallout3.esm` follows normal
Moriarty greeting/choices/payment, saves at stage 40, returns to Moriarty,
then follows Doctor Li's original greeting and father/location choices. Original
INFO results advance MQ03 to 30, MQ01 to 210 and MQ01LD to 150. No stage is
advanced by the test during this progression. The valid starting-state fixture
has MQ01 stage 10 and 100 caps. Audio completion tokens stand in for actual voice
playback; actor locations stand in for world traversal.

The checks cover objective completion, the authored early-skip outcome (no 300 XP
or achievement-4 grant), Sarah's Citadel placement, disabled encounter references,
post-quest enablers/collision, mid-quest and completed saves, return conversations
and replay protection for already visited stages. The test is explicitly skipped
by CTest without the original master; CI without game assets is not original-data
verification.

The complete original stage-result bodies 175, 200, 210 and 250 now pass host
preflight. That does not prove access to every ending, voice playback, physical
movement, combat or scene rendering.

## Alternative-route status

| Route/outcome | Evidence / remaining work |
|---|---|
| Moriarty 100-cap route | Normal greeting, original choices and completion-token result execution; stage 40, caps and save/return checks. |
| Moriarty 300-cap demand | Insufficient-funds result rejection checked; full conversation route remains unverified. |
| Silver / Gob / Nova / terminal | Original links identified; complete conversation/access/terminal routes remain unverified. |
| Early Rivet City / MQ03 skip | Original dialogue/script progression to stage 210, canonical world changes and persistence tested on host; traversal/residency rendering still needs headset verification. |
| Normal Three Dog / stage 200 | Original ending conversation verified in an isolated actor-location fixture: 300 XP, NOTE, radio variables, achievement, Sarah movement, reload and duplicate-reward protection. Physical GNR approach/door/battle remain unverified and need the missing scene support. |
| Three Dog speech shortcut / 175 | Nested MQ03 movement no longer blocks the result preflight; skill-success conversation and repeat-reward checks remain. |
| Dish already held | Separate DialogueGNR INFO/NOTE/MQ02 continuation identified; complete alternate path remains unverified. |
| Three Dog killed / 250 | Complete result preflight passes; real death-event delivery and consequences remain to be verified. |

DialogueGNR (00025960) owns Three Dog's relevant INFOs, independently of their
MQ01 stage effects. An audit restricted to INFOs whose QSTI is MQ01 misses these.
MQ01LD stage 50 also needs AddToFaction membership overlays and all their live
condition/combat consumers. The Behemoth entrance needs original PlaceAtMe
explosion effects as well as PlaySound3D. These remain explicit unsupported
commands, not skipped successful effects.

## Focused headset verification

1. Use a valid post-Escape save with MQ01 active and 100 caps. Talk to Moriarty
   through the original father/payment choices. Check the GNR objective/map marker
   and the 100-cap deduction. Save, leave, return and reload.
2. Travel to Rivet City and talk to Doctor Li. Follow the father/location choices
   through asking where the old lab is. Check MQ01 completion and the Project
   Purity objective. The early-skip outcome must not invent a 300 XP reward.
3. Save/reload and revisit Li/Moriarty; verify no duplicate charge, reward or
   encounter reset. Revisit affected GNR/Citadel areas to verify original NPC
   residency, relocated facing, enabled collision and absence of teleport streaks.
4. Recheck the existing Megaton bomb disarm/reward path. Host tests retain all
   three original reward branches and Swift Learner checks.

No physical Quest/ADB session is connected here. The available local asset subset
also does not provide a complete rendered/audio D.C. traversal. Host script tests
cannot certify those observations.

The isolated Three Dog ending fixture follows his original greeting and acceptance
choices from the stage-40 state, without a stage-advance command. It verifies
stage 200, MQ02 stage 10, 300 XP, MQ02VirgoNote, radio ExploitReady, achievement 4,
Sarah's relocation and reload/replay protection. Positioning the actor context in
GNR is not evidence that the preceding battle/door route works.

Native review retained first-resident restoration after MoveTo: that path also
initialises actor inventory. Applying a teleport must not mark a newly loaded
NPC as already restored and accidentally skip its equipment preparation.
