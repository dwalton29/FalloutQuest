# Original Megaton resident package inventory

Generated from the supplied Fallout3.esm with `tools/npc/audit_residents.py`. ACHR deleted/initially-disabled records are excluded; NPC_ effective AI package lists follow the authored template flag 16. This is a placement census, not a claim that every actor is currently executable. Exterior and interiors have separate scene lifetimes.

Exterior: **3**. Exterior plus Megaton interiors: **37**.

| Relevant package type | Entries in effective lists |
| --- | ---: |
| Travel | 57 |
| Sandbox | 90 |
| Eat | 33 |
| Sleep | 36 |
| Dialogue | 30 |
| Use Item At | 1 |
| Patrol | 4 |
| Guard | 2 |
| Follow | 4 |
| Accompany | 2 |
| Wander | 11 |
| Escort | 1 |

Counts include shared package assignments and inactive quest/schedule branches. They are not simultaneously active package counts.

| NPC | Reference | CELL | Package count |
| --- | --- | --- | ---: |
| Walter | `00003B59` | `00003A34` MegatonWaterProcessingPlant | 7 |
| Moira Brown | `0002D2BC` | `00003A2A` MegatonCratersideSupply | 9 |
| Mercenary | `0001FF18` | `00003A2A` MegatonCratersideSupply | 3 |
| Gob | `00003B3D` | `00003A35` MegatonMoriartysSaloon | 5 |
| Colin Moriarty | `00003B3C` | `00003A35` MegatonMoriartysSaloon | 9 |
| Nova | `00003B3F` | `00003A35` MegatonMoriartysSaloon | 8 |
| Jericho | `00003B5D` | `00003A2C` MegatonJerichosHouse | 24 |
| Child of Atom | `00041720` | `00003A2D` MegatonChildrenofAtom | 1 |
| Child of Atom | `0004171F` | `00003A2D` MegatonChildrenofAtom | 1 |
| Mother Maya | `00003B49` | `00003A2D` MegatonChildrenofAtom | 6 |
| Confessor Cromwell | `00003B48` | `00003A2D` MegatonChildrenofAtom | 2 |
| Doc Church | `00003B56` | `00003A2E` MegatonClinic | 8 |
| Patient | `00042866` | `00003A2E` MegatonClinic | 1 |
| Patient | `00042867` | `00003A2E` MegatonClinic | 1 |
| Patient | `00042868` | `00003A2E` MegatonClinic | 1 |
| Andy Stahl | `00003B55` | `00003A2F` MegatonTheBrassLantern | 8 |
| Leo Stahl | `00003B54` | `00003A2F` MegatonTheBrassLantern | 4 |
| Jenny Stahl | `00003B53` | `00003A2F` MegatonTheBrassLantern | 4 |
| Lucy West | `00003B5B` | `00003A31` MegatonLucyWestsHouse | 6 |
| Manya | `00003B58` | `00003A32` MegatonNathanandManyasHouse | 5 |
| Nathan | `00003B57` | `00003A32` MegatonNathanandManyasHouse | 14 |
| Megaton Settler | `000208FB` | `00004357` MegatonCommonHouse | 8 |
| Megaton Settler | `000208FA` | `00004357` MegatonCommonHouse | 8 |
| Megaton Settler | `000208F9` | `00004357` MegatonCommonHouse | 8 |
| Megaton Settler | `000208F8` | `00004357` MegatonCommonHouse | 8 |
| Megaton Settler | `000208F7` | `00004357` MegatonCommonHouse | 3 |
| Megaton Settler | `00014F1A` | `00004357` MegatonCommonHouse | 8 |
| Megaton Settler | `000043C5` | `00004357` MegatonCommonHouse | 8 |
| Megaton Settler | `000043CB` | `00004357` MegatonCommonHouse | 8 |
| Megaton Settler | `000043C9` | `00004357` MegatonCommonHouse | 8 |
| Megaton Settler | `000043C7` | `00004357` MegatonCommonHouse | 8 |
| Harden Simms | `00003B45` | `00003A29` MegatonLucasSimmsHouse | 9 |
| Maggie | `00003B5C` | `00003A33` MegatonBillyCreelsHouse | 7 |
| Billy Creel | `00003B5A` | `00003A33` MegatonBillyCreelsHouse | 9 |
| Mister Burke | `00014F77` | `00000A96`  | 21 |
| Lucas Simms | `00003B46` | `00000A96`  | 18 |
| Stockholm | `0001942F` | `00000A96`  | 5 |

## Moira Brown — `0002D2BC`

