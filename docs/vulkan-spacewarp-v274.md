# Vulkan SpaceWarp composition — v274

The world-layer/depth/reprojection path is superseded by [the v276 stability repair](spacewarp-stability-v276.md).

## Fault reproduced from v273

The supplied headset log had native producer/resources ready but synthesisAttached=0 at every status sample and motionMs=0 throughout. Normal gameplay reported coverage reasons 0x14 (transparency/UI) and 0x1c (transparency/water/UI), even after history reached two frames. Those guards stopped the entire producer. Automatic PTWFromStereo was not evidence of application SpaceWarp.

## Implementation

A live synthesis frame retains the complete original HDR scene and adds opaque colour/R32 depth attachments to its multiview pass. The original world/LAND/sky fragment shaders execute their original material math once and export extra outputs. Independent attachment write masks exclude blended materials, water, UI and authored disabled-depth-test/write objects from opaque coverage. Sky colour participates with far depth. Original cutout discard semantics remain intact. The water snapshot and continuation preserve the extra attachments.

The complete original image keeps the existing adaptation, bloom, film and before/after-film HUD order. A final two-output pass applies the same original film settings and shared full-scene exposure/bloom to the opaque colour. It compares that FP16-rounded base against the finished full image. Unchanged pixels go entirely through the opaque base; changed pixels are replaced by their original complete colour in a separate, premultiplied native projection layer. Its alpha is exactly zero or one, with RGB zero at zero alpha. This avoids guessing glass/water alpha, changing authored blending/refraction or double-applying bloom and gamma. The second layer does not receive synthesis metadata.

Motion reconstruction samples the retained opaque depth. Dynamic replay excludes the separated transparent objects while continuing to capture rigid, skin and facial motion for opaque objects. The transparency/water/UI coverage reasons are filtered only while the renderer has actually enabled this composition path. Missing geometry, invalid deformation, resource/capacity failures and other unsupported opaque cases still prevent synthesis. In the split frame, original KHR compositor depth follows the native effects layer; EXT/FB synthesis depth follows the opaque layer, rather than supplying mismatched full-scene depth to the opaque colour.

The path applies to both exterior and interior gameplay. History begins before selecting the native render pass. Loading, focus loss, scene/origin/world changes, modal interaction, warm-up, disabled synthesis and raw-HDR diagnostics use normal native rendering. Two successful history frames are required after discontinuities. Unsupported independent blending, R32 support or effects swapchain creation retain native rendering. Runtime colour/effects/depth/motion images are released only after the shared queue receives their commands. Effects swapchain views/framebuffers are recreated and destroyed before their runtime swapchain; scene sidecars follow motion producer teardown.

## Verification and limits

The real Vulkan GPU regression draws original water, blended world geometry and post-film HUD together. It verifies the former 0x1c reasons no longer block live split coverage, both motion eyes complete, CanSynthesize succeeds, opaque depth excludes water/HUD, base/effects images recompose within two display codes of normal rendering, and both transparent and opaque mask regions exist. It exercises linear UNORM and sRGB presentation, exterior/interior IDs, independent swapchain rotations and swapchain recreation. The same reasons still block unsplit frames, and opaque resource failure remains a blocker. SPIR-V compilation/link/validation covers every sidecar and composition variant. Existing rendering, loading, GPU lifetime and OpenXR adapter checks remain applicable.

This adds attachment bandwidth, one private finished-colour target, an effects swapchain and a film/composition pass. It does not shade the whole scene a second time. No manual frame skipping or new GPU-to-CPU waits are introduced. Actual benefit depends on Quest runtime cadence and these extra costs; software GPU timings are not a headset performance forecast. Transparent effects and HUD stay on a native projection layer without app-generated intermediate motion, so their intermediate-frame behaviour and layer boundaries require headset visual validation.

The next headset log should show synthesisAttached=1, nativeEffectsLayer=1, layers=2 and motionCoverage=1/1 during eligible gameplay. GPU records now include compositionMs alongside motionMs. Check both activation and stereoGpuMs/frame cadence; merely seeing initialized resources or a PTW mode is insufficient. No measured headset gain is claimed by this change.
