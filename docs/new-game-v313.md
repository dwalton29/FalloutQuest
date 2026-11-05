# v313: original birth-to-toddler transition

Continues v312 with appearance editing deferred. This checkpoint implements the
reusable lifecycle and movie commands needed for the original CG00 → CG01
handoff. Runtime code does not contain quest IDs, forced stages or replacement
opening routes.

## Script and player support

`player.SetScale` updates saved reference scale. `player.AgeRace` follows the
original RACE younger/older links, and uses its sex-specific height and body
assets. Scale affects tracked head/hands, floor-relative player height, movement,
body root/IK, capsule radius/height and step height. SpaceWarp is suspended while
scaled because its current rigid app-space history cannot represent scale.

`SetPCToddler` persists the original lifecycle flag. The original camera-only
walk/bob animation is not applied to the headset. Toddler body/leg animation
parity is still incomplete. This checkpoint implements the scale and movement
needed for toddler play, not the full original animation treatment.

`AutoDisplayObjectives` selects the requesting quest when enabled and includes
its displayed objective text on quest update cards. This adapts the existing VR
HUD rather than implementing a permanent vanilla objective list. Disabling it
preserves ordinary quest notifications. `KillQuestUpdates` clears queued/current
quest cards while retaining item and XP cards. Both participate in script
transactions. IsXBox/IsPS3 conditions return false on Quest; the original movement
tutorial stage uses these conditions, so rejecting them rolled back Dad’s INFO.

## Native original movies

`PlayBink` preserves its quoted relative filename and four flags: interruption,
world-audio muting, music pausing and letterboxing. A saved request owns the modal
until decoder EOF, an allowed B skip, or explicit A continuation after failure.
Opening/held buttons and focus loss cannot accidentally dismiss it. Other
character menus cannot overlap the request. GameMode time pauses while original
MenuMode handlers remain available.

The worker decodes original Bink video and the first Bink audio track using a
pinned minimal LGPL-only FFmpeg build. Bounded video and audio queues keep file
reads and decoding off the render thread. AAudio supplies Android movie audio;
the frame clock pauses with focus/loading/pause. Audio/video use an approximate
wall-clock timeline; exact audio-master sync has not been implemented.

Movies render on a black VR theatre plane through the existing UI texture path,
without the world/HDR pass. Source aspect ratio is retained when requested.
World channel muting and background music pause/resume follow the source flags.
The request survives save/reload and renderer teardown; playback restarts from
the beginning, not a persisted playback timestamp.

Files resolve case-insensitively below the installed Data/Video directory. The
shared source folder currently contains no Video directory. Missing/invalid
movies show an explicit continuation choice instead of silently advancing.
No game video is bundled. FFmpeg shared libraries, source/rebuild information
and LGPL notices are included in each APK and checked during CI packaging.

## Validation

The supplied original ESM integration drives both sex branches from the New Game
bootstrap through name entry, deferred appearance, final birth fade, original
movie request, and Dad’s first toddler dialogue to CG01 stage 10. The test signals
presentation completion; it does not advance quest stages manually. Movie-modal
script suspension and a save/reload checkpoint are covered. Voice completions,
projector animation capabilities and world publication are host fixtures, not
headset playback or scene-rendering evidence.

A self-authored valid three-frame Bink fixture tests real decoding, RGBA output,
timestamps/EOF, asynchronous playback, focus pause/resume, missing/corrupt files,
path resolution and controller dismissal policy. No copyrighted footage is
included in the fixture. Shared-library host playback and Vulkan-enabled Android
ARM64 compilation/linking pass. The package checker is exercised using actual
ARM64 outputs in a temporary QA archive; finished APK verification runs in CI.

Appearance editing remains deferred. The playpen sequence, birthday/G.O.A.T.
and Escape still require further end-to-end implementation and headset testing.
This checkpoint does not claim that leaving Vault 101 is complete.
