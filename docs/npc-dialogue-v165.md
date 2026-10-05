# Authored live-world NPC dialogue — version 165

Version: `0.43.0-world-dialogue-dev`, ARM64 Quest. Baseline: canonical main `8a1aa85864d07e4f32d3a576922a819880c17309` (version 164), fetched again before publication. This is a bounded first dialogue runtime, not complete Fallout AI or a complete script VM.

## Implementation and verification status

The runtime is wired into the Quest render/input/audio loop. Host tests exercise the real supplied Fallout3.esm Lucas graph, voice identity, metadata, multi-response sequencing, save/restore, targeting and actor overrides. The original conversation idle and both turn clips decode and sample against the original 66-bone skeleton. Native core syntax and the existing host/render regression checks pass. Quest headset presentation, audible playback, comfort and post-conversation physical interactions still require device testing; host tests cannot establish those results. CI/APK links and final commit are supplied in the delivery message.

No game assets were added to the repository or APK. Runtime ESM, bitmap fonts, NIF/KF, voice and other game resources continue to come from the installed original game. Tests take external asset paths; they do not embed copyrighted audio fixtures.

## Definitions, conditions and session

The existing `fo3pipdata` parser is extended, not replaced. Radio and note definitions remain shared immutable data; conversation state is separate in `dialogue/fo3-dialogue-session.*`. Conditions and bounded results have their own host-testable modules. `fo3-dialogue-runtime.inc` is the small render-thread bridge; panel state and presentation are separate from the graph.

DIAL now retains EDID, FULL, DATA category/flags, PNAM priority and QSTI ownership. Fallout 3 DATA categories are 0 Topic, 1 Conversation, 2 Combat, 3 Persuasion, 4 Detection, 5 Service, 6 Miscellaneous, 7 Radio. GREETING is found by its authored EDID, not a Lucas FormID. GOODBYE is preserved as an authored topic identity and INFO Goodbye flags terminate the session. FULL is player-facing; an authored INFO RNAM prompt overrides it. EDID never substitutes for missing player-facing text.

INFO retains topic/group association, TPIC override, QSTI quest, ANAM speaker, previous INFO PNAM, both flag bytes, next speaker, RNAM prompt, KNAM/DNAM challenge metadata, CTDA, ordered TRDT/NAM1 responses, NAME added topics, TCLT/TCLF links and both begin/end SCHR/SCDA/SCTX/SCRO script blocks. TRDT retains response number, sound override, emotion, emotion value and animation flags; SNAM/LNAM speaker/listener IDLE references and response notes/edits are retained. Normal dialogue resolves PNAM ordering at definition finalization. Radio retains its existing sequencing policy.

NPC metadata retains class, race, voice, script, template ID and category flags, authored faction ranks, PKID order, raw AIDT, karma, disposition base and combat style. Trait/faction template inheritance is resolved with cycle checks; race male/female VTCK fallback is available for voice. Levelled template selection is not synthesized.

Verified function IDs come from the Fallout 3 xEdit definitions, not New Vegas/Oblivion assumptions:

| IDs | Implemented context/semantics |
|---|---|
| 1 GetDistance | Known speaker/player references, original game units |
| 14 GetActorValue | Player SPECIAL 5–11, skills 32–45 (Speech 43), karma 23, health 16, AP 12, inventory weight 46 |
| 46 GetDead | Canonical actor/player health; unavailable health rejects evaluation |
| 47 GetItemCount | Canonical player inventory |
| 50 GetTalkedToPC | Persistent actor conversation state |
| 53 GetScriptVariable; 79 GetQuestVariable | Known authored variable indices and canonical persistent values |
| 56 GetQuestRunning; 58 GetStage; 59 GetStageDone; 546 GetQuestCompleted | Canonical Player quest state; authored start-game flag for otherwise uninitialized running quests |
| 67 GetInCell; 310 GetInWorldspace | Live context identity |
| 68 GetIsClass; 69 GetIsRace; 70 GetIsSex | Authored identity and trait inheritance |
| 71 GetInFaction; 73 GetFactionRank | Authored membership/rank and faction inheritance |
| 72 GetIsID; 136 GetIsReference | Base/reference identity, with explicit player/speaker/target contexts |
| 74 GetGlobalValue | Existing canonical catalog globals |
| 80 GetLevel | Player level |
| 131 GetPCIsSex | Authored player sex |
| 141 IsTalking | Explicit live session focus |
| 161 GetIsCurrentPackage | Only when a package is actually known/executing |
| 289 IsInCombat | Supplied runtime actor context |
| 372 IsInList | Authored FLST base membership |
| 427 GetIsVoiceType | Resolved template/race/NPC voice identity |

