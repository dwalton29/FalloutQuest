# v231: terrain submission, script state pages and GPU transfers

## Status and scope

Implementation baseline: `24c25e9b0566b7dd3a1f7d50eed7d7eee26b0cd8`, latest
`main` fetched before editing. v231 / `0.90.0-terrain-bounds-and-script-pages`.
Use the **O2 profile APK** for the device comparison; Debug remains available
as the matched compiler control. Both retain diagnostics and the existing
package/signing identity. The source SHA and native configuration appear in
logcat and new CSV captures.

**72 FPS has not been demonstrated. No Quest is connected to this environment.**
The host script improvement below is measured; the GPU/submission improvements
require a new, matched headset capture. There is no post-change headset FPS,
GPU time, or demonstrated remaining frame-time gap to report yet.

## Evidence and measurement corrections

Recovered inputs: `megaton-v224(9).csv` and `megaton-v224-logcat(9).txt`. The CSV contains 1,525 unblocked, submitted
gameplay frames. Its `# v226` label is **not reliable build identification**:
`performance-capture.h` still hardcoded that string in baseline main, including
v229/v230. The filename is not build evidence either. These are the capture
values reproduced in the assignment, not an independently identified v230 O2
test. Do not infer compiler A/B results from this file.

| CPU elapsed metric | Median ms | p90 ms | p99 ms |
|---|---:|---:|---:|
| Frame work excluding xrWaitFrame | 25.147 | 39.399 | 52.838 |
| Start-to-start cadence | 33.420 | 42.421 | 53.070 |
| First eye | 9.677 | 12.537 | 15.115 |
| Second eye | 4.758 | 6.144 | 6.996 |
| Gameplay | 5.362 | 17.527 | 29.418 |
| Actor preparation | 2.441 | 6.615 | 25.650 |
| Scripts | 0.025 | 13.641 | 14.589 |
| Acquire/wait swapchains, both eyes | 0.019 | 0.027 | 1.194 |
| xrEndFrame | 0.249 | 0.395 | 0.565 |

Percentiles above use the sorted lower index; interpolation gives slightly
different last decimals. **Scopes overlap**: do not add gameplay, scripts and
actor preparation, or phase draws to their containing eye totals. CPU elapsed
includes driver-call stalls and thread descheduling; it is not CPU instruction
time. GPU and CPU overlap, too: 25 + 28 is not the frame period.

For process 16341, 33 VrApi samples from 22:03:xx report:

* GPU utilisation median **0.93**, range 0.90–0.95.
* `App` GPU time median **28.00 ms**, range 20.30–32.77 ms.
* FPS median **31/72**, range 24–37/72; battery temperature 36 C.
* CPU/GPU level 4/4, mostly 2208/640 MHz, no power-save indication.
* `Fov=0D`: no active fixed foveation in this capture.

Meta explicitly defines `App` as application GPU time and `GPU%` as GPU
utilisation: https://developers.meta.com/vr/blog/ovr-metrics-tool-vrapi-what-do-these-metrics-mean/
It defines `CPU&GPU` separately; that value is not an independent app GPU timer.
All gameplay eye-timer pairs in this CSV are unavailable; wrapped/disjoint
engine results must not replace the runtime measurements.

The header's `msaa=1` was sampled before scene MSAA allocation and cannot
establish gameplay sample count. The scene uses the existing 2x FP16 HDR path.
New captures identify `format=231` rather than pretending the schema is an
application version, carry source/configuration, and record `sceneSamples` per
frame. The legacy `vertices` column counts NPC prepared vertices; it is not
whole-scene geometry. New `submittedVertices` uses GL submission counters,
including instancing, both eyes and reflection, alongside total draw calls.

## Root causes and implemented corrections

### 1. Terrain visibility metadata was incomplete

`InitializeFo3TerrainRenderQ76` populated bounds, but
`Q2000AdvanceOneBatch` published streamed LAND batches with all six bounds left
at zero. The prior Q20.18 test could reject a complete streamed world window
as though every patch were at the origin. Reverting its rejection avoided
holes but left **all base and paint batches submitted to each eye and mirror**.
The old below-water mirror test also trusted those uninitialised bounds.

