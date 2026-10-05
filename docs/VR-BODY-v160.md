# Authored VR body retargeting — Quest v160

This replaces the Q21 arm solver rather than reverting main to v158. World,
NPC, lighting, renderer and player-state behaviour are outside this change.
The original assets were inspected with the project's NIF decoder: the male
skeleton, upperbody, left Pip-Boy glove, right hand, both human eyes and
PipBoyArm. No replacement models, textures or UI were made.

## Audit findings

The former solver used a hand/finger vertex centroid as its forearm endpoint.
That conflated wrist location, hand orientation and the position of the palm.
Rotating the hand about that endpoint could displace the actual visible grab
anchor. The skeleton started at 112% arm length and only grew, up to 122%,
based on the largest observed reach. Its gravity-only elbow plane had abrupt
fallbacks near vertical reach; v159 supplied no previous-pose continuity.
HMD yaw drove torso follow above 35 degrees, even with stationary hands.
Controller targets were re-anchored to the head after inverse-root mapping,
while the root had an unrelated 8cm setback. The authored Head origin was
mistaken for the eye position. These were separate inconsistencies, not a
single sign correction.

Skin parts have flattened bone lists, not a complete articulated hierarchy.
The old role classifier also missed `Bip01 LUpArmTwistBone` and its right
counterpart because they have no space between side and UpArm. The original
skeleton has Clavicle -> UpperArm -> Forearm -> Hand. ForeTwist is a sibling
of Hand under Forearm, with the same bind origin as Forearm. Upper-arm twist
is a child of UpperArm. R ForeTwistDriver is a separate driver child of
UpperArm. Treating these as serial wrist joints would apply roll twice.

## Transform chain and units

1. OpenXR LOCAL supplies the midpoint of both view positions and **grip**
   positions/orientations. Aim poses remain for pointing and are not fallback
   body targets.
2. `ToVirtualPose` applies virtual locomotion translation and yaw once. Both
   HMD and grips are now in virtual world space: metres, +Y up, -Z forward.
3. `SolveTorso` maintains independent torso yaw. `BodyRoot` aligns the complete
   authored eye midpoint to the HMD. Eye centres are measured from the original
   bone-local eye NIFs, transformed by the skeleton's authored Head matrix.
4. Game coordinates become avatar render coordinates as `(x/70, z/70-1.55,
   -y/70)`. This is the existing mesh-upload conversion, including its floor
   translation. Skeleton origins use exactly that conversion.
5. Inverse BodyRoot maps virtual grips into avatar render space once. There is
   no second head-anchor correction or lateral hand offset.
6. Arm solving returns absolute bind-to-pose affine deltas in avatar render
   space. All skin partitions receive those same named-bone deltas. Existing
   palette skinning then applies BodyRoot once when rendering.
7. Finger curls remain in authored hand space and compose **before** the hand
   delta. Finger input cannot influence arm IK.

The measured skeleton lengths are about 0.248052m upper arm and 0.258980m
forearm on each side. The visible palm's measured offset from the wrist is
about 0.075686m for the glove and 0.082641m for the right-hand asset. These
are different original meshes; their offsets are not artificially equalised.
The eye midpoint is approximately `(0, 0.162266, -0.097571)` in mesh-upload
render coordinates. This places the camera at the eyes rather than the skull
base without inventing a body setback.

## Solver architecture

`player/fo3-vr-body.h` holds pure allocation-free math, immutable `ArmRig`
inputs, two `ArmState` histories, torso history, and resulting `ArmPose`
transforms. The runtime adapter builds rig points once from the actual decoded
hierarchy and caches the exact Hand-weighted visible palm anchors. It checks
that the original parent relationships exist. Missing required original eye
or arm assets disables this body instead of inventing anatomy.

Hand rotation maps the authored across-palm/inward orthonormal frame to the
tracked OpenXR grip frame. Across-palm is grip -Z; inward is -X on the left,
+X on the right. Authored finger roots define the corresponding mesh basis.
The desired wrist is `grip - handRotation * (authoredPalm - authoredWrist)`.
IK solves to that wrist. The hand retains its authored size and full tracked
orientation. Interaction palms derive from that same solved hand transform.

