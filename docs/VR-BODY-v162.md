# v162: raised-arm planes and Pip-Boy runtime availability

Baseline was latest main `42faefe8b93f4508f36777e30db0e8e181568580` (v161).
This pass preserves v161 eye alignment, centre-HMD orientation, torso policy,
stable symmetric user reach, original skin/palm/finger transforms and shared
ForeTwist attachment. No unrelated renderer/world/gameplay systems changed.

## Findings and replacement elbow policy

The v161 preferred plane retained negative gravity Y even with a high wrist.
The triangle already bent about 90 degrees in the representative pose, but
folded down rather than lifting/abducting the upper arm. Endpoint tests alone
could not detect this. The original skeleton does provide an articulated
Clavicle -> UpperArm -> Forearm -> Hand chain; no replacement rig is needed.

The new generic policy normalizes wrist height by calibrated humerus length.
At half a humerus below shoulder, gravity/down/out dominates. It blends with
smoothstep to a lifted outward plane by one-quarter humerus above shoulder.
That plane tilts 20 degrees upward from outward: abduction dominates elevation,
so it does not unnecessarily shrug the elbow above the wrist. Direction
projection automatically adapts this plane to across-body/face reach. Low
hanging reach retains a back component where gravity projection degenerates.
The previous pole is transported into the reach plane and approaches the soft
prior with the existing 250ms time constant/180deg-per-second cap. There is no
UI-dependent pose, positional elbow offset, desired bend-angle constraint or
controller-roll influence on the prior. These plane preferences and normalized
height regions are explicitly VR inference, not Bethesda animation data.

The existing analytical triangle determines bend from wrist position and
calibrated segment lengths. Arm scale is still deliberately configured from
shoulder-to-wrist measurement (default .62m); it never grows from observed
tracking. Emergency stretch is still reversible and capped at 3%.

Clavicle behaviour is unchanged after investigation: near-face reach is
normally within calibrated reach and requires no shoulder translation. The
correct upper-arm swing now lifts/abducts it. The original clavicle still
rotates, preserving length and torso connection, only to supply additional
reach, with the existing 12-degree bound. High/extended tests verify it remains
coherent. No floating shoulder or larger clavicle range was added.

Representative comparison uses original skeleton origins, default calibrated
reach, and a wrist 12cm inward, 12cm above, 40cm forward of shoulder. The same
wrist and original segment ratio produce an 88.96-degree internal elbow angle
in both solvers (180 degrees denotes straight).

| Metric | v161 | v162 |
| --- | ---: | ---: |
| Elbow height relative to shoulder | -10.68cm | +14.42cm |
| Elbow outward from shoulder | 6.34cm | 14.30cm |
| Wrist height minus elbow height | +22.68cm | -2.42cm |
| Bend angle | 88.96deg | 88.96deg |

This documents one mathematical fixture, not a measured headset pose. Original
hand-mesh grip poses are separately exercised below. Dense low/forward/face/
across/forward/low trajectories bound frame displacement to 1.5cm at 72Hz.
Pose-quality tests check shoulder/hand-relative heights, outward displacement,
65–115 degree bend in the representative face triangle, mirrored symmetry,
monotonic elbow elevation and wrist-roll independence. Existing reach,
singularity, skin-transform, torso and final-eye tests remain.

## Hard UI readiness bug verified against original Misc BSA

`MenuAssets()` demanded `&pipboy;` in STATS, ITEMS **and DATA**. Supplied original
`Fallout - Misc.bsa` has a DATA `MapMenu` identified by `&pipboymenu;` instead.
That original file always failed the readiness gate, leaving UI readiness
false; view measurement then remained invalid even with correct tracking and
screen geometry. v161's pose-only tests never tested this actual prerequisite.

The production signature helper now validates MapMenu's real menu root and
`pipboymenu` identifier. STATS/ITEMS/shared globals checks remain. Original
archive tests invoke the same helper on all four files, and unit tests reject
the former incorrect DATA signature. Assets/UI content are unchanged.
Low-frequency diagnostics include UI/session readiness so future asset/resource
failures can be distinguished from geometry or tracking rejection.

## Tracking path and bounded grace

Left grip remains the canonical physical body input; its POSITION/ORIENTATION
VALID and TRACKED bits are captured from the same located XR pose. Tracked
grip supplies viewing availability first. If grip remains valid but briefly
untracked, valid/tracked aim can supply tracking evidence for the **same
controller**. Aim position/orientation never replace the grip target or mount.

