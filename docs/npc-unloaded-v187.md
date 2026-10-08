# v187 — Bounded unloaded NPC package scheduling

Baseline: v186 `a0ddf077598556a9712083b189d6848383ca9d36`.

## Implemented checkpoint

`fo3unloaded::Scheduler` reuses the same test game clock as the
resident package executor (`GetFo3TimeOfDayHour`). It processes tracked
canonical actor states in sorted, capped batches of **64 actors per frame**,
only after the in-game minute advances. There is no off-scene animation,
collision scene, GPU mesh, NAVM graph or background thread.

For tracked actors outside the current player's resident CELL:
- Read effective ordered NPC PACK assignments using the canonical
  template/inheritance resolver.
- Evaluate original schedules, including overnight hour wrapping. Because
  FalloutQuest has only the 24-hour testing clock, month/day/date-gated
  packages remain unsupported.
- Evaluate original CTDA using canonical saved actor context and live
  player/quest state. Unsupported functions or contexts fail closed.
- Only pure location-directed Travel, Wander, Sandbox, Eat and Sleep intents
  (types 6/14, 5, 12, 3, 4) with a real PLDT reference are eligible.
  Scripts, Use Item At and other interaction/action procedures remain
  resident-only; no invented results or off-screen object activations.
- For a newly selected package with a different target CELL, traverse **one**
  authored, accessible XTEL hop using a directed BFS graph, validating
  source/destination disable flags and the NPC's own door/key/script
  permission. Save the original XTEL destination position and yaw through
  `Player::UpdateActor`. Only one hop is allowed per game minute.
- When the NPC reaches the target CELL, preserve the position from its
  last real XTEL arrival. Furniture, marker alignment and movement within
  the unloaded cell are not invented; those resume through the resident
  runtime when the player enters the CELL.
- Actor death/unknown health, hostility/combat, unsupported packages,
  absent or locked doors and missing destinations never trigger remote moves.
  The current resident CELL is excluded so resident actors keep full
  dialogue, NAVM, furniture and combat ownership.
- Actor state changes are persisted in existing v186 save format; no new
  record fields or separate snapshot duplication is introduced. On
  restart, the scheduler starts at the current game minute without
  fabricating offline real-time progression.

## Limitations

This is **tracked-actor scheduling**, not a full world simulation. Actors
not yet registered into `Player::Snapshot().actors` are not automatically
seeded from all ACHR placements in Fallout3.esm. Full game-wide actor census,
unknown levelled actors, external wasteland grid roaming, off-screen combat,
door unlock animations, cross-cell dynamic Follow/Escort/Accompany targets,
package result scripts, and exact unloaded NAVM travel-time modelling are
not implemented.

The current clock is a testing aid, not a persistent Gregorian date/calendar.
For that reason dated PACKs are not approximated. An NPC can follow authored
home/work CELL intent across XTEL, but it should not be described as
performing full sleeping/eating animations when unloaded.

## Validation

The standalone host target `npc_unloaded_runtime_tests` uses an
original-format ESM fixture with actual XTEL field semantics. It tests
hour windows, overnight schedules, package priority, directed door
transitions, canonical position/yaw, no duplicate hop in one minute, and
fail-closed door/CTDA/script/death/combat/resident restrictions.

Also run the existing v185 XTEL and v186 live handoff regressions, the full
player and world host suites and GitHub Android APK CI.

Quest acceptance: install v187, use the in-game test clock to advance through
a resident's authentic package hours after leaving their CELL, revisit
home/work and confirm the NPC appears only once with the correct package
and no stale root. This remains unverified until tested in-headset.

## Next checkpoint

Resolve untracked original ACHR census/activation and exterior persistent
cell residency, then validate multi-day schedules once the game supports a
canonical full calendar clock. Keep these separate from the initial
off-scene scheduler.