Both publication routes now call the same bounds builder on the actual
12-float renderer-space vertex stream. Invalid or unknown bounds draw rather
than disappear. Bounds remain attached through publication, movement and
retirement of each batch.

The old plane-extraction helper is removed. A conservative homogeneous test
transforms all eight expanded AABB corners with the exact matrix sent to the
LAND vertex shader. It rejects only if all corners lie outside one common
OpenGL clip plane, with six metres of spatial expansion and a relative numeric
tolerance. It does not divide by w. It runs separately for each eye and the
actual reflected projection. Base/paint order, vertex alpha, depth policy,
authored meshes/textures and loaded world coverage are unchanged.

Expected benefit: eliminate off-screen LAND draw/state/vertex work in all three
views. This is normal conservative frustum rejection, not reduced draw distance
or omission of visible game content. New `Q231 TERRAIN CLIP` logs retain tested,
drawn and rejected counts. **Actual headset reduction is not yet measured.**

### 2. Script transactions cloned the complete actor census

`Player(..., ScriptCandidateTag)` copied `State` for each event transaction.
After `RegisterOriginalActors`, this included every registered off-screen
actor, even for a quest-local or Karma-only event. Candidates were then
destroyed or swapped, repeating map-node allocation and reclamation across a
single GameMode tick. Compiler optimisation cannot remove required deep copies.

`State::actors` now uses 64 copy-on-write pages. Candidate preparation copies
page ownership, with no actor-record copies. Reads never detach, even through a
mutable State. Explicit edits detach only the affected page. Actor updates,
death and inventory/equipped-weapon invalidation use the isolated mutation path.
Snapshots, failed candidates, commit, rollback and save/restore retain value
semantics. Publishing/rolling back remains allocation-free.

This does not change script cadence, owner order, event filters, conditions,
world-state reservation, provenance, error handling or any AI scheduling.
Other State containers still copy: this is not a claim to have removed every
script cost.

### 3. Avoidable GPU transfers and synchronisation

* Remove the diagnostic-only synchronous `glReadPixels` from HDR history.
  It forced completion of preceding world/resolve work at first use and every
  144 updates; no gameplay or exposure algorithm depended on the CPU value.
* Make the pre-water colour+depth resolve conditional on the same existing
  conservative exposed-water/frustum decision used for reflections. An empty
  water view no longer incurs this intermediate full-resolution resolve.
* Invalidate multisample colour/depth storage **after the final resolve only**;
  preserve it after the earlier water snapshot because water/HUD still draw.
  Preserve the resolved colour/depth textures used by post and the compositor.
* Discard mirror depth after its final use; preserve mirror colour.

No shader, texture, MSAA, resolution, water, bloom, gamma or final-film quality
setting changes. At 1680x1760, one FP16 colour + D24 resolve has approximately
35.5 MB of logical destination data per eye before driver compression/tile
optimisation. Removing an unnecessary stereo resolve avoids ~71 MB of logical
destination writes plus source reads. This is a workload calculation, not a
measured memory-bus or GPU-time saving.

## Host measurements

Same x86 host, g++ 13.3 `-O2`, original uploaded ESM, separate unmodified-main
and changed executables. Each run warms 20 iterations and measures 200. Three
alternating runs produced these median ranges:

| Workload | Baseline | Changed |
|---|---:|---:|
| Transaction + commit + old-state destruction, original 619 actors | 44.1–46.1 us | 1.28–1.31 us |
| Original GameMode tick, 73 Megaton references | 1.03–1.18 ms | 0.254–0.285 ms |

The GameMode harness uses original scripts/default state, a fixed cell/world
and position, and a rejecting world-reservation callback. It is a repeatable
host workload, **not the user's headset save or a gameplay capture**. The result
demonstrates reduced transaction overhead without reducing tick frequency.
Do not extrapolate its factor to overall Quest FPS or to all 13.6 ms spikes.
Reproduce with `transaction_benchmark <outside-repository Fallout3.esm>` from
the host player CMake project. Without an ESM argument it uses 10,000 synthetic
actor records as a scaling test (not original-game evidence).

## Regression results and audit limits

