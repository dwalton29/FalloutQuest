# v284 — tiled occlusion and remaining hitch diagnosis

The v283 capture reports roughly 22.4 ms main-scene GPU cost and 27.8 ms total
GPU time in its first sustained gameplay section. The prior polygon buffer
retained 64 polygons, frequently exhausted its 40,000 comparison budget, and
reported no hidden shapes in half the sampled active checks. Separate 350 ms
application spans had no corresponding pipeline compilation event. This release
addresses occlusion and adds the measurements needed to locate those hitches.

## Tiled coverage

Each current eye has a 64×48 depth/coverage grid. A tile begins as an uncovered
rectangle. Subtract the interiors of the actual projected opaque polygons;
remaining pieces stay convex. Adjacent triangles/quads can therefore cover a
tile jointly. No centre-sample mask, synthetic solid bounding box, depth
readback, historical-camera result or distance reduction substitutes for
coverage.

A tile becomes covered only after all uncovered area has been removed. Its
conservative depth is the farthest depth of contributing surfaces. A later
polygon covering the entire tile can improve that depth. Partial near coverage
cannot reduce the depth of a later full farther surface. Clipping uses double
precision and a bounded current-frame registry of exact shared edges. Clipped
vertices retain edge identities, so roundoff does not leave artificial seams
when the camera rotates. Registry exhaustion skips additional source polygons. Bounded piece/vertex overflow
fails open, retaining visible geometry rather than approximating a filled tile.

Query bounds keep the existing world-space expansion and receive an additional
NDC expansion. Every overlapping tile must be fully covered and strictly in
front of the nearest object depth, independently in both eyes. Near-plane,
unknown, viewport-edge, budget and incomplete-coverage cases draw normally.

The source eligibility rules remain: original opaque static surfaces and
published LAND, excluding doors, loose/held items, animated actors, alpha/fading
occluders, hidden LAND quadrants and potentially mismatched cell borders.
Interior and reflection scopes retain their previous isolation. Publication,
origin, scene and camera changes invalidate the buffer and object decisions.

Nearby LAND gets an initial portion of the existing 900 µs build budget, using
rings around the local player position and interleaving the two nearest cells.
The remaining budget admits building surfaces. Rasterization checks the deadline
inside polygon processing; queries retain the 1,500 µs combined-work guard and
40,000 tile-test cap. Existing low-benefit/over-budget cooldown remains. Scratch
storage is reused; first visits can allocate bounded clipping storage. Device
performance and useful coverage still require the next Quest capture.

## Hitch measurements

No cause for the 350 ms gaps is proven by the old capture. A bounded allocation-
free frame trace now logs only frames taking at least 50 ms after `xrWaitFrame`.
It reports exclusive stages for XR begin, resident/save work, tracking/loading/
input, scripts/player work, interactions, streaming/doors/save, actors, UI,
render/present, XR end and the final tail. Early frame exits also report if slow.

Additional measurements report player saves taking at least 8 ms, completed
terrain-worker joins taking at least 2 ms, and worldspace-index lock waits taking
at least 2 ms. These preserve persistence and worker ownership semantics; they
do not attribute a nearby worker's elapsed time to the main thread by correlation.

Look for `Q284 FRAME HITCH`, `Q284 HITCH STAGE`, `Q284 SLOW OP` and
`Q284 ASSET WAIT`. `Q284 TILED OCCLUSION` adds covered tiles, clipping overflow
and raster work beside the existing hidden-shape/corner and CPU-cost counters.

## Validation

- Adjacent polygons/triangles, diagonal seams, real cracks, differing depths,
  partial-near/full-far regression and deadline/query-budget fail-open.
- Random rotated opaque polygons and query rectangles checked against independent
  dense point coverage of the original polygon union.
- Native Vulkan comparison of culled versus unculled stereo rendering: exact
  colour and depth, adjacent surfaces, genuine gaps, mixed depths, rotated views
  and an object visible around an occluder in only one eye.
- Existing runtime tests for camera changes, publication, missing data, interiors,
  mirrors, mutable objects, shared object decisions and cooldowns remain.
- Existing SpaceWarp motion/body, world/LAND, materials, water, UI, pipeline-cache
  and native-dispatch checks remain part of the full software Vulkan suite.

Software validation establishes correctness for these fixtures, not an FPS gain
or headset confirmation. The next comparison should include the same walking
route and head movements, and inspect both GPU scene time and CPU occlusion
cost. The existing `debug.falloutquest.occlusion=0` plus app restart provides an
unculled baseline if needed. No original game assets enter Git. Compressed
textures and material-block redesign are outside this release.
