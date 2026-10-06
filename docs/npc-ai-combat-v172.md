# Resident NPC combat runtime — version 172

Version `0.46.0-npc-combat-runtime`, built from canonical main
`553cded5a15ba81aaeebd290ee38fe2e88177246`. This is a substantial resident-actor
milestone, **not completion of Fallout 3 AI parity or all requested acceptance
criteria**. The package/procedure and NPC door work below remains unfinished.
No Bethesda assets are included in the repository or APK.

## Source-data audit

The supplied original Fallout3.esm was inspected before implementing the runtime.
It contains 1,647 NPC_, 3,266 PACK, 48 CSTY, 160 WEAP, 237 ARMO, 326 FACT and
1,247 IDLE records. Layouts were checked against
[xEdit's Fallout 3 definitions](https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.6/Core/wbDefinitionsFO3.pas),
its shared enums, and [NifTools nif.xml](https://github.com/niftools/nifxml/blob/develop/nif.xml).
New Vegas combat-style layouts were not used.

| Original data | Consumption and boundary |
| --- | --- |
| NPC_ ACBS | Stats, level/PC-level multiplier, template categories and Traits essential bit; existing derived-health GMSTs remain canonical |
| NPC_ AIDT, 20 bytes | Aggression 0, confidence 1, energy 2, responsibility 3, assistance 14, aggro-radius flag 15 and radius 16; energy/responsibility are decoded, not a complete crime/fatigue simulation |
| NPC_ CNTO/COED | Canonical actor-owned stacks using the existing container and LVLI expansion/transfer machinery; unsupported ownership/global/rank metadata fails explicitly |
| NPC_ PKID | Authored priority order, category inheritance, schedule and condition evaluation |
| SNAM / FACT XNAM | Membership/rank and faction combat reactions. XNAM offset 8 is combat reaction; offset 4 disposition is not substituted for hostility |
| ZNAM / CSTY | Traits combat style; CSTD flags and FO3 64-byte CSSD weapon restriction, range, wait/fire timers, combat radius and semi-auto delay fields decoded. Restrictions, maximum range and minimum semi-auto delay currently drive decisions; other decoded tactics await execution |
| NPC_ RNAM/CNAM/TPLT/DNAM | Existing race/class/template definitions retained; 14 explicit skill bytes reused for damage. Full autocalculated class/race skills and actor-value effects remain unsupported |
| PACK PKDT/PSDT/PLDT/PTDT | General/type/behavior flags, schedule, locations, specific reference targets, conditions and script-content rejection. Empty SCHR alone remains script-free |
| WEAP / AMMO / PROJ | Existing canonical definitions: inventory instance/condition, ammo, clip, NPCs Use Ammo bit, fire rate, attack/reload groups, spread, pellet count, damage and projectile/hitscan definitions |
| ARMO DNAM | Signed 16-bit authored DR divided by 100; `fMaxArmorRating` cap. Derived armour-condition/skill/effect adjustments remain unsupported |
| IDLE / KF / NIF | Existing authored skeleton, conversation, movement and blending; original WEAP attack-group enum and KF sequence names; Weapon bone and ProjectileNode muzzle |
| NAVM / NAVI | Existing NVTR/NVEX graph, validated reciprocal cross-mesh portals and surface grounding reused. Cover and door-link metadata are not decoded |
| Levelled records | Existing LVLI rolls become persistent actor inventory once generated. LVLN/LVLC actor spawning and inherited levelled actor templates remain unsupported |

CSTY observations: all 48 original records have CSTD 92, CSAD 84 and CSSD 64
bytes. PACK types observed include Follow 30, Escort 115, Eat 296, Sleep 246,
Wander 42, Travel 907, Flee 20, Guard 25, Sandbox 681 and Patrol 172. An
unsupported package is not silently marked complete; lower-priority supported
packages can still execute. Begin/end/change compiled or source scripts reject
that package. Full procedure flags, result scripts and idle/furniture selection
are not implemented.

## Ownership and state

The existing RuntimeState now explicitly owns Idle, Package, Dialogue, Combat,
Dying, Dead and reserved Unconscious activities. It stores target, action,
weapon instance, timers, last-seen threat and suspended ordinary package.
Combat supersedes dialogue, interrupts its session, and suppresses Talk.
Combat ending restores the ordinary package at the current runtime transform;
no return to ACHR spawn coordinates occurs. Death clears attack/reload/path and
dialogue ownership. Authored source definitions remain immutable.

Player remains the canonical gameplay store, including NPC damage, actor
inventory, equipment instance and persistent position/hostility. Actor contents
use the **existing state.containers stack store**, not a parallel combat or corpse
inventory. The visual source inventory is no longer the mutable ammunition owner.

## Package and navigation execution

Travel, Wander/Sandbox NAVM movement, Follow to specific resident-scene reference
(or player) and Guard travel-and-hold are implemented. Wander/Sandbox retains the
existing bounded deterministic reachable NAVM destinations inside authored PLDT
radius; it does not yet execute Fallout's furniture/eating/sleeping procedures.
Follow refreshes its destination when the resolved target moves and stops at the
authored PTDT distance. Guard is limited to the authored location; full guard
challenge/crime behavior is not implemented.

Escort, Patrol, authored Flee packages, Eat, Sleep, Dialogue packages and ForceGreet
remain unsupported. Combat coward fleeing is separate from an authored Flee
package. Object-type, object-ID and linked-reference PTDT targets are unsupported.
Month/date/weekday schedules require a canonical game calendar; the inherited
schedule executor only handles its supported daily hour windows and diagnoses
unsupported calendar constraints.

Pursuit/search and coward flee reuse the same authored NAVM path and grounding
executor as packages. Flee searches reachable graph nodes for an endpoint farther
from the remembered threat. It is a bounded interim route policy, not Bethesda's
full escape destination scoring. Replans are rate-limited to 0.75 seconds and
stationary pursuit paths are retained while valid. Assets and NAVM graphs are
prepared outside combat ticks; no ESM scan, BSA read or KF/NIF decode occurs while
firing. Only currently resident NPC visuals tick at high frequency.

A movement segment is rejected when the existing resident world occlusion surface
blocks the chest-bone trajectory. This is a conservative wall/door guard, not a
complete actor capsule sweep or crowd/avoidance implementation. Locomotion speed
remains the existing explicit 1.05 m/s VR bridge pending movement actor values.

## Doors and nonresident actors

The audited XTEL activation infrastructure queues **player-owned** scene/camera
transitions. It is deliberately not called by NPCs. Local door activation,
NAVM door links and independent NPC load-door transfers remain unimplemented;
blocked movement is diagnosed rather than walking through the resident obstruction.

Nonresident actors freeze their canonical game-coordinate transform, package
identity/progress, damage, inventory and hostile consequence. They are not
physically simulated in an unavailable scene. Routes rebuild on return using
NAVM instead of persisting pointers. Existing residency restrictions are unchanged:
explicit supported interiors and the existing supported exterior world; this
change does not add Capital Wasteland actor streaming. Cross-cell actor residency
migration/independent load-door transfers remain a required follow-up. Existing
resident actor unload/reload preserves its transform, but this is not full
arbitrary cross-CELL simulation.

## Detection and combat decisions

AIDT aggression and FACT reactions determine proximity acquisition. Ordinary
aggressive actors acquire enemies; very aggressive actors also acquire neutrals;
frenzied actors can acquire everyone. Assistance uses allied/friendly actors'
resident combat targets. Nearby friendly actors alone do not establish hostility.
Successful damage records an explicit attacker as a retaliation threat.

AIDT aggro radius or original `fSneakMaxDistance=2500` bounds acquisition, with
existing world line-of-sight rejection. This is a bounded visibility bridge;
light, sneak skill, sound, disposition/crime tolerance, detection actor values and
full Bethesda search mechanics are not implemented. Friendly accidental damage
currently establishes retaliation immediately; assault tolerance/crime semantics
remain a parity gap. Acquisition runs at 4 Hz. Lost visibility retains the last
known threat for an explicit 15-second runtime policy; target death/unavailability
ends combat. This duration is not represented as a verified Fallout GMST.

The runtime chooses a usable supported weapon from real canonical inventory using
condition, damage/rate and CSTY weapon restrictions. It faces the threat, pursues
when outside authored WEAP/CSTY maximum range, searches the last seen location,
checks muzzle-to-target world obstruction, attacks, reloads, switches when ammo or
weapon availability requires it, and flees for confidence Coward. Cautious/Average
health/strength comparisons, burst/cover tactics, retreat to minimum range,
unarmed fallback, full weapon scoring and advanced repositioning remain unfinished.

## Weapon, animation and ammunition boundaries

Shots originate from the authored skeleton Weapon attachment and weapon NIF
ProjectileNode, not the actor root. Pistol/rifle/automatic/shotgun/energy groups
use the original WEAP attack enum at DNAM byte 41, reload byte and validated KF
sequence names. Melee DEFAULT uses one verified original AttackRight_A variant;
Bethesda's full directional/variant selection remains unsupported. Missing original
models, muzzle nodes or required clips disable that weapon with diagnostics.
Heavy/explosive/projectile semantics remain excluded. There is no invented cover.

NPC and player ranged fire share EmitShot, moving flights, skinned surface-hit
selection and ApplyAttack. FireReady enforces WEAP rate/delay and CSTY minimum
semi-auto delay without catch-up volleys. Reload blocks firing and waits for the
larger of authored WEAP time and original clip duration; original Sound text keys
are preloaded and played. NPC gun degradation, jamming and exact animation event
cadence/attack-speed parity remain unfinished. Muzzle direction targets the threat;
full pitch retargeting of authored arm aim is still required on headset.

WEAP NPCs Use Ammo consumes reserve rounds into finite clips. Otherwise a compatible
inventory round authorizes effectively unlimited NPC magazines; virtual magazine
rounds are cleared before corpse loot and cannot become player ammunition. This
bounded policy needs follower/runtime parity verification; no ammo item is inserted
into actor inventory. Finite clip rounds remain attached to their unique weapon
instance during corpse transfer.

Original KF tracks and actor bones are used with the existing blend system. Extra
weapon-only tracks (e.g. ##Clip, ##Bolt, ##Slide) do not bind to the humanoid skeleton
and need a separate weapon-part pose adapter. Walk/conversation playback remains
unchanged. Combat aim/attack/reload and one original death pose are cached in the
scene worker. Hit reactions, combat-specific locomotion, full animation-group
variants and weapons held outside combat remain unfinished.

## Damage, death and loot

WeaponDamage and ActorWeaponDamage share one skill/condition/base-damage formula
and original GMST coefficients. ApplyAttack handles player or actor attackers and
player or actor targets, no self-hit, finite positive damage, target DR, essential
protection and explicit hostility. Player damage calls existing DamageHealth;
there is no separate combat-health variable. Moving projectiles retain captured
attacker/damage even if the shooter subsequently dies.

NPC armour DR sums nonconflicting inventory armour slots; player DR uses equipped
armour. These are base authored ratings with cap 85, not complete vanilla derived
armour ratings. Criticals, difficulty, perk/magic/script effects, armour-condition
adjustments, resistance types, automatic class skills and limb health remain
unsupported. Damage is not claimed as a complete vanilla formula.

NPC triangles retain stable body-region classifications based on weighted original
Bip01 skin bones (head, torso, left/right arms/legs, unknown). Hit diagnostics include
that region. BPTD authored damage multipliers and VR-player region classification
are not yet connected; no arbitrary head multiplier or world-space body boxes are
invented. These regions provide a basis for later reactions/crippling.

Nonessential actors enter Dying then Dead, stop ordinary/combat/dialogue ownership,
keep their final runtime transform and settle into the original IDLE deathpose1 KF
when available. This short authored death pose is an interim corpse, not a Havok
ragdoll or directional death selection. Without that installed asset, the actor
remains at its last available pose; a full death-animation fallback set is pending.
Essential Traits flags preserve 1 HP and reject corpse loot. Unconscious activity is
reserved but recovery/knockdown is not claimed implemented.

Dead actors use the existing floating VR loot panel and stack transfer APIs.
Removing a stack persists; transferring a weapon preserves its unique ownership
and finite clip, without a second dropped weapon. Worn meshes currently remain
visible after armour is looted; death items/LVLI, dropped-equipment visuals,
crime/search ownership and respawn/cleanup are follow-ups.

## Saves, verification and diagnostics

FQPS revision 7 appends sorted, validated actor records: FormID, cell/world,
package/progress, explicit hostile reference, game coordinates, yaw and equipped
weapon instance. Existing actorDamage and container stack/weapon extensions retain
health, corpse contents and magazines. Revisions 1–6 remain readable; existing
catalog fingerprints stay unchanged. Invalid counts, duplicate actors, nonfinite
positions, dangling equipment and malformed data reject the restore transaction.
No render state, tracking transform, transient clip or raw pointer is serialized.

Host tests include production package selection/schedules/dialogue suspension and
NAVM portal/grounding regressions; new canonical combat state/ownership tests;
production combat decision tests with recording pose/audio/projectile adapters;
LOS rejection, fire timing, reload, pursuit/search/flee, death, essential protection,
corpse transfers, position/equipment restore, legacy v6 migration and corruption
rejection. Existing player weapon/dialogue/inventory/body/collision suites pass.
The combat emitter adapter tests canonical damage dispatch; a complete native
NPC/player skinned hitscan/projectile integration fixture is still required.

Original-data integration verified six representative player weapon definitions
(10mm, hunting rifle, assault rifle, combat shotgun, laser pistol/rifle), original
Lucas greeting graph, original NAVM packages, five inventory-armed NPC references
(2A2BD, 2A2BE, 2A2C1, 2A2C5, 32111) and 11 original humanoid combat/death KFs.
Weapon-only tracks are explicitly allowed in the combat asset regression while
requiring at least 50 matching actor tracks and successful pose sampling. Original
assets remain optional, user-supplied test inputs.

Transition logs retain actor/ref/package/target/weapon IDs for package, path,
combat start/end, equip, fire, reload, damage/body region, flee, death, corpse and
loot. Persistent unsupported diagnostics deduplicate by actor/package/reason.

The Android workflow must pass for the delivered HEAD. Headset validation remains
required for weapon attachment orientation/pitch, attack/reload blend timing,
world/player skin collision, multi-actor combat, corpse pose/loot targeting, frame
cost, saved-position restoration and dialogue/physical weapon regressions. This
host milestone does not certify the requested fully playable acceptance scenario.

Creatures need canonical CREA/LVL actor definitions, their own skeleton/group and
natural attack adapters, body-part/resistance data and residency. Shared activity,
shot/damage/target infrastructure has no hardcoded NPC names, but no creature is
fed humanoid animations to claim compatibility.
