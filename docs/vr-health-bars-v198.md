# FalloutQuest v198 — transient VR health meters

## Primary original files examined

- User-supplied original `FALLOUT.INI`: `[GamePlay]` records
  `bHealthBarShowing=0`, `fHealthBarFadeOutSpeed=1.0000`,
  `fHealthBarSpeed=80.0000`, `fHealthBarHeight=4.0000`,
  `fHealthBarWidth=40.0000`,
  `fHealthBarEmittanceFadeTime=0.5000`,
  `fHealthBarEmittanceTime=1.5000`.
- Same INI: `[Interface]` HUD Main RGB 26/255/128.
- User-supplied `Fallout - Misc.bsa` (version 104), from the original
  `menus/main/hud_main_menu.xml`: `HitPoints` and `EnemyHealth` are
  independent HUD menu nodes, each with `systemcolor=&hudmain;`.
- `menus/prefabs/meter.xml`: the meter is a normalized `_Value` in 0..1,
  with solid (`Interface\\Shared\\solid.dds`) or ticked
  (`Interface\\HUD\\hud_tick_mark.dds`) modes and an optional dimmer background.
- Existing FalloutQuest `fo3hudassets::ParseButtonTai` resolves the original
  `Textures\\Interface\\InterfaceShared.tai` atlas entries. The VR bar requests
  `solid.dds`, then loads that original atlas through the existing texture reader.

## Implemented VR adaptation

- Damage-driven transient display: 1.5-second full brightness, 0.5-second
  fade. These durations are taken from the INI's *emittance* fields as a VR
  reveal policy; we do not claim the original 2D menu used these as its
  complete visibility schedule.
- Scale preserves the original 40:4 (10:1) health-meter proportions.
- Enemies are anchored to their live, authored skinned head bone (with an
  existing animated bounds fallback), face the user, and respect scene depth.
- Player health is anchored above the tracked left hand, drawn over the local
  hand, and hidden if left tracking is unavailable or the physical Pip-Boy
  currently has focus.
- Both bars are in original HUD Main green. We do not reuse the alternative
  warning-red colour as a replacement for the source HUD tint.
- The sole authority for HP is `fo3player::Player`: `Health/MaxHealth` for the
  player and `ActorHealth` for NPCs. Maximum actor health is reconstructed
  from current health plus the canonical accumulated actorDamage, retaining
  the last known maximum upon death.
- The state remembers previous HP per actor and reveals only on a decrease.
  Healing, spawning and routine visibility do not show an HP bar. Unloading
  the scene clears the transient state. Bars use no save changes.
- No changes to combat damage, collision, weapons, artificial NPC HP,
  scripted health, renderer depth buffers or actor locomotion.

## Headset checks

1. Shoot a Megaton NPC once: green 10:1 bar should appear above their animated
   head, display the canonical remaining HP, hold, then fade out.
2. Hit the same NPC again during the fade: full opacity must return and timer
   refresh, and lethal hits should briefly leave an empty bar.
3. Receive enemy weapon damage: left-hand bar should appear for the same
   duration, then vanish and leave the Pip-Boy unobscured.
4. Heal after damage: health fraction updates but healing alone must not
   re-trigger a hidden bar.
5. Check NPC behind a wall, multiple NPCs, and both-eye stereoscopy: enemy
   bars should be depth-occluded, with no per-eye jumps.
6. Check original 10:00 Megaton Common House departure for performance
   regression; no per-frame font parsing or extra health authority.