All six comparison operators and consecutive OR groups within AND groups are evaluated. Global comparison values are supported. Unsupported function IDs, run-on contexts, parameters, flags or unavailable state reject the branch and emit diagnostics. Unsupported conditions are never treated as true, even inside an otherwise passing OR group. NPC effective actor values/inventory, faction changes, disposition calculations, detection and nonresident reference state remain unsupported.

Conversation phases are explicit Inactive/Speaking/Choices. Session identity includes actor/base, DIAL/INFO, response index, eligible authored choices, selected choice, voice, audio generation, start distance and end reason. Quest priority and authored INFO order control resolution; Random/Random End builds a pool of eligible INFOs. Aim eligibility does not draw random responses. Choices come from TCLT, or eligible authored top-level/added topics when no explicit links exist. Empty/unsupported GREETING does not produce a Talk target.

## Original Lucas evidence

The supplied ESM contains 6,381 DIAL and 22,327 INFO records. Lucas is NPC_ `00000A60` (`LucasSimms` / FULL `Lucas Simms`), ACHR `00003B46` (`LucasSimmsRef`), voice `00061EA1` (`MaleUniqueSimms`). His race is `0000424A`, class `0001873F`; his faction memberships are `000428CC` rank 1, `000043F7` rank 0, `0004BB91` rank 0. No Lucas-specific IDs or conversation strings appear in production dialogue logic; these are regression expectations and audit evidence only.

Neutral first GREETING under DIAL `000000C8` selects INFO `0003DA20`, quest MS11 `00014E9E`, flags Say Once, two conditions (GetIsID Lucas; GetTalkedToPC == 0). It has one response, number 3, no sound override, emotion neutral/value 50, Use Emotion Animation flag. NAM1 is the original introduction beginning “Name's Lucas Simms, town sheriff.” Its symbolic voice identity is `@voice:MaleUniqueSimms:_0003da20_3`.

| Initial linked DIAL | Authored FULL | Resolved INFO |
|---|---|---|
| `00003B80` / MS11LucasGreet1a | Nice town you got here, sheriff. It's a pleasure to meet you. | `00003B82` |
| `00003B7F` | Pffft. Nice hat, Calamity Jane. | `00003B83`, response numbers 1 then 3 |
| `00003B7E` | `<Say nothing.>` | `00003B84` |

Each reply links onward to `0003D9E8`, `0003D9E7`, `0003D9E6`. Tests follow the polite response through acknowledgement INFO `0003DA07`, then obtain eligible general topics; save/restore retains consequences and a subsequent conversation selects a repeat greeting. Initial karma/sex restrictions remain ESM conditions, not Lucas-specific runtime rules.

The greeting's authored end result sets `DialogueMegaton.LucasGreet` to -1 and calls EnablePlayerControls. The variable is resolved through the authored quest/script variable table and persisted. EnablePlayerControls is explicitly adapted to the always-live VR control policy.

## Voice and subtitles

The existing exact symbolic voice resolver is reused. It indexes each installed voice directory once and caches suffix resolution, rejects ambiguity, and extracts original BSA audio through the existing bounded cache. The original installed file `sound/voice/fallout3.esm/maleuniquesimms/ms11_greeting_0003da20_3.ogg` was verified: 52,063 bytes, mono Vorbis, 44,100 Hz. It is not packaged.

Dialogue has a dedicated native queue event, Java MediaPlayer and completion token, separate from radio/note playlist generations. MediaPlayer completion advances response indices in authored order; missing assets or decoding/playback errors end the conversation. There are no guessed line durations. Begin/end results run at their session response boundaries. NAM1 subtitles correspond to the current response; choices are locked until the whole INFO response sequence completes. Stale completion tokens cannot advance a new sentence/session. Begin/stop requests cannot be discarded because the audio queue is full.