The elbow uses a torso-relative down/out/back prior, projected into the
shoulder-to-wrist plane. The previous pole is transported into the new plane
and moves toward the prior at no more than 180 degrees/second. Its continuity
uses elapsed time, not frame count. Roll cannot steer the pole. Two-bone
triangle geometry determines elbow and wrist; bend-plane frames determine
upper and forearm rotations, including turns opposite the authored arm axis.

Reach first rotates the authored clavicle toward an unreachable target by at
most 12 degrees, preserving clavicle length. Only remaining near-extension
reach can stretch upper and forearm by at most 3%. This is a per-pose symmetric
policy, not persistent calibration: relaxed poses immediately return to the
authored lengths. More distant targets are clamped, and palm error is reported
rather than hidden through unlimited stretching. Different user proportions
may therefore require an explicit future calibration feature.

Quaternion swing/twist decomposition extracts axial pronation/supination from
the full wrist orientation relative to the solved forearm frame. Forearm gets
half that axial roll; ForeTwist gets the full axial roll as a separate absolute
delta. Hand receives the full tracked orientation once; remaining flex and
deviation stay at the wrist. Upper-arm twist shares half the humeral axial
component. Roll is unwrapped across +/-180 degrees. At a degenerate 180-degree
wrist swing, the previous axial angle is retained.

The physical Pip-Boy uses the original ForeTwist bind matrix and the **same**
solved ForeTwist delta: `BodyRoot * rigidForeTwistDelta * authoredMountBind`.
Its authored screen centre follows forearm stretch, while the casing remains
rigid. There is no second inferred forearm or controller-driven device pose.
Pip-Boy UI, sounds, tabs, activation policy and player state are preserved.
The shoulder pickup zone now follows the solved right shoulder.

## Original data versus VR inference

Skeleton hierarchy, bind transforms, mesh geometry, skin weights, fingers,
eye centres, hand offsets and Pip-Boy mount/screen geometry are original
Fallout data. OpenXR grip mapping, temporal torso inference, elbow prior,
clavicle limit, soft-reach limit and twist distribution are VR retargeting
policies. This does **not** claim that 50%/100% twist sharing reproduces
Bethesda's original animation constraints. It uses the actual original twist
bones for a new tracked pose.

Torso yaw follows coherent physical yaw shared by HMD and both hand offsets,
while artificial yaw transports torso/history immediately. Head-only yaw
leaves the torso fixed with tracked stationary hands. Neck/head skin rotates
relative to torso; head pitch/roll and lower-body IK are not implemented in
this pass. With both hands unavailable, an 80-degree neck limit supplies a
slow fallback. Three tracked devices cannot uniquely measure torso yaw;
turning with independent hand gestures remains a headset validation case.

## Validation and diagnostics

`tests/player/vr_body_tests.cpp` checks representative relaxed, forward,
extended, bent, near-face, crossed and rear-shoulder poses; palm compensation;
left/right symmetry; bone lengths and endpoint connections; finite orthonormal
transforms; continuous vertical/extension sweeps; reversible reach after
extreme extension; wrist-roll wrap/decomposition; head-only yaw, snap turns,
physical turns and root/inverse-root mapping. These execute production math,
not source-string assertions.

`tests/nif/vr_body_asset_tests.cpp` accepts six external original asset paths:
skeleton, left glove, right hand, PipBoyArm, left eye, right eye. It verifies
real hierarchy, measures exact triangle-expanded Hand-weighted palm offsets,
solves real-mesh palm targets, checks authored eye placement and checks the
production rigid mount against ForeTwist. It is skipped in CI without game
files, and was run locally with the supplied originals. Proprietary NIFs are
not committed. Run:

```
vr_body_asset_tests skeleton.nif lefthandpipboyglove.nif righthand.nif \
  pipboyarm.nif eyelefthuman.nif eyerighthuman.nif
```

`VR BODY` logs every 180 tracking samples report target/palm/error, solved
shoulder/elbow, lengths/stretch, wrist roll, torso/head-relative yaw and mount
status. Rig construction logs measured original proportions. Steady-state
solving does not allocate, load assets, query GL or scan skeletons.

Headset validation is still required for comfort, elbow posture, wrist/forearm
skin deformation, palm alignment, physical turns with gesturing hands,
reach clamping, Pip-Boy screen orientation/activation and shoulder pickup.
Passing numerical tests establishes consistency, not visual naturalness.
