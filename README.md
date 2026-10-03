# FalloutQuest

Experimental standalone Meta Quest runtime for user-supplied Fallout 3 game data.

## Current milestone: 0.27.0 — Item pickup and door interaction

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