If both controller tracking-bit sources drop, valid grip can keep viewing
available for at most 200ms since the last reliable controller sample.
Continued valid-but-untracked poses cannot refresh this timer; timeout rejects
viewing. Completely invalid grip, invalid/untracked HMD, unfocused session,
loading or unavailable scene immediately reset the policy. Session/failed-XR/
missing-HMD/body-reset paths clear grace via `ResetFo3Pipboy`. Availability
never depends on the right controller. Aim evidence does not permit opening
with no canonical valid physical grip.

Tests cover tracked grip, tracked-aim evidence despite valid untracked grip,
short grace, timeout, no timer extension by inferred poses, invalid grip,
HMD/focus/loading rejection and recovery. The actual state machine keeps
Candidate/Active through short left tracking-bit interruptions while preserving
the existing 150ms intentional-view debounce.

## Physical screen, viewing window and controls

Original PipBoyArm `pipboyscreen:0`, original mesh normal, original ForeTwist
bind, solved forearm delta and HMD are authoritative. The device remains rigid
and derives from the same canonical ForeTwist used by skin. It is never rotated
independently toward the camera. Pronation is shared through Forearm/ForeTwist
as before; wrist residual and finger pose remain attached to corrected Hand.

No activation thresholds were widened. Entry still requires 16–72cm eye
distance, screen within 60 degrees toward eyes, within 55-degree HMD view cone,
raised screen, and 150ms debounce; stay hysteresis is unchanged. The improved
plane and fixed UI readiness now allow the actual pipeline to progress:
located valid grip -> solve -> ForeTwist -> authored device/screen -> HMD view
measurement -> Candidate -> Active/focus -> UI texture redraw/screen rendering.
Lowering or looking away dismisses through the existing geometric hysteresis.

The old count of successful 5-degree samples is replaced by 1-degree contiguous
window tests at **three neighbouring raised hand positions**. They rotate about
the solved forearm axis, using the original hand grip mapping, palm offset,
PipBoyArm normal/attachment and actual authored eye centre. Seam-crossing windows
are measured continuously over two revolutions; a minimum 60-degree window
(+/-30-degree tolerance) is required at every position. Results:

| Palm target in avatar render metres | Continuous entry window | Angle interval |
| --- | ---: | --- |
| (-.12, .06227, -.44757) | 94deg | 123–217deg |
| (-.08, .09227, -.41757) | 95deg | 129–224deg |
| (-.16, .03227, -.47757) | 92deg | 117–209deg |

Angles are relative to the fixture's neutral controller grip basis, not an
instruction to twist a real wrist by those absolute amounts. These tests
establish a continuous physical viewing region, not actual comfort on Quest.
Each uses the production tracking/state policy to enter Candidate then Active
with no right controller, and verifies dismissal when lowering the display.

Right controller controls remain unchanged and tested through Input -> Menu:
left/right stick changes STATS/ITEMS/DATA, up/down selects pages/list/stats rows,
A enters/equips/unequips, B backs out. Focus consumes world A; held A on closing
cannot fall through to world interaction. Existing runtime snap-turn
Candidate/Active guards and stick-release latching remain. Missing right grip
suppresses control input while left viewing remains available. No laser/touch
interaction was added. Warm UI renderer tests verify focus redraws and dormant
render suppression without per-frame GL queries.

## Quest validation

Host mathematical/control/renderer checks and original-asset checks establish
pose quality, transform consistency and the corrected runtime prerequisites.
They cannot establish actual tracking behaviour, controller comfort, original
skin deformation or reliably presenting the device in headset. Test natural
raised poses on both arms, face/cross-body motion, full extension, forearm
rotation and casual Pip-Boy viewing with right controller unavailable.

Capture `VR BODY` / `PIPBOY VIEW` logs if anything fails: they now include hand/
elbow relative geometry, internal elbow angle, raised blend, preferred/final
pole, UI/session readiness, separate valid/tracked flags, tracking source
(0 none, 1 grip, 2 aim evidence, 3 grace) and grace age, alongside all v161
HMD/eyes, shoulders/targets/palms/error, reach/stretch, roll and physical screen/
activation diagnostics. Logging remains once per ~180 tracking samples; pose
solving adds no allocation, asset scan, GL query or iterative IK.
