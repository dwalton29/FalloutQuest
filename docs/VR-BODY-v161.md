# Quest v161: headset-feedback corrections

This supersedes the v160 head alignment, reach, torso and elbow policies. The
original authored skeleton/skin, mesh-derived palms, grip basis, finger pose,
shared skin/interaction anchors, ForeTwist mount and player-state UI remain.
No new game assets or unrelated renderer/world/NPC changes are included.

## Verified v160 defects

* `pipLeftGripTracked_` was reset each action poll but never assigned from a
  located grip. A valid grip selected this false flag rather than tracked aim,
  making the viewing policy unavailable. Viewing also unnecessarily required
  the right grip. The availability test now uses a production pure helper;
  locating grip captures validity and tracking from the same XR flag result.
* Root translation aligned bind eyes, then neck yaw moved those eyes about the
  neck. Original eyes are forward of that pivot, so this introduced lateral
  displacement on looking around. The centre head also retained left-eye
  orientation despite averaging both positions.
* Only the original 0.507032m shoulder-to-wrist length plus clavicle and 3%
  stretch was available. Clamping discarded real controller displacement.
  Earlier tests exempted distant endpoint errors and missed adult workspace.
* Torso inference required practically identical two-hand and HMD turn arcs.
  Independent gestures or missing hands prevented normal physical follow.
* The elbow preferred the same down/out/back vector for every pose, with a
  constant-rate return toward it. That numerical constraint was not evidence
  of comfort.

## Canonical spaces and head solve

OpenXR LOCAL views are combined using midpoint positions and the normalized,
hemisphere-correct quaternion midpoint (unit-quaternion slerp at t=0.5).
`ToVirtualPose` transports HMD and grips once into virtual world metres,
+Y up/-Z forward. Locomotion yaw is transported into torso history once.
Fallout positions use the existing `(x/70,z/70-1.55,-y/70)` render conversion.

The cached original eye NIF centres are transformed by original Head bind
matrix. One absolute neck bind-to-pose delta rotates about original Neck
origin by head yaw minus torso yaw. Skin Neck/Neck1/Head roles consume this
same delta. Root translation is **HMD - torsoRotation * neckDelta(eyes)**.
Consequently root * neckDelta * authoredEyes equals HMD after posing, not
before it. No fixed lateral correction was introduced. Inverse root maps
grip positions and orientations once into the arm solve; skin, interaction
palms and device all consume the canonical results. Small shoulder translation
from compensating the actual eye-to-neck lever is intentional anatomical
motion, not an additional calibration offset. Head pitch/roll skin posing
remains outside this yaw-focused correction; full HMD orientation is used
for the physical display's viewing cone.

## Stable user reach calibration

Authored upper/forearm lengths remain 0.248052/0.258980m. Both receive one
stable symmetric scale preserving this ratio. The deliberately configurable
adult default is **0.62m shoulder-to-wrist**, scale about **1.223**. This is a
VR calibration assumption, not an original Fallout proportion and not an
automatic measurement of this user. Live maximum reach is never remembered.

Until an in-game calibration UI exists, measure shoulder joint to wrist joint
with the arm straight, in metres, and set before launching:

```
adb shell setprop debug.falloutquest.wrist_reach_m 0.62
```

Values 0.40–0.80m are accepted; absent/invalid values use 0.62m. Relaunch after
changing it. The property lasts until changed or rebooted. It is read during
body construction, never per tracking sample. Equipment rebuilds reread the
same deliberate setting. Different physical users should set their measured
value rather than treating the default as a personal calibration.

Normal wrist reach is 0.62m from the solved shoulder, plus the orientation-
dependent original wrist-to-palm displacement: its maximum outward palm
extent is roughly **0.696m left / 0.703m right**. These are radial upper bounds,
not a claim every hand orientation supplies that much outward displacement.
Shoulders stay attached through original clavicle rotation, bounded to 12°,
which uses the law of cosines to contribute the minimum rotation needed for
normal reach, bounded to a few centimetres of shoulder displacement. Beyond normal reach,
a separate reversible 1–1.03 emergency axial stretch adds at most 1.86cm to
wrist reach; still-unreachable targets clamp and diagnostics explicitly report
error/clamping. Normal adult test workspace reaches the targets without
emergency stretch. Calibration changes segment axial skin scales and elbow/
wrist translations coherently; the hand's authored size is preserved.

## Torso and elbow inference