The actor-relative channel updates gain from listener distance. Radio/note speech is ducked during dialogue and restored afterward; station state is not replaced by an NPC playlist. This backend does not provide HRTF or true spatial source playback. A future native spatial channel can consume actor anchor updates; a LIP decoder also needs a media-clock export in addition to the existing response token/identity and completion boundary.

## Live-world VR presentation and input

Talk targeting uses conservative bounds transformed from the actual skin palette/bind vertices and current actor pose. Dead actors and unsupported greetings are excluded. Existing nearest-target and world-triangle occlusion policy is reused. Prompt FULL names use the existing interaction HUD. A Talk activation does not queue a door transition or reset VR origin/yaw.

The dialogue panel consumes the solved live Head bone via a shared actor bone query; skeleton/skinning math is not duplicated in UI code. Panel anchor policy is 25 cm toward the player and 28 cm below the head. It smooths anchor motion (0.14 s) and gently billboards for readability. These are central VR adaptation values in `fo3-npc-state.h`, not Bethesda values. The panel remains attached to the actor; HMD tracking and world rendering continue normally.

The renderer reuses canonical Fallout bitmap glyph advances, texture, font layout and established green tint. Spoken text and authored choices wrap without arbitrary text truncation. Panel height follows content; four options are visible with selection scrolling. GL resources/layout are retained and rebuilt on content/selection changes, not parsed every frame.

Right stick up/down uses the working loot cursor's neutral guard, edge and repeat behavior. A selects, B exits. Original focus/select sounds are used. Loading and Pip-Boy take priority; opening/focusing Pip-Boy intentionally ends dialogue. Dialogue consumes A/B and right-stick navigation before containers/world interaction and suppresses snap turning/new grabs. Locomotion remains available for walk-away. A held weapon may fire and interrupt dialogue. Controller tracking loss cannot cause A to activate an object behind the panel.

## Actor behavior and interruptions

Each immutable actor visual has a lightweight runtime state: transform, package/activity/destination/speed, animation selector, combat target and temporary dialogue override. The override suspends prior activity/package/speed, holds locomotion and turns in place without teleporting or changing ACHR source orientation. A combat activity supersedes dialogue and is not overwritten when dialogue ends. This is the extension point for future package/path ownership; no fake package execution is added.

Body yaw turns at a bounded 1.15 rad/s outside a 0.55 rad threshold; small offsets use a limited smooth head yaw (0.6 rad). Previous orientation returns smoothly after exit. There is no vertical eye-gaze solver yet. Animation states are normal idle, left/right turn and conversation idle, with 0.3 s pose blending. KF clips are decoded during actor preparation and reused at switches.

Original turn assets are `locomotion/mtturnleft.kf` and `mtturnright.kf`. Conversation policy selects authored IDLE `LooseListenToPlayerRelaxedB` (`0001EF1F`), whose MODL is `Characters\\_Male\\IdleAnims\\talk_ToPlayerRelaxedB.kf`. The ESM's `TalkToPlayer` IDLE hierarchy includes flat-game MenuMode/weapon conditions, so choosing this relaxed member for live-world conversation is an explicit VR adaptation, not an implementation of the complete Bethesda IDLE scheduler. Both compressed and uncompressed transform tracks sample through the existing decoder. Version-27 conversation KF requires a single BSAnimNotes link instead of the version-34 note-array layout; this narrow decoder extension is tested with original assets. Response-specific speaker/listener IDLE references and full IDLE selection remain staged work.

Walk-away beyond 5 m, loading, focus loss, Pip-Boy focus, unloaded actor, dead actor or combat context end focus/panel/audio. Real weapon firing or damage interrupts dialogue. Essential actors still retain at least 1 HP under the existing weapon policy, and remain alive/talk-eligible until an unconscious state is implemented.

## Results, persistence and unsupported semantics

Bounded source commands: literal `set questOrReference.variable to value`, StartQuest, SetStage, CompleteQuest, SetObjectiveDisplayed, SetObjectiveCompleted, player.AddItem, AddTopic and EnablePlayerControls. They execute through Player APIs. Result application is transactional and rejects an unknown command/argument/form or canonical API failure. SetStage accepts only unconditional script-free stages; conditional/scripted quest stages are rejected rather than losing their consequences. Compiled-only results are rejected. Expression arithmetic/control flow, RemoveItem/Equip/Unequip, karma/disposition mutations, actor commands and a general GECK VM are not implemented. This means some later Megaton quest branches are deliberately unavailable; the Power of the Atom quest is not claimed fully playable.

