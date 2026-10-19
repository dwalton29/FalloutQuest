# v223 — stereo CPU work and NPC performance

Version: `223 / 0.82.0-stereo-cpu-and-npc-profiling`.
Baseline inspected: `767cd8255678ba2ec1a2447f51a9602dc5192573` (v222).

## Assessment and evidence

No Quest or ADB device is attached to this environment. There is no measured
before/after Quest FPS, populated-Megaton cost, GPU bottleneck ranking, thermal
result, or evidence of stable 72/90 FPS in this change. The reported 10–14 FPS
Wasteland capture comes from the task description; it is not an NPC benchmark.
72 Hz requires 13.889 ms; 90 Hz requires 11.111 ms of actual application cadence.

Source audit identified these concrete redundant operations:

| Area | Before | v223 | What stays live |
|---|---|---|---|
| AABB visibility | Extract/normalise six planes for every bound test; stereo installs/restores whole frustum scopes per object | Normalise at camera preparation; stereo union shares two frustums, including same-frame camera invalidation | Every bound test, padding, moving door bounds, each eye's view |
| Opaque order | Re-sort all opaque resident objects every stereo frame | Retain material-key order until visual epoch/storage/count changes; exhausted epoch disables reuse | Visibility, collection state, LOD decisions, batching and matrices still rebuild per frame |
| NPC visibility | Copy `GpuObject`, including strings/vector/shared ownership, just to translate six floats | Copy only the six bound coordinates | Every resident's simulation and current visibility |
| GPU palettes | Upload unchanged palettes each update | Exact byte comparison; suppress only identical uploads | Every changed root/bone/direction bit; first upload and texture recreation; CPU fallback and facial geometry |

A seeded host test compares 225,000 cases against the original visibility code,
including degenerate matrices, invalid bounds and normal/reflection padding.
A host run of 600,000 bound tests measured **28.448 ms before / 15.555 ms after**.
This is x86 CPU microbenchmark evidence, not frame time or a Quest FPS forecast.
The benchmark prints fresh values in CI; host load affects the absolute result.

Draw calls, triangles and mesh detail are intentionally unchanged. Ordinary GL
uniform/material operations are unchanged. An identical palette avoids one
`glTexSubImage2D` plus unnecessary palette binding/active-unit transitions;
`paletteSkipped` and actual `paletteBytes` report whether this helps real scenes.
Changed palettes require a small retained CPU copy, so net device benefit still
needs measurement. No approximate pose equality, frozen animation, reduced NPC
population, resolution change, draw-distance change or multiview was introduced.

v222's Actions run 39284575277 failed **before APK compilation**. Its new source
test sliced from UploadCpuObject through InitializeScene, then rejected the
allowed initial scene emittance scan. The test now stops at the upload
function's own outer brace. v222 terrain decode/XEMI runtime fixes are preserved.

## Capture design

Debug APK only. A new nonzero `debug.falloutquest.perf_capture` token arms one
session. At most 4096 frames are held in fixed memory. Changing the token,
setting zero, reaching capacity, or normal renderer shutdown writes
`files/performance.csv`. It replaces the previous capture; copy it before the
next one. Control is checked at most once per second. File formatting and I/O
occur outside recorded frame scopes, once at stop; those stop/write frames
are unsuitable as a gameplay benchmark. Force-stopping before stop loses the
in-memory capture. GPU completions in the final few frames may remain absent.

The capture has no per-frame logcat/file writes, flushes, mutexes or dynamic
sample allocations. CPU scope timing is dormant when capture is off. The
existing low-frequency diagnostics remain available. `NPC AI PERF` now also
reports simulation/combat, persistence, unloaded scheduling and arrival costs;
previously its only timed field was package execution. Actor counters reset at
the logical frame boundary, so earlier interaction/perception sampling is no
longer erased when actor preparation starts.

CSV fields:

- XR wait/begin/end CPU time, work after wait through frame reporting, cadence between work
  starts, each eye's CPU time and swapchain acquisition/wait time. Cadence zero
  on the first sample is deliberately excluded from summaries.
- Controller sync and view tracking, gameplay/scripts/interactions, streaming,
  actor preparation and detailed-world batch preparation.
- NPC simulation/combat, packages, persistence, unloaded simulation, arrivals,
  target lookup, sight/occlusion, common NAVM path solver, movement and weapon
  choice. These parent/child scopes overlap: do not sum them. Perception can
  sample animation; the simulation scope therefore includes that child work.
- Animation update envelope, clip sampling, skeleton sampling, facial updates,
  palette generation and uploads. Existing PLAYER/NPC PERF retains CPU fallback
  skinning, directions, vertex/VBO bytes and per-phase delayed GPU diagnostics.
- Resident/visible/unique animated/combat actor counts, resident rig parts,
  palette part updates, prepared CPU/GPU vertices, palette bytes, skipped
  unchanged palette uploads, path queries/expansions, NPC draw calls (both eyes,
  opaque and alpha), all-frame GL draw/state/uniform/texture counts.
- Per-eye native-LOD/detailed/NPC/player opaque CPU costs. These are current
  render-phase samples, not additional work to add to each eye's total.
- Asynchronous per-eye elapsed GPU queries and their matched sum. They include
  that eye's sky/world/alpha/terrain/reflection/post/UI commands, but **exclude
  GPU preparation/uploads before the eyes**. They are not full application GPU
  frame time. Loading preparation retains its separate existing GPU timer.
  Disjoint, unavailable or dropped samples remain `-1`, never zero. Results are
  read only after availability and at least three later frames. Query owners
  share disjoint invalidation; phase GPU queries yield while an eye query owns
  the elapsed target. With capture off, original per-phase GPU queries resume.

