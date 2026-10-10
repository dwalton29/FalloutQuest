# NPC tactics and route congestion — v201

## Pathing

Actor-owned package and combat routes take a bounded snapshot of other living,
loaded residents and their next waypoints. A* adds a soft congestion cost near
those points. When authored NAVM has alternative corridors, a clear route can
win; an occupied sole passage remains connected and retains its original goal.
Fixed travel, patrol, furniture and XTEL endpoints are never moved. Shared
marker ownership and v200 crowd recovery remain in effect. Roaming endpoint
checks now also recognise combat actors' selected endpoints.

The snapshot holds at most 128 points. A small spatial grid limits each portal
query to its nine nearby buckets; it does not scan every resident at every A*
expansion. Radius 140 game units, peak cost 280 per point and the snapshot budget
are explicit Quest policies derived from the existing local crowd spacing,
not original engine congestion constants. Heights still separate stacked paths.
This is soft routing preference, not a recovered capsule-clearance solver.

## Combat

Weapon selection retains the canonical equipped instance through wind-up,
recovery and reload. At a free decision it prefers a supported usable weapon
whose source range reaches the target, then the existing damage-rate estimate.
Melee estimates use the loaded attack clip duration and WEAP attack multiplier.
A weapon change resets that weapon's attack, burst and melee decision timers.
Ranged wait-to-fire no longer delays melee decisions.

Cover rays originate at the actual threat aim point. The earlier implementation
added the actor's torso height to an already elevated threat point, shifting
rays above the real target. Candidate torso, head and available pelvis heights
are now checked independently; all sampled points must be hidden for cover.
Exposed firing positions still need an open torso ray and actual muzzle sight
at the hit event. Cover/escape/firing routes retain their intent type so a
cached escape route cannot masquerade as a cover route.

CSTD supplies the melee attack percentage, Choose Attack using % Chance flag,
recoil/unarmed attack bonuses and hold timer bounds. Supported melee actors
can hold rather than continually queueing another swing. Original attack clips,
hit keys, damage and persistent health remain authoritative.

The full 21-float CSAD record is decoded and validated, including legal signed
fatigue coefficients. Selected source context multipliers influence dodge
chance, forward/back preference and melee attack chance. The under-attack
signal is a loaded NPC's committed attack against this actor. Player misses,
fatigue, encumbrance, skill-derived modifier formulas and block context are not
reconstructed. Applying those named multipliers to the existing bounded Quest
NAVM decision bridge is an explicit adaptation, not Bethesda solver parity.
Exact 0% and 100% decisions are honoured. Minimum decision interval 0.1 seconds
remains a standalone scheduling bound.

## Verification and remaining work

Production regressions exercise an alternate corridor around current/planned
traffic, corpse/departed/stacked-path exclusions, sole-passage preservation,
weapon range choice and commitment, melee hold/hit events, partial cover and
correct ray-origin heights, and incompatible tactical-route reuse. Parser
checks reject invalid hold bounds and nonfinite CSAD, while accepting signed
source coefficients. Actual Fallout3.esm checks verify default CSTD/CSAD data.

Ordinary melee block/power-attack animation mappings and damage rules remain
unsupported; VATS power-attack IDLEs are not substituted. Full NPC accuracy,
hearing/sneak/light detection, essential unconsciousness, advanced NAVM rules,
capsule clearance and complete unloaded travel timing remain outstanding.
Headset acceptance should check busy Megaton routes, marker approaches, fixed
travel, moving attacks, cover height and melee pacing. Host checks cannot
establish visual fluidity or complete Fallout 3 combat parity.

Source layout reference: TES5Edit Core/wbDefinitionsFO3.pas, CSTY CSTD/CSAD/CSSD,
dev-4.1.6. Original values come from the supplied Fallout3.esm.
