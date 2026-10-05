# FalloutQuest

Experimental standalone Meta Quest runtime for user-supplied Fallout 3 game data.

## Game installation: 0.31.0 — Public Steam install folders

Copy your own Steam installation to Quest **Internal shared storage**:

| Game | Folder | Expected master location |
| --- | --- | --- |
| Fallout 3 | `FalloutQuest/Fallout3/` | `Data/Fallout3.esm` |
| New Vegas | `FalloutQuest/FalloutNV/` | `Data/FalloutNV.esm` |

You can copy the contents of the Steam install into the game folder, or drag
its entire named folder in unchanged. For example both
`FalloutQuest/Fallout3/Data/Fallout3.esm` and
`FalloutQuest/Fallout3/Fallout 3 goty/Data/Fallout3.esm` are detected.
Keep only one installation inside each game folder. Preserve the full `Data`
folder, including original archives, Music, Sound and subfolders; no extraction
or conversion is needed. Windows executables are not run by FalloutQuest.

The setup screen requests Android shared-storage access and checks the master,
Meshes, Textures and Misc archives before starting. Android's native path is
`/sdcard/FalloutQuest/`. New Vegas discovery and folder separation are reserved
for future compatibility; the current runtime only launches Fallout 3.
DLC and plugin execution are not enabled by copying additional files.

On a sideloaded headset without an accessible permission settings screen:

```bat
adb shell appops set --uid com.falloutquest.app MANAGE_EXTERNAL_STORAGE allow
```

Example transfer in Windows CMD (shows ADB progress):

```bat
adb shell mkdir -p /sdcard/FalloutQuest/Fallout3 /sdcard/FalloutQuest/FalloutNV
adb push "C:\Program Files (x86)\Steam\steamapps\common\Fallout 3 goty" /sdcard/FalloutQuest/Fallout3/
```

Restart FalloutQuest after moving/changing an installation: all ESM/BSA indexes
use one immutable Data root per process. Player saves and decoded audio cache
remain app-private. Existing private game files are not automatically moved.

## Loading animation and audio fixes

The runtime now plays original item pickup sounds (YNAM), door/container open
and close sounds (SNAM/ANAM/QNAM), and the original menu focus sound while
scrolling floating loot. Successful actions emit effects once; blocked actions
remain silent. CELL XCAS → ASPC SNAM supplies an ambient loop where authored.
CELL XCMO → MUSC supplies music; exteriors without an override use the original
DefaultExplore definition. Explicit no-music cells stay silent. Interior cells
without a music override are left silent for now.

