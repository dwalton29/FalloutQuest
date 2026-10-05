# Physical weapons development build — v164

The runtime integration is implemented; headset acceptance is pending. Host and
original-asset verification do not establish physical comfort, visual placement,
or successful headset execution. The development APK is for that acceptance test.

## Canonical data and ownership

`WEAP` uses the Fallout 3 15-byte DATA and 136-byte DNAM layout. Ammo, projectile,
fire/dry/equip sounds, animation class, reload information, spread, rate, rumble,
critical data and condition rules are decoded before gameplay. Original records
for the normal 10mm, hunting rifle, assault rifle, combat shotgun, laser pistol
and laser rifle have regression coverage. These immutable additions preserve the
legacy inventory catalog fingerprint.

Normal 10mm: `0000434F`, `Weap10mmPistol`, `10mm Pistol`,
`Weapons\1HandPistol\10mmPistol.NIF`; ammo `00004241`, `Ammo10mm`, `10mm Round`.
Original DATA: clip 12, damage 9, weapon health 150, weight 3, value 225. DNAM:
one-hand pistol animation 3, semi-automatic, one round/one projectile per shot,
minimum spread and spread 0.5 degrees, approximately 6 shots/sec, reload A,
reload time 1.3 seconds, attack delay minimum 0.1 and maximum 1 seconds. PROJ
`0002CD5F` has hitscan enabled, speed/range 10000 and zero gravity. Original
sounds: fire 3D `00036ADB`, fire 2D `00036ADA`, dry `0001F21C`, equip `00021E6E`,
unequip `00021E6F`. Firing uses the authored `ProjectileNode` axis and position.

The one-time save bootstrap selects the highest-condition existing normal 10mm
instance or adds exactly one at the existing full-condition convention. It equips
that ID. Existing condition is preserved. If canonical compatible reserve is zero,
it adds five authored clip capacities (60 rounds); an empty pistol loads up to 12
from that reserve. Otherwise existing reserve/loaded ammunition are reused. A
persisted flag prevents regranting, including after dropping the pistol. Starting
ESM inventory is not changed. A newly acquired ordinary world weapon starts empty
because the REFR does not reconstruct a saved runtime magazine; no free ammunition
is manufactured. A broken existing pistol remains broken until repaired.

Each weapon is an individual canonical `Stack.id` (64 bits), with condition,
loaded rounds and charging-action state. Ammo remains normally stackable.
Dropping transfers that Stack into `State.worldWeapons`, removes inventory
ownership and unequips it. Pickup moves it back without allocating a new ID or
merging. Authored REFR pickup marks the original reference collected once;
firearms are excluded from A/Take and generic loose-object collection paths.
Pip-Boy equipment selection changes the active body presentation and leaves the
previous weapon safely in inventory. Unequipping never creates a world drop.

## Body and physical controls

All access offsets live in `fo3-weapon-asset.h`, in solved body coordinates.
The right hip uses the original right-thigh anchor plus 10cm outward and 3.5cm
rearward. The back uses the solved right shoulder plus 4cm outward, 4cm down and
15cm rearward. Access volumes and visible poses are separate adaptations. Hip
ellipsoid radii are 16/20/16cm, shoulder 20/22/20cm, with 4cm exit hysteresis.
The left belt pouch uses the original pelvis minus 22cm laterally, 6cm downward
and 1.5cm rearward, with 18cm access radii. These are VR policies, not Bethesda
holster dimensions. They follow the solved torso, not raw HMD yaw.

Right grip draws/holds. Releasing inside the appropriate holster keeps the same
instance equipped and owned; releasing elsewhere drops it with sampled linear
and angular velocity. Runtime weapons use the existing dynamic-box collision
query, SI gravity, short substeps, damping and settling policy. They never become
fake ESM references. Absolute world positions and cell/world identity survive
scene-origin changes; inactive cells retain their saved poses. A non-droppable
quest/script weapon safely returns to its holster when transfer is denied.

Original FP skeleton/aim KFs supply the primary attachment. Original NIF ancestors
identify magazine, slide and bolt geometry. The support palm is derived from FP
metacarpal origins, with a 20cm fore-end access segment. Left grip near it engages
an analytic two-point swing anchored at the right palm; right roll defines the
transported plane, left roll cannot flip the weapon. Close/reversed hand geometry
retains the one-hand orientation. Acquisition eases over approximately 83ms;
release immediately restores one-hand control. The visible support hand/fingers
receive the original aim orientation about their palm, without changing tracking.

