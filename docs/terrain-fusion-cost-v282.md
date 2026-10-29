# v282 — contain native terrain fusion shader cost

The v281 headset capture reports correct terrain but a severe performance
regression. The captures are different traversals, not a controlled identical
camera benchmark. Their Wasteland timing medians nevertheless locate the large
increase in scene shading:

| Time (ms) | Previous v279 Wasteland | v281 capture |
| --- | ---: | ---: |
| Total stereo GPU | 27.592 | 56.520 |
| Main GPU scene | 21.118 | 49.320 |
| CPU work span | 15.553 | 20.262 |
| CPU record span | 12.286 | 14.022 |
| CPU draw recording | 3.263 | 3.734 |
| CPU upload | 0.253 | 0.450 |

The new capture also has GPU scene p95 99.321 ms and frame-reuse wait p95
14.910 ms. CPU spikes include waiting on GPU resources, so they must not all be
attributed to gameplay calculations. NPC package/navigation counters remain
small in this capture. SpaceWarp synthesis is attached in the sampled statuses.

## Changes

- A fused program registration previously selected the nine-sampler shader for
  *all* LAND batches. Only 70–81 of 229–268 LAND draws in representative samples
  actually had fused geometry. Select the original material for every retained
  12-float batch, with its three samplers and 688-byte uniform prefix instead of
  nine samplers and 768 bytes. The shader builder rejects incompatible prefix
  layouts. The immutable queued material snapshot is still used; no per-draw
  uniform remapping or new allocation is introduced for fallback selection.
- Remove the shared eight-light direction/attenuation arrays passed into each
  layer helper. Use the original per-material local-light calculation, keeping
  intermediates within the layer. Excess register pressure/spills on Adreno are
  a plausible explanation, not a measured hardware-counter finding.
- Specialize fused pipelines for their actual 1–3 paint layers, allowing the
  driver to remove unused layer work. Paint count participates in the pipeline
  cache key and is read from the queued draw's immutable uniform snapshot.
  Spatial alpha skips and explicit texture gradients remain intact.

Fusion geometry, texture resolution, view distances, normal/specular/shadow/fog
calculations, SpaceWarp motion and foveation policy are unchanged. These changes
remove unnecessary material cost; they are not a measured FPS gain on Quest.

## Validation

The native software Vulkan suite compares fused/retained colour and exact scene
and retained motion depth against the original material, in both eyes, ordinary
and split SpaceWarp passes and density attachment emulation. It covers 0–4 paint
layers, unsupported and mixed fallback, indexed/nonindexed input, mipmapped
textures, normal/specular/HDR/fog/shadow/clip cases, and 0/1/8 local lights
including an out-of-range light. It also checks that retained terrain pipelines
never use the fused layout and that their descriptors use the original size.
Shader ABI/compile/validation, production factory fallback, sliced upload, and
GLES material parity tests are retained. Software tests establish correctness,
not headset speed or actual Adreno density execution.

For a controlled comparison, restart in the same scene with
`debug.falloutquest.terrain_fusion` set to `0` (original terrain) versus `1`
(fusion). The existing `FQVK MATERIAL fusedLandDraws`, `FQVK GPU sceneMs`,
`FQVK TIMING stereoGpuMs` and `FQVK CPU workSpanMs` counters identify each path
and its cost. No original game assets or diagnostic captures are stored in Git.
