# Vulkan migration status — v245

The full migration is **incomplete**. GLES remains the only gameplay presentation
backend. No Vulkan-default or 72 Hz claim is made.

## Implemented source

- OpenXR `XR_KHR_vulkan_enable2` device handshake: graphics requirements,
  runtime-selected physical device, instance/device creation and graphics queue.
- Native Vulkan resource uploads from the existing decoded NIF and DDS streams,
  including cold and streamed LAND. No new scene or replacement assets.
- Production world and original v237 LAND GLSL compiled to SPIR-V at build time.
  Shader interface/clip-space conversion preserves original lighting and fog code.
- Native two-layer FP16 color/depth render pass with view mask 3. One supported
  draw submission broadcasts to both cameras using `gl_ViewIndex`.
- Indexed and non-indexed geometry, opaque instances, normal/gloss/diffuse maps,
  vertex colors, alpha testing, ordered LAND paint blending, dynamic uniform
  snapshots, per-eye positions, and depth/cull/blend pipeline variants.
- Original PC sky gradient/cloud/sun shader and dome geometry, translation-free
  per-eye sky cameras, original weather colors and sampler/mip policies.
- Original recovered water shader port, authored CELL/WATR parameters and noise
  texture uploads. Refraction samples matching native stereo color/depth layers;
  one opaque snapshot precedes water and a LOAD continuation preserves depth.
  Native planar reflection remains unfinished (uses original no-map shader path).
- Native HDR post chain using the original adaptation, copy, vertical bright,
  horizontal Gaussian and final film SPIR-V programs. Stereo shares eye-0
  adaptation exactly as GLES does. RGBA8 history, FP16 bloom and final colour,
  authored cinematic settings, quantisation and gamma arithmetic are retained.
- Shared original HUD/font shader and animated notification shader, original
  decoded font/atlas textures and vertex uploads for prompts, dialogue/barter,
  loot, level-up, pause, health, ammunition, XP, performance HUD and lockpick
  text. Per-object combined MVPs retain their model transform in both views.
  Pip-Boy screen RTT remains unfinished.
- Original loading/lockpick model shader, indexed original NIF vertex/material
  streams, original artwork and slides, authored UI clipping/fade, compass and
  KF-derived model matrices. Loading now records into the native stereo probe
  using the loading cameras. Lockpick clears native depth before its models.
  Source/GPU tests do not prove complete headset loading-animation parity.
- Native actor weight/index streams and original float bone palettes, live palette
  updates, differential FaceGen geometry updates, CPU skin fallback uploads and
  original six-face/mip DDS cube resources. Live palette motion has software-GPU coverage; actual NPC/FaceGen scenes still
  need on-device validation. Source integration alone is not NPC visual parity.
- Explicit transfers/barriers, resident buffers/images, generated DDS mip chains,
  fenced command reuse, pipeline caching, optional FP16 stereo image readback.
- Separate partial GPU timestamps and skipped-draw diagnostics. Ordinary FQPerf
  remains GLES; probe mode adds work and is **not a valid API comparison**.
- CMake Vulkan linking and SPIR-V generation, plus a GLES-only CMake OFF switch.

## How the intermediate probe works

The probe mirrors supported first-eye world/LAND draws into a native multiview
render target while the existing game continues drawing and presenting through
GLES. It does not yet replace either OpenXR swapchain. It is a temporary
validation path, not an optimized renderer or the final architecture. GLES IDs
are transitional resource keys; CPU asset decoding and gameplay remain unchanged.

Leave it disabled for ordinary gameplay or performance captures. Debug/Profile:

```powershell
.\adb shell setprop debug.falloutquest.vulkan_probe 1
.\adb shell setprop debug.falloutquest.vulkan_dump 1
.\adb shell am force-stop com.falloutquest.app
```

Launch normally. Look for `FQVK WORLD PROBE` in logcat. PFM dumps contain the native post result in display-linear colour when the
post hook ran (otherwise scene HDR); logcat identifies the domain. They are stored in the app's internal files directory:

```powershell
cmd /c ".\adb exec-out run-as com.falloutquest.app cat files/vulkan-probe-left.pfm > vulkan-probe-left.pfm"
cmd /c ".\adb exec-out run-as com.falloutquest.app cat files/vulkan-probe-right.pfm > vulkan-probe-right.pfm"
```

The dump is once per process, after a nonempty world pass. Disable and restart:

```powershell
.\adb shell setprop debug.falloutquest.vulkan_probe 0
.\adb shell setprop debug.falloutquest.vulkan_dump 0
.\adb shell am force-stop com.falloutquest.app
```

`debug.falloutquest.renderer=vulkan` deliberately logs the parity gate and keeps
GLES selected. It must not select an incomplete game renderer.

## Unfinished implementation (not merely headset validation)

- Vulkan OpenXR graphics binding, layered swapchain presentation, acquisition /
  release and session-loss recreation; current device handshake alone is not M1.
- Complete actor/weapon visual validation and Pip-Boy screen RTT. Loading and
  lockpick native models still require full in-game animation/alpha comparison. Native pipeline state now handles reversed/front culling, depth bias
  and separate color/alpha blend operations, but requires scene validation.
- Native planar water reflections and native MSAA/resolve/depth submission.
  The HDR/imagespace/bloom chain is implemented in the probe, but still needs
  complete in-game colour and temporal adaptation comparison on Quest.
- Native draw-list generation independent of GLES commands, stereo-union culling
  for every pass and backend-owned material/resource identities.
- Hardware foveation. Fragment-density extension availability is only logged;
  extension presence alone is not feature support or validated foveation.
- Asynchronous batched uploads, descriptor reuse across frames and buffered
  recording. The probe intentionally uses blocking transfers/fences and may
  significantly slow loading and frames. It is not the optimized architecture.
- Full Vulkan FQPerf pass capture and matched six-environment headset benchmarks.
- v244 terrain appearance: a depth-state defect is fixed in source. The eye
  path disables depth testing; RenderScene restores it before LAND, and LAND
  previously never enabled it. LAND now explicitly uses depth test + LEQUAL,
  writes base depth, and restores incoming state. End-to-end visual correctness
  still requires headset verification. Original v237 programs and
  existing LAND/LTEX/TXST/parent-worldspace resolution remain the reference;
  fusion remains quarantined. No worldspace inheritance, geometry, fog or draw
  distance changes are included here, and no complete terrain repair is claimed.

## Validation boundaries and next work

Shader compile/link, ABI checks, GLES bridge/fallback tests and a software-Vulkan
pixel test are in CI. The GPU test uses the actual original world/LAND port and
production resource/command code; a fake OpenXR device handshake supplies the
software GPU. It checks per-eye camera output, depth equality, ordered paint,
indexed layouts, live skin palettes, instancing, sky camera translation, layered
water refraction, repeated snapshots, opaque depth preservation, the native
exposure/bloom/film chain and recreation. These pixel tests passed locally on Mesa llvmpipe after correcting
stereo timestamp query allocation. Shader compile/link checks also passed. It is not an OpenXR compositor or Quest test.

Next implement Pip-Boy RTT, complete the optional
planar reflection path and validate full actor/material scenes before switching
OpenXR presentation.
Replace the mirror with one backend-independent stereo draw list as those passes
become native. Resolve terrain from original ESM/DDS data and captured output.
Only enable Vulkan-default after the complete on-device flow passes and the six
matched scenes have measured GPU, CPU and frame-pacing results.

Baseline provided by the user: v244 Wasteland median stereo GPU 31.7 ms. Vulkan
headset results: **none**. Required budget at 72 Hz: 13.89 ms. No speedup is yet
established.
