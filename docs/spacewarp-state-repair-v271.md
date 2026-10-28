# SpaceWarp colour preservation and all-scene defaults — v271

The reported missing terrain textures, player model, distant LOD and sky occurred with SpaceWarp `on`, while the ordinary renderer worked. This change repairs an XR/GL state-isolation gap and enables synthesis by default for every gameplay worldspace and interior. Original game meshes, textures, sky materials and LOD selection are unchanged.

## State repair

The previous guard began inside the motion renderer, after XR motion/depth image acquisition, and ended before their release. The primary colour/depth releases and `xrEndFrame` were also outside it. A runtime-side sampler or binding change in those gaps survived into later scene rendering. Resetting the texture cache cannot undo a bound sampler object: sampler state can override a material texture's filters and completeness, including the texture containing the actor bone palette.

The new boundary encloses XR creation/setup, frame begin, motion acquisition and rendering through all eye-image releases, and final frame submission. It restores the real driver state and its matching cached state. All 16 renderer-used texture/sampler units are covered, including 2D and cube-map bindings. Viewport/scissor, framebuffers, VAO/buffers, depth state, blend state, clear values, colour masks and pixel-unpack state are restored too. Ordinary colour rendering explicitly reinstates the engine's texture-object sampling policy after acquisition. The motion renderer reuses the outer boundary instead of taking a duplicate snapshot.

The shared uniform cache also had a reproducible invalidation bug. A uniform write while the current-program cache was unknown still changed the real driver's program, but left that program's old value cached. Rebinding the program and requesting the old value then incorrectly skipped the necessary upload. Such scalar and array writes now invalidate the affected cache history. A real colour-pixel test exercises this case; the old cache logic produces the wrong colour even after rebinding.

A software GLES regression reproduces the gap by leaving a mipmapped sampler bound across a handoff. Cache invalidation alone fails to restore the colour/skinning samples. With the full boundary, before/after pixels match for textured land, a palette-skinned draw, distant mesh and cube sky across repeated simulated acquisition/release/submission state changes. It also verifies zero GL queries in native Vulkan mode. These are synthetic test resources, not replacement game artwork.

This reproduces a concrete failure mechanism; the actual headset's runtime state changes have not been observed here. Periodic `FQ SPACEWARP boundary` logs include a `samplerChanges` bitmask immediately before restoration, so a headset log can confirm which handoffs changed sampler bindings.

## All-scene policy

An unset property now selects synthesis. Explicit `on`/`auto` do the same; `0` disables it, while `validate`/`1` and `visualize` retain their diagnostic meanings. No ADB enable command is required for normal startup.

There is no Wasteland-only filter in deformation capture, frame eligibility or composition attachment. Interior world ID zero is valid. Changing worldspaces or scene generations resets temporal history and establishes two new submitted stereo frames before synthesis resumes. Interior tests verify actual EXT metadata attachment after warm-up.

Capability and complete-coverage fallbacks remain: loading, focus loss, modal interactions, unsupported HUD/water/transparency, incomplete history and producer failures still use native rendering. Vulkan continues to use its native backend because this motion producer is GLES-only. Being indoors no longer disables the feature.

## Checks

The new colour-handoff pixel test runs in CI alongside the existing motion, depth occlusion, skinning, facial deformation, GPU upload reuse and OpenXR adapter tests. Host tests cover default mode selection, interior eligibility, world transitions and temporal invalidation. Android Debug/O2 and Vulkan CI must pass for the published build. Headset visual and cadence verification remains necessary.
