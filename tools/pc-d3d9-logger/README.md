# Fallout 3 D3D9 renderer-state logger

This is a 32-bit `d3d9.dll` proxy for the PC copy of Fallout 3. It forwards to the real Windows D3D9 runtime, then hooks the game's D3D9 device so a single frame can be inspected without changing the game's shader code.

The tool is intended to answer one specific FalloutQuest question: **what exact shader/state/constants does Fallout 3 bind when it draws Megaton?**

## What one F10 capture records

For every draw call in the captured frame:

- pixel/vertex shader bytecode hashes and shader-model tokens;
- textures bound to stages 0-7, including size, format and a small content signature where lockable;
- `D3DRS_SRGBWRITEENABLE`;
- `D3DSAMP_SRGBTEXTURE` plus filtering/address state for samplers 0-7;
- depth, culling, alpha blending, fog and colour-write render state;
- all non-zero pixel float constants `c0-c31`;
- all non-zero vertex float constants up to the device's `MaxVertexShaderConst`;
- indexed/non-indexed draw parameters.

The logger queries the device at the draw call rather than relying on earlier `Set*` calls, so state applied through D3D9 state blocks is still visible.

## Build locally

Requirements: Visual Studio 2022 / Build Tools with **Desktop development with C++**, Windows SDK and CMake.

Run:

```bat
cd tools\pc-d3d9-logger
build.bat
```

The output is:

```text
build\Release\d3d9.dll
```

This must be built as **Win32/x86** because Fallout 3 is a 32-bit process.

## Capture Megaton

1. Check the Fallout 3 folder first. If it already contains a third-party `d3d9.dll` (for example another wrapper), do not overwrite it; move/rename that wrapper temporarily.
2. Copy this logger's `d3d9.dll` beside `Fallout3.exe`.
3. Launch Fallout 3 normally.
4. Go to the exact Megaton view you want to compare with FalloutQuest and hold the camera still.
5. Press **F10 once**.
6. The logger captures the *next complete presented frame only*.
7. Exit the game normally.
8. `Fallout3D3D9.log` will be beside `Fallout3.exe`.
9. Remove/rename the proxy `d3d9.dll` when finished to return the game to its normal configuration.

If the proxy loaded correctly, the log begins with `FQ_D3D9_LOGGER` and includes `D3D9_PATCHED`, `DEVICE_PATCHED`, `FRAME_BEGIN` and `FRAME_END` records.

## Map the capture to Shader Package 17

`analyze_capture.py` understands Fallout 3's SDP records and HLSL CTAB metadata. With `shaderpackage017.sdp` and the capture log:

```bat
python analyze_capture.py C:\path\to\shaderpackage017.sdp C:\path\to\Fallout3D3D9.log
```

It writes:

- `Fallout3D3D9-summary.txt` — shader-pair usage and SP17 shader-name mapping;
- `Fallout3D3D9-pplighting.csv` — only PPLighting draws whose CTAB exposes `AmbientColor`, `PSLightColor` and `BaseMap`, with those values plus BaseMap/NormalMap sRGB state.

The CSV is the useful artifact for the current cyan-lighting investigation. It lets FalloutQuest compare the PC renderer's real `AmbientColor`, `PSLightColor`, shader choice, sampler colour-space state and bound texture signatures against the Quest renderer instead of relying on visual A/B guesses.