Persistent state extends the existing length-delimited PIP4 save payload, preserving compatibility with version-164 saves. It retains talked actors, per-actor Say Once INFO keys, added topics and authored numeric script variables. Existing inventory/quest Player state remains the single store. Results increment save revision, feed Pip-Boy DATA/maps and radio state immediately, and are flushed at dialogue boundaries. No session, panel, animation override or half-spoken sentence is restored. Say Once/talk history is recorded on completed INFO responses; interruption timing parity with the original engine remains to be verified.

Other staged semantics: speech-challenge probability/presentation, refusal/always-darken/say-once-per-day behavior, actor-to-actor conversation/next-speaker transfer, some run-on contexts, NPC effective actor values/inventory, disposition/faction changes, package-based conditions without executing packages, response-specific IDLE choreography, full lip/emotion playback, and quest/actor script VM execution. Unsupported conditions/results reject branches; parser metadata is retained for later implementation. Normal dialogue does not claim to replace the existing radio evaluator; both observe the same persistent quest/dialogue consequences.

Event diagnostics: DIALOGUE TARGET/START/GREETING/INFO/RESPONSE/CHOICE/RESULT/END, UNSUPPORTED CONDITION/SCRIPT, NPC ANIM and NPC DIALOGUE OVERRIDE. Unsupported messages are deduplicated and target/animation logs occur on changes. Graph/ESM data is cached up front, voice directories/resolutions and decoded actor clips are cached, and live anchors are small bone lookups. No synchronous GPU query is introduced.

## LIP and facial investigation

The verified companion file is `ms11_greeting_0003da20_3.lip` in the exact same voice directory/basename as the OGG, 6,520 bytes. Its leading 32-bit words are 1, 21804, 1 followed by packed binary animation data (not text or a Fallout 2 phoneme table). A complete bounds-checked Fallout 3 LIP decoding specification/decoder is not established by this milestone; assigning meanings to remaining fields or phoneme timing would be a guess.

The project can solve/skinn bones and assemble FaceGen identity geometry/textures, but does not yet load/apply the runtime facial expression/mouth morph system and its phoneme/viseme mapping. Authentic lip sync requires decoding the original LIP curves/times, verifying FaceGen control/morph targets against the head TRI/related facial assets, exporting the dedicated player's actual media clock, and applying retained TRDT emotion curves. No random jaw movement or invented facial expression is used.

## Authored NPC/AI investigation

The supplied ESM has 1,647 NPC_, 3,266 PACK, 1,247 IDLE, 7,198 NAVM, one NAVI, 326 FACT and 48 CSTY records. 5,332 REFR/ACHR records contain XLKR linked-reference data. NAVM contains authored navigation geometry/connectivity; NAVI contains cross-mesh connection records (NVCI). They are not currently used for actor routing. PACK carries PKDT general/type/behavior flags, PSDT schedules, PLDT/PTDT locations/targets, CTDA and begin/end/change scripts. IDLE carries MODL, CTDA, parent/previous-sibling ANAM and animation/loop/replay metadata. These systems should be parsed as authored data, not recreated from EDID names.

Lucas AIDT bytes are `010432320039d000000000000000010000000000`: aggression 1, confidence 4, energy 50, responsibility 50, mood 0; remaining service/assistance/radius fields are retained raw. His NPC ZNAM combat style is absent; no synthetic style is assigned. The class, faction ranks and trait template rules above are retained.

Lucas's actual PKID order and PSDT schedules follow. All listed schedules have month/day-of-week -1 and date 0. Times below are the actual schedule hour/duration fields, not invented routines. An hour of -1 is unrestricted; condition/package priority still decides eligibility. Several “Observe” EDIDs are actually Travel packages, not a newly invented Observe type.

