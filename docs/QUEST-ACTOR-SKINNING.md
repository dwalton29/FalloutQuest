# Quest actor skinning (APK 147)

Based on main ce35c8a (APK 146). The supplied Megaton capture establishes that
actor preparation, previously hidden in the first opaque eye pass, dominates
this scene. It does not establish the individual skinning/upload sub-times.

## Evidence and measurement status

| Measurement | Supplied APK 146 Quest capture | APK 147 |
|---|---:|---|
| NPC opaque CPU, left/right | 48.136 / 0.203 ms | Pending Quest capture |
| Player opaque CPU, left/right | 33.030 / 0.094 ms | Pending Quest capture |
| Detailed world CPU, left/right | 6.647 / 4.621 ms | Pending Quest capture |
| Detailed world GPU, each eye | approximately 3.5–4.0 ms | Pending Quest capture |
| Native LOD draws / vertices | 0 / 0 | Unchanged architecture |
| Combined opaque stereo CPU | 92.754 ms | Pending Quest capture |
| Full application CPU frame | Not separately measured | New explicit measurement |
| Quest application FPS | approximately 11–14 | Pending Quest capture |
| Player/NPC CPU skinning sub-times | Not captured separately | GPU path performs no CPU vertex skinning |
| Animated VBO upload bytes | Full expanded meshes; byte count not captured | GPU path performs zero steady-state full-mesh uploads |
| Palette upload bytes/time | Not applicable | New per-actor telemetry; pending Quest capture |
| Player/NPC GPU times | Not supplied | Existing delayed GPU queries now reported by actor |
| Megaton visible / instanced / fallback | approximately 897 / 0 / 878–900 | Pending Quest capture |

Source inspection confirms the old NPC path evaluated the pose, skinned all unique
positions and direction vectors, expanded triangles, and uploaded full VBOs.
The player path recalculated pose/fingers, skinned every expanded position and
normal/tangent/bitangent, normalised directions, and uploaded full VBOs. Player
palm-anchor discovery also rescanned mesh vertices during each update. The supplied
outer timers cannot quantify how much of 48/33 ms belongs to each operation.

## Implementation

The existing IK/finger solver and Fallout clip/hierarchy evaluation produce bone
transforms once in an explicit stage before either eye. Static bind geometry and
weight/index attributes remain resident. Each mesh caches its referenced bone
subset, uploaded as an 8-by-N RGBA32F nearest-filtered data texture. Each row holds
separate position and direction matrices: player axial stretch affects positions
but preserves the old rotation-only direction policy. NPC transforms retain the
original placement/game-to-scene conjugation and weight normalisation. Player
bind remainder and overweight policies are unchanged. The vertex shader skins all
four geometric vectors before the existing material/lighting shader.

Rigid head attachments use their authored/current associated bone as a single
transform with weight one, with static geometry and the same corrected placement.
FaceGen geometry, textures, appearance assembly and material choices stay at setup.
Palette mappings, bone roles/names, anchor results and finger/delta storage are
cached. No draw function prepares a pose. Both eyes use the same palette.

Runtime checks cover vertex texture units, texture dimensions and attribute slots.
GLES 3 supports RGBA32F data textures; texelFetch needs no floating-point filtering.
Unsupported limits select an explicitly logged CPU reference fallback. Debug-only
`debug.falloutquest.cpu_skin=1` selects the same CPU reference for parity comparison.
It is not the previous implementation and must not be described as APK 146 timing.

NPC render-pose preparation uses the union of both eye frusta and a conservative
setup-time envelope covering the entire loaded animation clip, hierarchy, scale,
Hermite overshoot and rigid attachments. Unknown/unbounded envelopes fail open.
Logical elapsed animation time continues while render pose preparation is skipped.
The envelope can be loose, which reduces culling efficiency but preserves visibility.

