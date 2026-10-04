# Quest exterior opaque diagnosis (APK 146)

This is a measurement pass on main based on 499822c, not a DISTLOD implementation.
Default range, material behaviour, geometry, streaming and sequential stereo are
unchanged. No code is restored from the abandoned branch. The multiview experiment
is absent. Main already contains the separate earlier opaque state-cache work.

## What is known, and what still needs measurement

The supplied Quest baseline is 45–49 ms for the **combined** opaque CPU phase.
It does not isolate native LOD, detailed statics, NPCs or the player, and has no
separate GPU measurements. Those component times, draw counts and diagnostic
effects cannot be inferred from the supplied logs.

History confirms fe8f37c expanded the Level4 object search from block radius 5
(121 potential blocks) to radius 8 (289 potential blocks), with 324 cache slots.
This is a 2.39x potential search window, not proof of a 2.39x submitted workload
or time increase. Native LOD does call the general DrawSceneObject material path.
These facts support testing native LOD first; they do not establish that it
accounts for most of the 45–49 ms. No original DISTLOD assets were supplied for
this task; an authentic replacement is deliberately not implemented.

## Measurements

`OPAQUE BREAKDOWN` emits both eyes every 60 stereo frames, with frame and eye IDs:

- nativeLodUs / nativeLodCpuUs: native terrain tiers, Level4 objects and High VWD;
- detailedWorldUs / detailedCpuUs: detailed streamed geometry and instancing;
- npcUs and playerUs: their own opaque phases;
- totalUs: existing full opaque wall-clock measurement including pass setup;
- nativeLodGpuUs and detailedGpuUs: separate delayed, per-eye rolling GPU means.

CPU scope timing includes query begin/end overhead. Do not add eye 0 and eye 1
and compare that sum with an earlier single-eye timer. GPU samples are not paired
with the current CPU frame. GPU values are -1 when unavailable and sample count
is zero. `gpuSupported`, `gpuDisjoint` and `gpuDropped` expose validity/backlog.
The 32-slot query ring polls at least three stereo frames later, checks availability
before reading a result, drops samples if full, and clears every phase/eye mean
on disjoint events. No blocking reads, glFinish or fence waits are added.
NPC/player GPU queries are also sampled, but the published comparison focuses
on native LOD and detailed world. Query boundaries on a tile GPU can introduce
measurement overhead; compare all A/B runs with the same instrumentation.

`OPAQUE PERF` preserves totalCpuUs and aggregate state/uniform/submission counts.
Its gpuQueryMeanUs is the mean of individual phase queries, **not total opaque GPU
time**; use OPAQUE BREAKDOWN for attribution.

`NATIVE LOD WORK` reports actual submitted shapes by Level32/16/8/4 terrain,
Level4 object and High VWD, actual GL draws, submitted vertices and triangles.
Resident block counts are split into Level4/coarse/High as well as the combined
count. Considered blocks are each visited once per opaque eye pass. Window and
refinement rejects are block selection decisions. `frustumRejectedBlocks` means
all opaque shapes considered in a selected nonempty block were rejected by the
existing per-shape frustum check; there is no newly introduced block culler.
`frustumRejectedShapes` counts those actual early returns. Empty/alpha-only blocks
are not counted as frustum rejects. Terrain suppressed by the existing centre/
terrain-window handoff is not a whole-block reject if its object layer is eligible.

`DETAILED WORLD WORK` separately reports stereo-conservative visible objects,
instanced placements, actual instanced/fallback GL draws, total actual draws,
submitted vertices/triangles. Instanced geometry counts include every instance.
Visible candidates can exceed submitted shapes if an existing per-eye check
rejects a fallback object. Neither these counts nor the old 69+40 example include
native LOD, NPCs or the player. All current scene object draws are triangle-list
DrawArrays, so triangles = submitted vertices / 3; vertex counts are submitted
vertex work, not unique mesh vertices.

## ADB capture and A/B controls

Install the APK with `adb install -r FalloutQuest-146-lod-diagnostics.apk`.
Set properties before starting/restarting the app; they are read once per process.
Debug builds only; release builds ignore these diagnostic switches.

Reset to baseline A:

```sh
adb shell am force-stop com.falloutquest.app
adb shell setprop debug.falloutquest.fast_material 0
adb shell setprop debug.falloutquest.render_scale 1
adb shell setprop debug.falloutquest.lod_radius20 0
adb shell setprop debug.falloutquest.lod_minimal 0
adb shell setprop debug.falloutquest.lod_clip_bypass 0
adb logcat -c
adb shell am start -n com.falloutquest.app/.MainActivity
```

