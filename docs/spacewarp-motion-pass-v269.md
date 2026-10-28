# Wasteland SpaceWarp motion producer — v269

The GLES eye render path now produces actual runtime-sized motion/depth images. This extends v268's runtime/history foundation. It still runs at native application cadence and does not attach synthesis metadata. Complete UI/water/effect coverage and artificial-locomotion submission remain the next integration work; this version claims no frame-rate improvement.

## Renderer integration

- Query the newly allocated OpenXR swapchain images once at startup. Each eligible eye acquires and waits for its own motion/depth images, renders, flushes and releases them. Failed acquisition, wait or release latches the producer off until restart. A failed wait is never followed by an invalid release. Session shutdown owns any unrecoverable outstanding image.
- Read the resolved single-sample world depth after the normal eye post resolve, before the next eye reuses it. Reconstruct static-world camera motion in one unlit full-screen pass. The depth silhouette already includes detailed scenery, instances, native LOD, LAND and authored alpha-test cutouts.
- Capture only actual main-eye dynamic draws, excluding reflections and the additive environment pass. Replay their current geometry against previous successfully submitted model matrices, bone palettes and CPU position streams. This covers player body/hands, NPCs, equipped/stowed/world weapons, loose physics items, animated doors and script-animated shapes. Script-animated shapes are excluded from static instancing so their individual transforms can be recorded.
- The replay uses original indexed/non-indexed geometry, texture alpha thresholds, material/vertex alpha, culling/winding and decal offset. It performs no lighting, normal-map, glow, environment or material-colour calculations. Alpha-tested geometry alone samples its diffuse texture.
- GPU skinning reproduces the production player's residual weights and the NPC palette-normalisation rule. CPU skinning stores the resulting position stream; facial parts store their deformed bind positions before palette skinning. Unchanged facial streams and unchanged palette snapshots are reused.
- Previous-pose GPU resources are cached by drawable lifetime identity, allocated only on growth and uploaded once per logical frame. Both eyes share the upload. Retire departed identities, with an 8 MiB GPU history budget in addition to v268's bounded CPU history. Overflow invalidates coverage. There are no GPU readbacks in gameplay.
- The isolated GL pass restores framebuffer bindings, viewport, program, VAO, buffers, texture/sampler units, pixel-unpack state, depth range/mask/function, colour masks, winding, culling, offset and affected capabilities. It compiles before legacy viewport/depth interception macros; the world state cache is invalidated at the boundary.
- EXT motion encoding converts GLES displacement to Vulkan NDC: `(x, -y, z/2)`. FB uses GLES NDC displacement. Background depth remains 1 with zero supplied vectors, for runtime background handling once submission includes the correct artificial delta pose.

## Eligibility and remaining gaps

The producer and actor deformation snapshots run only for a ready, focused Wasteland world (`0000003C`). Interior/Megaton scenes, loading and modal interactions retain the normal renderer. Session-owned XR targets persist across transitions; temporal scene history resets as in v268.

Missing previous geometry, changed deformation dimensions, contradictory stereo snapshots, exhausted history, incomplete GPU resources, visible water, blended/non-depth-writing objects and colour-only HUD draws all invalidate complete synthesis coverage. Unsupported content is reported rather than silently treated as static scenery. These gates deliberately mean coverage will be incomplete in many normal gameplay frames, especially with weapon HUD, damage bars, notifications or water visible. Those paths must be handled before enabling ordinary Wasteland synthesis.

## Device diagnostics

Set the property before starting the app:

```sh
adb shell setprop debug.falloutquest.spacewarp validate
```

This fills motion/depth targets while retaining ordinary colour presentation and native cadence. `1` is an alias for validation. For a direct vector view:

```sh
adb shell setprop debug.falloutquest.spacewarp visualize
```

Restart after changing modes. Eligible world views show grey for zero motion and colour for signed XYZ displacement; UI is replaced in this diagnostic view. Visualization samples the motion image while the app still owns its acquired XR image. Reset the property to `0` and restart to disable allocation, capture and motion rendering.

`FQ SPACEWARP motion` logs show eye, valid producer output, dynamic draw count, complete coverage, reason bitmask, CPU submission time, uploaded bytes and allocation count. Reasons: `01` missing history, `02` deformation mismatch/missing data, `04` transparency/depth policy, `08` water, `10` HUD, `20` resources, `40` history capacity/consistency, `80` unsupported dynamic instancing. CPU time is not GPU time and does not forecast savings.

## Verification

Production GLES pixel tests run with surfaceless EGL/Mesa. They check static camera motion, rigid transforms, EXT axis conversion, depth occlusion, alpha cutouts, facial/CPU previous positions, NPC and player skin weights, vector visualization and driver state restoration. They also assert zero additional uploads/allocations for the second eye and capacity reuse in the next frame. Explicit backend guards reject initialization, replay and visualization in Vulkan mode; the test interposes the real driver-query boundary to assert no entry. Pinned OpenXR tests cover image lease success/failure/timeout/destruction as well as existing capability and submission adapter tests. Host history tests exercise draw capture and coverage reasons.

The prior Android workflow passed its host tests but failed before compilation while downloading the NDK (`unknown archive`). SDK installation now retries at most three times. A fresh workflow must validate the Android translation unit and APKs.

Motion-pass design follows Meta's [native developer guide](https://developers.meta.com/vr/documentation/native/android/mobile-asw/), and encoding follows [XrFrameSynthesisInfoEXT](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrFrameSynthesisInfoEXT.html). These are new VR renderer calculations, not extracted Fallout shaders or replacement artwork. Original game assets are unchanged.
