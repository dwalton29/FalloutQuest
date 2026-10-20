# v237 — Indexed native LOD and LAND, exact paint coverage

## Device basis

Quest 3S v236, commit 067094836f129288d5908b0e4e3a46542dc03963,
1680x1760 per eye, native O2, FP16 HDR, 2x MSAA:

| CSV session | Gameplay frames | Mean cadence FPS | Mean combined-eye GPU | GPU p95 (nearest lower sample) | Mean application work |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1792463397184 (Megaton) | 2777 | 45.03 | 21.14 ms | 30.18 ms | 8.23 ms |
| 1792463486169 (Wasteland) | 1180 | 19.16 | 50.68 ms | 59.34 ms | 20.44 ms |

GPU samples are delayed and taken every eighth frame (347 / 147 samples).
Fine geometry/resolve timestamp attribution remains unreliable on tile GPUs;
HDR envelopes include child stages. Changing views/populations prevent an
uncontrolled v234/v236 speedup comparison. The Megaton scene has two residents;
the Wasteland scene has none. CPU work includes render-driver stalls, and is
not additive with GPU time. Target 72 Hz is 13.889 ms with headroom desirable.

## Implemented

* Both original native LOD admission paths now explicitly mark immutable LOD
  provenance. The existing validated original-NIF indexing path accepts this
  provenance alongside STAT/SCOL/TREE. It covers Level4 objects/terrain,
  Level8/16/32 terrain and .High VWD without inventing an ESM record type.
  Skinned, dynamic, malformed or uneconomical meshes retain expanded geometry.
* Cold and streamed LAND use exact indexed 12-float vertices. Every position,
  normal, UV, paint alpha and vertex colour is part of the bitwise key. Triangle
  order and attributes reconstruct exactly. When indexing saves no total bytes,
  the original array path remains. Shadow and main/reflection draws use the
  appropriate index/array path.
* LAND vertices and indices share one GPU allocation with VAO-local element
  binding. Existing cancellation, deferred destruction and retirement therefore
  own the whole allocation. Streamed upload accounts for both regions and
  handles arbitrary byte boundaries without publishing an incomplete buffer.
* Paint triangles are removed only if alpha is exactly zero, or a later
  GPU-textured paint triangle with bit-identical ordered positions has alpha
  exactly one at all vertices. Reverse batch AND triangle order is respected.
  Missing/capped/failed textures never provide coverage. Base LAND is retained
  for depth, shadows and water/compositor reconstruction. Partial blends retain
  the original shader and layer order; this is not a baked-material approximation
  or a complete single-pass terrain renderer.
* Streamed completed geometry advances its batch cursor before a budget yield.
  Previously, completion at the budget boundary could reset staging then upload
  and publish the same batch again on the next update.
* Indexed and indexed-instanced draws now enter the existing GL draw/corner
  counters. v236 array-only counters understated indexed work: do NOT interpret
  a v237 draw/corner counter increase as an automatic performance regression.
  `Q237 NATIVE INDEX` reports actual indexed draw, original-corner and unique
  vertex counts. Cold LAND and streamed coverage publish separate diagnostics.

2x MSAA, FP16 HDR, render resolution, view distance, authored LOD selection,
water/refraction/reflections, bloom, lighting and NPC/gameplay policy remain at
the working v236 defaults. v235 sorting is not restored. HDR/MSAA render-to-texture,
full terrain pass fusion, water occlusion, multiview and world-target foveation
still require separate implementation and device validation; they are not claimed
as completed by this change.

## Validation

Functional host tests reconstruct indexed vertex bits, preserve all seam
attributes, test actual native-LOD eligibility/fallbacks, compare ordered RGB and
alpha blending over 400 deterministic varied-layer cases, and run the production
streamed uploader against a byte-recording GL driver with 17-byte budgets. Cases
include zero/full/partial alpha, missing textures, nearly identical positions,
same-batch ordering, indexed/array upload and VAO element offsets. Existing GL
cache, timestamp, visibility, loading, render-order, inventory and interaction
regressions remain in Android CI alongside Debug/O2 compilation.

## Headset acceptance

1. Install the v237 **O2 profile** artifact as an update and use the updated
   `tools/performance/capture-quest.ps1` (still locates adb beside the script).
2. Capture the same save, standing position, view, game time and population in
   Megaton and a settled Wasteland view; compare GPU median/tails and cadence.
3. Confirm native `indexedDraws` and `uniqueVertices` show actual LOD use. Check
   cold LAND `coveredPaintTriangles` and streamed per-cell coverage/timing.
4. Inspect near and distant terrain, paint transitions, normals/specular,
   colours, seams, all LOD tiers/landmarks, water and both eyes.
5. Walk across several streaming boundaries in both directions, then return
   through an interior. Verify no duplicate terrain, holes, corrupt indices or
   new upload stalls. Test lockpick exit and depth-dependent water/composition.
6. Test a populated Megaton and combat separately: this capture's population
   is too light to establish the final CPU budget.

Performance improvement and sustained 72 Hz are **unverified until device A/B**.
