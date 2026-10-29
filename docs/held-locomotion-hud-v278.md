# Held movement, final body pose and SpaceWarp HUD — v278

The headset confirmed v277 resolves the whole-image pitch judder. The remaining report describes a jiggly player model while walking and a held weapon staying behind, pulling the arm backward until it catches up.

## Concrete causes and changes

The engine's existing movement settings produce `(77 * 4 * 1.1) / 70 = 4.84 m/s`. The physics grip servo permits at most 4 m/s of positional correction. Previously, an attached weapon had to chase the entire moving player through that controller-motion servo. At sustained running speed it could never keep up. This is separate from SpaceWarp's vector convention.

`World::MoveHeld` now applies the actual application locomotion transform to attached bodies and grip targets before tracking and weapon updates. This covers resolved walking, gravity/steps and pivoted snap turns, but excludes real tracked head/hand motion. The existing finite-effort solver still handles controller-relative motion and contacts. Carry movement uses the same convex sweep and wall-slide implementation as ordinary integration; no body is placed through a wall. Two held bodies sharing the transform do not collide with each other's stale pre-transport position; ordinary simulation still resolves their contacts. Unheld props and dropped weapons remain in world space. Releasing a held body adds its actual carried centre velocity; an explicitly supplied release velocity remains authoritative. The implementation is specific to VR locomotion, preserving the original NIF geometry, grip anchors and authored material/mass inputs.

The arm rig previously sampled the prior physics grip during its early update, then a once-per-tracking-serial guard prevented its later render update. Interaction preparation now solves current tracking palms without feeding old physical palms back into control targets. Existing loose grips also receive the current raw tracking target before the shared physics step. After weapon/loose-body physics, the final rig uses the current physical palm, evaluates contact-limited fingers and publishes the final skin palette. The arm filter state is restored before that final solve, so it advances once rather than filtering twice. Support-hand alignment is applied after this final solve. Both colour rendering and the SpaceWarp history see this final palette.

## FPS HUD

Both HUD modes show app FPS and a separate `SW ~… FPS` readout when synthesis metadata was submitted successfully. The SpaceWarp number estimates useful output as successful app submissions plus one prospective generated frame per synthesis submission, capped at the current display refresh rate when available. Mixed fallback windows therefore do not blindly double every app frame. `SW OFF` denotes the latest frame being submitted without synthesis. `~` is intentional: this is not a measured compositor-generated-frame count, nor proof that all generated frames met their deadline. OVR Metrics/logcat remains the independent runtime measurement.

The refresh-rate field now comes from the optional `XR_FB_display_refresh_rate` API and its change event. It no longer mistakes a half-rate `predictedDisplayPeriod` for panel refresh. Unsupported refresh reporting displays `Hz N/A`. The detailed metrics occupy a second text row using the existing authored Monofonto font; no font asset is substituted.

## Validation and limits

The real Bullet regression measures sustained 4.84 m/s transport at 30, 36, 72 and 90 app FPS. Free-space carried X lag is zero in these tests. It also checks both held objects, unheld isolation, release velocity, snap rotation, wall blocking and tangential sliding. Existing mass response, plate lift, surface slide, angular settling, corner blocking, gravity, stacking, capsule landing and authored inertia tests pass. Source ordering checks require the final rig after weapon update, final-only palette/finger publication and support alignment after the final pose. Existing VR body/arm continuity and HUD font/consumer checks pass.

No headset result is claimed by these tests. Repeat sustained walking while holding a pistol and a long gun, stopping, snap turning, two-hand support and walking toward a wall. The change targets confirmed physics/timing defects; any remaining synthesis-only skin distortion needs fresh video or motion-buffer capture. The HUD's SpaceWarp value is explicitly estimated, so it must not be reported as measured generated FPS.