Install your original **Data/Music/** directory and either **Data/Sound/** or
**Data/Fallout - Sound.bsa** alongside the existing ESM in the selected install's
`Data/` directory. Extracted Sound/Music folders directly inside the selected
install root are also accepted. Preserve subfolders. These assets are not bundled
in the APK. Missing files log their original path and do not block gameplay.
The extracted Sound folder is available in Dropbox; the separate Music folder
was absent when this milestone was built.

Android decodes on a separate audio thread, with a bounded native request queue,
six one-shot voices, one music player and one ambient player. File reads and BSA
extraction happen on the native audio worker, never the render thread. Original
static attenuation is applied; music volume uses the supplied INI's 0.3 default.
Music folders use a shuffled continuous playlist, a runtime adaptation rather
than exact Bethesda scheduling. Losing Android/OpenXR focus releases playback;
resume restarts the current track/loop. Headset playback and latency need testing.
Spatial positioning, region sound scheduling, reverb, footsteps, physics impacts,
combat, NPC voices and radio are pending.

## Container loot: 0.28.0 — Floating container loot

Aim at a supported container to see an in-world loot list. Use **right stick
up/down** to scroll and **A** to take the highlighted stack. There is no submenu;
left-stick movement continues and right-stick turning is suppressed while the
list is active. Aim away to dismiss it. The user-requested Fallout 4 style flow
uses Fallout 3's original names, font, HUD colour and A-button sprite, with an
explicit VR floating layout and highlight.

The installed ESM supplies CONT contents and nested LVLI definitions, counts,
condition, level rules, chance-none and initial global values. Loot generates
once per container and remaining stacks save with player inventory. FQPS v3
loads existing v1/v2 saves. Keys, ownership and script guards still apply.
Loot RNG is runtime-owned rather than Bethesda RNG/save parity. Respawn timers,
quest-driven inventory/global changes, theft, storage and scripted containers
remain pending. Panel positioning, readability and controls need headset testing.

## Item pickup and door interaction: 0.27.0

Aim the right controller and press **A** to take supported loose items or use
authored load doors. Item names come from FULL; counts and condition come from
placed REFR XCNT/XHLP. Targeting follows moved props and checks authored collision
for intervening walls. Collected references disappear from rendering, grabbing
and collision, and stay collected across CELL changes, streaming and restarts.
FQPS v2 saves preserve world removals and can read existing v1 player saves.

Authored locked doors require their matching key in inventory. Lockpicking,
scripted activation, NPC/faction ownership/theft, containers and animated local
doors are pending. These unsupported actions are blocked. Explicit Player-owned
items and doors are supported; CELL ownership is inherited conservatively.
The existing Fallout HUD font/widget supplies prompts; the 3 m controller ray
and small item aiming tolerance are VR adaptations. Headset testing remains.

## Player stats and inventory foundation: 0.26.0

The startup worker loads the original Player NPC_ record, its SPECIAL, manual
skills, level, karma and CNTO inventory, plus 1,762 original inventory item
definitions. Health, AP and carry capacity use the installed ESM's game settings.
A scene-independent player session owns stack identities, quantities, item
condition, equipment slots and current health/AP. It saves to a versioned Quest
state file on scene commitment, OpenXR session stop and renderer shutdown.
Invalid or incompatible saves are preserved rather than overwritten.

This is the gameplay data/API foundation. There is no new Pip-Boy menu or pickup
input yet; equipment mutations do not rebuild the visible player body. Scripts,
character creation, perks/effects, level progression, consumption, combat,
encumbrance movement penalties, container ownership and Bethesda .fos import
remain separate work. The prototype starts from the authored Player template,
not a simulated Vault 101 quest progression or invented starter gear.

## Original NPC idle playback: 0.25.0

Actors in Megaton and explicitly loaded interior CELLs now share an authored
appearance assembly path instead of selecting only Lucas Simms. Female race
parts and wearable armour slots resolve correctly; appearance template categories
are inherited independently. Original skeletons and idle KFs drive weighted
body/clothing meshes and rigid head attachments. Compressed cubic B-spline,
linear, Hermite and XYZ rotation tracks are decoded from the supplied game data.

NPC CPU preparation runs in the scene worker; GPU shapes upload one per frame
behind the loading screen and publish with their scene. Old actor buffers retire
on transition. Actors remain at authored initial placements: schedules, navigation,
dialogue, combat, creature and levelled-actor resolution are later milestones.
No synthetic movement or replacement character assets are included.

FalloutQuest is a native ARM64/OpenXR reimplementation of the runtime needed to
interpret Fallout 3's original data on Quest. It is not a port of the original
Windows executable.

The Q2–Q16 prototype phase proved the major data/rendering path: BSA and ESM
access, NIF geometry/material loading, authored CELL/REFR placement, interiors,
doors/XTEL transitions, LAND terrain, collision/player movement, exterior
streaming, Level4 LOD, weather/imagespace/lighting work, loading/UI
infrastructure, and standalone OpenXR rendering.

Q17 froze the mature generated Q16.27 runtime into ordinary C++ source files.
The historical Q-series CMake text-rewrite chain is no longer part of the build.

The consolidation provides one thread-safe BSA index/extraction layer for mesh,
texture/cubemap and raw HUD assets. Worldspace placement selection and residency
planning now compile independently, with request-local selection and one owner
for live CELL streaming state. Existing DDS decoding, residency rules and GPU
budgets remain in place. See `docs/ARCHITECTURE.md`, `tests/assets/README.md` and
`tests/world/README.md` for ownership boundaries and portable checks. Startup and door transitions share background CPU preparation, budgeted GPU
uploads and phase timing. Player and renderer subsystems are being extracted
incrementally.

At the Q18 baseline, the Capital Wasteland streaming path built an
immutable in-memory index of authored Wasteland CELL/REFR/base metadata once,
then resolves rolling resident windows from that index instead of rescanning
Fallout3.esm on every cell crossing. Full-detail objects/collision remain a 3x3
active set, a 5x5 visual resident/prefetch set is retained, LAND remains a 7x7
runway, and Bethesda Level4 meshes provide the distant world.

Startup and door/CELL transitions now use original LSCR artwork on a tracking-space
panel 2.5 metres ahead, a random original weapon/prop at 1.6 metres toward the bottom left, and the
original loading01.nif compass at the panel's bottom right. Archive decoding and
vertex preparation run on workers; each loading mesh uploads on a separate
frame. The opaque loading target covers both eye images until scene completion.
Hidden world, sky, shadow and reflection draws pause under the loading screen;
Megaton/Wasteland initial collision snapshots prepare on a worker and publish
before finalization. Loading artwork UVs are flipped independently of model UVs.
The loading menu now reads its overlay and compass paths from the original
Misc.bsa XML and uses MainMenu RGB (199,255,165) with its authored 0.75-second
fade. LoadingAnim01.NIF geometry is composited over the artwork with authored
alpha/blend settings, keeping its own colour rather than the menu tint.
NIF controller animation playback remains pending inspection of the UI assets.
Wasteland warmup no longer reveals unfinished detail/LOD after a time limit.

See docs/ARCHITECTURE.md for the current source layout and development rules.

## Build

GitHub Actions builds an ARM64 debug APK for Quest. The native target uses
Android NDK 27, C++17, OpenXR, EGL and GLES3.

## Asset policy

This repository does not contain Bethesda game assets, executables, ESM files,
BSA archives, textures, meshes, sounds, or other copyrighted Fallout 3 data.
Users supply files from their own legitimate Fallout 3 installation.

This project is not affiliated with or endorsed by Bethesda Softworks or Meta.

Player body v162: [raised-arm planes, Pip-Boy runtime readiness and validation](docs/VR-BODY-v162.md).
The [v161 calibration and eye-alignment notes](docs/VR-BODY-v161.md) describe the preserved head/reach architecture.
The [v160 authored-rig audit](docs/VR-BODY-v160.md) records the original architectural rework.
