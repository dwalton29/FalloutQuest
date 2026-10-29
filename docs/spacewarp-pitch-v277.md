# SpaceWarp pitch convention — v277

## Diagnosis

The v276 headset report says the full image still judders during upward/downward head rotation. The earlier headset log identifies `OpenXR runtime: Oculus` and `FQ SPACEWARP capability ... api=EXT`. Removing model-layer differences in v276 did not establish compositor stability.

The native producer emits Vulkan raster displacement: GL camera displacement is multiplied by `(1,-1,0.5)`, with depth stored in [0,1]. This is appropriate for the EXT specification's stated Vulkan NDC convention. The missing distinction is the runtime's interpretation of vertical displacement. Godot's production OpenXR implementation explicitly documents conflicting runtime conventions: its mobile motion pass converts native raster Y to Y-up; its EXT adapter defaults to that convention and offers a Y-scale reversal for Android XR. Commit `4f35394962b422da932c9cfb8d26e4c09e13598a` retains this default and documents platform selection. Its prior comment explicitly notes that the test runtime differs from the nominal Vulkan convention. This is evidence for a Meta compatibility correction, not proof of this headset's final output.

References:

- https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrFrameSynthesisInfoEXT.html
- https://github.com/godotengine/godot/blob/4f35394962b422da932c9cfb8d26e4c09e13598a/modules/openxr/extensions/openxr_frame_synthesis_extension.cpp
- https://github.com/godotengine/godot/commit/4f35394962b422da932c9cfb8d26e4c09e13598a
- https://github.com/godotengine/godot/blob/master/servers/rendering/renderer_rd/shaders/forward_mobile/scene_forward_mobile.glsl

## Change

The EXT submission adapter identifies the runtime through `xrGetInstanceProperties`. On Oculus/Meta runtimes it submits `motionVectorScale=(1,-1,1,1)`. Other runtimes retain `(1,1,1,1)`. The GPU buffers, colour orientation, depth orientation/range, X/Z displacement, tracked poses and artificial locomotion delta are unchanged. Both static and animated vectors receive the same interpretation through the runtime metadata. No scene content or draw-distance changes are involved. Existing FB submission is unchanged; the diagnosed headset selects EXT.

The resource log includes `submissionMotionYScale=-1` on Meta EXT. This conversion is deliberately at the submission boundary: reversing source UVs or the depth image would align neither silhouettes nor the colour layer.

## Verification

OpenXR adapter tests exercise Oculus, Meta and reference runtimes, both eyes, both pitch directions, existing depth chains and runtime-property failure. They require the expected runtime interpretation of a stationary forward point during a tracked pitch, without flipping depth or X/Z. The real Vulkan regression now rotates the camera in both pitch directions across all motion pixels in both eyes, comparing native displacement with an independent analytic ray/rotation calculation. Earlier tests only held a pitched camera stationary, which did not exercise this case. Static-zero, translation, dynamic/skinned/deformed/cutout coverage, retained world depth, colour composition, recreation and synchronization validation remain covered by the existing suite.

Software tests can verify buffers and metadata, but cannot verify the Quest compositor's output or deadline behaviour. The correction targets the direction-sensitive whole-image symptom. It does not claim that the wasteland now consistently meets the half-rate GPU budget. Install v277, retain `debug.falloutquest.spacewarp=on`, restart and repeat slow look-up/look-down in the same location. A fresh log is needed if the image still bounces; avoid treating producer readiness or a successful software test as headset confirmation.
