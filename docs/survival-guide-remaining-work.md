# MS03 remaining dependency map

Source: supplied Fallout3.esm QUST/SCPT/INFO/TERM/ENCH/MGEF and reference data. This is a development map, not a completion claim. The original MS03 catalogue contains 462 INFOs in 118 topics; task order and alternatives remain controlled by those conditions and scripts.

| Task | Original gameplay dependency | Current continuation checkpoint / next work |
| --- | --- | --- |
| Radiation | Environmental/ingestible radiation, quest GameMode thresholds (executable comparisons 200/600), Moira treatment, mutation reward | Canonical AV reads, source healing, supported instant aid and persisted treatment restrictions added. Environmental sources, complete treatment path and mutation effects still require original-data gameplay verification. |
| Minefield | Merry-go-round GameMode distance, Arkansas UseWeapon/StartCombat, mines and optional mine return | Existing reference location/GameMode and item state can be reused. Combat command delivery and mine gameplay remain to connect. |
| Mole-rat repellent | Weapon ENCH, scripted effect start/finish, typed reference locals, faction checks, timed kill/dismember arguments, counters | Requires shared weapon magic/effect execution and typed reference state; effects must not be replaced with a quest-specific hit counter. |
| Mirelurk observation | Spawning-ball OnActivate, original MESG choice, item removal, OnDeath Player bonus failure | Existing message/event/kill transactions are available. Need end-to-end original fixture and physical activator admission. |
| Injury | Health percentage/limb condition GameMode, actual damage, Moira treatment | Canonical queries/limb state/ResetHealth and treatment control primitives added. Limb damage from real attacks/falls, medical sequence and restart conversation still require route verification. |
| Rivet City history | Original Bannon/Vera/Seagrave/Belle/Pinkerton INFOs, map/cell/disposition conditions, correct/incorrect report and discount | Reuse dialogue and world traversal. Audit unsupported conditions and run original alternate reports through final reward. |
| RobCo | Mainframe OnActivate/message, telemetry unit, qualified Activate with non-player activator, robot/turret scripts, world light/mist changes, terminal faction choices | Shared messages, controller actions, faction relations and world publication exist. Activation semantics and robot gameplay must be connected. |
| Arlington library | Yearling dialogue/death route, catalogue/archive terminals, authored holotapes, bonus and turn-in | Reuse original terminals, inventory and dialogue. Verify all original terminal results and death alternative. |
| Book ending | Chapter transitions, style/bonus tallies, XP/karma, Survivor perk variant and active effects, authored book/note, news and local achievement | Shared journal, XP, karma and note state exist. Add missing result commands/effect application and original full-route/duplicate-reward verification. |

Speech/expertise shortcuts, lies, abandoning the book, Moira's ghoul/death variations, timing and task orders need explicit route coverage. No forced stage or variable seed may stand in for earning a checkpoint. Preserved Super-Duper Mart, Megaton bomb and Following in His Footsteps tests remain regression checks.
