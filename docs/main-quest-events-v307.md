# Main-quest script events and faction membership — v307

Baseline main fce1cb6 (v306). This is a reusable engine checkpoint driven by
Following in His Footsteps, not certification of the complete quest.

## Original-data progression covered

The supplied Fallout3.esm contains MQ01cityAmbushSCRIPT (0001D43F). Player entry
runs MQ01LD stage 45 and starts a five-second local timer. Its GameMode then runs
MQ01LD stage 50: original result scripts change actor aggression, add Sarah,
Vargas, Reddin and Colvin to MQ01BrotherhoodDCFaction at rank zero, and advance
MQ01 to stage 50. The new host test enters the actual ESM primitive, dispatches
the original event, saves/reloads partway through its timer, and checks those
results and repeat-entry protection. Production NPC combat acquisition is also
exercised with the resulting original faction memberships; actor positions,
skinned sight points and projectile/audio output remain test adapters.

The Colvin arrival test uses a valid stage-74 starting snapshot to isolate one
original checkpoint. The production NAVM Travel executor reaches the authored
marker and delivers OnPackageDone MQ01ColvinAdvanceFromCheckpoint (000223BC).
Colvin's original SCPT executes MQ01 stage 75, which executes MQ01LD stage 70.
This test does not traverse the preceding D.C. approach or establish stage 74.

MQ01BehemothSCRIPT (00020285) contains OnHitWith WeapFatMan. Canonical attack
receipt now increments its original nukeCount only for that weapon. This isolates
the weapon-filter event, not Fat Man explosion/projectile execution or the battle.

## Reusable implementation

- AddToFaction/Addfac, RemoveFromFaction/Removefac and SetFactionRank use saved,
  reference-owned membership overrides. They do not mutate NPC/CREA base records.
- GetInFaction/GetFactionRank conditions and source expressions, combat acquisition
  and assistance, retreat-to-ally checks, fast-travel danger checks and ownership
  consume the same effective memberships. Player faction inheritance uses the
  original Factions template category, rather than Stats.
- Script writes to levelled actors' AI values realize their original templates
  before accessing AIDT. This fixes the original ambush's previously rejected
  SetAV aggression commands without selecting quest-specific actor substitutes.
- The package executor sends actor SCPT OnPackageStart and OnPackageChange on
  adoption/reselection. OnPackageDone is connected to supported Travel arrival,
  Travel furniture entry, Escort destination and nonrepeating Patrol completion.
  Temporary combat/dialogue interruptions do not masquerade as package changes.
- Saved package delivery state prevents duplicate start/completion callbacks when
  a resident visual is recreated or a save is restored. Rejected callbacks remain
  rejected, with bounded retries instead of per-frame transaction reconstruction.
- OnHitWith carries the attack's weapon separately from its attacker. OnHit and
  IsActionRef retain attacker identity. Native reference publication uses the
  existing prepared transaction/coordinator path for package callbacks.
- Original-master matching now checks named event filters against original live
  EDIDs, including package/weapon filters, while retaining the separate activator.
  It rejects substituted filters and preserves the previous valid match report.
- Optional save version 27 retains effective memberships and package delivery
  state; older saves retain their supported format when these overlays are empty.

There are no MQ01 IDs, editor-name checks, fabricated dialogue or forced quest
stages in production behavior. Original IDs and checkpoint snapshots occur only
in tests and documentation.

## Validation

Focused host checks: NPC combat state/runtime, NAVM package runtime, event scripts,
source/master matching, ownership, world-script adapters, MQ01 scene progression,
MQ01 dialogue outcomes and all three MS11 bomb reward branches. Original-data
checks use the supplied master explicitly; CTest without it does not prove those
paths. Tests cover preview isolation, failed-result rollback, effective faction
consumers, saved removals, mid-timer reload, arrival replay protection and existing
Moriarty/Doctor Li/Three Dog and bomb rewards.

Android APK compilation and physical headset testing are separate from these
host checks. See the delivery message/CI run for Android status. No headset or
ADB device is connected in this environment.

## Remaining quest blockers

- Typed reference locals/expressions and full SayTo argument handling for MQ01's
  speaker/target scheduler.
- Scripted UseWeapon and the remaining scene-specific command *types*, implemented
  generically: PlaySound3D, PlaceAtMe explosion effects, DamageObject, PlayIdle/
  IsIdlePlaying and PushActorAway. In particular Reddin's arrival into stage 104
  still rejects its unsupported positional sound; it is not silently skipped.
- PACK OnEnd/OnChange result fragments and original PKDD force-greet configuration
  remain unsupported. Actor SCPT package callbacks added here are a separate
  mechanism; this checkpoint does not claim the Simms introduction is fixed.
- Complete dialogue/terminal alternative routes, full rendered D.C./GNR traversal,
  encounter timing, Behemoth entrance/battle and all Three Dog endings still need
  their remaining dependencies and end-to-end headset verification.

## Focused headset checks

Use a backed-up, valid post-Escape/MQ01 save. Follow Moriarty's original information
route and travel toward GNR. Check the basement ambush, Brotherhood participation
and checkpoint actor movement. Save/reload during the approach and revisit the
area; check for repeated arrivals or restarted one-time effects. Capture logcat
for SCRIPT PACKAGE EVENT REJECTED / NPC PACKAGE UNSUPPORTED at the first stalled
scene. Do not manually advance quest stages to hide a missing dependency.

Recheck Megaton bomb disarm/reward and an owned locked door before continuing.
