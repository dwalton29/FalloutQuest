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

## P1: multiview investigation — not yet enabled

Quest supports multiview; the native GLES path to target is GL_OVR_multiview2 plus
GL_OVR_multiview_multisampled_render_to_texture. Runtime startup now checks exact
extension names and both framebuffer entry points and logs support. This confirms
the actual driver capability when the next build is run, rather than inferring it
from the device name. No new OpenXR instance extension enables GLES multiview.

The renderer still submits main-world draws sequentially. A shader extension
alone would be insufficient: current scene, LAND, sky and water programs have
single uMvp/eye uniforms, post targets are GL_TEXTURE_2D, and depth/MSAA resolve,
exposure and bloom operate on one eye at a time. No unverified single-pass path is
advertised as active.

Migration sequence:

1. Add separate multiview variants of the static-world and LAND shaders using
   `#extension GL_OVR_multiview2 : require`, `layout(num_views=2) in`, per-view MVP
   and eye-position uniforms selected with gl_ViewID_OVR. Keep monoview variants
   for reflection passes and unsupported drivers.
2. Allocate two-layer color/depth array targets and attach them through
   glFramebufferTextureMultisampleMultiviewOVR with 2 samples. Validate shader link,
   framebuffer completeness and GL_MAX_VIEWS_OVR before selecting this path.
3. Submit opaque static/LOD/LAND batches once. Resolve/read each layer into the
   existing per-eye post path as a first migration stage. The remaining alpha,
   environment, NPC, water and HUD passes must use compatible depth/sample targets;
   never blit single-sample depth into the current multisample renderbuffer.
4. Migrate the remaining world passes, then bloom and composition. Keep exposure
   updating once per frame and preserve the monoview mirror. Optionally submit
   two-layer OpenXR swapchains with imageArrayIndex 0/1; these are not required
   merely to produce multiview offscreen images.
5. Compare GPU timestamps, CPU submission time, allocations and draw counts on
   the same scene before enabling by default. Verify both eyes independently,
   reflections, cell changes, loading, controller HUD and depth reprojection.

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

No Quest frame-time measurements or headset visual verification have been made
in this environment. Test near geometry while moving laterally, then water while
stationary/turning/walking, and cell transitions. Check startup's compositor-depth
and multiview logs and that MSAA chooses 2x. Single-pass stereo is remaining work.