| Order | PACK | Authored EDID | Type | Hour/duration |
|---|---|---|---|---|
| 1 | 0003DA35 | MS11EscortBurke | Escort | -1 / 0 |
| 2 | 0003DA34 | MS11SimmsBattle | Dialogue | -1 / 0 |
| 3 | 0007D415 | MS11SimmsBattleTravel | Travel | -1 / 0 |
| 4 | 0003DBCE | MS11SimmsWaitForGreeting | Travel | -1 / 0 |
| 5 | 0004DDDA | MS11LucasSayHelloTravel | Travel | -1 / 0 |
| 6 | 0001E95E | MS11LucasForceGreet | Dialogue | -1 / 0 |
| 7 | 00055471 | MS11LucasPatrolBomb | Patrol | -1 / 0 |
| 8 | 00003F77 | MegLucasObserveTownPackage9x1 | Travel | 9 / 1 |
| 9 | 00003F79 | MegLucasWanderCrater10x1 | Wander | 10 / 1 |
| 10 | 00003BC2 | MegLucasLunchOutsidePackage13x1 | Eat | 13 / 1 |
| 11 | 00003F7C | MegLucasObserveTownTowerPackage14x2 | Travel | 14 / 2 |
| 12 | 00003F7F | MegLucasObserveOutTowerPackage16x1 | Travel | 16 / 1 |
| 13 | 00003F84 | MegLucasHangOutHome19x2 | Wander | 19 / 2 |
| 14 | 00003F85 | MegLucasObserveTownPackage21x2 | Travel | 21 / 2 |
| 15 | 00003F88 | MegLucasBreafastHome7x2 | Eat | 7 / 2 |
| 16 | 00003F87 | MegLucasSleepHome2x5 | Sleep | 2 / 5 |
| 17 | 000BD075 | MegLucasSandboxDefault | Sandbox | -1 / 0 |
| 18 | 00003B8F | MegLucasStandInPlaceDefault | Travel | -1 / 0 |

Representative original locations: ObserveTown → reference 00003F76; WanderCrater → 00003F78/radius 2000 game units; LunchOutside → 00003B52; Tower Travel → 00003F7B; HangOutHome → 00003F83/radius 1000; breakfast → 00003F81; sleep → 00003EA2. SandboxDefault PLDT is near-current-location type 3/radius 4000. EscortBurke has cell 00003A29 and target 00014F77; authored package end source sets Lucas's BurkeShouldShootMe variable. SayHelloTravel targets player reference 00000014/radius 150 and its source sets talking/greet variables then SayTo. ForceGreet source changes Lucas's greet variable. None of those package scripts/routines is faked or executed by this dialogue milestone.

The audit is reproducible with `python3 tools/dialogue/audit_esm.py /path/to/Fallout3.esm`. It reports record counts, Lucas PKID schedules, raw targets/conditions/scripts and representative dialogue metadata without bundling source assets.

## Full NPC simulation gap report

