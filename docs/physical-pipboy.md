Current player body: [v160 authored VR retargeting](VR-BODY-v160.md). Historical Q21/v159 arm descriptions below are superseded.

# Physical Pip-Boy foundation — Quest 159

The original device is a rigid `PipBoy3000\PipBoyArm.NIF`, not a skinned mesh. Its attachment metadata names `Bip01 L ForeTwist`. All device shapes share `gPipWorld`: `gQ210PlayerRoot × solved rigid forearm/wrist-roll delta × authored Skeleton.NIF ForeTwist bind`. The canonical skeleton decoder supplies the bind matrix; coordinate conversion includes 70 game units/metre and the existing -1.55 m mesh floor offset. Display-centre placement inherits existing axial arm retargeting; the device itself is not stretched. Wrist flex is excluded; roll comes from the existing solved forearm and hand deltas. No separate controller attachment or altered arm targets.

## Authored assets inspected

- `PipBoy3000\PipBoyArm.NIF`: 113 blocks, rigid, no embedded animation/controller blocks. Display `pipboyscreen:0`, observed block 76, `BSShaderNoLightingProperty`, `Textures\Pipboy3000\Screen.dds`. Production selection uses shape name, texture and material semantics, not block number.
- Curved display: 56 source vertices; original positions/indices retained. Model-space bounds `(8.32809,-1.30896,3.57031)`–`(14.2409,3.29459,4.30787)` game units. Average authored normal `(-0.0346384,-0.0824987,0.995989)`. UV bounds `(-0.0104533,0.0019694)`–`(0.752926,0.762479)` normalize once to the dedicated texture, with V flipped for the offscreen GL framebuffer.
- Separate original glass `glare:0`, observed block 68, `ScreenGlare.dds`, remains an authored alpha material. `ScreenLit:8` behind the display also remains. The unsupported authored `PipboyLightEffect:0` light cone is suppressed; no synthetic world point light.
- Named `PipBoyButton01/02/03`, `TabKnob`, `ScrollKnob`, `RadNeedle`, `StatsGlow/ItemsGlow/DataGlow`. Only the active original main-tab glow is shown. Future physical controls can invoke the same `Action` enum as controller navigation.
- `Characters\_Male\Skeleton.NIF`: original `Bip01 L ForeTwist` bind, while existing UpperBody, glove and hand skinning remain authoritative.
- Supplied `Fallout - Misc.bsa`: `menus\globals.xml`, `main\stats_menu.xml`, `main\inventory_menu.xml`, `main\map_menu.xml`; inspected `prefabs\card_info.xml`, `list_box.xml`, `list_box_template.xml`, `tabline.xml`, `tabline_template.xml`, `text_box.xml`. Original globals specify 1024×768; title font 4, cards/list font 2, list spacing 20 pixels.
- Original `Monofonto_Large.fnt/.tex` (baseline 31) and `Monofonto_VeryLarge02_Dialogs2.fnt/.tex` (baseline 36), through the shared FNT parser and glyph layout. Font 4's 36-byte atlas name continues into an unused header slot; canonical bounded filename reading was extended without changing glyph/baseline/spacing semantics.
- Original `Interface\Shared\Background\pipboy.dds`, `Interface\InterfaceShared.tai` A/B sprites and atlas, inventory record icons, `Interface\Icons\PipboyImages\Derived Statistics\hit_points.dds`, and `Pipboy3000\pipboyscanlines.dds`. Resources are resolved from the user's Textures BSA at runtime. The inspected Dropbox background entry is zero bytes, so a full background image could not be verified offline; runtime falls back to black if missing. No proprietary assets are committed.
- Supplied `Fallout3.esm`: canonical player, inventory/item catalog, GMST overrides, original SOUN editor IDs `UIPipBoyAccessUp/AccessDown/Mode/Select/Tab/Highlight`. Audio uses the existing asynchronous worker. Default English labels were checked against the supplied Fallout3.exe and menu hierarchy.
- Supplied `FALLOUT.INI`: Pip-Boy green `(26,255,128)` and scanline scale 50. Recovered grayscale scanline texture modulates the display; there is no invented animated noise. Original blur/flicker settings were inspected but their fullscreen desktop effect is not recreated in this physical screen foundation.

## Pose and input

Initial thresholds require headset tuning; these are not claimed as physically verified.

| Signal | Enter | Remain active |
|---|---|---|
| Screen distance | 0.16–0.72 m | 0.12–0.85 m |
| Screen normal toward head | within 50° | within 65° |
| Head forward view cone | within 45° | within 60° |
| Screen height vs authored shoulder | above shoulder −0.25 m | above shoulder −0.35 m |
| Solved hand height vs shoulder | above shoulder −0.30 m | above shoulder −0.40 m |