| Priority | PACK | Type | Editor ID | Daily schedule `(hour,duration)` | Location `(type,value,radius)` |
| ---: | --- | --- | --- | --- | --- |
| 1 | `0005CFC0` | Sandbox | MoiraGhoulSandboxDefault | `(-1, 0)` | `[0, 335479, 600]` |
| 2 | `0005CFBD` | Dialogue | MoiraGhoulWaitForPlayer | `(-1, 0)` | `[0, 380862, 1000]` |
| 3 | `0003E5DA` | Use Item At | MS03EntryMegaton | `(-1, 2)` | `[0, 255440, 110]` |
| 4 | `00027F5D` | Travel | NPCVendorFollowPlayerTriggerPackage | `(-1, 0)` | `[2, 0, 500]` |
| 5 | `00004153` | Sandbox | MegMoiraOfferServices8x12 | `(8, 12)` | `[0, 487250, 275]` |
| 6 | `00004155` | Eat | MegMoiraEatHome21x2 | `(21, 2)` | `[0, 88194, 0]` |
| 7 | `00004156` | Sleep | MegMoiraSleep0x2 | `(0, 2)` | `[0, 15733, 25]` |
| 8 | `00004157` | Sandbox | MegMoiraDefaultPackage | `(-1, 0)` | `[0, 16351, 1000]` |
| 9 | `0003E5DB` | Travel | MegMoiraOfferServices215Default | `(-1, 0)` | `[0, 791386, 0]` |

## Gob — `00003B3D`

| Priority | PACK | Type | Editor ID | Daily schedule `(hour,duration)` | Location `(type,value,radius)` |
| ---: | --- | --- | --- | --- | --- |
| 1 | `0006ED0C` | Travel | MS11GobWatchBurkeKillSimms | `(-1, 0)` | `[0, 95899, 10]` |
| 2 | `0001769C` | Travel | MegGobRadioComplain | `(-1, 0)` | `[0, 191598, 0]` |
| 3 | `0006A073` | Patrol | MegGobSweep21x3 | `(21, 3)` | `[0, 434223, 0]` |
| 4 | `00003FA0` | Sleep | MegGobSleepOwnBed4x4 | `(4, 4)` | `[3, 0, 0]` |
| 5 | `00003FA3` | Sandbox | MegGobTendBarMoriartysPackageBASE | `(-1, 0)` | `[0, 16296, 300]` |

## Colin Moriarty — `00003B3C`

| Priority | PACK | Type | Editor ID | Daily schedule `(hour,duration)` | Location `(type,value,radius)` |
| ---: | --- | --- | --- | --- | --- |
| 1 | `0006ED0E` | Travel | MS11ColinMoriarityWatchBurkeKillSimms | `(-1, 0)` | `[0, 453901, 10]` |
| 2 | `0003DA30` | Dialogue | MS11FinDeliverLetter | `(-1, 0)` | `[0, 453901, 50]` |
| 3 | `00003F95` | Travel | MegMoriartyBalcony8x2 | `(8, 2)` | `[0, 16276, 0]` |
| 4 | `00003F96` | Travel | MegMoriartyUseComputer10x1 | `(10, 1)` | `[0, 16272, 0]` |
| 5 | `00003F9C` | Sandbox | MegMoriartyWatchBarPackage13x1 | `(13, 3)` | `[0, 16296, 375]` |
| 6 | `00003F99` | Travel | MegMoriartyBalconyEvening17x3 | `(17, 3)` | `[0, 16276, 0]` |
| 7 | `000664A3` | Sandbox | MegMoriartyTendBar4x4 | `(4, 4)` | `[0, 16296, 375]` |
| 8 | `00003F9F` | Sleep | MegMoriartySleep0x4 | `(0, 4)` | `[0, 16271, 0]` |
| 9 | `00003F9E` | Sandbox | MegMoriartySamdboxBarDefault | `(-1, 0)` | `[0, 434220, 400]` |

## Nova — `00003B3F`

| Priority | PACK | Type | Editor ID | Daily schedule `(hour,duration)` | Location `(type,value,radius)` |
| ---: | --- | --- | --- | --- | --- |
| 1 | `0001769E` | Travel | MegNovaRadioStand | `(-1, 0)` | `[0, 96504, 0]` |
| 2 | `00003FC6` | Sleep | MegNovaSleepOwnBed2x4 | `(2, 4)` | `[0, 16275, 0]` |
| 3 | `0006A075` | Eat | MegNovaEatPackage16x1 | `(16, 1)` | `[0, 88055, 0]` |
| 4 | `0006DDEF` | Dialogue | MegNovaTalkBillyCreel | `(-1, 0)` | `None` |
| 5 | `0006DDF1` | Dialogue | MegNovaTalkJericho | `(-1, 0)` | `None` |
| 6 | `0006DDF0` | Dialogue | MegNovaTalkNathan | `(-1, 0)` | `None` |
| 7 | `0006DDF2` | Dialogue | MegNovaTalkMoriarty | `(-1, 0)` | `None` |
| 8 | `00003FC8` | Travel | MegNovaDefaultMoriartys | `(-1, 0)` | `[0, 434274, 0]` |

