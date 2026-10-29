# Authored load-door activation (v287)

The Megaton entrance still failed to offer its activation prompt because the rendered
`MegatonMainGate01.NIF` mesh is a flat floor marker. Its visible geometry spans
approximately (-10.046, -33.216, 0) to (21.784, 2.705, 0) NIF units. Both the previous
visual-bounds picker and the world-door fallback around the reference DATA point
miss ordinary horizontal controller rays above the ground.

The same original NIF has an authored Havok entrance box spanning approximately
(-422.746, -63.699, 5.835) to (455.111, 84.799, 451.913). Load doors now retain shared,
immutable model-space activation bounds derived from their original collision
shapes during CPU asset preparation. Picking transforms the ray into that volume
with the placed reference's rotation and scale, preserving distance in scene metres.
No asset decoding occurs in the frame picker. Nonvisual load doors retain the same
activation metadata. Local swinging doors continue to use their moving visual bounds.

The existing enabled-state, lock, ownership, teleport and interaction-occlusion
checks remain in place. This is a general load-door fix; no Megaton reference IDs
or enlarged aiming radii were added.

`door_activation_tests` runs synthetic rotation/scale/range/occlusion checks in CI.
Supplying an original Megaton gate NIF as its first argument additionally proves
that a horizontal ray at 1.6 metres misses the old padded visual marker and hits
the authored BHK volume at 2 metres. Original game assets are not committed.
Existing interaction runtime and collision/indexed-NIF source checks also pass.
Quest controller aiming and the actual cell transition still require device testing.
