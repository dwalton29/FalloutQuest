# v239 — Original LAND shader work reuse and true v237 A/B

## Scope
Standalone Quest 3S, canonical main. Do not alter Bethesda source textures, NIFs, LAND geometry, draw distance or original authored LOD policy. v238 layer fusion remains default. Resolution, FP16 HDR, 2x MSAA, water and gameplay are unchanged.

## Work saved
Share sun direction and each active local light's direction and attenuation once per fragment across the original stacked LAND materials. Preserve material normal, normal-dependent Lambert, specular, per-layer shadow/bias, authored fog and ordered alpha blending. Skip exact zero-alpha paint samples (original result is unchanged) without deleting triangles. Gains are hypotheses until on-device GPU measurement; register pressure may offset arithmetic savings.

## Debug/O2 profile comparisons
Set the flags before process launch; force-stop and relaunch on each change.
A: terrain_legacy=0, terrain_unshared=0 => v238 fusion plus shared lighting (new default).
B: terrain_legacy=0, terrain_unshared=1 => v238 fusion with original per-material lighting, isolating shader work.
C: terrain_legacy=1, terrain_unshared=0 => true v237 shader and unfused v237 indexed terrain layers.
The existing debug.falloutquest prefix applies to both properties. In C, the unshared switch is ignored.
The shader logs Q239 TERRAIN SHADER: legacyV237=... unsharedLights=..., and the terrain fusion log exposes folded triangles and fallback. Note that this corrects a previous comparison confound, but does NOT establish that the user's reported broken legacy render is now fixed on-device.

Use an identical save, standing viewpoint, camera direction, weather/time, NPCs, streaming-settled cells, headset clocks, and render settings. Capture at least 20-30 s per mode, repeat each in alternating order and compare GPU median/p95 and cadence, not overlapping tile GPU phase timer sums. Log Q231 TERRAIN CLIP visibility counts, Q238 TERRAIN FUSION, Q239 TERRAIN SHADER and GPU usage. Test Megaton, a Wasteland view, water-visible and water-invisible. Inspect ground blends, seams, normals, fog, shadow, reflections, both eyes and streaming boundaries. A broken/missing terrain scene invalidates that comparison and must be debugged before inferring FPS wins.

Host Mesa GLES tests compare the production optimized shader and the original-lighting variant against retained v237 multipass for FP16 and RGBA8, with layered paint, zero-alpha regions, normals/specular, local lights and outside-radius lights. CI also compiles Android Debug and O2 APKs. No Quest FPS claim until captures.
