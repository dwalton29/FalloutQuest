# Quest opaque submission pass

The supplied Wasteland run measured 45–49 ms CPU wall time in the opaque pass
and 11–14 application FPS against a 72 Hz / 13.89 ms target. GPU time was not
measured in that run; CPU timings cannot establish whether shader cost dominates.
No after-change headset measurements are available yet.

This pass retains the first commit's 2× MSAA, optional OpenXR compositor depth,
shared/cached 512² reflections, stereo-conservative visibility and per-frame
instancing cache. It excludes the second commit completely. Stereo still renders
each eye sequentially; there is no multiview shader, target or import path.

## Submission changes

All native render translation units route GLES state changes through the shared
renderer-owned cache via the platform prelude. The cache covers program, VAO,
array buffer, active texture, separate 2D/cube bindings on units 0–15, depth,
blend factors, cull/winding, polygon offset and alpha-to-coverage. It avoids
duplicate driver calls, handles deleted resource names and resets on EGL context
creation. Untracked framebuffer/viewport queries remain at pass boundaries.
New code bypassing these entry points must invalidate the cache explicitly.

`DrawSceneObject` makes no driver state queries or save/restore calls. It computes
the final authored stencil winding/two-sided state, flips winding for mirrors,
and transitions out of decals only for ordinary materials. Native terrain LOD
retains its tier-specific positive offsets. Caller raster state is restored at
scene/mirror boundaries before other passes.

Existing frame/eye lighting, fog and camera uniforms remain uploaded at pass
boundaries; player LOD data and terrain LOD clipping bounds now also upload once
per eye/pass. A fixed-capacity per-program/location value cache suppresses equal
scalar, vector and matrix uploads, including identical consecutive material
values under the existing q2016/q2017 batch keys. It allocates no memory per draw.
This deliberately keeps the existing shader layout rather than introducing a
UBO ABI change alongside state ownership. Genuinely differing material values
and object transforms still upload. Uniform arrays bypass value caching and invalidate overlapping scalar history.

## Measurement

`OPAQUE PERF` reports rolling means over up to 120 eye passes:

- `prepCpuUs`: batch sorting, visibility, grouping and matrix preparation;
- `submitCpuUs`: remaining opaque CPU wall time, including instancing buffer
  upload, native LOD and actor/body updates;
- `gpuUs` and `gpuSamples`: asynchronously completed EXT timer-query samples;
- draw calls, state changes, uniform uploads, texture/VAO binds, driver state
  queries, detailed visible placements, submitted vertices and triangles;
- diagnostics, disjoint events and skipped samples when the query ring is full.

GPU timer support is negotiated from GL_EXT_disjoint_timer_query and counter
bits. Eight queries are reused only after completion. Results are polled at least
three stereo frames later; unavailable results are never read. Disjoint events
invalidate pending/rolling GPU samples. No glFinish, fence wait or blocking query
read is used. gpuSupported=0 or gpuSamples=0 means GPU time is unavailable, not
zero. Mobile tile-renderer timings may include boundary overhead; compare like
for like. CPU and delayed GPU rolling windows are independent, not paired samples.

## Developer A/B procedure

Use the same save, position, view direction, time/weather and traversal route.
Allow streaming/shader warmup to settle, then capture at least 20 seconds per run.
Record the baseline supplied above and attach each new log; do not claim a speedup
from compilation or host tests.

Debug APK only; set each diagnostic before launching/restarting the app:

```sh
adb shell am force-stop com.falloutquest.app
adb shell setprop debug.falloutquest.fast_material 0
adb shell setprop debug.falloutquest.render_scale 1
```

Run normal materials/full scale first. Then restart with fast_material=1 and
render_scale=1. The diagnostic bypasses normal mapping, specular, local lights,
glow, shadows and fog in opaque fragments, retaining the same vertex shader,
diffuse/alpha tests, material opacity, LOD clipping/fade and geometry visibility.
Alpha, environment, reflections and UI retain normal materials. This visual
mode is for diagnosis only; release builds ignore both properties.

Finally restart with fast_material=0 and render_scale=0.7. Both swapchain and
scene targets shrink together, preserving color/depth correspondence. Defaults
are full scale and normal materials; reset both properties afterwards.

A large GPU reduction with simple materials points toward fragment/material
cost. A large reduction at 0.7 scale points toward fragment/fill cost. Similar
GPU time at both scales points toward geometry or other GPU overhead; high CPU
submission with low GPU time points toward CPU/driver submission. These are
diagnostic indications, not proof in isolation.

## Placement lifetime audit

The Q19 streamer already compacts gObjects when cells leave CacheRadius=3 and
are no longer explicitly wanted. It retains the 7×7 warm set and bounded
directional prefetch strips, protecting existing LOD handoff. Re-publication
replaces prior placements in that cell instead of appending duplicates.
Retired placements cease drawing immediately; a budgeted deletion queue drops
their shared mesh references. Mesh buffers are deleted at the last reference;
the texture caches intentionally retain shared assets until scene teardown.

`DETAIL LIFETIME` logs active detailed cells, GPU-ready cells, placements added
and removed at publication/retirement, total live/staged/deferred placements,
shared buffer entries/refcounts and retained 2D/cube textures. Compare these
over traversal back and forth after warmup. More objects in a denser window can
be normal; unbounded departed placement growth would be a bug. No residency or
gameplay policy changed in this pass.

Acceptance still requires a Quest run to validate authored material/culling
parity, head-motion stability and actual CPU/GPU improvement.
