# Vulkan performance pass — v275

The world-layer/depth/reprojection path is superseded by [the v276 stability repair](spacewarp-stability-v276.md).

The v274 headset capture measured median stereo GPU time 29.56 ms (p95 34.23 ms), with 78% of frames exceeding the 27.78 ms half-rate 72 Hz budget. The main scene cost 22.61 ms median. The goal of this pass is to reduce pixel work and external-memory traffic while retaining authored scene content, draw distance, material effects and 90% render scale.

## Native fixed foveation

Previously Vulkan only queried fragment-density-map support and always logged foveationEnabled=0. Android now enables VK_EXT_fragment_density_map with fragmentDensityMap and fragmentDensityMapNonSubsampledImages at device creation when supported. It creates and uploads an immutable R8G8_UNORM stereo map once, before recording any pass that reads it. The map dimensions follow maxFragmentDensityTexelSize. The normalized centre radius 0.75 stays full rate; edges request 2x2 fragments; extreme corners past radius 1.2 request 4x4. UNORM densities use 127/255 and 63/255 (rounded down), so the requested fragment areas are at least 2x2 and 4x4 rather than just below those thresholds. These are conservative fixed lens-centred quality choices for standalone VR, not game artwork. Peripheral softness is the visual trade-off; actual hardware may choose finer rates.

Separate foveated clear/LOAD passes cover both normal and split opaque/effects gameplay. Water snapshot continuation retains the density attachment and original colour/depth data. On foveated frames, the first pre-film UI draw switches to a full-rate colour/depth LOAD pass. Opaque sidecars stay stored and detached, avoiding their unnecessary reload/store for HUD. Any later world draw restores the full-rate sidecar pass before it renders. All following UI, postprocessing, motion and layer composition remain full rate. Loading is full rate. Images remain ordinary non-subsampled textures to preserve existing water, HDR and motion sampling. Missing features, format support or density setup retain normal full-rate Vulkan. Set debug.falloutquest.vulkan_foveation=0 before restarting the app for an A/B comparison; unset or 1 enables the default policy.

## Material specialisation

The generated Vulkan fragments retain their original uniforms and material expressions but expose pipeline specialisation constants for lighting mode, zero local lights, and absent world reflection passes. Selection uses queued uniform snapshots, so later live-state changes cannot change the queued material. The compiler can remove inactive normal/specular/light/reflection work without reducing active lights, replacing textures or changing authored material features. The developer generic-shader comparison disables the specialisation.

An offline frozen-specialisation check with the same SPIR-V optimisation sequence reduces the unlit fragment from 6 image-sample instructions / 57 GLSL extended operations to 2 / 17. These are static shader counts, not measured GPU speedups. Real GPU parity checks compare lit/unlit and zero/nonzero-light variants against the generic shader, including cutouts and clipping.

## Compact finished colour

On sRGB SpaceWarp frames only, the finished original image uses a private 8-bit sRGB colour attachment rather than RGBA16F. This halves that target's bytes per pixel (8 to 4) and its associated write/read payload. Main scene, opaque colour, bloom and exposure inputs keep their existing formats; no HDR lighting is clamped early. Original post-film overlays blend in the sRGB attachment, matching the established direct presentation path. Film output and HUD keep the original order.

The final composition compares full/base colour in rounded sRGB display codes, tolerating one code for storage/compiler rounding. This prevents an opaque image from becoming an entirely native effects mask merely because of storage quantisation. Real changed effects retain their completed original colour. UNORM fallback retains FP16 and its original comparison. Runtime swapchain recreation also recreates private compact-target framebuffers before destroying their render passes.

## Checks and next capture

The Vulkan suite checks original world/LAND/sky/water/UI, motion coverage, skin/facial motion, depth routing, independent images, compact sRGB recomposition within two display codes, generic/specialised material equality, asynchronous ownership and recreation. Lavapipe does not implement real FDM: an explicitly labelled test inspects the production density attachment descriptions and strips only the unsupported density attachment/stage/access before passing commands to the real software GPU. It verifies clear/LOAD transitions, native/split water, full-rate pre/post-film HUD, loading, disable fallback and teardown. Stereo density-map symmetry and rates are checked separately. This exercises dispatch and lifetime, not actual reduced-rate rasterisation. Real Quest peripheral quality, depth at coarse fragment boundaries and performance remain headset verification tasks.

Check FQVK CAP foveationEnabled=1 and FQVK MATERIAL foveatedDraws>0, lightingSpecializedDraws>0, compactFilm=1 on eligible gameplay. Compare stereoGpuMs, sceneMs, compositionMs and frame cadence against v274 in the same location/view. No specific FPS gain is claimed without a new capture. Water still uses the original full-resolution refraction snapshot; this pass does not reduce world content or remove authored lighting.
