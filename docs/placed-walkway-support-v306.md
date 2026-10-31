# Placed walkway support — v306

This includes v305's LOD lighting, cutout/depth and world material fixes and refines its walkway support correction. The isolated original-model tests passed in v305; an expanded original-placement fixture exposed five further failures. The fixture contains 589 available original Megaton static/door placements from the supplied master, 43,495 collision triangles and 61 ramp/scrap-plate references. This is a host fixture of original placement metadata, not a replay of the user's current save or a physical headset test.

## Cause and correction

The short downward cast could report a valid upward-facing lower/edge contact while the capsule was already entering the higher sloped facet. Restricting recovery to missed casts did not address that case. Centre/offset grounding probes also picked the point closest to the previous foot height instead of the capsule's true supporting height. The full-height raised cast could intersect a low ceiling before resolving its feet.

Supported movement now queries the capsule's bottom sphere downward from the existing step envelope every frame. This gives actual capsule-foot support on slopes and thin joins, and does not confuse head/rail contacts with ground. A slope/displacement-bounded downward contact distance retains adhesion on walkable descents. Airborne characters retain continuous capsule descent/landing; no distant-floor snap or invisible bridge is introduced. An accepted rise must fit the existing step envelope and pass head/upper-body clearance. If it cannot fit, horizontal motion stops at the previous pose instead of walking below the surface. Repeated gravity substeps reuse the highest validated head clearance; flat/descending support does not repeat ceiling queries.

The world's original Havok triangles, transforms, layers and existing step-height policy are unchanged. The support solver is shared, with no Megaton, reference, model or quest exception in production code.

## Verification

- All four host physics tests pass, including ledge gravity/landing, tall-riser/ceiling rejection, grab and ragdoll regressions.
- The two original shack stair meshes still pass ascent/descent with the real controller/gravity path.
- All 231 isolated original ramp/plate trajectories pass.
- The placed-scene test passes 3,850 eligible movement trajectories at 36, 45 and 72 Hz across all 61 represented walkway references. It excludes 274 starting poses that cannot accommodate the player's head. v305 failed five trajectories in this expanded fixture; v306 passes them.
- All 224 supplied Megaton meshes retain their visible shape counts under the new world material rule; none changes from visible to failed decoding. The separate Mart test removes its extra material-only shape while preserving its ten textured surfaces.
- v305's local original-shader and software-Vulkan tests passed; the same Vulkan validation also passed in CI. Android results must be read from the matching v306 Actions run. No attached headset is available.

Optional original-data commands:

`collision_tests --walkways <architecture/megaton directory>`

`collision_tests --walkway-scene <extracted mesh root> <original placement manifest>`

The manifest uses the existing collision-test format: reference, record type, XYZ, rotation XYZ, scale, model path. It contains original master metadata, never invented geometry. No proprietary fixtures are checked into Git.

On headset: enter Megaton, cross scrap plates and ramp joins in both directions, stop/turn on joins, crouch, then leave/re-enter. Check that a genuine ledge still starts a fall and that low ceilings/tall steps block movement. Also check far-hill lighting at two times of day, mast cutouts against the sky and the Mart base from multiple sides. Physical confirmation remains distinct from the host/GPU checks.