| Category | Original Fallout data | Current project support | Missing runtime / next milestone |
|---|---|---|---|
| Package parser | PACK PKDT/PSDT/PLDT/PTDT/CTDA, begin/end/change scripts | NPC PKID order/AIDT retained; audit reader | Typed bounds-checked PACK definitions, target/location unions, flags and scripts; implement first |
| Package scheduler | Ordered NPC PKID, schedules/conditions, quest/script packages | Lightweight activity/package state and dialogue suspension/restoration | Game-time eligibility, selection, reevaluation, completion/path ownership; script-free Travel/Sandbox scheduler next |
| Navmesh/pathfinding | NAVM vertices/triangles/edges, NAVI/NVCI cross-mesh links | Player collision and world residency; no actor navmesh routing | Parse navmesh, cell graph, A* and local corridor/avoidance; accompany Travel |
| Locomotion/root movement | Authored locomotion/turn KF and accumulation roots | Skeleton sampling, cached clip selector, turning; placement otherwise static | Collision-constrained actor translation, root-motion/speed integration and blend graph; accompany Travel |
| Doors/path transitions | DOOR teleport/local animation, cell refs and navmesh links | Working player doors/transitions | Actor requests, lock/permission handling, wait/open/pass/close and cross-cell continuation |
| Sandbox interactions | Sandbox/Wander PACK, furniture/idle markers and IDLE conditions | Actors and authored visuals load | Eligible furniture/marker reservation, reach/use/release, interruption; after routing |
| Patrol | Patrol PACK, locations and XLKR chains | Linked references audited; no patrol executor | Parse links/waypoints, patrol traversal and authored idle waits |
| Eat | Eat PACK schedules, food targets, furniture/IDLE | Inventory/visual assembly | Canonical target search, seat/use animation and package completion; no fake lunchtime |
| Sleep | Sleep PACK schedules, bed location/ownership | Actor appearance; player-state clock foundation | Bed reservation, approach/enter/exit, authored animation and wake interruptions |
| Factions/disposition | NPC SNAM ranks, FACT relations, class/karma/disposition/AIDT | Authored membership/rank conditions and retained traits | Persistent rank/relationship changes, canonical disposition/social formulas and crime reactions |
| Perception/detection | AIDT aggression/confidence/energy/responsibility/assistance/radius, game settings | Interaction ray/occlusion only | NPC senses, visibility/hearing, detection thresholds and awareness events |
| Combat AI | CSTY, AIDT, factions, packages, combat DIAL/INFO | Canonical weapon hits/actor damage and dialogue interruption hook | Threat selection, combat override, cover/position/attack decisions; supersede packages/dialogue |
| Weapon use | NPC inventory/equipment, WEAP/AMMO, actor attack/reload KF | Player physical weapons and NPC worn/equipped appearance | NPC equip/aim/fire/reload ownership, ammo, projectiles/hits and authored animation synchronization |
| Flee/pursue | PACK FleeNotCombat/Follow/Accompany and confidence/combat data | Combat target/activity placeholders | Route-based pursuit/escape, search timeout and package recovery |
| Death | Canonical health, ACBS essential flags, death items/animations | Persistent damage/health; dead Talk is rejected | Death state transition and authored death animation/inventory consequences |
| Ragdoll | Actor skeleton/collision/constraints in original NIF | GPU skeleton/skinning and hit geometry | Physics skeleton, constraints, collision response, animation-to-ragdoll handoff |
| Corpse inventory | NPC inventory, death item LVLI and actor/container ownership | Player/container transfer APIs; NPC visual inventory | Persistent corpse contents, loot targeting and canonical transfer/crime semantics |
| Essential/unconscious | ACBS essential flags and authored recovery behaviors | Existing essential damage clamp at 1 HP | Unconscious/recovery state, authored animations and temporary interaction restrictions |
| Exterior actor streaming/persistence | ACHR persistent refs, cell/world ownership and package destinations | Resident placed actor assembly and persistent actor damage | Nonresident simulation policy, live transforms/packages/equipment across cell boundaries and save/load |
| Scripts | SCPT/INFO/QUST/PACK compiled/source blocks and references | Bounded dialogue results, canonical quest/inventory APIs and variables | Verified VM commands/control flow, quest stage/package/actor script execution; reject unsupported compiled logic until implemented |

Follow, escort, guard, force-greet, observe-at-destination and combat/flee behavior must be driven by the original package types/targets/flags/scripts. A sensible next milestone is typed PACK/NAVM parsing plus a condition-driven, collision-safe script-free Travel/Sandbox executor, with dialogue as its temporary override. Broader scripts and combat can then occupy higher-priority actor states without replacing the dialogue graph or immutable visual assembly.

## Test commands and device checklist

Host: configure/build/ctest `tests/player`, `tests/nif`, `tests/assets`, `tests/world`, `tests/physics`, `tests/audio`. Original dialogue: `dialogue_tests /path/to/Fallout3.esm [directory containing original Lucas OGG/LIP]`. Original animation: `actor_animation_tests skeleton.nif talk_toplayerrelaxedb.kf mtturnleft.kf mtturnright.kf`. The optional asset tests require the user's originals; CI does not redistribute them. One existing VR-body asset test skips without its optional external assets.

On Quest: enter Megaton, aim at Lucas within 3 m, verify Talk Lucas Simms and wall occlusion, press A, inspect natural turn/live world/head tracking, hear original greeting/subtitle, select every initial option with right stick/A, follow subsequent exchanges, use B/Goodbye, walk beyond 5 m and fire a held weapon. Confirm panel/audio/focus cleanup and subsequent Pip-Boy, container, door and physical weapon behavior. Inspect event logs rather than assuming that host-only validation proves headset comfort or audio routing.

Sources for binary field/function verification: [xEdit Fallout 3 definitions](https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.5/Core/wbDefinitionsFO3.pas), [niftools KF schema](https://github.com/niftools/nifxml/blob/develop/nif.xml), [Fallout 3 GECK dialogue semantics](https://geck.uesp.net/wiki/Category:Dialogue), and the supplied original ESM/KF/OGG/LIP files. The runtime does not use external summaries in place of original record data.
