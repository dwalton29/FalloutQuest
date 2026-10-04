# Quest render optimization pass

Implemented against main at 24b8995. This pass preserves main-eye world,
water, environment highlights, HDR/bloom and loading presentation.

## Compositor depth

Enumerate and conditionally enable XR_KHR_composition_layer_depth. Create one
DEPTH_COMPONENT24 OpenXR swapchain per eye when that exact format is advertised.
Acquire/wait color and depth independently; render the existing scene and resolve
2x MSAA into the existing single-sample color/depth post target. After the color
composite, blit resolved depth into the acquired OpenXR depth image. Release images
and chain persistent XrCompositionLayerDepthInfoKHR structures into both projection
views, using [0,1], 0.04m near and 125000/FO3_UNITS_PER_METRE_Q715 far.

If either eye cannot supply depth, submit color without depth for both. Do not
submit world depth during loading: that overlay uses the physical HMD camera,
whereas gameplay uses a virtual locomotion camera. Incomplete color renders do not
submit a projection layer. A failed depth copy is logged and falls back to color.

Depth supplies geometry information for positional reprojection. It cannot create
pixels outside the rendered field of view or behind occluders; headset testing must
confirm how much of the lateral-motion edge artifact it removes.

## MSAA and water

* Request 2x MSAA, retain the existing 1x fallback.
* Reduce the planar mirror from 1024x1024 to 512x512. Keep HDR when supported.
* Use one midpoint camera, a union of eye FOVs and a small angular border. Both eyes
  use the same cached projective matrix, so sampling coordinates match the image.
* Reuse for at most two additional frames while translation is below 1cm, MVP
  elements change by less than 0.002, and the plane changes by less than 1mm.
  Cell, GPU target, scene storage/count and plane changes invalidate reuse. Same-frame
  reuse shares the mirror across both eyes. Different planes still require a refresh.
* Tighten mirror frustum padding from 1m to 0.1m. Retain existing terrain culling,
  native LOD and the above-water clipping plane.
* Omit decals, loose objects, actor/body geometry, below-plane shapes and very small
  distant clutter from detailed mirror geometry. Skip secondary additive cubemap
  highlights inside the mirror; main-eye highlights and water Fresnel/normal/specular
  optics remain. Alpha silhouettes that pass the same usefulness filter remain.

One 512-square update uses one eighth of the target pixels of two 1024-square
updates. That is a pixel-budget comparison, not a measured GPU-time or FPS gain.
Sharing a planar image approximates near-water binocular parallax; inspect nearby
shorelines, large reflected landmarks and head movement on device.

## Stereo batches

Build a flat opaque pointer order sorted by batch key once per stereo frame.
Main and mirror visibility filters reuse it. Retain vector capacities rather than
allocating unordered_map nodes and per-group vectors each eye. The main visible
set is the union of the two eye frusta; build/upload matrices once and reuse for
both eyes. Separate mirror buffers prevent reflection uploads from corrupting the
main cache. A scene-storage/count change forces a rebuild before cached pointers
are reused. Capacity can grow when the resident scene grows; steady-state eye
submission does not construct CPU batch containers.

Bypass representative-only culling for an already-selected instance batch. Testing
only its representative could discard all the visible objects in the batch when
that representative is outside the current eye frustum. Singles retain per-eye
culling. Matrix uploads and cached vectors are released during scene teardown.

## P1: enabled opaque-world multiview migration

The native GLES path now checks GL_OVR_multiview2,
GL_OVR_multiview_multisampled_render_to_texture, both attachment entry points and
GL_MAX_VIEWS_OVR. Matching eye extents and a working 2x post target are required.
A separate world shader variant enables GL_OVR_multiview2 and declares two views.
It indexes both projection/view matrices and eye positions by gl_ViewID_OVR; the
fragment eye position uses a flat view-index varying. Monoview shaders remain
available for reflections, special materials and unsupported drivers.

Eligible opaque statics and native LOD render once per stereo frame into two-layer
HDR color and DEPTH_COMPONENT24 array textures attached through
FramebufferTextureMultisampleMultiviewOVR at 2 samples. A uniform map is built once
per shader generation by name; per-material uniform updates are translated to the
multiview program's locations. The visible set remains the union of both eye frusta.

Switching away from the layered framebuffer resolves its implicit multisample
attachments. Each eye then imports its layer with a fullscreen color/depth shader
into the existing 2x MSAA target. This uses gl_FragDepth rather than an illegal
single-sample-to-multisample depth blit. Resolve coverage is imported using alpha to
coverage; opaque sample color is recovered from the resolved geometric coverage.
The existing eye passes, water refraction/depth inputs and compositor depth continue
using their usual targets and projection conventions.

Materials with alpha tests, vertex alpha, non-opaque material alpha, blending,
decals, loose objects or unusual depth-write/test rules stay on their existing
eye path. These need a later coverage-aware migration. Terrain, NPCs, player body,
water, additive environment highlights, HUD, sky and post-processing also remain
sequential. This is actual single-pass rendering for eligible world passes, not
full-scene single-pass rendering. Ordinary non-multiview hardware retains all
features through the existing renderer.

Shader conversion/link, uniform-map validation, framebuffer validation or draw
errors disable the multiview path and log the reason. Source-program/size changes
rebuild resources; scene teardown deletes the layered target and shader programs.
The persistent batch cache records its rendering mode so falling back cannot reuse
a list that omitted the special-material objects.

Runtime diagnostics report actual submitted world draw counts, CPU submission time,
layer-import time and whether the path activated. These are CPU timings, not GPU
timestamps. There is extra memory and fullscreen layer-copy bandwidth in this first
migration stage; no performance gain is claimed before Quest measurements.

Remaining migration:

1. Convert LAND to a separate multiview shader and draw into the same stereo target.
2. Move the other world/actor/sky/water passes and preserve their compositing order;
   handle alpha-tested/translucent coverage without the current layer import.
3. Convert bloom/composition to array sampling and optionally submit two-layer
   OpenXR color/depth swapchains directly, removing the per-eye layer imports.
4. Compare CPU/GPU frame times, peak memory and draw counts on the same scene.
   Verify near geometry, both eye images, shoreline reflections, loading, HUD,
   depth reprojection and cell changes on device before claiming an FPS improvement.

Sources:
* https://developers.meta.com/horizon/documentation/unity/enable-multiview/
* https://registry.khronos.org/OpenGL/extensions/OVR/OVR_multiview2.txt
* https://registry.khronos.org/OpenGL/extensions/OVR/OVR_multiview_multisampled_render_to_texture.txt
* https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrCompositionLayerDepthInfoKHR.html

## Validation

Native ARM64 build uses NDK r27 and the project's OpenXR 1.1.61 AAR, including a
full optimised shared-library link. The build exposed an existing inline fog
wrapper which did not export a symbol for the terrain translation unit; it now
has one runtime definition, allowing the optimised native link to succeed.

Host regression tests cover left-only/right-only visibility, spanning/invalid
bounds, mirror padding and reflection refresh/invalidation conditions. Existing
scene dispatch and world residency/preparation/loading tests also pass. CI runs
the render-policy regression test before assembling the APK.

Monoview, multiview and layer-import GLSL variants pass offline compilation/linking,
and shader-conversion failure cases are covered in the host tests. APK 144 includes
the new path.

No Quest frame-time measurements or headset visual verification have been made
in this environment. Test near geometry while moving laterally, then water while
stationary/turning/walking, and cell transitions. Check startup's compositor-depth
and multiview logs and that MSAA chooses 2x. Remaining work is migration of the other passes and removal of the layer-import stage.
