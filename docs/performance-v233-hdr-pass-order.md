# v233: reflection before the main HDR render pass

## Device evidence and limits

The v232 O2 session `1792452726304` has 1,847 gameplay frames, median CPU
work 13.936 ms, cadence 33.280 ms and measured combined eye interval
32.352 ms. It has 8–9 resident NPCs, so it is not a matched comparison to the
previous v231 Debug session with 2 residents. GPU utilisation in its device log
is 95–99%. All 7,571 returned timestamp pairs passed arithmetic/disjoint checks.

Those checks do not establish correct per-shader attribution: most geometry
intervals are approximately 1–2 microseconds, while both-eye resolve scopes
have a median sum of 9.948 ms. On a tiled GPU earlier rendering can complete at
later framebuffer boundaries. Treat these as observed intervals, not proof
that the copies themselves consume 10 ms or that geometry is free. Reference:
https://registry.khronos.org/OpenGL/extensions/EXT/EXT_disjoint_timer_query.txt

## Structural change

Before: main HDR clear/sky/world/LAND → switch to mirror framebuffer → restore
unfinished main framebuffer → snapshot colour/depth → water → final resolve.

After: same mirror → main HDR clear/sky/world/LAND → snapshot colour/depth →
water → final resolve.

The early reflection uses the existing visibility decision, dominant plane,
stereo-centre reflection camera, current scene/light publications, geometry
lists, shaders, 512x512 target and cache invalidation. It runs after the main
world lighting uniforms are configured but before any main colour/depth draw.
The main camera MVP is explicitly restored after the reflection. Reflection
and main clears enable depth writes for their clear, with state restored.
Loading/fallback paths retain their clear, with sky suppressed for loading and
interiors. The main sky uses exactly the MVP already prepared by RenderEye.

This removes the *mirror-induced* interruption of a populated HDR target; the
necessary water snapshot still divides main rendering into two segments.
It is expected to reduce redundant store/reload work, but the size of any
headset benefit has not been measured. There are no new forced GPU flushes,
waits or glFinish calls. Resolution, 2x MSAA, HDR formats, reflection fidelity,
material/world/actor order within the main scene, bloom and assets are retained.
No gameplay, script scheduling, NPC population or draw-distance change.

The pre-water and final depth resolves both remain. Water and ordinary HUDs
usually do not write depth, but lockpicking and other late rendering can alter
it before compositor submission. Removing that final resolve without a robust
write dependency would risk stale compositor depth.

## Measurements

CSV format 233 retains the original columns and appends `gpu0/1_hdrFirstPassUs`
and `gpu0/1_hdrContinuationUs` envelopes. The first begins with the main clear
(after any reflection) and ends after the water snapshot resolve, or after the
final resolve when no snapshot occurs. The continuation begins before rebinding
the main HDR target after the snapshot and ends after final resolve. These
include world/water/UI and resolve work in their intervals; they overlap the
fine scopes. Counter validity does not guarantee perfect hardware attribution.
Compare their coverage with the eye totals before trusting a pass breakdown.

`reflectionPasses`, `waterSnapshots`, `colorResolvePixels`, `depthResolvePixels`
count actual calls/transfer pixels across both eyes on every captured frame.
The configuration identifies `hdrPolicy=reflection-before-main-clear`.
GPU measurements still sample every eighth frame; inspect sampled versus
unsampled cadence for measurement overhead. Fine draw scopes remain available
for diagnostics but the summary labels their tiled-GPU attribution unreliable.

## Capture

The updated `tools/performance/capture-quest.ps1` requires v233 O2 by default,
checks the unique capture ID/save/schema/native build, and locates adb.exe next
to the script or in the current directory before trying PATH. It needs no PATH
workaround when placed in platform-tools. Install the new APK as an update,
load Megaton, keep the headset focused and run:

```powershell
powershell -ExecutionPolicy Bypass -File .\capture-quest.ps1
```

Collect comparable standing views with visible and invisible water and an
interior; also check lockpicking and compositor depth. Keep the same save,
position, view, game time and headset conditions when comparing v232/v233.
Target 72 FPS remains unverified.

## Validation

Host timestamp/capture tests and renderer source regressions pass locally.
New scheduling checks cover reflection-before-clear/sky/world, main-MVP restore,
water snapshot ordering, main/reflection clear depth masks, retained compositor
depth and timing endpoints. The PowerShell collector integration test runs when
pwsh is available, with an executable adb beside the script, and rejects Debug
before beginning a recording. Android CI builds both Debug/O2, validates actual
compiler flags/diagnostics and runs the existing host/gameplay/shader suites.
Visual equivalence, GPU timing attribution and performance require the headset.