Start the game using the usual launcher. Capture (works in Windows terminals too):

```sh
adb logcat -v threadtime -s FalloutQuest:I "*:S" -e "OPAQUE BREAKDOWN|NATIVE LOD WORK|DETAILED WORLD WORK|OPAQUE PERF|DETAIL LIFETIME|DETAIL COMMIT" > quest-lod-A.txt
```

Stop capture with Ctrl+C. Warm up the same save, fixed position, direction,
time/weather and streaming until block counts settle. Capture at least 20 seconds
of stationary viewing, then the same traversal in each mode. At 11 FPS, 60-frame
logging yields about one paired snapshot every 5.5 seconds; longer captures help.
Only change one switch per run, force-stop and restart between runs:

| Run | lod_radius20 | lod_minimal | lod_clip_bypass | Purpose |
|---|---:|---:|---:|---|
| A | 0 | 0 | 0 | Current 31-cell, block-radius-8 range |
| B | 1 | 0 | 0 | Older 20-cell, block-radius-5 search/render range |
| C | 0 | 1 | 0 | Same native object geometry with diffuse/alpha/fog diagnostic |
| D | 0 | 0 | 1 | Same native selection with fragment cell clipping bypassed |

For example, B:

```sh
adb shell am force-stop com.falloutquest.app
adb shell setprop debug.falloutquest.lod_radius20 1
adb shell am start -n com.falloutquest.app/.MainActivity
```

Reset all three properties to 0 afterwards. Confirm the active flags in NATIVE
LOD WORK, not just that setprop returned successfully. If a device rejects
property changes, report that error; do not interpret an unchanged run as an A/B.

B changes Level4 object search and selection, with 121 vs 289 potential blocks.
Cache capacity stays 324, terrain range is untouched. High VWD keeps its existing
replacement rule, so shrinking the normal horizon can expose High meshes; inspect
High VWD counts too. Wait for residency to settle in each restarted run.

C uses an early diffuse/alpha/fade/fog branch in the existing program for native
Level4 object and High VWD opaque draws only, before normal/specular/environment/
glow/local-light work. Terrain, detailed objects, actors, reflections and alpha
passes retain their original materials. The vertex path, geometry, culling,
range and transforms are identical. This diagnostic is not authentic DISTLOD.
It still uses the general program and its uniform/binding submission, so a drop
supports expensive fragment material work; no drop alone cannot rule it out
because register allocation and submission overhead are retained.

D suppresses the existing native clipping-enabled uniform during the measured
native pass only. It does not change bounds, transforms, selection or the default
shader loop. Terrain can overlap detailed cells and shade more fragments; a GPU
drop despite that supports clipping cost. No drop is inconclusive because removing
discard also increases surviving fragment work. Do not ship this visual mode.

## Object lifetime

Static inspection finds explicit replacement/compaction of departed placements
in both the per-cell publisher and older window commit, and bounded warm/wanted
retirement. Individual placements leave gObjects immediately; shared geometry
references can survive in staged uploads/deferred deletion, and texture caches
intentionally retain assets. No eviction-policy change is made.

`DETAIL COMMIT` reports before/retained/entered/retired/finalLive and a balance
check at both publishers and far-cell retirement. `DETAIL LIFETIME` reports active
cells, visual-ready resident cells (including empty cells), pending staged shapes,
deferred deletions, shared buffer refs and cached textures. `outsideWarmUnwanted`
counts live placements outside the warm window without an explicitly wanted cell.
A transient value at publication can precede the next retirement update; a value
that persists after retirement would support a retention error. Compare returning
to the same cells after warmup. Denser cells and bounded pending overlap can grow
live count without a leak. Runtime health remains unconfirmed until this capture.

## Decision after capture

Report native CPU/GPU and detailed CPU/GPU independently, plus their draws,
vertices, triangles and mode changes. Compare A to B, C and D independently.
Do not call the 45–49 ms detailed-world cost until the split shows that.
If native LOD dominates and C strongly lowers native GPU time, the evidence
supports a dedicated authentic DISTLOD path as a next task. If CPU remains high
with modest GPU time, investigate submission/back-pressure and native draw count.
If detailed world dominates, pursue that measured path instead. No final
optimisation architecture is implemented by this diagnostic pass.
