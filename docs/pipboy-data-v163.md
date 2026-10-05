# Pip-Boy data/runtime v163

Version `163`, `0.41.0-pipboy-data`, extends v162. The original rigid device mount, raise/view activation, wrist-roll attachment, raised elbow planes, arm solver and finger posing are retained. No Bethesda assets are bundled.

## Controls

| Input while focused | Action |
| --- | --- |
| Left grip press | Cycle STATS → ITEMS → DATA; 0.70 press / 0.30 release; holding never repeats |
| Right stick left/right | One subpage per deliberate flick, requiring neutral |
| Right stick up/down | Rows or detail text; delayed vertical repeat |
| A | Equip/unequip, safe Aid use, quest/note details, audio play/stop, radio tune/off, enter map interaction |
| Stick in map interaction | Continuous two-dimensional pan |
| A in interactive World Map | Waypoint at cursor, snapping to nearby authored marker; repeat at same position removes it |
| B | Back one level, including leaving map interaction |
| Lower/look away | Close physical screen; radio and notes continue |

Grip still drives finger pose. Main tabs use original Pip-Boy sounds. Focus blocks world A, snap turning and loot scrolling. Held controls require release/neutral after ownership or map-mode transitions.

## Architecture and provenance

`pipboy/fo3-pipboy-data.*` indexes immutable definitions during the existing single ESM catalog walk, retaining enclosing WRLD/DIAL groups. `fo3-pipboy-session.*` owns persisted mutable state; `player/fo3-pipboy-player.cpp` supplies canonical gameplay APIs. Map context, menu navigation, broadcast logic, page presentation and Android playback remain separate.

Record layouts were verified against supplied Fallout3.esm and [TES5Edit FO3 definitions](https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.5/Core/wbDefinitionsFO3.pas). Original MapMenu and stats_menu XML from Fallout - Misc.bsa were inspected for engine-populated lists, map windows, icons and button semantics. Presentation reuses original backgrounds, atlas icons, scanlines and the canonical CP1252 bitmap-font renderer.

### STATS and ITEMS

Existing Status, SPECIAL, Skills and equipment actions remain canonical. Perks show only acquired ranks, using original FULL/DESC/ICON and decoded rank/effect definitions. `GrantPerk` validates original rank bounds and persists state. It does not execute all Fallout perk entry points; no automatic perk grants or level-up system are invented. General currently reports the real persisted Locations Discovered count; unsupported vanilla statistics are omitted.

Aid validates every effect before consumption. Current support covers unconditional zero-duration, zero-area/self-range Health or ActionPoints ValueModifier effects, respecting damage/restoration flags. Original Purified Water (`000151A3`) is verified. Timed, conditional, scripted, limb, radiation, addiction and unsupported effects reject the entire use without consuming an item, displaying a reason. This includes original conditional Stimpaks. A full magic system is still required for those items.

### World Map

WRLD ICON supplies original imagery. Wasteland (`0000003C`) names `Interface\Worldmap\Wasteland_1024_no_map.dds`; MNAM gives 2048×2048 usable size and NW (-30,30) / SE (20,-20) cell bounds. World coordinates divided by 4096, authored ONAM scale/offsets, MNAM bounds and image-Y inversion produce UVs. PNAM parent-map inheritance and child ONAM transforms apply to player, marker and known quest-target positions. Megaton marker (`00062743`) belongs to child MegatonWorld (`00000A74`), inheriting Wasteland.

Markers come from original MapMarker (`00000010`) REFR DATA/XMRK/FULL/FNAM/TNAM, including type, visibility/travel flags and enable parents. Original icon aliases supply imagery. Authored visible markers may appear undiscovered; discovered IDs persist, without granting the whole map. Player position follows the canonical exterior origin and tracked world position; interiors retain the last exterior map location. Nearby cursor selection snaps custom waypoints to authored markers. Selected unconditional known quest targets use the same map transform.

No exact vanilla discovery-radius setting was found in supplied GMSTs. The explicit adaptation discovers enabled markers in the occupied authored 4096-unit exterior cell, twice per second. It does not claim vanilla proximity behavior. Dynamic script enablement and quest map unlocks require further support.

Fast travel is blocked: canonical traversal currently requires validated source/destination doors and lacks a complete exterior relocation transaction covering streaming, destination validation, stale interactions and combat/time policy. Waypoints do not teleport.

### Local Map

A read-only observer rasterizes already published authored collision triangles into a cached 256×256 monochrome height/edge image. It samples geometry below the player's head, uses actual scene bounds and canonical coordinate conversion, and overlays player/facing and current-cell doors with original assets. CELL/collision-publication revision invalidates the snapshot; geometry and texture are reused, never rebuilt per eye.

This is a collision-derived map, not the original full scene-rendered local map. Non-colliding decorative meshes and unloaded exterior geometry are absent. Multilevel interiors can overlap; changing floors without collision publication does not rebuild the slice.

