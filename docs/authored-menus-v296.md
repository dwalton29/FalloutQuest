# Original menu layouts and world visibility — v296

## Source and connected behaviour

The live Pip-Boy renderer now reads `stats_menu.xml`, `inventory_menu.xml` and
`map_menu.xml` from the user's original `Fallout - Misc.bsa`. Dialogue reads
`dialog/dialog_menu.xml`; barter reads `barter_menu.xml`; script messages, travel
confirmation and transaction feedback use `message_menu.xml`. Their transitive
prefab includes, expressions, native template slots, source fonts and DDS/TAI
assets control the screen hierarchy. Original XML and graphics are not bundled.

`ui/menu/fo3-menu-layout.h` evaluates source fragments, scoped trait references,
conditional expressions, native overrides and instantiated templates.
`fo3-menu-renderer.h` resolves original font metrics and image dimensions,
clips glyph/image geometry, applies crop offsets, preserves layer order and
batches adjacent compatible sprites. Original font and atlas textures share
GPU ownership across these menus. The supplied loose InterfaceShared.tai is empty;
when an index is unavailable, bare source aliases resolve only to unique DDS
basenames in the original Interface archive index. Ambiguous names are rejected
rather than selecting an arbitrary file. Full source paths retain exact lookup. Source layout and vertex buffers are cached;
static stereo eyes do not rebuild them. Pip-Boy redraw invalidation follows
presented player data rather than every NPC pose/script timer revision.

`fo3-menu-player.h` binds canonical inventory instances, equipment, HP/AP/XP,
SPECIAL/skills, perks, radiation, map locations, quest objectives, notes, radio,
merchant inventories/prices/caps/pending selections and dialogue topics/responses.
AMMO ONAM supplies the original ammunition abbreviation. Merchant weapon previews
use the same damage calculation as player inventory weapons. Map source templates
retain selected-marker enlargement and viewport clipping; coordinate projection,
portal guidance, fast travel and pan/zoom still use the existing gameplay systems.
Session content participates in cache identity so reopening another conversation
or merchant cannot reuse stale lists even when panel revision counters restart.

Executable UI GameSetting defaults are required: many are absent from the ESM.
The compiled string fallback table was verified against the supplied PE32;
ESM/localized overrides take precedence. Reproduce the check with:

```sh
python3 tools/extract_menu_strings.py --exe /path/Fallout3.exe \
  --check app/src/main/cpp/ui/menu/fo3-menu-strings.h
```

Regeneration accepts `--menus /path/extracted/menus --output /path/header`
and optional `--proof /path/proof.json`. The tool reads aligned GameSetting
records; it does not distribute the executable or original XML.

## VR policy and limits

Original relative layout coordinates are retained. Pip-Boy renders at the original
1024×768 onto its existing curved NIF screen, with the existing original scanline
texture. World dialogue/barter/message menus use a 1280×1024 virtual viewport,
centred on the existing interaction anchor at 0.001 metre per source pixel.
These placement/scale choices are VR adaptations, not recovered native constants.
Existing stick/A/B inputs remain: barter still selects quantities through the
existing trigger/stick path and confirms via its final transaction selection.
The original accept footer uses the A glyph to match that VR input. Unconnected
barter filter arrows and inventory repair/drop/hotkey buttons are hidden.

The engine does not yet track individual player limb condition, the complete
native timed-effect ledger, all original general-stat counters or the animated
Pip-Boy face/karma presentation. Limb meters are omitted rather than deriving
fictional readings from total HP. The source body artwork and CND/RAD/EFF layout
are present. General currently exposes the tracked discovery count; this change
does not claim complete original Stats gameplay data or pixel-perfect animation,
waveform, glow/blur or native mouse/controller navigation parity. Existing local
map imagery remains generated from scene collision; original map frames and
marker templates surround it. Native map marker base sizes and zoom limits remain
explicit VR policy. No quest stages, inventory effects, prices, rewards or save
format were changed to obtain these layouts.

## Visibility contract

World, water and image-space effects finish before interactive screens and HUDs.
Dialogue, barter, script messages, terminal text, loot, interaction prompts,
health bars, compass, ammo, XP and item-added notifications draw in the late UI
pass with world depth testing/writes disabled. Existing loading/modal suppression
rules still apply: a notification hidden by an active modal is not an occlusion bug.

When focused, the original Pip-Boy casing/screen/glass are removed from the normal
body pass and rendered over the finished world using the existing isolated modal
depth surface. They still occlude each other, while apparel/terrain cannot cover
the screen and world/Spacewarp depth remains intact. The dormant wrist model
retains ordinary physical world rendering. The focused screen's display colour is
decoded before final output encoding, preserving authored green without applying
world film grading again. This does not alter apparel skinning or add geometry.

## Verification

- Host parser suite: original six menu trees and their prefabs load, source
  expressions evaluate without unsupported operators on exercised paths, template
  slots/bounds/visibility/reset and intrinsic image sizing are checked.
- Original-data renderer fixture: supplied Misc BSA, real font2/4/6/7 FNT metrics
  and Fallout3.esm; all 15 Pip-Boy pages, original inventory/ammo fields, Moira
  barter stock, map projection/selected marker/travel confirmation, dialogue
  selection/cache transitions and message-box viewport transitions are exercised.
  GPU calls/texture IDs are mocked here; this is binding/geometry/state evidence,
  not proof of complete DDS asset availability or headset appearance.
- Warm renderer: no driver queries; state restoration; static/dormant frames
  produce no extra draw/upload work. Shared layouts are not rebuilt per eye.
- Pip-Boy, weapon and barter host regressions pass. Render-order source checks
  include notifications and isolated focused-wrist depth.
- Production software Vulkan tests exercise an overlay behind world geometry,
  authored-green output, focused Pip-Boy screen colour roundtrip and modal depth
  isolation, with validation layers. These test the production graphics path,
  not tracking, real apparel or physical headset readability.
- Android compile/APK result is reported separately in delivery. Physical Quest
  verification remains outstanding; there is no attached headset in this session.

Original-data reproduction after configuring `tests/player`:

```sh
menu_layout_tests /path/Fallout\ -\ Misc.bsa
pipboy_render_tests /path/Fallout\ -\ Misc.bsa /path/original-font-fnts /path/Fallout3.esm
```

## Focused headset checks

1. Raise/lower the wrist wearing several outfits. Focused screen and casing should
   stay visible through sleeves, a held item and nearby walls; glass/casing should
   still self-occlude. Lowered wrist should return to normal world occlusion.
2. Visit every Stats/Items/Data page; scroll long lists and descriptions, equip/use
   items, change quest, read a note and tune radio. Check original typography,
   brackets, tabs, selected rows and item cards. CND/RAD/EFF cycle with Up/Down
   while inside Status; unavailable limb meters should not show invented values.
3. Check world/local map player orientation, selected location name, zoom/pan,
   fast-travel Yes/No and cancellation. Leave/return through a door and check
   quest portal guidance. Save/reload and check existing map/player state.
4. Talk to Gob/Moriarty then another speaker; switch topic/subtitle phases. Barter
   with Moira then another merchant; select goods on both sides, adjust quantity,
   confirm once, reopen and verify updated caps/items without duplicate effects.
5. Put a wall/held object between you and a prompt, terminal, loot list, health
   bar and open menus. Trigger item-added and XP messages outside modals. They
   should remain legible above geometry with consistent green in both eyes.
6. Look for `AUTHORED MENU failed` / `incomplete` in logcat if artwork is missing;
   the runtime reports missing source assets once per document instead of
   silently substituting generic menu artwork.
