# Quest and scripting checkpoint 1

## Investigation baseline

Inspected canonical main `583093d18c876150e0093cb6737a47b96c090e00` (v201), the
Pip-Boy data/session decoder, Player quest/save APIs, dialogue condition/result/
session dispatch, and the user's original Fallout3.esm before changes.

Remaining gaps on that baseline:

- QUST INDX stages collapsed all QSDT items into combined flags/logs; stage
  conditions, SCTX/SCDA/SCHR/SCRO and objective target conditions were discarded.
- SCPT kept source and variable names but discarded bytecode and script metadata.
- Dialogue result commands rejected every scripted/conditional stage. Direct
  Player SetQuestStage only changed numbers, with no original result execution.
- Quest completion doubled as running state; StopQuest and journal history had
  no persistence. Mutable globals had no canonical saved overlay.
- CTDA evaluation rejected a final OR group, although original MSObjectives
  contains such groups. This prevented authentic nested stage processing.

## Verified original dependency

MS11 `00014E9E`, The Power of the Atom, has original Lucas acceptance INFOs
`0001E367` (100 caps), `0001E366` (no reward), and `0001E363` (500 caps).
Each calls SetStage MS11 10. Reward choices also assign MS11.SimmsBonus.
Stage 10's single item calls SetStage MSObjectives 11 (`000C0F66`). That stage
contains **15** separate conditional items. Applicable items set MS11 objectives;
objective 10 has the original text **Disarm Megaton's atomic bomb.** and target
`00014BC8`. No MS11, actor, objective or FormID special cases exist in the runtime.

MS11 is start-game-enabled; its stage 10 transition is still necessary for the
objective. MSObjectives allows repeated stages (DATA flag 0x08); MS11 does not.
The highest visited stage is GetStage, rather than the last assigned stage.
Stage conditions are distinct from quest dialogue conditions.

## Architecture

- QUST retains stage and objective record order, each QSDT item, conditions,
  flags, journal text, source, compiled data, headers, typed raw script records,
  reference associations, and separate objective target conditions/metadata.
- SCPT retains source, compiled bytes, header, references and raw subrecords.
  Definitions remain separate from instances. Existing owner/index variable keys
  distinguish quest variables and individual NPC reference variables.
- `scripting/fo3-script-runtime.inc` shares the existing result command compiler
  and canonical Player APIs between dialogue and stage scripts. Context boundaries
  are explicit: dialogue result or quest stage, not automatic event execution.
- SetStage starts an instance, marks history before immediate nested results,
  evaluates each item separately and executes applicable scripts in order.
  Repeated-stage flags govern duplicate execution. Completion/failure flags
  update journal status independently of running/stopped state.
- An entire result or stage request rolls back Player state and revision on
  failure. Depth 32 and a 4096-instruction budget bound recursion. Compilation
  rejects unknown commands even in unexecuted source branches. Expression/block
  validation precedes effects; eligible false stage items do not execute scripts.
- Dialogue preflight uses the same execution transaction, including both result
  phases, restoring state via RAII. It does not copy the immutable ESM catalogue.
  APIs remain restricted to the existing session thread; no external world effects
  or frame-driven script scheduler have been added.
- QST1 follows the previous Pip-Boy save extension. Previous saves still load;
  objectives, visited stages, inventory, equipment and NPC data are retained.
  QST1 saves running state, ordered journal item history and mutable globals.
  Existing owner/index variable persistence is reused. Legacy saves infer running
  from their old active status, since no stopped flag existed.

## Source subset

Statements: StartQuest, SetStage, CompleteQuest, StopQuest,
SetObjectiveDisplayed, SetObjectiveCompleted, qualified `set owner.variable to`,
mutable global assignment, player.AddItem, AddTopic, and the existing
EnablePlayerControls VR policy. `if`, `elseif`, `else`, `endif` support comparisons,
AND/OR, parentheses, finite numbers, unary signs and addition/subtraction.
Expression queries: GetStage, GetStageDone, GetQuestRunning,
GetObjectiveDisplayed, GetObjectiveCompleted, qualified script variables and
canonical globals. Stage-item CTDA reuses the existing condition engine.

GetDeadCount is limited to verified nonrespawning actor bases, original ACHR
census and canonical persistent deaths. Respawning/unknown bases fail explicitly.
GetDisabled reads the original reference flag only when no enable parent exists;
parent-controlled references fail explicitly. No enable/disable mutation command
is implemented, so there is no unsynchronised shadow world state.

## Explicit limits

This is a small source interpreter, not a full Bethesda VM. Compiled-only scripts
are retained and rejected. Source and bytecode are not treated as interchangeable.
Stage-local variables, ref-valued locals, arbitrary object scripts, event blocks,
GameMode scheduling, full function/expression syntax, random stage conditions,
enable-parent resolution and respawning death counters remain unsupported.
Existing OnActivate's validated NPC greeting subset remains separate until a
future shared event-context implementation; other unsupported events are not run.

MS11's attached GameMode script is retained but not scheduled. It handles later
Simms/Burke confrontation and fail-state updates, not initial acceptance. Later
branches require ShowMap, RewardKarma, EvaluatePackage, enable/disable, moveto,
kill, TriggerLODApocalypse, weather globals, faction/race changes, spells,
completeAllObjectives, RewardXP, achievements, messages, cell naming and other
world functionality. These are rejected rather than replaced with placeholders.

Conditional objective targets are retained. The existing map intentionally omits
conditional objectives until their conditions can be evaluated in a map context;
the initial bomb target is unconditional and uses the existing map/compass path.

## Headset check

Install the matching debug APK as an update; do not uninstall or clear saves.
Use the existing original game installation. In Megaton, talk to Lucas with the
right-hand interaction and existing right-stick/A dialogue controls. Follow the
friendly greeting, acknowledgement, "Tell me more about your town",
"Why is the town called Megaton?", the disarm topic,
and the 100-cap acceptance. The no-reward option should use the same stage path.
Open Pip-Boy DATA / Quests, find The Power of the Atom, and select it: objective
10 should read Disarm Megaton's atomic bomb. Existing target support points at
the original bomb reference. Exit normally to flush the save, relaunch, and check
that the objective and reward variable persist. Talk again to check repeat safety.
Stop testing before bomb disarming or destruction: those branches remain outside
this checkpoint. Unsupported results log QUST/stage/item and command/condition
provenance and leave the transaction unchanged.

No headset testing is claimed. Host original-data and Android build results are
reported with the implementation commit.

## Semantics references

Original game records are authoritative for the dependency graph and all text.
Stage/quest semantics were cross-checked against the GECK documentation and xEdit
format notes, particularly immediate stage results, repeated stages, independent
completion/stopping, QSDT boundaries and highest completed stage:

- https://geck.uesp.net/wiki/SetStage
- https://geck.uesp.net/wiki/GetStage
- https://geck.uesp.net/wiki/Quest_Stages_Tab
- https://geck.uesp.net/wiki/Quest_Data_Tab
- https://github.com/TES5Edit/meta/blob/master/UESPWiki/QUSTDef.wiki
