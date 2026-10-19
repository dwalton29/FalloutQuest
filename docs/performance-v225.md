# v225 — measured script tick CPU stalls

Baseline: v224, `e51cdb39f8291dc943532c8dfb50c6f7ff5039c9`.
Build: `225 / 0.84.0-script-tick-compilation-cache`.

## What the v224 headset capture establishes

Inputs: `megaton-v224.csv` and `megaton-v224-logcat.txt`, compared with the
previous v223 capture. The CSV contains 3,951 frames: 2,004 loading, 1,788
unblocked/submitted gameplay, and 159 blocked gameplay. All unblocked gameplay
samples are in Megaton persistent CELL 0xA96 / world 0xA74.

This is not a controlled whole-frame A/B: v223 had seven active residents and
2x MSAA; v224 has two to six active residents (mean 4.3) and reports 1x MSAA.
The v224 capture includes startup and a blocked interval. No diagnostic render
scale, CPU-skinning or simplified-material switch is enabled in either header.
The v225 changes do not change resolution, MSAA, authored assets or population.

| Gameplay CPU scope | v223 mean ms | v224 mean ms | v224 P95 ms | v224 max ms |
|---|---:|---:|---:|---:|
| Complete work after XR wait | 111.351 | 55.218 | 129.609 | 2,163.855 |
| Actor preparation | 55.942 | 10.979 | 28.297 | 551.613 |
| NPC route work | 41.060 | 0.485 | 4.378 | 17.826 |
| NPC simulation | 22.782 | 0.340 | 1.915 | 10.052 |
| NPC packages | 27.516 | 4.070 | 9.160 | 16.213 |
| Broad gameplay | 36.906 | 25.816 | 100.080 | 1,752.488 |
| Scripts, newly separated | unavailable | 20.632 | 96.350 | 101.170 |
| Player motion, newly separated | unavailable | 0.200 | 0.688 | 8.013 |
| Interaction, newly separated | unavailable | 3.173 | 4.159 | 28.169 |
| First eye CPU | 11.524 | 11.496 | 19.909 | 204.581 |
| Second eye CPU | 5.691 | 5.433 | 6.786 | 8.699 |

Scopes overlap and must not be added together. The v224 route scope includes
eligibility traversal as well as actual A*; the older scope only counted A*.
Actual A* calls peak at three per gameplay frame, versus 93 in v223. The
previous hundreds-of-milliseconds route bursts are absent in this capture.

Of the 1,788 gameplay frames, 386 spend more than 20 ms in the scripts scope.
Those script updates average **95.475 ms**; other frames average 0.026 ms in
that scope. Their complete CPU work averages 132.839 ms versus 33.848 ms on
frames without the script spike. This identifies a regular CPU stall at the
existing 4 Hz source scheduler cadence, rather than a player-motion cost.

### Cadence must not inherit loading gaps

Cadence stored on row N spans frame starts N-1 to N. The first gameplay row
therefore contains the preceding loading frame's 22.220-second work gap. The
first row after the blocked interval also crosses a different state. Selecting
only by the current row incorrectly attributes these intervals to gameplay.
The summarizer now includes cadence only when both adjacent, consecutive rows
belong to the reported group. It does not remove long frames within a group.

The 1,786 comparable gameplay intervals average **60.276 ms**, or **16.59
application FPS**. The two uninterrupted gameplay sections independently yield
16.21 and 16.66 FPS when their entering boundary intervals are excluded. v223's
single uninterrupted capture was 8.93 FPS. Different MSAA, NPC counts and
scenario phases prevent attributing the full FPS difference to v224's code.
Neither capture establishes stable 72 Hz application rendering.

### Gameplay GPU queries remain invalid

`CAPTURE_END` reports `gpuInvalid=3888`, `gpuDisjoint=0`, `gpuDropped=0`.
The 2,004 loading frames have completed GPU eye pairs; **none of the 1,788
unblocked gameplay frames has a valid pair**. The v224 boundary change did not
resolve the gameplay driver-query problem. Rejected values remain absent;
loading GPU numbers cannot establish gameplay GPU headroom. The summarizer
now explicitly reports completed/unavailable GPU pairs per group.

## Changes in v225

