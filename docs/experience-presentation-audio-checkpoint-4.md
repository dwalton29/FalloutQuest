# FalloutQuest XP and Level-Up — presentation/audio checkpoint 4

Canonical branch: `main`. Platform: native C++ / OpenXR / standalone Quest.
This checkpoint changes presentation and audio delivery, not scripting or
player advancement rules.

## Exact original sources

Read `menus/levelup_menu.xml` and `menus/main/hud_main_menu.xml` from
the user's own `Fallout - Misc.bsa`, and `AVIF`, `PERK` and `SOUN`
records from `Fallout3.esm`.

- LevelUpMenu defines a 1024x800 main rect, 470px skill/perk list on the
  left, a 400x400 selection illustration at x=520, right-hand description
  and point-counter, headline brackets, Reset/Continue/Back controls.
- HUDMainMenu/XPMeter defines the XPBracket and XPPointer sprites, XPAmount,
  XPLabel, XPLastLevel, XPNextLevel, XPLevelUp and the shared TAI atlas.
- PERK's ICON is used directly as the selected perk's graphic.
- AVIF's ICON, FULL and DESC are now parsed for each of 13 playable FO3
  skills (not the unused Throwing AV slot). These source records provide
  skill art and descriptive text without hardcoded file guesses.
- Level-up menu uses the installed `solid_black.dds` for its background
  when present, the existing exact original HUD font, green HUDMain tint and
  placement proportions from the menu definition. The XML menu is **not**
  executed by a generic Bethesda Flash/UI engine: this is a Quest-native
  faithful layout adaptation using recovered geometry and source sprites.

## XP HUD

Every committed reward carries the previous/current XP and authoritative
level thresholds to the existing bounded, rollback-safe notification queue.
`fo3-xp-meter-renderer.h` reads the original
`glow_XP_bar_over.dds` and `glow_XP_bar_pointer.dds` TAI entries and
uses the original atlas. The pointer animates from prior to awarded XP
fraction once per logical frame. Both stereo eyes display the same geometry
and timing. If the user has no `Fallout - Textures.bsa`, the existing XP
text remains visible without fabricated Vault Boy art or atlas images.

## Original sound identifiers

The original Fallout3.esm SOUN mapping and its existing audio worker are
the source for all sounds, with no bundled substitutes.

- `UIPopUpExperienceUp`: ordinary XP gain
- `UILevelUp`: crossing a level threshold
- `UILevelUpText`: opening the skills allocation menu
- `UIPerk`: confirming the earned perk
- `UIMenuFocus`: moving the selection
- `UIMenuPrevNext`: changing skill points
- `UIMenuOK`: continuing/resetting a page
- `UIMenuCancel`: moving back

All eight effects are preloaded asynchronously at the start of the level-up
experience, and sounds are now dispatched independently of whether the
HUD texture/font initialization was successful.

`MUSSuccess` refers to `sound/fx/mus/mus_success.wav` in the original
ESM but is **not automatically played** here; its source existence alone
does not prove it belongs to the level-up sequence.

## Tests

- AVIF decode regressions in pipboy_data_tests cover Barter/Unarmed icons
  and the unused Throwing slot.
- Notification tests cover XP fractional values, transient meter cleanup,
  invalid range rejection and exactly-one original SOUN event.
- Existing host source-layout checks preserve canonical once-per-frame
  fo3notify preparation and prohibit unsupported graphics staging changes.
- Current CI builds the native OpenXR Android package; a headset visual/audio
  check remains necessary to verify exact clipping, original atlas presence,
  controller interactions and archive-dependent sample playback.

## Required runtime files

`Fallout3.esm` and `Fallout - Misc.bsa` define the content. For actual
source art and audio, the device still needs the player's original
`Fallout - Textures.bsa` and `Fallout - Sound.bsa` (singular, as recognized
by the existing audio runtime), or supported loose original Sound files.
Do not package copyrighted Bethesda archives in the APK.
