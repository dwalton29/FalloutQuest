# SpaceWarp head-motion stability — v276

Follow-up: the headset still reported full-image pitch judder. [v277](spacewarp-pitch-v277.md) adds a Meta EXT submission Y-convention correction; the held-pitch tests below did not exercise changing pitch.

The headset A/B isolated the reported model jiggle to the SpaceWarp path: disabling Vulkan foveation did not remove it; disabling SpaceWarp did. That identifies the subsystem, not a proven individual cause. This change fixes concrete layer/depth/reprojection weaknesses found in that path. Actual headset confirmation is still required.

## Consistent world colour

v274/v275 placed only opaque colour in the synthesis layer. Water, blended materials, reflection passes and disabled-depth world surfaces were recovered from a full-image difference into an ordinary projection layer. One NIF could therefore contain diffuse pixels undergoing synthesis and reflection/transparency pixels undergoing a different compositor path. Frame-colour parity tests did not establish temporal stability between those paths.

All world material passes now write identical colour/blending into the world sidecar: world, LAND, sky/clouds, reflections, emissive and water. Only UI is excluded. The native difference layer is now restricted to changes caused by UI. An empty-UI frame must produce an entirely transparent native overlay even with reflected NIF material, water and blended world geometry present. This is tested in real Vulkan for both sRGB and linear-UNORM output, with and without density-attachment emulation. Display-code comparison tolerates one storage-rounding code in either output transfer to avoid noise selecting the native layer.

The original main colour, film, bloom, UI order and authored material expressions remain the reference. Composing the two layers is checked within two display codes. Translucent world pixels share the retained world depth/motion proxy; the ordinary single-motion-per-pixel transparency limitation still applies, especially to independently moving overlapping transparent surfaces. This is preferable to distributing pieces of ordinary world models across two temporal reconstruction paths, but does not promise artifact-free transparency.

## Canonical world depth

The R32 colour attachment that exported gl_FragCoord.z is removed. It was tied to colour-fragment invocation locations, which can be coarse under foveation. The original D32 world depth is now retained with an on-GPU copy before the first pre-film UI draw, or immediately before film if there was no earlier UI. Motion reconstruction samples this retained depth; HUD writes cannot contaminate it. Original main depth still serves the native UI layer's KHR depth submission. The depth snapshot replaces the old full-resolution R32 allocation, and the scene now has two colour attachments instead of three.

A rare depth-writing world draw after the retained snapshot invalidates synthesis coverage for that frame. Colour still renders normally and history can continue; the engine never claims complete coverage for stale retained depth. The regression writes HUD depth deliberately and verifies the motion/depth output still contains the world depth underneath it. Foveated UI transitions, later non-depth-writing world passes, water, independent runtime indices, recreation and teardown stay covered. There are no new per-frame CPU waits or readbacks.

## Stable clip reprojection

Static motion previously inverted the camera in float GPU data, reconstructed potentially large world coordinates per pixel, and projected them through the previous camera. Even equal view-projection matrices could produce nonzero motion through float cancellation. The CPU now forms previousVP * inverse(currentVP) with double-precision intermediate inversion and multiplication; only the composed clip-to-clip result is uploaded. Equal matrices explicitly produce identity, giving exact zero stationary motion. The static shader uses one clip-to-clip transform. Dynamic shader world/clip multiplication order explicitly matches the original colour vertex path.

The GPU regression uses real perspective matrices at wasteland coordinates and a farther origin, pitched stationary views and a pure tracked upward translation. It requires exact zero stationary motion in both eyes and the correct positive native-Y vector for upward head translation. CPU history checks cover identity, translation and singular-matrix rejection. These tests validate the supplied data and render paths, not the Quest compositor's complete behaviour.

## Testing on Quest

v276 retains Vulkan foveation, material specialisation and compact sRGB finished colour. Existing debug properties are respected. The previous diagnostic set debug.falloutquest.spacewarp=0, which persists independently of APK installation. Re-enable with `adb shell setprop debug.falloutquest.spacewarp on`, then force-stop/reopen com.falloutquest.app. Use `on`, not `1`: the existing `1` value selects producer validation without attaching synthesis.

Repeat slow vertical/lateral head movement in the same location first, with foveation left at the A/B setting. Look for synthesisAttached=1 and the producer log's worldColour=shared, uiOnlyLayer=1, reprojection=precomposed and worldDepthRetained=1. If the instability persists, capture a fresh log and motion/depth overlay; it would be incorrect to claim the headset defect conclusively resolved from software validation alone.
