# Physical weapon milestone — implementation checkpoint

This is **not** the completed physical-weapons milestone or a v164 release.
The application remains v163 until the complete interaction loop is integrated
and verified. Do not infer headset verification from host tests or CI builds.

## Implemented foundation

- Read original FO3 WEAP DATA/DNAM, ammo/projectile links, first-person STAT,
  sound IDs, reload/action information, and relevant condition GMSTs.
- Read original PROJ hitscan flags, speed, range, gravity, and muzzle-flash model.
  Extra immutable records do not change the legacy catalog fingerprints.
- Weapons are individual `Stack.id` instances, including starting inventory and
  container contents. Ordinary items/ammo retain existing stacking behavior.
- `TakeContainerStack` transfers the existing weapon ID instead of allocating a
  replacement. `PickupWeapon` collects the original reference once and equips.
- `DropWeapon` / `PickupWorldWeapon` transfer the same instance between inventory
  and `State.worldWeapons`, preserving condition, loaded rounds, and action state.
  Dynamic ownership uses 64-bit runtime IDs, not invented Bethesda FormIDs.
- Canonical firing consumes loaded ammunition only and applies category-based
  weapon condition loss. These APIs do not yet produce runtime projectile hits.
- Canonical reload APIs return ejected rounds to reserve, transfer at most one
  authored clip into the weapon, and require a separate chamber action.
- Save format v5 length-prefixes the existing Pip-Boy state and persists weapon
  metadata, world pose/velocities, and the one-time development-grant flag.
  Versions 1–4 migrate after validation; old weapon count stacks are split while
  retaining their original ID. Rejected restores do not mutate player state.
- The explicit development bootstrap reuses an owned 10mm pistol or grants one,
  seeds five clips only when reserve is zero, and transactionally loads it.
  It is deliberately **not called by the runtime yet**.
- Production input/math helpers implement neutral-before-press focus blocking,
  authored semi/automatic timing, body-oriented reach volumes, and a two-hand
  swing that preserves the right-hand roll and rejects degenerate/reversed hands.

Broken weapons may remain equipped/carried, but cannot fire. This avoids rejecting
a save solely because the last shot wore an equipped weapon down to zero condition.
An existing development pistol's condition is retained, not silently repaired.

## Verification

`tests/player/weapon_tests.cpp` exercises production helpers and player APIs with
a small fixture and, when given the original Fallout3.esm, all six representative
weapons: 10mm pistol, hunting rifle, assault rifle, combat shotgun, laser pistol,
and laser rifle. It verifies original authored IDs and values, ammunition
conservation, instance identity, reload/action persistence, v4 migration, and
malformed-save rejection. Assets remain outside git and the APK.

The checkpoint `cf2c2a2` (parser/input math) passed the full GitHub Actions Quest
APK workflow. Later ownership/save checks must be verified separately.

## Remaining before milestone completion

1. Cache original NIF, skeleton/KF attachments and named slide/clip/bolt/muzzle
   transforms. Confirm them against the Dropbox `/Fallout3` source assets.
2. Present equipped weapons at torso-driven right hip or right shoulder/back
   anchors; implement right-grip draw and release-inside-holster behavior.
3. Connect original-world right-grip pickup and dynamic-world physics/pickup.
   Retire collected original render/collision references once. Keep normal-item
   A pickup, shoulder inventory, doors, and containers unchanged.
4. Integrate the two-hand solve with the canonical visible left arm/hand, smooth
   acquisition, and support release. Give Pip-Boy focus priority over all inputs.
5. Implement firing from the authored muzzle, original projectile behavior,
   canonical target damage, sound events, flash, weapon recoil, and haptics.
   Do not turn laser projectiles into ballistic hitscan or apply HMD recoil.
6. Implement the physical 10mm magazine pouch/insertion and slide pull around the
   canonical reload APIs; seed the development weapon only when this loop is ready.
7. Add original-font green loaded/reserve HUD with cached outside-eye preparation.
8. Complete host/native/asset checks; increment to v164, build and deliver the
   APK; document remaining combat limitations and actual headset-test status.

There is no completed physical renderer, runtime weapon input integration,
physical reload, combat hit delivery, or new milestone APK in this checkpoint.
