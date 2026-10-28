# Responsive physical grips (v264)

The v263 grab spring made every item use 900 N/m of linear stiffness and
35 units of angular stiffness, with dimensional damping set to 1. Those values
were independent of authored mass/inertia. This created considerable lag for
heavy props and sustained oscillation for weapons. Bullet's spring stiffness
limiter also reduced rotational stiffness for small inertias.

Grips now use six compliant locked constraint axes in the same contact solver.
Per-tick position correction is 0.55, with small inverse-mass/inertia-scaled
constraint softness. This gives a damped attachment response without overriding
body transforms or replacing authored mass, friction, restitution or inertia.
The original surface anchor, physical contacts and solver release momentum stay
in use. This is a VR interaction policy, not a claim about Fallout 3's Havok
solver settings.

The extra convex sweep previously treated a rotation starting against a surface
as a reason to clamp the entire transform, even during tangential/separating
motion. Its callback now checks the motion at the contact point, including the
angular component, and discards non-inward hits before choosing the first
blocking hit. A 0.1 mm inward-motion tolerance (below the 1 mm sweep
allowance) leaves minute resting-contact corrections to the discrete solver,
avoiding toolchain-sensitive latching. Inward impacts and thin-body rotational collision remain guarded.

Weapon support tracking now captures a controller-to-palm offset at acquisition,
as primary-hand tracking already did. Both tracking targets come from raw
controller transforms; the constrained rendered hands are not fed back into the
two-hand aim target. A held weapon submits its current target and advances the
shared physics world before reading the solved gun pose for aim, throw samples,
reload interaction and rendering. A frame guard keeps the world advancing once.
Unheld items retain the existing shared-world advancement path.

## Reproduction and measured checks

`tests/physics/grip_response_tests.cpp` exercises production Bullet/world code.
The before measurements used v263's world implementation against the identical
motions, with only the new pass/fail thresholds disabled in a scratch copy.

| Check | v263 | v264 |
|---|---:|---:|
| 0.2 kg prop, peak following error at 1 m/s | 15.5 mm | 6.82 mm |
| 1 kg prop, peak following error at 1 m/s | 34.5 mm | 6.82 mm |
| 10 kg prop, peak following error at 1 m/s | 106 mm | 6.82 mm |
| 30 kg prop, peak following error at 1 m/s | 176 mm | 6.82 mm |
| Off-centre long gun, worst angular error during late settling window | 53.0 degrees | below 0.001 degrees |
| Resting box, requested 0.4 m slide while yawing | 0 m | 0.397 m |
| Resting plate, requested gentle 0.1 m lift with tilt | 0.0615 m | 0.0986 m |

These are deterministic synthetic solver measurements, not headset latency or
performance benchmarks. The tests also require less than 1 cm stop overshoot,
tracking at 72/90/120 Hz input rates, and a fast-spinning thin plate staying above
the floor. Existing world tests cover held-wall blocking, stacking, authored
inertia, surface anchors and release momentum. The physics suite, relevant
player/weapon/interaction tests and render source guards pass locally. Android
and Vulkan checks run in CI. Final comfort and collision feel need headset use.