## Jericho — `00003B5D`

| Priority | PACK | Type | Editor ID | Daily schedule `(hour,duration)` | Location `(type,value,radius)` |
| ---: | --- | --- | --- | --- | --- |
| 1 | `00028044` | Dialogue | DEMOMegJerichoDialogueJennyStahl | `(-1, 0)` | `[0, 103723, 210]` |
| 2 | `00028D97` | Travel | DEMOMegJerichoStandNearBrassLantern | `(-1, 0)` | `[0, 103723, 0]` |
| 3 | `000B7624` | Travel | FollowersJerichoFiredWaitOusideMegaton | `(-1, 0)` | `[0, 751139, 0]` |
| 4 | `0004083A` | Travel | FollowersJerichoFiredWaitMoriartys | `(-1, 0)` | `[0, 16274, 0]` |
| 5 | `00071EE0` | Guard | FollowersJerichoFollowPlayerWAITsmoke | `(-1, 0)` | `[2, 0, 0]` |
| 6 | `00071EE1` | Follow | FollowersJerichoFollowPlayerLONGsmoke | `(-1, 0)` | `None` |
| 7 | `00071EE2` | Follow | FollowersJerichoFollowPlayerDEFAULTsmoke | `(-1, 0)` | `None` |
| 8 | `0004083E` | Guard | FollowersJerichoFollowPlayerWAIT | `(-1, 0)` | `[2, 0, 0]` |
| 9 | `0004083C` | Follow | FollowersJerichoFollowPlayerLONG | `(-1, 0)` | `None` |
| 10 | `0004083B` | Follow | FollowersJerichoFollowPlayerDEFAULT | `(-1, 0)` | `None` |
| 11 | `0001F941` | Dialogue | MegJerichoTalkNova9x2 | `(9, 0)` | `None` |
| 12 | `0001F946` | Dialogue | MegJerichoTalkGob9x2 | `(9, 0)` | `None` |
| 13 | `0000400F` | Eat | MegJerichoDrinkMoriartys9x2 | `(9, 2)` | `[0, 88056, 0]` |
| 14 | `00004010` | Sandbox | MegJerichoWanderOutside11x2 | `(11, 2)` | `[0, 103728, 1000]` |
| 15 | `00004011` | Eat | MegJerichoEatBrassLantern13x2 | `(13, 2)` | `[0, 103723, 384]` |
| 16 | `00004012` | Sandbox | MegJerichoSandboxOutsideMoriatrys15x3 | `(15, 3)` | `[0, 103728, 8000]` |
| 17 | `0001F942` | Dialogue | MegJerichoTalkNova18x3 | `(9, 0)` | `[3, 0, 0]` |
| 18 | `0001F945` | Dialogue | MegJerichoTalkGob18x3 | `(9, 0)` | `[3, 0, 0]` |
| 19 | `00004013` | Sandbox | MegJerichoDrinkMoriartys18x3 | `(18, 3)` | `[0, 16216, 800]` |
| 20 | `00004014` | Sandbox | MegJerichoWanderOutsideDrunk21x3 | `(21, 3)` | `[0, 16248, 1000]` |
| 21 | `0001F943` | Dialogue | MegJerichoTalkNova0x2 | `(9, 0)` | `[3, 0, 0]` |
| 22 | `0001F944` | Dialogue | MegJerichoTalkGob0x2 | `(9, 0)` | `[3, 0, 0]` |
| 23 | `00004015` | Eat | MegJerichoDrinkMoriartys0x2 | `(0, 2)` | `[0, 16274, 0]` |
| 24 | `00004017` | Sleep | MegJerichoSleepHome2x7 | `(2, 7)` | `[0, 15992, 110]` |

## Doc Church — `00003B56`