1. Cache immutable command compilation, including identical compiler failures,
   within a Player's frozen catalogue. The key contains exact source text,
   owner, local-script identity, capture mode/budget and compiled-only status.
   No mutable expression value, branch result, script effect or transaction is
   cached. Expressions and evidence checks still execute on each candidate.
2. Before creating a full GameMode transaction candidate, check the first
   matching original event block with the ordinary compiler. An identical
   unsupported command, malformed event or unresolved filter fails with the
   same diagnostic and consumes delivered reference input as before, without
   copying all saved actor state first. Later blocks retain normal execution
   ordering and rollback. The 4 Hz cadence and quest/reference order remain.
3. Share the bounded compiler cache across detached candidates for the same
   immutable catalogue. Retain at most 256 entries and a conservative 2 MiB
   allocation estimate. An oversized result is used without retention; held
   immutable programs survive cache eviction during nested execution.
4. Correct cross-state cadence attribution and report GPU availability for each
   capture group. Preserve the raw CSV, all CPU samples and long within-group
   cadence intervals.

## Validation

A host benchmark using the original ESM and the same 619 registered actors
runs 32 quarter-second GameMode updates without resident world publication.
The v224 executable takes **1,135.84 ms**; v225 takes **168.559 ms**, an **85.2%**
reduction in this host run, including cold compilation. These are host CPU
measurements, not v225 Quest frame times. They do not prove that unsupported
original scripts have become executable.

Regression coverage checks live quest gates after cache hits, repeated effects,
transaction rollback and a failed inventory command becoming valid after an
inventory change. It also checks source/owner/local-script/capture-budget
separation, catalogue separation, compiled-only errors, bounded eviction and
held-program lifetime. Existing GameMode world reservation, input consumption,
provenance and persistence tests remain in place.

Original-file validation compares cached and ordinary compiler outcomes,
commands and captured bindings for original event bodies, in both plain and
captured modes. It also exercises original bomb/menu/disarm/persistence paths.
Use:

```sh
/path/to/host-player/event_script_tests /path/to/Fallout3.esm /tmp/script-inventory.json
/path/to/host-player/script_transaction_tests /path/to/Fallout3.esm
```

Completed validation:

- All 36 host player tests pass.
- 4,106 original event-body checks match ordinary compiler results, commands
  and evidence bindings (2,053 blocks in plain/captured modes).
- Original ESM bomb activation, cancel, disarm, Karma, notices and persistence
  checks pass. The old fixture's Enable diagnostic expectation was stale:
  the isolated v224 baseline fails the same assertion because the authored
  stage reaches unsupported Burke MoveToMarker first. The fixture now checks
  that actual rejection and preserves its atomicity assertions.
- The isolated v224 baseline also fails the old MQ05 single-root fixture:
  FuseToggle has eight original XESP children. That fixture now recovers all
  nine affected references from the original enable-parent records, retaining
  exact projection, evidence, inventory and publication assertions. The full
  original transaction run passes, including MS11 provenance and MQ05 fuse
  activation, original-master evidence matching, consumption and publication.
- Capture native checks and all five capture-report Python tests pass,
  including rejection of wrapped GPU results and exclusion of cross-state
  cadence intervals.
- `git diff --check` passes.

## Remaining work and next headset capture

This pass targets the regular measured script stall. Other captured stalls
remain: actor arrivals reach 276.911 ms, actor preparation reaches 551.613 ms,
the first gameplay work sample reaches 2.164 seconds, and the last loading
frame reaches 22.220 seconds. The available log starts after the loading work;
it does not contain that frame's detailed loading-stage diagnostics. Separate
loading-profile output is needed to attribute it accurately. These samples
are retained, not concealed by the cadence correction.

Install v225, use the same save/camera/weather/time and MSAA setting, allow
Megaton and residents to settle, then start a fresh capture token **22501**
using the [ADB procedure](performance-v223.md). Keep this capture focused on
unblocked gameplay; collect loading-profile output separately for launch stalls.
Compare script P95/maximum and complete work/cadence; check quest updates,
NPC schedules/doors, conversations, scripted interactions and saved state.
GPU gameplay conclusions remain unavailable until valid measurements exist.
