# v241 — conservative current-frame occlusion

The user prioritised hiding genuinely invisible geometry while retaining the
long Wasteland view. This milestone adds analytic CPU occlusion for static
NIF shapes and native object/terrain-LOD shapes. It makes no further distance,
fog, material-quality or mesh-detail reductions. Published LAND is an occluder;
LAND batches themselves, actors, doors, physics objects and water are not culled
by this milestone. Large terrain blocks may remain visible when no single
selected surface proves complete coverage. No headset FPS gain is asserted.

## Correctness and data

- Original STAT/SCOL NIF triangles are selected during existing CPU mesh
  preparation. Retain at most 16 large triangles per shape; merge only pairs
  sharing an oppositely directed original edge. No bounding boxes or guessed
  solid walls are used as occluders.
- Diffuse pixels must all have alpha 255, including resident/borrowed images.
  Reject blended, alpha-tested, vertex-alpha, no-light opacity falloff, decal,
  skinned, moving and non-depth-writing sources. Original backface winding and
  two-sided state are honoured. Fading Wasteland detail cannot occlude.
- LAND sources are committed visual/ground CELLs with matching world, origin,
  scale, live opaque GPU batches and force-hide masks. Never read worker/legacy
  scratch. Skip seam border quads and authored hidden quadrants. Streamed ground
  heights are the same stitched heights used to build published geometry.
- Project each source into both current eye matrices. Reject near/far-clipped,
  malformed, folded and non-convex surfaces. A projected convex original quad is
  covered by its two original triangles, including non-planar LAND quads.
- Reject a target only if its entire expanded AABB is strictly inside a real
  occluder in each eye and strictly behind the farthest source vertex in depth.
  Use a 15 cm bounds expansion, 0.004 NDC edge inset and 0.00001 NDC depth gap.
  Partial visibility, uncertain data and viewport/near-plane intersections draw.
- Cache decisions only for the current stereo preparation generation. Rebuild
  on frame, stereo matrices, scene storage/count, world or visual revision
  changes. No historical camera visibility is used. Mirrors ignore main-eye
  rejection so main-hidden objects can still appear in reflections.

## Work and overhead

Select up to 64 static source shapes and interior quads from the two closest
committed LAND CELLs. Each eye retains the 64 largest projected sources. Test
up to 40,000 polygon predicates per preparation and stop conservatively.
Source preparation has a soft 900 microsecond guard; combined measured build
and query work has a soft 1,500 microsecond guard. Checks are periodic and OS
scheduling can exceed these guards. All remaining targets draw when a guard
is reached. Checking spends CPU time; it is not intrinsically free.

After an over-budget frame pause for 30 frames. After saving fewer than 10,000
unique shape corner entries pause for seven frames. Retries use fresh current
views. The corner threshold is a heuristic, not a measured GPU break-even rule.
There are no GL calls, GPU query waits, readbacks or added depth passes in the
occlusion checker. Culled instances are removed before matrix upload/batching;
individual/native draws reject before GL state submission. Lifetime follows
existing CPU/GPU scene ownership, with no unbounded triangle cache.

`Q241 OCCLUSION` reports source polygons, tested/hidden shapes, unique hidden
corner entries, predicate count, CPU time, guard stops and cooldown. Hidden
corner entries are not a claim about unique transformed vertices or total
per-pass GPU savings. Existing capture counters measure actual submissions.

## Foveation correction and comparison

The v240 five-region draw replay is now opt-in. It multiplied geometry work
and is a poor default for this workload. Keep it off for this comparison:

```bat
adb shell setprop debug.falloutquest.foveation 0
adb shell setprop debug.falloutquest.occlusion 1
```

Occlusion is on by default. Set `debug.falloutquest.occlusion` to `0` for an
otherwise identical baseline, and `1` to re-enable. Restart after changing
either property. Use the same camera/path and compare draw/geometry counts,
GPU eye time, CPU work and Q241's checking cost. Include walls, hill crests,
water, rapid head movement, doors/CELL transitions and both eyes. Open views
can benefit little and should show cooldown rather than constant wasted work.

Portable tests cover real polygon coverage versus rectangle bounds, depth,
two-triangle unions, opacity, near-plane/viewport failures, non-convex quads,
randomised coverage, work caps, stereo caching, reflection independence,
publication changes, camera movement, loading and cooldown. The APK workflow
runs both new tests alongside the existing renderer/world/gameplay checks.