| Priority | PACK | Type | Editor ID | Daily schedule `(hour,duration)` | Location `(type,value,radius)` |
| ---: | --- | --- | --- | --- | --- |
| 1 | `00078FE7` | Dialogue | DoctorGreetPlayer | `(-1, 0)` | `[2, 0, 500]` |
| 2 | `00078F40` | Travel | DoctorHoldPosition | `(-1, 0)` | `[2, 0, 0]` |
| 3 | `00027F5D` | Travel | NPCVendorFollowPlayerTriggerPackage | `(-1, 0)` | `[2, 0, 500]` |
| 4 | `0006A589` | Travel | MegDocChurchGreetPC9x12 | `(9, 12)` | `[0, 16149, 0]` |
| 5 | `00004148` | Sandbox | MegDocChurchSandboxClinic9x12 | `(9, 12)` | `[0, 16715, 4000]` |
| 6 | `0000414D` | Eat | MegDocChurchEat21x3 | `(21, 3)` | `[0, 16715, 4000]` |
| 7 | `0000414E` | Sleep | MegDocChurchSleep0x9 | `(0, 9)` | `[0, 15603, 0]` |
| 8 | `0000414F` | Sandbox | MegDocChurchDefault | `(-1, 0)` | `[0, 16715, 4000]` |

## Lucas Simms — `00003B46`

| Priority | PACK | Type | Editor ID | Daily schedule `(hour,duration)` | Location `(type,value,radius)` |
| ---: | --- | --- | --- | --- | --- |
| 1 | `0003DA35` | Escort | MS11EscortBurke | `(-1, 0)` | `[1, 14889, 0]` |
| 2 | `0003DA34` | Dialogue | MS11SimmsBattle | `(-1, 0)` | `None` |
| 3 | `0007D415` | Travel | MS11SimmsBattleTravel | `(-1, 0)` | `[1, 14901, 0]` |
| 4 | `0003DBCE` | Travel | MS11SimmsWaitForGreeting | `(-1, 0)` | `[0, 15246, 0]` |
| 5 | `0004DDDA` | Travel | MS11LucasSayHelloTravel | `(-1, 0)` | `[0, 20, 150]` |
| 6 | `0001E95E` | Dialogue | MS11LucasForceGreet | `(-1, 0)` | `None` |
| 7 | `00055471` | Patrol | MS11LucasPatrolBomb | `(-1, 0)` | `[0, 349290, 0]` |
| 8 | `00003F77` | Travel | MegLucasObserveTownPackage9x1 | `(9, 1)` | `[0, 16246, 0]` |
| 9 | `00003F79` | Wander | MegLucasWanderCrater10x1 | `(10, 1)` | `[0, 16248, 2000]` |
| 10 | `00003BC2` | Eat | MegLucasLunchOutsidePackage13x1 | `(13, 1)` | `[0, 15186, 0]` |
| 11 | `00003F7C` | Travel | MegLucasObserveTownTowerPackage14x2 | `(14, 2)` | `[0, 16251, 0]` |
| 12 | `00003F7F` | Travel | MegLucasObserveOutTowerPackage16x1 | `(16, 1)` | `[0, 16251, 0]` |
| 13 | `00003F84` | Wander | MegLucasHangOutHome19x2 | `(19, 2)` | `[0, 16259, 1000]` |
| 14 | `00003F85` | Travel | MegLucasObserveTownPackage21x2 | `(21, 2)` | `[0, 16246, 0]` |
| 15 | `00003F88` | Eat | MegLucasBreafastHome7x2 | `(7, 2)` | `[0, 16257, 0]` |
| 16 | `00003F87` | Sleep | MegLucasSleepHome2x5 | `(2, 5)` | `[0, 16034, 0]` |
| 17 | `000BD075` | Sandbox | MegLucasSandboxDefault | `(-1, 0)` | `[3, 0, 4000]` |
| 18 | `00003B8F` | Travel | MegLucasStandInPlaceDefault | `(-1, 0)` | `[2, 0, 0]` |

## Blocking semantics

Eat (33 assignments) and Sleep (36) are major missing procedures. Dialogue packages (30) require scene/quest procedure semantics distinct from player dialogue. Use Item At (1) and Accompany (2) also remain unsupported. Cross-cell destinations, unsupported CTDA/script commands and levelled statistic templates can reject an otherwise recognized package. Sandbox currently picks reachable points; it does not implement furniture/idle reservations or complete Bethesda action selection.

Stockholm (`0001942F`, base `00003B1C`) has statistics template `LvlHunter` (`0003A586`), which inherits statistics through `VarWastelander` (`0002E2A4`, LVLN). The canonical levelled actor spawn resolver is not implemented. Its health is unavailable; this is now logged explicitly rather than masquerading as a completed stationary AI actor.
