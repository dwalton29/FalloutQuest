# v309: original-script New Game bootstrap

New Game reads `[General] SCharGenQuest` from the user's Fallout.ini or
Fallout_default.ini, searches inside Data and beside Data, and starts that quest
in a private player-session transaction. Its original MenuMode event runs the
setup. The resulting player MoveTo supplies the initial CELL, position and
rotation before scene preparation. No quest, stage or start-marker FormID is
embedded in this startup path. Missing configuration/setup returns to the title
with an error instead of selecting the cave.

Continue/Load preserve their saved arrival. A pending scripted teleport takes
precedence over an older arrival sidecar; an original scripted arrival also
provides recovery when its sidecar is absent. The cave resolver remains only
for legacy saves without either kind of arrival metadata.

## Implemented runtime support

- Transactional player MoveTo and render-thread scene-transition delivery;
  pending moves are acknowledged after successful scene publication.
- Save v28 stores character-generation, location-specific loading, activation
  sound and pending player-arrival state. Older supported saves still load.
- SetInCharGen prevents level-up allocation. Normal New Game retains the ESM
  clock defaults instead of the development NPC-routing clock override.
- Original player idle packages retain their IDLA animation references. The
  original KF can drive avatar skinning; OpenXR head tracking is not driven by
  its camera animation. Unsupported player package types/scripts reject.
- Static and animated image modifiers coexist, expire independently, and can
  be removed with RemoveImageSpaceModifier/RIMOD.
- SetNoActivationSound/ClearNoActivationSound, TriggerScreenBlood and forced
  subtitle arguments on Say/SayTo have presentation adapters. Screen splatter
  uses original textures and original size/opacity/duration settings; its flare
  layer and full original shader parity remain outstanding.

## Verified scope and remaining work

The supplied original ESM test executes CG00's original MenuMode bootstrap and
stages 0, 5, 6, 8, 9 and 10. It checks the authored birth marker, first selected
Dad INFO and forced-subtitle flag, blood request, static/animated effect
lifetimes, save round trip, and matching executed source against the master.
Audio playback, final camera/avatar placement, original KF availability and
visual presentation require headset verification.

This is the beginning of birth, not complete character generation. SexChange,
name entry, race/face editing, player age/scale, later character-generation
MenuMode scheduling, childhood cinematics and the full CG00-to-CG04 chain are
not complete. Choosing a sex can reach an explicitly unsupported command.
Transient presentation queues/effects are not yet reconstructed on a mid-scene
reload; canonical quest variables, controls, packages and teleport state persist.

## Validation

- Vulkan-enabled Android ARM64 native library build.
- Nine targeted host suites: new-game, player state, event scripts, script
  transactions, world scripts, production world-script adapters, menu rendering,
  title menu and pause/debug.
- Loading/render-dispatch, HDR order, UI layering and scene-publication source
  checks.
- Original integration: `new_game_tests /path/to/Fallout3.esm`.

The ordinary `new_game_tests` CI invocation also checks an unrelated synthetic
quest/marker, canonical isolation, rollback and unsupported-command rollback;
it does not need redistributed game assets.