Locally passed synthetic transaction/reference-plan tests, script scheduling,
Player state and weapons; original ESM tests for MS11 acceptance through Lucas,
bomb activation/cancel/disarm, nested quests, MQ05 fuse-box Enable/Disable,
provenance, publication/rollback, merchant barter including Moira, theft/Karma,
locks/XP and save/restore. The page test verifies no copies during reads,
bounded page detachment, snapshot isolation, commit-style swap/rollback and
move/clear/iteration. Existing allocation-forbidden transaction checks pass.

The terrain test compares 20,000 triangles against an independent six-plane
polygon-clipping oracle: no triangle capable of producing a raster contribution
is rejected. It covers asymmetric OpenXR GLES projection, rotation, large
coordinates, near/far clipping and mirror views; also unknown/invalid bounds
and the exact 12-float publication layout. This is mathematical host coverage,
not a replacement for stereo headset inspection.

| Area inspected | Finding / remaining work |
|---|---|
| Stereo architecture | Existing batches, poses and midpoint reflection are shared; main draw submission and post remain sequential per eye. First eye includes shared preparation/reflection. Their cost difference is not evidence of left-eye-only GPU difficulty. |
| Driver/OpenXR | Acquire/wait and end-frame small in capture. High GPU occupancy is verified, but the split of eye CPU time into useful submission vs driver blocking requires a GPU/CPU stage trace; no invented attribution. xrWaitFrame pacing is outside workUs. |
| GPU pipeline | FP16 HDR, 2x scene MSAA, water scene snapshot, mirror, bloom and compositor depth copies remain substantial work. Main rendering is not direct-to-swapchain. |
| NPCs | GPU skinning and once-per-stereo-frame pose/palette preparation retained. Arrival decoding/expanded streams run on worker; GL uploads can still stall render thread. No population, animation or AI reductions. |
| Scripts | Full-census copy fixed; source interpretation, provenance, remaining State copies, and live world publication remain. Device spike attribution after this fix is still needed. |
| Loading/streaming | Existing private worker preparation and main-thread GPU publication preserved. No new thread reads or modifies a live Player. Transition/load stalls must be separated from sustained gameplay. |
| Hardware policy | 72Hz and high runtime CPU/GPU levels already observed; raising them blindly is not a solution. No hard affinity or untested scheduling policy added. Thermal sustainability is unverified. |
| Foveation / multiview | Not enabled by this change. Foveating only the final XR target would not foveate the expensive offscreen scene. Proper multiview needs compatible layered HDR/water/depth/UI paths and stereo validation; the reverted experiment is not restored. |

## Headset acceptance and remaining gap

Install the profile APK over the existing package without clearing data.
Record the source/config marker. Compare against baseline Profile with the same
save, viewpoint, resident population, refresh rate, resolution/MSAA, activity and
thermal conditions. Capture at least 60 seconds after load/arrivals settle,
and separately capture combat, arrivals, door/interior transitions and loading.

Compare median/p90/p99 work and cadence, Scripts >10ms counts, actor arrival
stalls, eye costs, draws/submittedVertices, `Q231 TERRAIN CLIP` counts, and
VrApi App GPU time/GPU utilisation/FPS/Stale. Engine GPU=-1 remains missing,
not zero. Check both-eye LAND/paint through full head rotations, near-ground
views, slopes and streamed-cell boundaries; verify water/mirror, HDR adaptation,
UI/depth, combat, dialogue, quests and save/reload.

The baseline is about **11.26 ms above the CPU budget** and **14.11 ms above
the runtime GPU budget**, before scheduling headroom. No post-change device
gap is known. If GPU time remains dominant, capture render-stage timings
before selecting the next architecture change; do not assume the compiler or
this CPU benchmark solves the GPU budget. An APK that builds is not proof of
72Hz or of zero visual regressions on hardware.

Capture SHA-256 (CSV): `2959ab3ad66324b7696749e8ed5b26d3188bc6cfbe13257cdf3e5f3f911bc56c`.

Capture SHA-256 (logcat): `f36c561531c9a3466761740afd17a63014c87f3a8451c5157d82d5bafb6117b3`.
