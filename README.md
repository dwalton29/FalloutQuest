# FalloutQuest

Experimental standalone Meta Quest runtime for user-supplied Fallout 3 game data.

## Current milestone: Q18 — Wasteland cell residency

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

Q18 is focused on the Capital Wasteland streaming path. The runtime builds an
immutable in-memory index of authored Wasteland CELL/REFR/base metadata once,
then resolves rolling resident windows from that index instead of rescanning
Fallout3.esm on every cell crossing. Full-detail objects/collision remain a 3x3
active set, a 5x5 visual resident/prefetch set is retained, LAND remains a 7x7
runway, and Bethesda Level4 meshes provide the distant world.

See docs/ARCHITECTURE.md for the current source layout and development rules.

## Build

GitHub Actions builds an ARM64 debug APK for Quest. The native target uses
Android NDK 27, C++17, OpenXR, EGL and GLES3.

## Asset policy

This repository does not contain Bethesda game assets, executables, ESM files,
BSA archives, textures, meshes, sounds, or other copyrighted Fallout 3 data.
Users supply files from their own legitimate Fallout 3 installation.

This project is not affiliated with or endorsed by Bethesda Softworks or Meta.
