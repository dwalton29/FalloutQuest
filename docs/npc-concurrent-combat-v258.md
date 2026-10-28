# v258: concurrent NPC movement and weapon playback

The first combat fluidity repair separates resident locomotion from weapon
playback. Ranged actors can advance an existing reposition/cover route while
reloading, flee during reload, and evaluate source CSTD manoeuvres during a
committed shot. The weapon instance remains pinned through its committed action;
actual firing still requires ammunition, range and live muzzle visibility.

Reload progress now runs before perception, pursuit, search and body-reaction
returns. Starting a necessary reload also precedes pursuit/search/cover decisions. The canonical magazine transaction executes once when its deadline is
reached, including with an occluded or out-of-range target. Losing/changing the
committed weapon cancels that reload without filling another instance. Source
reload sound keys follow the same normalized phase as the reload pose, scaled
to the existing canonical reload duration. Reload cannot fire a shot in its
completion tick. New shot/reload pose changes invalidate the same-frame pose
cache, so an earlier sight/muzzle query cannot leave the rendered weapon behind.

## Original data inspected

Downloaded originals were inspected locally, not committed:

- `_male/locomotion/male/mtforward.kf`: leg priority 30.
- `_male/1hpaim.kf`, `1hpaimup.kf`, `1hpaimdown.kf`: leg priority 25,
  spine/arm priorities 30/35, sequence weight 1.
- `_male/1hpattackleft.kf`: lower-body priority 26 and weapon/upper-body
  priority 55, authored Hit key at 0.0333333 seconds.
- `_male/1hpreloada.kf`: leg priority 27, torso/arm priority 65, duration
  1.3 seconds, original reload sound keys. Thus walking legitimately retains
  the legs while reload controls the torso/arms.
- Fallout3.esm IDLE/MODL resolves MTHitTorsoA to
  `Characters/_Male/IdleAnims/MT_HitTorso.kf`; the original hit KF has priority
  70 across the body. The examined arm/leg reaction KFs are also available.
  Full-body reactions retain the existing brief movement interruption and
  attack/reload protection; this release does not turn them into invented
  partial flinches.
- Fallout3.exe 1.7.0.4 contains AimUp/AimDown groups and defaults of 90 degrees
  for both `fAimUpDegrees:Combat` and `fAimDownDegrees:Combat` (setting-name
  pointers at file offsets 0xD12978 / 0xD12984, adjacent float 0x42B40000).

The KF decoder now retains and validates sequence weights and per-controller
priorities. The pose compositor uses only authored transform channels; original
low-priority weapon leg tracks cannot replace the walk. Per-weapon AimUp/Down
clips are optional, staged alongside the equipped group's existing source KFs.
Pitch applies their difference from neutral Aim to the current weapon action,
so attack/recoil and independent gait clocks survive vertical aiming. Missing
variants retain neutral Aim. Accumulation roots stay in place; NAVM/collision
continue to own world movement.

## Explicit bridge policies and limits

This remains a Quest composition bridge, not a recovered Bethesda animation
manager. Equal-priority ties prefer the later weapon layer. The existing
0.3-second transition bound applies only to weapon-layer changes and is capped
at the source Hit time when entering an attack; repeating a shot does not
restart the gait blend. Source Blend/BlendIn commands are not interpreted as
seconds or frames. New weapon layers use cached bone mappings rather than
rebinding every frame. Pitch uses the recovered 90-degree defaults; INI aim-angle
overrides are not read. Horizontal torso-yaw aiming, full Bethesda blend graph,
block/power attacks and existing unsupported weapons remain outside this pass.

## Validation

- Production actor/render-bridge tests now execute weapon compositing (the old
  host-only exclusion was removed): repeated shots preserve the gait blend and
  evolving pelvis pose; reload uses its own clock; root stays in place; source
  pitch math and missing-variant fallback work; zero-weight layers do not win.
- Production combat/NAVM tests cover reload plus movement, flee, loss of sight,
  out-of-range target, automatic reload initiation during pursuit/search/cover/flee,
  one magazine transaction, phase-aligned sounds emitted
  once, and manoeuvre evaluation without cancelling an already committed shot.
  Existing weapon commitment, melee, cover, damage, death and restore tests pass.
- `combat_pose_asset_tests /path/to/originals` checks the actual pistol/skeleton/
  walk assets: decoded priorities/weight, walking legs during reload, original
  reload hand pose, source AimUp at 90 degrees, and accumulation-root protection.
  CI compiles this optional test; it skips without user-owned original assets.
- Existing animation math and original walk sampling pass. The production actor
  glue also passes a host syntax check with the Android event loop excluded;
  Android CI compiles the complete native application.

Headset acceptance remains necessary for foot contact, rapid-fire recoil,
turning, slopes, elevated targets and reloading while moving through busy routes.
No on-device visual acceptance is implied by the host checks.
