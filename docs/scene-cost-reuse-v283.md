# v283 — scene shading, bandwidth and first-use pipeline costs

## Scope

The v282 capture has median stereo GPU times of 27.9 ms in the sampled Wasteland,
27.0 ms at Rivet City without synthesis, and 28.8 ms near Tenpenny Tower. Main
scene rendering accounts for about 20–22 ms. The application budget for 36 Hz
with 72 Hz synthesis is 27.78 ms. These are different traversals and cameras,
not a controlled performance benchmark.

CPU draw recording also has isolated 195–257 ms spikes. Pipeline creation was
synchronous during draws and used no explicit Vulkan pipeline cache. It is a
strong suspect; v283 adds actual compilation timings rather than attributing
all application hitches to this operation.

## Implementation

- **Terrain lights:** retain the existing selected light list and original
  radii, colours, signed contributions and ordering. Per batch, conservatively
  reject only sphere/AABB pairs that cannot intersect the submitted geometry.
  Include a numerical tolerance; invalid/unknown bounds retain lights. Both
  original 12-float and fused 15-float materials use the resulting count. Zero
  lights selects the existing specialized shader. No light selection radius or
  authored visual setting changes.
- **Water:** defer the opaque colour/depth snapshot until the first exposed
  cell that can intersect either native eye. Use conservative padded bounds in
  the same renderer space as the vertex shader. Do not take a snapshot or upload
  water geometry when no cell survives. The existing shared snapshot is taken
  once before the first water draw, preserving refraction of the pre-water scene.
  Probe/GLES dispatch remains supported. This saves the measured ~1.4–1.7 ms copy
  only in qualifying views; it is not a guaranteed saving when water is visible.
- **Material submission:** build a uniform location index once per registration,
  retain correct array-element strides, and skip byte copies for unchanged
  values. Cache skin/round-mode field offsets. Every queued draw still takes its
  immutable material snapshot; later uniform changes cannot modify an earlier
  draw. Pipeline keys normalize disabled culling, blending and depth state, so
  irrelevant state changes cannot create duplicate native pipelines.
- **Pipeline reuse:** use a real `VkPipelineCache` for world and fullscreen
  creation. Key on canonical shader bytes, schema, vendor/device/driver and cache
  UUID; validate the stored checksum and standard cache header and fall back to empty on rejection.
  Persist explicit observed pipeline state words, never raw C++ structs. Bound
  file/recipe sizes and reject invalid or currently unavailable states.
- **Prewarming:** before the first interactive frame, compile compatible learned
  recipes and a small set of common opaque world/terrain variants, including
  native/split and current density passes. This shifts some first-launch work
  into startup. Newly encountered states can still compile during gameplay;
  subsequent launches can prewarm those too. This does not guarantee zero hitches
  or improve steady GPU cost on its own.
- **Persistence:** snapshot dirty driver data no more than once per ten seconds,
  checked every 180 world frames, then write owned byte copies on a joined file
  worker using replacement files. Vulkan calls remain on the render thread.
  Shutdown flushes pending data; force-stop can lose the last unsaved additions.
  Snapshot CPU time is logged and may itself have a small cost.

Rivet City's existing SpaceWarp history rejection is outside this performance
pass. SpaceWarp resolution, dynamic-body motion, synthesis eligibility, texture
resolution, draw distances and authored material formulas are unchanged.

## Validation and headset follow-up

- Software Vulkan colour/depth parity versus original multipass terrain, now
  comparing the full light list against production light culling for 0/1/8 lights
  in normal, split and density attachment emulation passes.
- Geometric tests cover boundary intersections, inside/outside/unknown/nonfinite
  bounds, signed light colours and preserved list order.
- Actual Vulkan cache save/reload, recipe prewarm and corrupt-file fallback;
  either-eye water visibility and conservative clip boundaries.
- Existing queued uniform ownership, skin/facial motion, world/UI/water/post
  output, terrain upload, fallback and native dispatch tests remain in place.

These are correctness checks on a software GPU, not a Quest performance result.
Use identical positions/views for the next comparison. First-launch startup
warming is intentionally different from a warm-cache second launch. Counters:

- `FQVK PIPELINE compileUs=... warmup=...`: actual first-use vs startup creation.
- `FQVK PIPELINE WARM`: startup count and elapsed time.
- `FQVK PIPELINE CACHE`: load/save size and render-thread snapshot duration.
- `Q283 TERRAIN LIGHTS`: candidates, retained and rejected references.
- `FQVK UNIFORMS`: cumulative calls and unchanged values.
- Existing `snapshotCopyMs`, `sceneMs`, `stereoGpuMs` and `workSpanMs` show the
  corresponding GPU and CPU costs. Do not add medians of overlapping scopes.
