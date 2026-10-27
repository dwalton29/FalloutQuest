# Vulkan migration status — v247

The full migration is **incomplete**. Experimental Vulkan gameplay now selects a
CPU scene frontend and does **not create an EGL/GLES context**. GLES remains the
default. Context-free material rendering, draw ordering and the native OpenXR
image path have software-GPU coverage. Actual Quest operation, complete visual
parity and performance remain **unverified**. No 72 Hz claim is made.

## v247: context-free scene submission

- `rendering/render-submission.h` defines CPU resource handles, named uniform
  bindings, material/raster state and self-contained draw commands without a
  graphics API dependency. Native handles are generated independently of GLES.
- The existing native engine, original asset decoders, scene traversal, animation
  and UI preparation remain shared. A compatibility frontend accepts their
  existing GL-shaped calls. In Vulkan mode these update CPU state and handles;
  they never compile GLES shaders or allocate GLES textures/buffers/framebuffers.
  Native programs use the existing original GLSL-to-SPIR-V build products.
  Legacy shader/FBO status queries only preserve shared preparation control flow;
  actual native shader, allocation and framebuffer failures stop Vulkan output.
- Vulkan consumes an ordered draw list, with captured texture bindings, raster
  state, instance matrices, uniform bytes and sampler bindings. Later state or
  uniform edits cannot alter earlier submissions. Lists drain at water snapshots,
  post-processing, depth clears and UI/pass boundaries, and before resource
  replacement/deletion. Native resource retirement still waits for GPU completion.
- The Vulkan renderer no longer includes the GLES state cache or calls GLES.
  Program registration resolves CPU binding names; optional GLES probe mode uses
  the fallback frontend's resolver. No native draw reads a live GLES cache.
- Native initialization skips EGL and the OpenXR GLES requirements query, and
  does not enable the GLES OpenXR extension. Vulkan device limits inform skinning
  capability checks. GLES-only timer/foveation extensions are unavailable through
  the native frontend; no GL extension function is loaded for native frames.
- Existing loading, skin palettes, FaceGen, world/LAND, sky, water, HDR/film,
  overlays, Pip-Boy RTT and lockpick continue through the shared decoded-data
  uploads and original native shader ports. No replacement assets or terrain
  fusion changes are introduced. Complete headset parity still needs verification.

The CPU frontend is a migration compatibility layer, not a new engine. GL-style
source call sites and names remain; further source/API cleanup is useful, but an
active GLES context, GLES-generated resource identities and mirrored GLES draw
execution are no longer required by the experimental path. GLES/EGL remain linked
for the selectable fallback. The standalone probe still requires GLES.

Native logs identify `sceneSubmission=cpu-commands`, `eglContext=none`,
`glesResources=none`, and `FQ NATIVE FRONTEND ... glesDriverCalls=0`. Confirm the
latter on Quest rather than inferring device execution from host tests. Existing
`FQ PRESENTED api=Vulkan` follows a successful OpenXR frame submission.

New tests exercise CPU resource allocation, named/array uniform locations,
immutable draw snapshots, device limits, viewport/clear-state queries, zero GL
calls and fallback restoration. The real software GPU test now uses the production
CPU frontend and native program registration without EGL, and reads back original
material output after mutating live state/uniforms. Existing multiview, terrain,
actors, water, film, UI, Pip-Boy and colour/depth compositor tests are retained.
An entry-point audit rejects GL calls lacking a native dispatch boundary.

Uploads and frame completion remain blocking. Native MSAA, planar reflections,
foveation, automatic session-loss recovery and full Quest benchmarks are pending.
The CPU compatibility frontend retains some legacy preparation/bookkeeping work;
removing it, reusing draw/descriptor storage and batching uploads are next steps.

## v246: experimental Vulkan presentation

`debug.falloutquest.renderer=vulkan` creates an actual
`XrGraphicsBindingVulkan2KHR` session using the runtime-selected Vulkan device
and graphics queue. Format enumeration selects linear RGBA8/BGRA8 UNORM;
unsupported formats or layered-swapchain creation fail explicitly. One two-layer
swapchain uses native Vulkan image views and multiview framebuffers. The native
world/terrain/actors/sky/water/UI/film output is sampled by a native stereo copy
pass directly into those runtime images. No GLES texture copy or readback feeds
headset output. Array indices 0/1 and the original located poses/FOVs are submitted
by the existing `xrWaitFrame` / `xrBeginFrame` / `xrEndFrame` loop.

One scene traversal records both cameras. In v246, GLES setup remained required;
v247 replaces this with the CPU frontend described above. The standalone probe
keeps its concurrent GLES drawing behaviour.

Acquisition/wait happens once per stereo frame. A timeout is not treated as
image readiness. The GPU fence completes before release, with runtime images in
`COLOR_ATTACHMENT_OPTIMAL`. Runtime images are borrowed; only application views
and framebuffers are destroyed. STOPPING drains and destroys native swapchains;
READY recreates them while retaining resident Vulkan resources and resets
exposure. Focus/input/gameplay pause behaviour is shared with GLES. Session or
instance loss currently exits through the existing lifecycle; relaunch is
required. Automatic session-loss recovery is still unfinished.