OpenXR core exposes no compositor dropped-frame/reprojection counters here.
`submitted=1` means xrEndFrame accepted a projection layer; it does not prove
presentation. The summarizer labels cadence-derived missed display intervals
as **estimates**, not compositor counters. Use runtime/OVR Metrics tooling for
actual app/compositor FPS, reprojection, thermal clocks and full GPU frame time.
Fine-grained GPU NPC/weapon/alpha attribution requires a subsequent device GPU
capture; do not interpret the sum of opaque phase means as whole-frame GPU time.

The header records headset model, actual eye sizes, active MSAA, render scale,
CPU-skin and material/LOD diagnostic options. Period is recorded per row so
refresh changes do not silently mix budgets. Thermal state is collected below.

## Exact ADB capture

Install the Actions `FalloutQuest-debug` APK. Start it normally, load the desired
save/scenario and wait for initial streaming to settle for the settled run.
From the repository checkout:

```sh
adb logcat -c
adb shell getprop ro.product.model > megaton-metadata.txt
adb shell dumpsys thermalservice >> megaton-metadata.txt
adb shell dumpsys battery >> megaton-metadata.txt
adb shell setprop debug.falloutquest.perf_capture 22301
```

Play the scenario for 30 seconds. Then stop the capture **without closing the
app**; allow two seconds for the next control check:

```sh
adb shell setprop debug.falloutquest.perf_capture 0
adb logcat -d -s FQPerf
adb exec-out run-as com.falloutquest.app cat files/performance.csv > megaton-v223.csv
adb shell dumpsys thermalservice >> megaton-metadata.txt
adb logcat -d -v threadtime > megaton-v223-logcat.txt
python3 tools/performance/summarize_capture.py megaton-v223.csv
```

Use a different positive token (22302, 22303, ...) for every session. For loading
or streaming captures, arm before crossing the door/world boundary. Preserve
both settled and streaming files. Record save, location, view direction, game
hour, weather, headset OS, battery/power state and runtime metrics with them.
The summary prints mean, median, P95, P99, maximum and the percent over the
72/90 Hz budgets separately for work, cadence, delayed GPU and subsystem scopes.
It separates loading, ordinary gameplay, NPC-light/present, combat and streaming;
use one controlled scene per file rather than treating those automatic groups
as matched experiments. Compare multiple files by passing them together.

## Reproducible device scenarios (not run here)

Keep save, weather, game hour, camera, headset, refresh, MSAA and resolution equal.
Capture 30 seconds settled, and another 30 seconds including streaming where
applicable. Repeat each after the same warmup; record thermal state at both ends.

| Scenario | Repeatable action | Correctness check |
|---|---|---|
| Empty Wasteland, stationary | Same save and landmark view, no movement | Original distant detail and terrain |
| Wasteland motion | Walk the same route and turn at the same markers | Both eye edges; no streaming stalls |
| Megaton schedules | Same daytime save near entrance, resident schedules active | Original identities/count, furniture and movement |
| Megaton group view | Same position facing the bomb/NPC group | Smooth original KF, no frozen/disappearing actors |
| Interior NPCs | Same shop/save, fixed camera | Original lights/materials/equipment |
| Conversation | Same speaker/topic sequence | Eyes, mouth, lip movement and voice |
| Multi-NPC combat | Restore same save, attack same actor with same weapon | Pursuit, aggression, fleeing, NPC weapons and death |
| Exterior NPC streaming | Same route with resident actors present | No duplicate arrivals; simulation continues |
| Repeated doors | Same interior/exterior door three times | v218–222 loading presentation, collision/grounding, saved damage |

No NPC-disable switch was added: compare naturally populated/light scenes and
read their recorded counts. Inspect CPU fallback mode separately, if explicitly
requested for diagnosis, then restore its property before normal measurement.

## Next decisions after a headset capture

1. Rank cadence/work/eye/streaming P95/P99 before assigning CPU or GPU blame.
   Empty-world detailed preparation is a proven source of redundant CPU
   operations, but its percentage of Quest frame cost remains unmeasured here.
2. Compare NPC simulation/packages/paths against animation/palette/upload/draw
   under matched settled Megaton and combat views. Perception still scans live
   actors and can request current poses; reducing those decisions or changing
   their order needs evidence and gameplay tests, not arbitrary throttles.
3. Inspect arrival and streaming spikes; workers already prepare assets and
   arrivals admit bounded parts. A single original mesh/texture upload can still
   exceed a frame; splitting publication needs device timings and generation tests.
4. If render submission/GPU dominates after CPU savings, use a Quest GPU capture
   for material/instance/alpha/reflection work. Existing instancing and main-eye
   batch reuse already operate; changing stereo architecture needs correctness
   evidence. Do not restore an abandoned multiview path speculatively.

Stable 72 FPS is **unverified**. 90 FPS feasibility is **unknown** until populated
scenes show <=11.111 ms CPU/GPU headroom and stable cadence under thermal load.

## Host validation for this pass

- Assets, world residency/preparation/publication, collision, NIF, audio and
  player/NPC suites pass: 76 registered CTest cases, with three original-asset
  fixture cases skipped by their default invocation (73 executed).
- Separately ran original ESM NPC-record resolution, original ESM combat
  policies (161 Defensive packages / 17 CNAM combat styles), and strict
  complete/partial decode checks on supplied `megatoncommonhouse.nif`.
- GL state, asynchronous query/disjoint/ring ownership, stereo/reflection,
  palette mapping/publication, 225,000 frustum equivalence cases, capture
  bounds/schema/percentiles and Steam install-discovery checks pass.
- All render Python checks pass, including production GLES GPU-skinning
  transform-feedback parity, shader validation, loading isolation, actor scope,
  notifications, materials and lighting. No on-device visual/behaviour run was
  possible here; use the scenario table before asserting fidelity or FPS gains.
