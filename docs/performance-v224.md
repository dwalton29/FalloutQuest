# v224 — measured Megaton routing stalls

Baseline: v223, commit `88445f57d1430d2d94349d889a47fb699b5156a3`.
Build: `224 / 0.83.0-npc-xtel-route-stalls`.

## Actual headset evidence

Inputs: `megaton-v223.csv`, `megaton-v223-logcat.txt`, `megaton-metadata.txt`.
Quest 3S, 1680×1760 per eye, 2× MSAA, render scale 1.00, 72 Hz.
383 submitted gameplay frames, seven active resident NPCs, no combat, same
Megaton cell/world throughout. Three rows marked streaming. Cadence covers
42.799 seconds between captured frame starts; reciprocal mean cadence is
**8.93 application frames/s**, not the configured 72 Hz display rate.
No v222 populated-Megaton comparison is available; these are v223 measurements.

| Scope | Mean ms | Median ms | P95 ms | P99 ms | Max ms |
|---|---:|---:|---:|---:|---:|
| Complete CPU work after XR wait | 111.351 | 39.458 | 438.631 | 456.668 | 466.903 |
| Application cadence | 112.040 | 41.239 | 438.697 | 456.749 | 466.947 |
| Gameplay / scripts / input interactions | 36.906 | 6.247 | 100.926 | 121.670 | 137.198 |
| Actor preparation | 55.942 | 11.964 | 323.727 | 341.733 | 352.829 |
| NPC route work | 41.060 | 0 | 291.064 | 295.454 | 296.592 |
| NPC simulation, including perception children | 22.782 | 0.072 | 156.612 | 157.874 | 161.040 |
| NPC packages | 27.516 | 6.640 | 163.599 | 169.203 | 176.676 |
| First-eye CPU | 11.524 | 10.349 | 21.682 | 30.627 | 38.348 |
| Second-eye CPU | 5.691 | 5.710 | 6.913 | 7.380 | 7.563 |
| Detailed-world preparation | 0.877 | 0.846 | 1.223 | 1.387 | 1.500 |
| Streaming | 0.079 | 0.065 | 0.117 | 0.282 | 0.716 |
| xrEndFrame CPU | 0.362 | 0.346 | 0.563 | 0.644 | 0.842 |

All 383 CPU-work samples exceed both the 13.889 ms and 11.111 ms budgets.
134 frames exceed 100 ms; 57 exceed 300 ms. The 304 frames with no A* queries
average 54.558 ms of CPU work; the 79 with queries average 329.898 ms.
The final 60 frames average 399.776 ms, with route work averaging 250.141 ms.
These scopes overlap: route costs are children of simulation/package work;
do not add them to those parents or to actor preparation.

Heavy frames perform 60–93 A* searches and up to 44,548 node expansions.
Detailed-world rendering averages 2.184 + 1.134 ms across eyes, versus the
hundreds of milliseconds in NPC route bursts. Persistence averages 0.105 ms,
arrivals 0.003 ms; neither explains these stalls. NPC animation averages
6.024 ms (including its palette/upload/facial children); occasional facial
updates reach 27.358 ms and remain a secondary profiling target.
Metadata reports Thermal Status 0 before and after, with CPU/GPU HAL readings
around 52–56°C / 49–53°C. This capture does not establish a thermal bottleneck.

### Invalid GPU measurements

All 380 completed per-eye GPU pairs contain impossible unsigned-wrapped
elapsed results near `UINT64_MAX`; the last three pairs are unavailable.
That is 1,140 invalid values across the two eyes and their sum, plus nine
missing values. They are **not valid GPU frame times** and cannot rank GPU
cost. The updated summarizer rejects them in historical v223 captures too.
Runtime VrApi lines provide separate observations in logcat, but are not
substituted for matched per-frame GPU measurements from this CSV.

## Runtime changes

1. **Remove discarded A* routes from door eligibility.** Previously
   `Q240NpcRemoteDoor` ran the full crowd-aware A* solver for every eligible
   XTEL door before ranking candidates. The route vectors were discarded.
   It now uses one directed traversal of the same reciprocal/validated NAVM
   portals to identify reachable triangles. The same nearest-surface limits
   (350/500 game units), terminal-ground projection and 96-unit approach
   tolerance apply. Candidates keep the old fewest-CELL-hops / nearest-door
   ranking and original tie order. Actual movement still builds the unchanged
   crowd-aware A* route when needed; no navigation/AI cadence is throttled.