Pip-Boy retains left-grip tab cycling and right UI navigation. Focus, loading and
session loss suppress weapon controls and safely holster a held weapon. Resumption
requires neutral trigger/grip edges. Existing loose non-weapon shoulder inventory,
A-button doors, containers and ordinary pickup remain intact.

## Firing and physical reload

Right trigger crosses 0.65 to fire and resets below 0.25. Semi-auto uses press
edges; automatic rate follows the authored WEAP rate. A valid shot consumes loaded
rounds only, applies authored condition degradation GMSTs, plays original sound,
shows the original PROJ muzzle-flash NIF where supported, and gives a short Quest
pulse. Original sounds are warmed on the audio worker when models are cached.
No ESM parsing or BSA model reads happen in the firing path. Visual recoil is a
comfortable VR policy: at most 6mm rearward and about 2 degrees, exponentially
returning, reduced with support. It never changes HMD/camera tracking.

Hitscan follows the authored flag. Non-hitscan missiles use decoded speed/gravity,
swept collision segments and range. Actor hits test current authored skinned
surfaces after a broad phase. Damage captures weapon/skill/condition at firing,
then uses a canonical actor-hit API and persistent actor damage. NPC base health,
endurance and level use decoded original data/GMSTs; essential actors retain one
health pending unconscious-state support. Armour/DR, perks, critical effects,
difficulty, location multipliers, full AI/death/ragdoll reactions and levelled
spawn resolution remain separate combat layers. Explosive/heavy projectiles are
not enabled without their own verified attachment/explosion integration. Moving
energy missiles currently have collision/damage but no travelling visual effect.

Held firearms display `loaded / reserve` using the existing original Monofonto
font and Fallout HUD tint, 8cm above and 4cm forward of the right aim controller.
It is absent when holstered or Pip-Boy/loading owns focus. Reserve is exactly the
canonical inventory total displayed by Pip-Boy Ammo.

For the 10mm (and audited normal assault rifle): hold right grip, press B to eject,
left grip at the belt pouch to retrieve the authentic magazine geometry, move it
to the original magazine node and release within 8.5cm with matching alignment.
Insertion transfers at most clip capacity from compatible reserve. Then left grip
near the original slide/bolt and pull rearward 4.5cm to charge. B, access radii,
alignment and pull threshold are explicitly VR adaptations. Reload sounds come
from the original reload KF Sound keys, not invented filenames.

Ejected rounds return immediately to canonical reserve; the briefly falling
magazine is an empty cosmetic component, not an independent ammo possession.
Retrieval reserves no rounds, insertion transfers once, and interruption cancels
the uninserted representation without losing or duplicating ammo. This lossless
policy is the milestone abstraction; retaining independent loaded magazines is
future work. Cylinder, tube-fed, internal/bolt-action, energy-cell and heavy
reload families require individual asset/animation audits and mechanics. Reload
animation letters alone do not establish those families.

## Persistence, validation and acceptance

Save v6 accepts v1–v5, preserves their inventory/equipment/condition, containers,
collected refs and Pip-Boy/quest/note/map/radio extension, and adds weapon-instance
metadata, dynamic world poses/velocity and actor damage. Equipped weapons restore
holstered; a held tracking pose is not serialized. Malformed extensions are
rejected transactionally. The normal 10mm bootstrap is idempotent. Saves use the
existing scene-transition/shutdown flush; firing dirties canonical state.

Host coverage includes ownership transfer, magazines/reserve conservation,
condition, malformed saves, old-save migration, idempotent bootstrap, trigger
edges/rates, focus neutrality, holster hysteresis, two-hand degeneracy, actor hits
and damage persistence. Original ESM tests exercise all six representative weapons;
original NIF/KF tests exercise the 10mm, hunting rifle and assault rifle. All
existing host suites and source/shader checks are required by the APK workflow.

Event diagnostics include equip, grant, draw, holster, drop, pickup, support,
fire, empty and reload/magazine transitions, with IDs, loaded/reserve, condition,
support/holster and dynamic ID. Assets remain external to git and the APK.

Headset acceptance must exercise the complete pistol draw/fire/empty/reload/
holster/drop/floor-pickup/save loop and the rifle shoulder/support/firing loop,
including alignment, comfort, performance and thrown-weapon collision. No headset
was available during implementation; these physical observations are not claimed
as tested. The build is a development milestone, not a headset-verified release.