Quest 159 arm-rig pass removes the synthetic 2.5 cm hand offset, uses a gravity/authored fallback elbow plane instead of a fixed diagonal pole, drives Fallout's authored ForeTwist bones with signed controller forearm roll, and mounts the Pip-Boy from that same solved ForeTwist transform. Torso-root rear-coordinate gating was removed because the actual head-facing and view-cone tests already establish that the display is in front of the user.

Dormant → Candidate → Active after 150 ms continuously valid enter pose. Exceeding wider exit limits releases focus immediately and dims the screen. Invalid tracked left/HMD poses, invalid right hand, application focus loss, hidden/invalid XR frames, loading and body reinitialization close focus. Tracking resumes through a fresh debounce. Viewing state is not saved.

Right stick horizontal: STATS/ITEMS/DATA. Vertical: choose an original subpage/category; A enters it. Inside ITEMS, vertical selects rows and A equips/unequips eligible canonical stacks. B returns to category selection. Focus consumes world A, loot scroll and right-stick turning; neutral/release gates prevent held-button fallthrough when closing. Left-hand tracking and locomotion remain live. No global pause is introduced; existing physics, NPCs, doors, streaming, time and ambient audio continue.

## Honest player bindings

- STATS: real level, current/max HP and AP, SPECIAL, 13 original active skills (unused Throwing excluded), karma and current/max carry weight. No player-name or XP primitive exists. Perks, General/limb/radiation/effect runtimes remain unavailable.
- ITEMS: original Weapons/Apparel/Aid/Misc/Ammo order; stack IDs reference `Player`, not duplicated inventory. Names, counts, condition, weight, known value, equipped marker, original icons and caps count come from the catalog/state. Shoulder pickups invalidate via `Player::Revision()` automatically.
- A uses canonical `Equip`/`Unequip` and existing save flushing. Canonical torso armour models replace the fixed outfit; appearance changes retain calibrated arm lengths/torso and held objects. Other equipment flags use canonical state; physical held-weapon/combat and headwear rendering are not added here. The prior Vault outfit fallback is preserved when a loadout has never supplied canonical torso equipment.
- Consumable use lacks item-effect execution; inventory dropping lacks a safe inventory-to-world spawn primitive. Both remain unavailable, without removing items or inventing effects.
- DATA keeps original Local Map/World Map/Quests/Notes/Radio subpages, explicitly unavailable. No fabricated stations, coordinates, markers or objectives. Original world-map asset was identified in XML but is not presented as a functioning map.

## Rendering, invalidation and verification

Persistent 512×384 RGBA8 framebuffer (half the original 4:3 layout), bilinear clamp display texture, persistent shaders/fonts and preallocated UI VBO. Icon cache is bounded to 48 entries. Screen is emissive through the existing no-lighting material, with gain 0.85 active / 0.012 dormant; casing retains normal world lighting and depth. Glass remains in the normal alpha pass. Both eyes sample the same device texture through their normal world MVP.

Logical update runs once per frame after player IK and before world interaction. Redraw occurs on wake, navigation or canonical player revision changes. Dormant and unchanged frames do not clear/redraw the target or upload UI geometry. Original images are cached after first use; no asset parsing/loading in eye draws. Renderer-owned GL state is restored through the v154 cache guard; framebuffer/viewport ownership is explicitly handed to the following eye renderer.

Host policy tests cover the requested pose, debounce, hysteresis, tracking-loss, once-per-frame/stereo, revision, bounds, canonical equipment and focus behaviours. Recording-driver tests execute the warm renderer and show zero synchronous GL queries, restored cached state and zero dormant/static framebuffer work. Optional original-file test verifies the real screen/glass/controls, no controllers, skeleton anchor, XML and both fonts. CI runs these alongside the existing regression suites and arm64 Quest APK build.

Physical Quest tests A–I, screen readability, final pose calibration, headset FPS and GPU timing remain unverified: no headset/ADB connection is available in this environment. `PIPBOY PERF` reports bounded CPU timings and actual guarded-path query deltas; these are not headset measurements made here.

```sh
adb logcat -v time FalloutQuest:I '*:S' | grep -E 'PIPBOY (ASSET|VIEW|UI|PERF)'
```

Optional original-file verification:

```sh
build/host-nif/pipboy_asset_tests /path/PipBoyArm.NIF /path/Skeleton.NIF '/path/Fallout - Misc.bsa' /path/Monofonto_Large.fnt /path/Monofonto_VeryLarge02_Dialogs2.fnt
```
