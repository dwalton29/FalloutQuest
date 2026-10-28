# v257: authored player and NPC health meters

Replaces the solid rectangle health HUD with the original Fallout 3 meter art.
Player HP uses `glow_hud_left_seperatorglow.dds`; NPC HP uses
`glow_enemy_health_seperator.dds`. Both repeat `hud_tick_mark.dds` rather than
stretching a solid sprite. Brackets resolve through InterfaceShared.tai and
reuse the prompt atlas or each other's atlas when possible. Original loose
DDS files are supported when an atlas entry is unavailable. All assets load
from the user's game installation; no original textures are committed.

## Original sources inspected

- Fallout - Misc.bsa, Menus/Prefabs/HUDTemplates.xml: meter height 20,
  tick width/_ImageWidth 8, default _TotalWidth 300; player bracket 512x256;
  enemy bracket 500x64 at (-7,3).
- HUDMainMenu.xml: distinct HitPoints and EnemyHealth roots, HUD main color.
- Fallout3.exe 1.7.0.4: HP initialization at 0x92AA55..0x92AB02 sets
  original X 20, Y 41, bracket alpha 255, meter alpha 175. Enemy initialization
  at 0x92AF7B..0x92B03D sets original X 171, Y 3, total width 240 and alpha 175.
- The meter helper at 0x936A40..0x936CBF truncates fill to whole image widths;
  centered meters truncate to pairs and subtract half the resulting fill
  from original X. Living actors retain a minimum image width. Player call
  0x934691 uses left alignment, minimum one tick; enemy call 0x935994 uses
  centered alignment, minimum one tick while alive. Zero health has no ticks.
- Inspected the original three DDS images to verify bracket orientation and
  tick spacing. HUD tint remains the existing Fallout.ini default green.

The meter is built in authored screen units before conversion to metres.
Existing VR adaptations remain: damage-only reveal/fade, a player meter above
its tracked left hand, NPC meters above their animated heads, billboard
orientation and the existing physical widths/depth policies. This change
replaces meter artwork/layout, not the rest of the flat-screen HUD or labels.

## Validation

`tests/render/health_bar_render_tests.cpp` covers native tick quantization,
critical/zero/full HP, left-aligned player and centered enemy drain, distinct
bracket UV regions, repeated tick UVs, atlas sharing, cold initialization,
empty bracket rendering, stereo draw/state restoration and no warmed asset
reads, synchronous state queries or buffer reallocations. It runs in APK CI.
Existing health-state and HDR pass-order tests also pass locally; the production
header compiles against both GLES and Vulkan. Physical readability still needs
headset verification.