### Quests

QUST definitions retain names, priority/flags, scripts/conditions, stages/logs, objectives/text and target references. Canonical APIs: `StartQuest`, `SetQuestStage`, `SetObjective` (display/complete/fail), `FinishQuest`, `SelectQuest`. Stage history/objective status persist. The UI lists known canonical quests and current authored objectives; unconditional selected targets feed maps. Definitions are not automatically advertised as active. Empty new-save quest/perk lists are valid.

There is no complete Fallout script VM. Gameplay events were not assigned invented progression. Setting a stage stores state without executing original fragments, dialogue or rewards. Conditional targets are withheld until their conditions can be validated.

### Notes

Only inventory-acquired notes appear under DATA, excluded from Misc. NOTE DATA distinguishes sound (0), text (1), image (2), voice (3). Text TNAM is retained fully and wrapped/scrolled with shared font metrics. Image notes use original paths. Sound SNAM resolves SOUN; voice SNAM NPC/TNAM DIAL resolves original INFO responses and speaker voice types. Identity/sex variants are evaluated conservatively; unsupported conditions are withheld.

Voice filenames resolve by actual INFO-form-ID/response suffix inside the authored voice-type directory, avoiding guessed truncated filename prefixes. Directory indexing is cached off the render thread. A toggles play/stop; completion clears playback state. Notes continue outside details/Pip-Boy. Exact vanilla continuation behavior was not proven from supplied XML.

### Radio

**RADS are radiation stages, not radio stations:** DATA contains threshold/SPEL. Radio identity comes from radio-flagged TACT FULL/VNAM/SNAM, REFR XRDO placement/range and enable parents. QUST GetIsID binds stations to broadcasts; RadioHello DIAL/INFO links describe original order. Original SOUN or voiced INFO responses supply audio. There is no hardcoded roster or alphabetical-file playlist.

A bounded broadcast interpreter supports GetIsID, GetQuestVariable, GetStage, GetStageDone, original INFO links/random flags, and validated simple authored quest-variable assignments/arithmetic. Unknown conditions, unsupported statements and bytecode-only fragments fail closed with diagnostics. An original GNR graph test advances 12 segments. This is not a general quest VM: unsupported quest/news/unlock branches are withheld. Availability honors initial enabled state and transmitter ranges; linked-interior reach is conservative and can use the last exterior location. Static/noise attenuation and arbitrary dynamic station enablement are not implemented.

Native worker events and dedicated Java radio/note MediaPlayers serialize sequential playback. Radio suppresses exploration music; ambience/effects continue. Off restores current-cell music. CELL changes and closing Pip-Boy do not reset broadcasts. App/XR focus releases playback but retains sequence/index, restarting the current segment on resume (no sample-position resume). Generation tokens reject stale completions. Extraction/cache is bounded to 128 files/64 MiB, protecting current broadcasts/ambience and recent effects. Missing assets stop safely and log failures.

Original Sound/Voices/Textures BSAs or equivalent loose files must exist in installed game data. Supplied files did not include full audio/map-texture archives; audible playback and full original-map imagery were not physically verified here. No copyrighted sounds/images are included in the APK.

## Saves and verification

Save v4 retains the exact v3 payload and fingerprint rules, appending a bounded Pip-Boy extension. Readers accept v1–v4; old saves initialize empty new state. The extension stores acquired perk ranks, quest stage/history/objectives/status/selection, discovered IDs, waypoint, tuned/off station and supported counters. Corrupt/truncated extensions reject atomically. This is not Bethesda `.fos` import/export.

Player tests cover grip/flick/repeat/ownership, maps and parent transforms, discovery/waypoint persistence, quest/perk transitions, acquired note filtering, original text/audio references, safe Aid, conditions/broadcasts and migration. Existing body tests remain unchanged. Host assets/audio/world/physics/NIF suites and render/source/font/shader checks passed. Original-file tests found 32 WRLDs, 206 markers, 192 QUSTs, 840 NOTEs, 87 PERKs and 22 station definitions.

A save produced by unmodified v162 against the original ESM restored with exact fingerprints `1577784072,1450618725,1064499287`, HP 183, AP 66, four inventory stacks, one collected reference, one container state and weight 4. Synthetic older-version/corrupt-extension tests also run in CI.

Optional original-file checks (copyrighted assets are not committed):

```sh
cmake -S tests/player -B build/host-player
cmake --build build/host-player --parallel 2
ctest --test-dir build/host-player --output-on-failure
build/host-player/player_state_tests /path/to/Fallout3.esm
build/host-player/pipboy_data_tests /path/to/Fallout3.esm [/path/to/v162-save]
```

Headset follow-up must check original textures/audio, physical readability, floor slices, moving markers, grip ownership, app/XR interruption and preserved v162 raised-arm pose. Host tests and APK compilation cannot replace physical validation.
