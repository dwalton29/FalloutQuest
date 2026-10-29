# SpaceWarp skin coverage — v279

The headset A/B confirms the remaining player-body jiggle stops with `debug.falloutquest.spacewarp=0`. v278 corrected separate held-physics and arm-publication defects; those changes did not resolve this synthesis-only body distortion.

## Reproduced defect

The original Vulkan regression used a large flat quad and synthetic constant depth. An additional sloped-quad test still passed. A finely tessellated curved skin surface exposed the missing case: the camera and skinned body translate together by 4.84/36 metres, so all visible body pixels should have zero screen-relative motion. The colour pass renders at 64x64 and the runtime motion target at 32x32. Actual retained main-pass depth is used, with a perspective camera at X=157, indexed geometry and a skin palette.

Before the fix, 52 of 328 interior body pixels in the first eye retained static-world motion instead of zero. The largest supplied displacement was 0.219482 NDC. This occurs even though draw-level motion coverage reports complete. The coarse replay rasterizes a different sample position from the fine depth texel chosen by nearest sampling. On small adjacent triangles with different depth slopes, its derivative correction can compare one triangle's extrapolated depth with another triangle's actual depth and reject a visible body fragment. That fragment then inherits the static-scenery proxy. Thin silhouettes can similarly lose raster coverage. This is a concrete reproduction of a matching failure mechanism, not a capture of the exact headset NIF.

## Correction

Moving geometry now replays into an owned two-eye RGBA16F motion image at scene resolution. It uses the original current and previous roots, palettes/deformation, material cutout and depth occlusion rules. Each covered fragment writes a validity marker alongside its vector. Static reconstruction then selects the exact same scene texel for motion and retained depth, replacing the scenery proxy where a visible dynamic fragment exists. The runtime swapchains retain their queried recommended dimensions.

Both full-resolution eye passes finish before the array image is sampled. Render-pass dependencies order previous reads, colour writes and subsequent sampling on the existing shared session queue. No per-frame CPU waits/readbacks are added. The dynamic image and its eye views/framebuffers are owned by the motion producer and destroyed after GPU idle, including partial initialization failure. Renderer initialization failure continues to retain native presentation.

This also aligns cutout sampling derivatives with scene resolution. It does not alter the world colour, original materials, pitch-sign correction, SpaceWarp FPS estimate or v278 locomotion fixes. The main scene is not lit/rendered again: only the existing dynamic motion geometry is replayed at finer sample positions. Static world motion stays depth-reconstructed.

## Cost and validation

The new RGBA16F stereo image uses `width * height * 2 * 8` bytes: about 36.55 MiB at the last logged 1512x1584 scene size, plus ordinary driver allocation overhead. It adds clear/store bandwidth and more dynamic-motion fragments. Existing `motionMs` includes these passes; software-GPU timings are not a Quest performance prediction. Headset correctness and GPU budget must both be checked.

After the fix, all 328 interior pixels have zero wrong-motion samples in both eyes, across the suite's linear/sRGB and density-attachment-emulation cases. Existing rigid, skinned, facial/deformed, cutout, occluded, missing-resource, stationary/pitched cameras, UI depth retention, world composition and recreation checks pass under real Vulkan with synchronization validation and no validation errors. Density emulation does not validate actual reduced-rate Adreno rasterization. Shader compilation/link/SPIR-V validation covers both motion programs.

On Quest, re-enable with `adb shell setprop debug.falloutquest.spacewarp on`, force-stop/reopen, then repeat walking while looking at the torso/arms. The producer log now contains `dynamicCoverage=scene-resolution`. A fresh performance log is needed to measure the added motion-pass cost. The software regression establishes the corrected coverage rule, not final headset confirmation.
