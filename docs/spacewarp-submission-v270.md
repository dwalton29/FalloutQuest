# Opt-in Wasteland SpaceWarp submission — v270

The v269 motion producer is now connected to OpenXR composition submission. This is an experimental, explicitly requested mode; it is not enabled by default, has not been validated on a headset here, and makes no measured FPS/latency claim.

```sh
adb shell setprop debug.falloutquest.spacewarp on
```

Restart the app after changing modes. `on` permits synthesis metadata on eligible, completely covered stereo frames. `validate` (or the existing `1` alias) produces buffers at native cadence; `visualize` displays those vectors without synthesis. `0` disables the feature and its allocation/rendering work after restart.

## Submission rules

- Only GLES and Wasteland WRLD `0000003C` are eligible. Interiors and other worldspaces, loading, modal interactions, loss of focus and failed stereo rendering remain native.
- Two successful stereo submissions establish history. Each eye must produce/release valid motion and depth, with every captured dynamic drawable represented. Missing/stale history, changed deformation dimensions, inconsistent eye snapshots and memory/resource failures prevent attachment.
- v269's visible water, blended/non-depth-writing material and colour-only HUD gates remain in force. This includes notifications, damage bars and the held-weapon ammo HUD. The application can therefore return to native cadence often during normal gameplay. This version does not claim complete Wasteland material/UI support.
- In `on` mode, those unsupported frames skip the GPU motion pass and its uploads. Visible previous-pose GPU capacity is retained without driver calls, and CPU submission history continues, allowing covered frames to resume without rebuilding buffers. Validation/visualisation modes still render the diagnostic pass. A failed synthesis submission disables the producer until restart instead of repeatedly sending rejected metadata.
- The original scene epoch must still match immediately before attachment, protecting against an in-eye transition or load. The existing `XR_KHR_composition_layer_depth` chain is preserved when available. Every frame builds fresh projection views, so omitting the synthesis node immediately returns that frame to normal submission.
- Runtime pacing comes from the attached extension: EXT requests a relaxed frame interval, and FB uses its SpaceWarp submission contract. The application does not add sleeps, skip simulation frames or change the fixed physics tick.

## Artificial locomotion

Motion vectors use the previous successfully submitted VP matrices; the additional `appSpaceDeltaPose` uses only the engine's artificial app-to-world yaw/translation. Tracked head motion is not added a second time. The helper rejects non-rigid/non-yaw transforms and normalises yaw differences across the ±pi boundary.

For FB, the calculation follows Meta's `inverse(previousAppPose) * currentAppPose` example. EXT explicitly requires translation relative to the current application coordinates, so the same world displacement is expressed in the current yaw basis. The engine's yaw-only rotations commute, giving the same incremental rotation for both APIs. A scene-origin rebase, teleport or snap-turn cut invalidates history before this delta can be submitted.

## Checking activation on device

`FQ SPACEWARP frame` records `synthesisAttached`, submission success, native mode, per-eye complete coverage and predicted display period. `synthesisAttached=1` means both coverage gates passed and metadata was attached; successful submission plus the headset's VrApi/OVR Metrics cadence must confirm that the runtime actually synthesises frames. Neither capability detection nor `gpuSubmitted=1` proves activation.

Use `validate` and `visualize` to inspect vectors first. In `on` mode, check walking, turning, combat, grabbing, streaming, entering/leaving interiors, focus loss and HUD transitions. In particular, transitions between native and synthesised cadence and controller latency still need headset measurement. UI layer/effect support remains the next renderer work.

## Verification and references

Pinned OpenXR tests exercise identity/translation/turning/wrapped-yaw deltas, API coordinate bases, invalid transforms, per-eye coverage rejection, interior fallback and resumption, pose submission and preservation of the existing depth chain. The v269 GLES pixel, upload-reuse and native-backend isolation tests remain in CI.

Primary contracts: [Meta native AppSW guide](https://developers.meta.com/vr/documentation/native/android/mobile-asw/), [EXT submission](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrFrameSynthesisInfoEXT.html), [FB submission](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrCompositionLayerSpaceWarpInfoFB.html).
