# Original main menu — v308

Launch now enters a tracked, stationary title screen before the CELL/player
catalog loader starts. The installed `menus/options/start_menu.xml` and its
transitive prefabs supply the title, typography, dynamic list, highlights,
scrolling and warning text. Original files are read from the user's Misc BSA;
no game files are bundled. The runtime binds the native-populated entries and
visibility/alpha slots. New dynamic string defaults were checked against the
supplied executable's actual GameSetting records, using `extract_menu_strings.py`.

Dropbox inspection verified `Textures/Interface/Main/main_background.dds`
(1024×512 power-armour artwork), `main_title.dds` (256×64), and the original
font metrics/atlases. This version uses that static background with its aspect
preserved. It does not claim the animated slide/projector title sequence or
startup-video playback. The title screen uses original main-menu RGB(199,255,165)
from FALLOUT.INI, separate from gameplay HUD green. Original menu focus/accept/
cancel/adjust events resolve through the existing ESM sound catalog.

The panel is 2.56×1.92 metres at 2.5 metres, anchored once in raw tracking space.
Those dimensions and placement are explicit VR policy. Right stick navigates;
A selects; B returns; left Menu recentres. Input requires a neutral sample on
first opening and after focus loss. Both Vulkan multiview and GLES use raw title
poses; world simulation, movement, world rendering and Spacewarp synthesis are
suppressed. The existing loading renderer owns the subsequent world handoff.
The title has no controller-ray or hand interaction in this milestone.

## Actions and persistence

- Continue restores the most recently modified supported FalloutQuest save.
- Load lists the legacy `player-state.fqps` and independent new playthrough saves,
  labelled with local modification dates. These are FalloutQuest saves, not PC
  `.fos` imports. There are no fabricated player-name, screenshot, level or
  playtime previews. Save corruption is checked by Player::Restore on the CPU
  worker; failure aborts loading, preserves the file and returns to a notice.
- New asks for confirmation, then allocates a separate playthrough file without
  deleting or overwriting any existing save. It explicitly explains that the
  current supported start is the original Vault 101 cave arrival. Childhood/
  character generation is not implemented by this change.
- Settings provides persistent master/music/effects/voice volume, audio defaults,
  control help and recentering. Values reach Android's audio thread and already
  playing voices. Original unsupported PC display settings are not exposed.
- Credits reads the original installed `Data/Credits.txt` or its install-root
  sibling, if available, as paginated text. Original logo/directive formatting
  and automatic credit scrolling are not implemented. Missing text produces a
  clear notice. Dropbox did not contain this file.
- Quit confirms, finishes the Android activity, and uses normal XR shutdown.

This is a startup menu. The existing in-game pause/debug menu is preserved;
returning from an active world to the title and manual save-slot management are
not added here. Automatic/lifecycle saves use the selected playthrough path.

Arrival metadata is stored atomically alongside each save, containing the
current CELL/world, tracked horizontal position, capsule-feet height and game
heading. Its identity matches the entire 20-byte FQPS header, including the
canonical payload checksum, so stale metadata cannot restore a position from a
different save revision. Failed or missing metadata uses the existing supported
cave start and logs the legacy fallback; canonical player/quest state is still
restored. Old saves have no arrival metadata until a successful save in v308.
This companion file does not change the FQPS format.

Music uses the existing focus-aware audio worker and original
`Music/Special/MainTitle.mp3`. The default title gain is 0.6 (the supplied INI);
gameplay music remains 0.3. The separate Music directory was absent in Dropbox;
the runtime supports it from the user's own game installation and logs missing
music instead of substituting a track. Settings and credits are runtime bindings,
not executions of the Windows game executable.

## Validation

- Android ARM64 native CMake build/link with Vulkan enabled, NDK r27, the exact
  OpenXR 1.1.61 Android loader and reviewed Bullet 3.25 source.
- All Android Java sources compile against the Android 35 SDK.
- Title model tests cover no-save state, confirmations, stick/button edge and
  focus barriers, selected-save routing, volume bounds/persistence, unique new
  playthrough paths, legacy discovery and stale-arrival rejection.
- Actual Misc BSA drives every title page and long-list scrolling/reset tests.
- Production menu Surface binding tests use the original Misc BSA, original
  font 2/4/6/7 metrics and ESM, checking generated title glyph geometry, clipping,
  original title colour, and existing Pip-Boy/barter/message regressions. GPU
  textures and GL calls are mocked in this fixture.
- Existing player-save, pause/debug, menu-parser, loading-pipeline, native
  dispatch and GPU-session-lifetime checks pass.

Physical Quest appearance, recenter comfort, title music playback and complete
launch/restore/quit lifecycle still require headset QA. No headset is attached.
