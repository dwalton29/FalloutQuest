# v232: timestamp GPU pass capture

This build instruments the renderer; it does not claim a GPU speedup or 72 FPS.
The delivered profile APK also enables the existing native -O2 configuration.
It retains Android debuggability, FQPerf and the same app ID/signing key.

## Why this change

The fresh v231 session 23102 contains 2,132 gameplay frames, source
`a4f9995eef5ebd9e3df1b09cbc9a38c15a2ce892`, `native=debug-default`,
1680x1760 per eye and 2x scene MSAA. Median CPU work is 16.901 ms,
median cadence 29.669 ms. The previous capture has 25.147/33.420 ms,
but resident counts differ (2 vs 3); this is not a controlled performance A/B.
The v231 capture saved successfully but rejected 4,258 elapsed-query samples.
The device log's GPU utilisation is 93–98%; it cannot identify costly passes.

## Instrumentation

New captures use independent `glQueryCounterEXT(..., GL_TIMESTAMP_EXT)` endpoint
pairs, rather than the failing elapsed-query backend. Timestamp counter support
and bit width are checked explicitly. An unsupported implementation reports
`gpuSupported=0`, `timestampBits=0`; it does not fabricate durations or silently
reuse the known-bad elapsed backend. Device validation is still required.

A fixed ring of 96 pairs allows nested eye/pass intervals. Both endpoints must
be available, and at least three later frame serials must have elapsed before
results are read. No glFinish, synchronous timestamp read, or forced wait is
introduced. Query slots are never reused before completion; ring pressure drops
new measurements. Shared disjoint epochs invalidate outstanding intervals even
when another timer owner consumed the GL disjoint flag. Width-aware timestamp
subtraction handles a short counter wrap; out-of-width, backwards/implausibly
long intervals remain missing. Capture-end counters include valid, invalid,
zero and first-invalid raw endpoint values to diagnose further driver failures.

CPU frames are recorded continuously; GPU queries are issued every eighth
frame to limit disturbance. GPU queries themselves may perturb tile scheduling:
compare sampled and unsampled CPU/cadence before treating a timing run as an FPS
benchmark. Final pending query pairs may remain missing when capture stops.

CSV format 232 appends per-eye GPU microseconds for native LOD, detailed opaque
world, opaque NPC/player, alpha, environment, main terrain, reflection,
water, MSAA resolves, exposure, post, bloom, compositor depth copy, sky and
mirror terrain. Missing/unsampled/not-executed remains -1, never zero.
Repeated instances (e.g. two resolves) sum within the same frame/eye/pass,
but only export when every expected instance completed successfully; a partial
aggregate stays missing.
Post includes bloom; reflection includes mirror terrain. Eye totals contain
these scopes and unlabelled HUD/setup work. Do not sum overlapping scopes.
A cached reflection emits no reflection/mirror interval. Each completion carries
the originating frame serial and eye, not the frame where it became available.
Old standalone opaque/loading diagnostics retain their existing backend; during
a bounded capture the old opaque elapsed queries are suppressed. Use the new
CSV GPU columns, not the legacy opaque GPU means, to assess this experiment.

Original shaders, assets, resolution, MSAA, draw distances, geometry, render
ordering and gameplay behaviour are unchanged.

## Windows capture

Install the O2 profile APK with `adb install -r <apk>` (no uninstall/data reset).
Load Megaton, keep the headset awake and the game focused, then from the repo:

```powershell
powershell -ExecutionPolicy Bypass -File tools/performance/capture-quest.ps1
```

The script requires v232 O2 by default (`-AllowDebug` is an explicit comparison
option), assigns a unique session token, observes CAPTURE_START, waits 60 seconds,
stops, waits for CAPTURE_END/saved=1, and verifies CSV schema/session/native build.
It saves unique CSV and logcat filenames with UTF-8 encoding. A failed start,
failed save or stale file raises an error instead of exporting the previous CSV.
Use `-Seconds 60 -OutputDirectory .` to specify duration/output.
If GPU support is unavailable, or invalid/zero samples still dominate, retain the
log and use a device GPU profiler; do not infer pass costs from CPU timings.

Reference: Khronos EXT_disjoint_timer_query specification:
https://registry.khronos.org/OpenGL/extensions/EXT/EXT_disjoint_timer_query.txt

## Validation

Host tests exercise independent nested timestamps, endpoint readiness, delayed
reads, shared disjoint invalidation, counter wrap, invalid values, exhausted ring,
shutdown, session attribution, per-eye isolation and repeated-pass accumulation.
Existing loading/render dispatch and capture tests pass locally. Native compiler
flag validation runs in Android CI after both build variants exist; the local
host has no Android compile commands. On-device timing validity and performance
remain unverified until the next headset capture.