Megaton initial statics missed geometry sharing because uploads ran before the new
exterior context was committed. Sharing immutable STAT/SCOL/TREE geometry now uses
mesh properties independently of the previous scene state. Actual instanced draws
still require the committed exterior context and the existing geometry/material
keys. Native LOD, doors, loose objects, decals, alpha blending and unique external
emittance retain their existing exclusions. No mesh/material keys are broadened.

## Capture and acceptance

Install and restart, resetting previous performance diagnostics:

```sh
adb install -r FalloutQuest-147-gpu-actor-skinning.apk
adb shell am force-stop com.falloutquest.app
adb shell setprop debug.falloutquest.cpu_skin 0
adb shell setprop debug.falloutquest.fast_material 0
adb shell setprop debug.falloutquest.render_scale 1
adb shell setprop debug.falloutquest.lod_radius20 0
adb shell setprop debug.falloutquest.lod_minimal 0
adb shell setprop debug.falloutquest.lod_clip_bypass 0
adb logcat -c
adb shell am start -n com.falloutquest.app/.MainActivity
adb logcat -v threadtime | grep -E 'ACTOR SKIN|ACTOR FRAME PREP|PLAYER PERF|NPC PERF|APPLICATION FRAME PERF|OPAQUE BREAKDOWN|OPAQUE PERF|DETAILED WORLD WORK|NATIVE LOD WORK'
```

Wait for scene/texture preparation to settle; collect at least 120 steady-state
stereo frames in the same Megaton view with animated NPCs, controller hands and
finger motion. Repeat in Capital Wasteland (existing NPC residency restrictions
remain unchanged). Also traverse and turn away/back to check pose visibility.
For CPU/GPU parity, restart with `cpu_skin=1`, then restore zero and restart.
For an actual before/after timing comparison use APK 146 in the same scene.

`ACTOR FRAME PREP` includes pose, fingers, mesh bone matrices, animated uploads,
setup and the complete prep wall time. `animatedUploadCpuUs` includes driver
back-pressure; normally it is solely palette upload time. `PLAYER/NPC PERF` also
includes both eyes' opaque and alpha draw submission, CPU-skinned vertex counts,
palette bytes and theoretical full-mesh bytes avoided. ROLLING lines average each
CPU substage. GPU left/right values are delayed rolling opaque GPU query means;
-1 and zero samples mean unavailable. They do not include alpha or CPU prep and
are not paired with the current CPU sample. APK 146 disjoint handling remains.

`APPLICATION FRAME PERF` measures complete app work after xrWaitFrame through
xrEndFrame, including tracking, streaming, actor prep and both eyes. Cadence
includes the next wait and yields observed application loop rate, not compositor
refresh rate. Compare this and actor prep, rather than just reduced opaque timers.
Telemetry is emitted every 60 stereo frames. Existing LOD/state/work diagnostics
and authored draw range remain intact.

Acceptance on Quest is still required: arms/body/hand/controller alignment, finger
curls, Pip-Boy, head/hair/headwear/eyes/teeth, race skin/FaceGen, clothing/material
normals, no exploding geometry, no bind flashes and identical left/right poses.
Performance targets are player pose/palette near 1–2 ms, visible NPC pose a few ms,
zero CPU skin vertices/full-VBO bytes in normal mode, and a material decrease in
full frame CPU time. These are targets, not measured results. No headset is
connected to this execution environment.

## Validation and remaining work

Host tests execute the actual production GLES vertex shader with transform
feedback and compare positions plus normal/tangent/bitangent outputs. They cover
player rotation/stretch, remainder, overweight/zero weights, NPC normalisation,
scene/root placement and unskinned static geometry. Additional tests cover palette
subsets, conservative animated bounds, rigid head attachments, FaceGen/armour
placement, once-per-stereo update and GPU-only upload scope. Existing renderer,
world, actor and gameplay tests remain in the APK build workflow.

There is no measured post-change dominant bottleneck yet. The supplied capture
supports removing CPU actor skinning first; remaining detailed world submission,
GPU skinning, animation evaluation and separate Wasteland LOD costs must be ranked
from the new full-frame capture. No DISTLOD replacement, multiview, water, draw
range or broad material optimisation is included.
