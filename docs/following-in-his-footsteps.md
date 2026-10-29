# Following in His Footsteps: implementation checkpoint

Baseline: main 09d5a3c (v288). Original quest MQ01 / 00014E87.
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
| Reference movement | MoveTo using original destination reference, rotation/cell/world ownership, resident/unloaded actor migration and scene publication. MQ03's nested Dr Li movement blocks the stage-175 shortcut. |
| Positional effects | PlaySound3D and original explosion/weapon effects, using authored locations and ownership. Stage 104 currently rejects marker.PlaySound3D. |
| Events | OnHitWith must carry the weapon filter separately from the attacker; preserve IsActionRef/OnHit/OnDeath identity. Do not substitute the weapon as activator. Verify OnPackageStart/Done and speech-completion filters. |
| Typed references | Durable reference locals and reference-valued expressions for MQ01's speaker/target scene logic. Numeric locals must not stand in for FormIDs. |
| Dialogue | Normal GREETING/choices, speech outcomes, Silver, Gob/Nova, Three Dog's alternatives and unavailable-condition diagnostics. A result-script preview alone does not establish access to a choice. |
| World route | Original encounters, triggers, doors, combat and navigation across the D.C. approach and GNR; unloading/return must not duplicate encounters or discard timers. |
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

No complete quest route has been demonstrated. Android compilation and physical
headset testing are separate evidence requirements. In particular, headset
verification needs a post-Escape save, sufficient caps for Moriarty, the GNR
approach/battle, and saves before and after each transition/completion branch.
The original MS11 disarm/reward regression remains required.

Host verification: event_script_tests, weapon_tests with Fallout3.esm,
npc_combat_tests, original MS11 completion tests (all three branches plus
Swift Learner), and scene-collision/indexed-NIF source contracts pass. These do
not establish headset playback, navigation or frame time.