2. **Preserve travel intent during perception.** The perception policy query
   called package selection, which cleared/replaced `xtelDoor`/`xtelCell` even
   while package execution reused a cached selection. Perception now restores
   that intent; package execution owns adopting a movement target.
3. **Accept doors owned by the resident exterior scene.** The selector handled
   Megaton grid-cell doors, but traversal required their source CELL to equal
   the persistent loading CELL. It rejected valid doors and repeatedly retried
   authored schedules. Traversal now uses the existing `SceneOwnsCell` rule,
   preserving the narrow Megaton residency semantics and live lock/key checks.
   The log's repeated Burke `XTEL link no longer valid` failures match this
   source bug. NPCs can now complete their original interior-bound schedules;
   this is not an NPC population reduction for benchmarking.
4. **Keep immediate and final CELL destinations distinct.** A multi-door
   package validates/transfers through the first original XTEL destination,
   while its final target remains in the original package definition. It no
   longer mistakes a valid intermediate hop for an invalid direct transfer.
5. **Repair GPU capture boundaries and reject invalid samples.** Elapsed eye
   queries now begin after OpenXR swapchain acquisition, end before release,
   and are flushed with the application GL work. They no longer span runtime
   swapchain/context work. Unsigned-wrapped negative results are counted as
   `gpuInvalid` and remain absent (`-1`) in CSV; no absolute-value repair or
   invented timing is applied. Availability/disjoint checks remain nonblocking.
   A fresh device capture must confirm whether this boundary change yields
   valid measurements on the Quest driver.

Further CPU attribution adds `scriptsUs`, `playerMotionUs`, `interactionUs`.
The 36.906 ms mean broad gameplay scope still needs this separation before
changing script transaction/persistence semantics or collision behaviour.
`npcPathUs` now includes door eligibility traversal as well as actual A* work;
`paths`/`expanded` continue to count actual A* calls/node expansions.

## Correctness and benchmark validation

Synthetic tests exercise ranked reachable doors, an unreachable preferred door,
locks, persistent/grid CELL ownership, rejection of another world, and a
multi-hop package. Eligibility builds no A* routes; actual handoff still uses
the original movement and canonical actor persistence path. A combat regression
checks that perception leaves active XTEL travel intent intact. GPU timer tests
inject a wrapped result and verify that it never becomes a valid timing sample.

An optional original-file test compares all selected doors and approach points
against the v223 selector on original Megaton NAVM/XTEL records. It also tests
handoff through the chosen original doors. Run it with:

```sh
/path/to/host-player/npc_xtel_runtime_tests /path/to/Fallout3.esm
```

Validation completed:

- All 36 host player tests passed.
- Native rendering/timer/capture and rendering Python checks passed.
- Original Megaton records: 54 identical door/approach choices and 36 successful
  original-door handoffs. Discarded eligibility A* queries fell from 810 to 0.
- The original-data host benchmark took 1,057.17 ms with the v223 selector and
  294.222 ms with v224 (72.2% less selection CPU time in this run).
- `git diff --check` passed.

These are CPU/algorithm measurements on original data, not measured v224 Quest
FPS. Log resident totals include inactive vector slots; the CSV's seven actors
are the active resident count for this capture.

## Remaining limits and next capture

This pass targets the measured route storm and its retry causes. It does not
prove stable 72 FPS: even no-query frames and the combined eye CPU envelope
still exceed the budget, and the broad gameplay scope has substantial periodic
cost. GPU headroom and 90 FPS feasibility remain unmeasured.

Install v224 over v223, keep the same save, weather, hour and camera, then use
the [v223 ADB procedure](performance-v223.md) with a fresh token, for example:

```sh
adb shell setprop debug.falloutquest.perf_capture 22401
# Play the same populated Megaton scenario for 30 seconds.
adb shell setprop debug.falloutquest.perf_capture 0
# Wait two seconds while the app remains open.
adb exec-out run-as com.falloutquest.app cat files/performance.csv > megaton-v224.csv
adb logcat -d -v threadtime > megaton-v224-logcat.txt
python3 tools/performance/summarize_capture.py megaton-v223.csv megaton-v224.csv
```

Check that Burke and other residents actually approach/traverse their scheduled
doors; check combat interruptions, conversations/facial animation, furniture,
interior/exterior transitions and saved actor state. Compare route eligibility
cost, A* counts, P95/P99 cadence, broad gameplay's new subscopes and valid GPU
sample counts. Keep the original resolution/MSAA/content settings.