Failure stops native presentation and submits no projection layer; it never
creates a GLES session while claiming Vulkan output. Logs distinguish
`FQ RENDERER session=Vulkan presentation=Vulkan`, `FQVK FRAME`, and
`FQ PRESENTED api=Vulkan` (the latter follows a successful `xrEndFrame`).

Optional compositor depth uses a separate two-layer D32 swapchain when the
OpenXR depth extension and format are available. Colour/depth acquisition indices
are independent. Native scene depth is copied with explicit transfer barriers and
released in `DEPTH_STENCIL_ATTACHMENT_OPTIMAL`; each eye receives its matching
array layer and the exact near/far range from the runtime camera. Loading submits
colour only. Missing depth support retains explicit colour-only composition.
Native MSAA, foveation and planar reflections remain unfinished; Vulkan uses one
sample. Existing terrain depth-state
fixes are retained, with fusion quarantined. Uploads and end-of-frame fences
remain blocking. Ordinary FQPerf GPU timers still measure GLES work, so native
captures are marked `benchmarkValid=0`; `FQVK FRAME stereoGpuMs` covers the native
recorded passes including the final swapchain draw, excluding uploads and XR wait.
Neither metric proves Quest performance before hardware testing.

Software Vulkan tests exercise real two-layer UNORM runtime-style images,
multiple acquired image indices, upright asymmetric stereo geometry, loading
and film output, projection metadata, fence-before-release, independent colour/depth image rotation, D32 depth values
and composition metadata, colour-only compatibility, recreation with resident
resources retained, timeout rejection and unsupported-format rejection.
Compositor callbacks are mocked; this is not a real OpenXR runtime test.

### Headset validation (required before promotion)

Install v247 Debug or Profile, then use PowerShell from platform-tools:

```powershell
.\adb shell am force-stop com.falloutquest.app
.\adb shell setprop debug.falloutquest.vulkan_probe 0
.\adb shell setprop debug.falloutquest.renderer vulkan
.\adb logcat -c
```

Launch from the headset. Capture `adb logcat` and verify the session/presentation
logs above. Check loading artwork/models, Megaton, Wasteland terrain and an
interior; verify eye separation and orientation, NPC/weapon animation, FaceGen,
water, exposure changes, HUD/dialogue/barter/loot, Pip-Boy, lockpick and pause.
Remove/replace the headset and suspend/resume the app; confirm swapchain
recreation and retained scene resources. Transition exterior/interior repeatedly.
Record visible regressions and skipped draws before comparing matched scenes.
No Quest measurements have been collected here.

Restore default GLES after testing:

```powershell
.\adb shell am force-stop com.falloutquest.app
.\adb shell setprop debug.falloutquest.renderer gles
```

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
  Native RGBA8 Pip-Boy RTT records original menu/font/art draws on demand,
  retains the GLES texture origin and feeds the original world screen material;
  scanlines retain their vertical-repeat sampler. No screen readback is used.
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

Use `renderer=gles` for the probe instructions above. `renderer=vulkan` selects
the explicitly experimental presentation path described at the top of this file.

## Unfinished implementation (not merely headset validation)

- Actual headset verification of the new Vulkan OpenXR binding, stereo layered
  swapchain and compositor submission. Automatic session-loss recovery is pending;
  STOPPING/READY swapchain recreation is implemented and software-tested.
- Complete actor/weapon and Pip-Boy in-game visual validation. Loading and
  lockpick native models still require full in-game animation/alpha comparison. Native pipeline state now handles reversed/front culling, depth bias
  and separate color/alpha blend operations, but requires scene validation.
- Native planar water reflections and native MSAA/resolve. Native single-sample
  D32 compositor depth now has software coverage; headset reprojection remains unverified.
  The HDR/imagespace/bloom chain is implemented in the probe, but still needs
  complete in-game colour and temporal adaptation comparison on Quest.
- Complete headset validation of context-free draw submission and CPU/native
  resource identities. The compatibility frontend still has GL-shaped source
  call sites; native execution is independent of an active GLES driver/context.
  Audit stereo-union culling and visual parity for every pass on Quest.
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

Next validate context-free Vulkan gameplay on Quest, batch native uploads,
reuse descriptor/draw storage, complete native MSAA and planar reflections,
and validate full actor/material scenes.
Retain the shared backend-independent stereo draw list as remaining passes
become native and their headset parity is established. Resolve terrain from original ESM/DDS data and captured output.
Only enable Vulkan-default after the complete on-device flow passes and the six
matched scenes have measured GPU, CPU and frame-pacing results.

Baseline provided by the user: v244 Wasteland median stereo GPU 31.7 ms. Vulkan
headset results: **none**. Required budget at 72 Hz: 13.89 ms. No speedup is yet
established.