Torso stays still inside a 35° neck-yaw deadzone. Outside it for 200ms, it
follows smoothly toward that boundary with a 300ms exponential time constant
and 180°/s speed cap. Hands are deliberately not a required torso sensor;
independent gestures cannot inject yaw. This works with one/both controllers
unavailable. Snap yaw immediately transports state exactly once. Returning
the head inside the deadzone stops follow. Three trackers cannot uniquely
recover true torso yaw; sustained head turning is the documented VR inference.

The elbow prior now varies continuously with reach direction and flexion:
gravity dominates ordinary forward reach; hanging arms bend back; high reach
adds outward abduction; cross-body and folded face/wrist poses permit more
outward abduction. The prior is projected into the current reach plane, while
the previous pole is transported there for continuity. A soft exponential
return (250ms time constant, bounded angular speed) avoids making this prior
an immediate rail. Its weights are VR anatomical policy, not NIF data.
Controller axial roll does not choose the elbow plane. Elbow triangle solving
still preserves calibrated segment lengths, and extension singularities keep
the transported pole. Headset testing must judge whether these soft priors
feel sufficiently free, especially across the chest and near the face.

Hand mapping and forearm roll retain v160's audited original mesh/bone basis:
orientation first establishes the authored palm offset's wrist target;
quaternion axial decomposition extracts forearm pronation separately from
residual wrist flex/deviation. Forearm receives half the axial roll,
ForeTwist the full axial roll, as absolute bind-to-pose deltas, not chained
independent full-hand rotations. UpArmTwist shares humeral roll. Original
ForeTwist is a sibling of Hand under Forearm, not a second serial wrist.

## Pip-Boy viewing

Original PipBoyArm screen and ForeTwist bind relationship remain authoritative.
Its rigid mount derives from the canonical solved ForeTwist, retaining rigid
device dimensions when calibrated forearm skin is axially scaled. There is no
separate controller-derived pseudo-forearm. Original mesh tests validate this
relationship and the original screen geometry.

Viewing availability requires gameplay ready, not loading, focused session,
valid/tracked HMD and valid/tracked left pose. Valid grip takes priority;
untracked valid grip cannot masquerade as tracked aim. Availability permits
an aim fallback, but the canonical physical body/mount still requires valid
grip: no aim-to-grip substitution is made for rendering. The actual viewing
pipeline additionally requires a solved mount, player session and loaded UI.
Right-hand validity gates control input only.

Opening observes the actual screen: raised to within 25cm below shoulder,
16–72cm from eyes, screen normal within 60° toward eyes, within a 55° head
view cone, maintained for 150ms. Staying active uses 35cm shoulder drop,
12–85cm distance, 70° facing and 65° cone. Redundant hand-pivot height is gone:
wrist flex cannot veto an already raised readable screen. This broad viewing
region is intentional; screen facing and sustained viewing still prevent
accidental activation. These are initial ergonomic policies, not measured
headset-comfort results.

## Validation and diagnostics

Production math tests cover realistic reach distances including calibrated
extension and unreachable poses, upper/forearm scale consistency, symmetry,
face/cross/high/low/relaxed poses, elbow continuity and roll independence,
no cumulative growth, small looks, sustained slow/fast turns under independent/
missing hands, returning to centre, exact-once snap, posed-eye coincidence
through ±30/60/90° torso-fixed/following yaw and unbiased quaternion midpoint.
Pip-Boy tests exercise XR flag extraction, availability selection and activation,
including valid tracked grip with no right controller, untracked grip, aim
fallback, invalid left/HMD, unfocused session and loading.

Optional original-asset validation takes original skeleton, left glove, right
hand, PipBoyArm and two eye NIFs. It checks actual hierarchy, pivot/basis,
50–65cm grip-target errors, posed-eye alignment, multiple original-screen
raised-pose viewing angles, screen/mount relationship and
rigidity; copyrighted NIFs remain outside the repository.

Every ~180 tracking samples, logs report HMD/posed eyes/error, torso/neck yaw,
world-space shoulders/controllers/palms/elbows, palm error, calibrated scale,
emergency stretch, target wrist distance, normal wrist/palm reach, clamp and
roll, plus mount, tracking availability, screen distance/facing/cone, raised
condition and current activation phase. Capture `VR BODY` and `PIPBOY VIEW`
while reproducing the headset issues. Automated tests establish transform
correctness; reach calibration fit, visible skin deformation, elbow freedom,
physical-turn comfort and casually raising the Pip-Boy require Quest testing.
