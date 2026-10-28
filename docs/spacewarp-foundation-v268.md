# Wasteland Application SpaceWarp — first implementation, v268

This commit establishes the runtime, resource and temporal-history contract. It does **not** enable half-rate gameplay yet: the world renderer does not currently produce complete motion-vector/depth coverage for actors, hands, weapons and alpha-tested geometry. Allocation or valid camera history alone must never turn synthesis on.

## Implemented

- Optional OpenXR capability negotiation prefers `XR_EXT_frame_synthesis`, with `XR_FB_space_warp` fallback. The implementation uses the same OpenXR 1.1.61 declarations as the Android dependency.
- GLES validation allocates per-eye runtime-recommended, single-sample RGBA16F motion and sampled DEPTH24 swapchains when those formats are advertised. Partial creation is cleaned up; failure retains native rendering. Resources are session-owned, not recreated at every CELL crossing. Vulkan rendering remains native.
- Stereo submission adapters preserve existing projection-view `next` chains, provide matching depth ranges, and reject incomplete/stale motion coverage. EXT requests the relaxed frame interval only when attaching fully validated data.
- Eligibility requires the original Wasteland WRLD (`0000003C`), a ready/focused scene and no loading/modal interaction. Interiors and Megaton's separate exterior remain native.
- History advances only after successful, complete stereo submission. Missing eyes, failed submission, camera cuts, full scene generations and origin changes invalidate it. Two successful frames establish the history baseline. Rolling object publication does not itself reset history.
- Dynamic-history records carry geometry identity, model matrices, bone palettes and deformed positions. Contradictory records between eyes are rejected. Storage is bounded to 8 MiB per committed/pending set, 2,048 identities each.
- Static-depth reprojection math and a dedicated unlit GLES motion-vector shader are present and independently tested. The shader is not yet wired into the scene frame graph. Backend/API motion-vector convention conversion is an explicit input.

## Device validation

The foundation is opt-in so ordinary gameplay has no additional swapchain allocation or motion-pass cost. Set `debug.falloutquest.spacewarp` to `validate` (or `1`) and restart the app to negotiate and allocate validation resources. `FQ SPACEWARP` logs report the selected API, per-eye recommended dimensions and history readiness. Both values currently retain native rendering and report `motion-pass-pending`; neither is a finished SpaceWarp toggle.

## Next renderer work

Connect the low-resolution motion/depth pass to the existing scene frame graph. Reconstruct static-world motion from valid world depth, then overwrite moving geometry using previous submitted transforms, bone palettes and deformation streams. Match authored cutout visibility; handle hands/weapons, sky, water and UI explicitly. Supply artificial-locomotion delta poses using the selected API's coordinate convention. Only then allow the coverage-gated submission adapter to attach.

Headset validation must inspect vectors during walking, snap turns, combat, physical grabbing, streaming and interior transitions, and measure actual app/compositor cadence and input latency. This milestone claims no FPS gain.

## Verification

Host tests cover matrix reconstruction, camera motion, sky rejection, Wasteland exclusions, history warm-up, failed submissions, per-eye consistency, geometry replacement, memory limits, capability preference/fallback, resource rollback and projection-chain ownership. The OpenXR mock test verifies SHA-256-pinned official 1.1.61 headers; CI also compiles and links the GLES shader pair.

The build includes a correction to the enlarged holster work: the original front-of-shoulder rejection plane is retained, while the larger horizontal/vertical reach and outer hysteresis boundary have updated regression coverage.
