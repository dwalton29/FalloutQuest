# FalloutQuest

Experimental standalone Meta Quest runtime for user-supplied Fallout 3 game data.

## Current milestone: Q17 — consolidated native runtime

FalloutQuest is a native ARM64/OpenXR reimplementation of the runtime needed to
interpret Fallout 3's original data on Quest. It is not a port of the original
Windows executable.

The Q2–Q16 prototype phase proved the major data/rendering path: BSA and ESM
access, NIF geometry/material loading, authored CELL/REFR placement, interiors,
doors/XTEL transitions, LAND terrain, collision/player movement, exterior
streaming, Level4 LOD, weather/imagespace/lighting work, loading/UI
infrastructure, and standalone OpenXR rendering.

Q17 freezes the mature generated Q16.27 runtime into ordinary C++ source files.
The historical Q-series CMake text-rewrite chain is no longer part of the build.

See docs/ARCHITECTURE.md for the current source layout and development rules.

## Build

GitHub Actions builds an ARM64 debug APK for Quest. The native target uses
Android NDK 27, C++17, OpenXR, EGL and GLES3.

## Asset policy

This repository does not contain Bethesda game assets, executables, ESM files,
BSA archives, textures, meshes, sounds, or other copyrighted Fallout 3 data.
Users supply files from their own legitimate Fallout 3 installation.

This project is not affiliated with or endorsed by Bethesda Softworks or Meta.
