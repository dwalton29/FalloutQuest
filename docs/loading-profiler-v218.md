# v218 — On-device loading bottleneck profiler

Parent: v217 loading scene recovery (21e640b). This checkpoint instruments the
existing loader; it does **not** change scene admission, assets, visual fidelity,
collision policy, upload scheduling, or frame timing.

## What gets recorded

The on-device trace uses the Android log tag `FQLoadProfile` and an app-private,
crash-resilient UTF-8 file:

`/data/user/0/com.falloutquest.app/files/loading-profile.log`

Events contain a monotonic `tUs`, a scene generation (for correlating transitions),
and elapsed times in **milliseconds**. Repeated game loads append to the same file,
which rotates at approximately 2 MiB on the next process launch. Each entry is
flushed so the last completed events are retained if the runtime exits unexpectedly.
The file is diagnostic data, not a game save.

- `BEGIN`: cold boot vs CELL transition, target CELL/WRLD, game readiness.
- `PROGRESS`: every ~2 seconds while the render loop advances; shows if
  the CPU worker is still running, how many mesh uploads have completed, NPC
  progress, collision worker readiness and GPU upload-admission state.
- `SHADER`: compile/link time and result for initial scene program.
- `CPU_READY`: worker queue delay and execution; player catalogue,
  entry/XTEL resolve, placement metadata, mesh expansion, NPC assembly,
  texture predecode, context preparation; placements, actor/mesh counts,
  longest single mesh parse and slowest predecoded DDS path.
- `SLOW_MESH`: per-object `UploadCpuObject` wall clock at >=16 ms,
  with original NIF model path and REF FormID. Only the first twelve slow
  meshes per scene generate individual entries; totals and the worst entry
  remain in `SCENE_READY`. This GL API wall clock can include texture
  cache misses, driver synchronization, and implicit GPU work; it is not
  strictly GPU execution time.
- `MESH_SKIPPED`: optional source visual failed to upload (first 12 recorded).
- `SLOW_NPC`: original actor part upload at >=16 ms (first 12).
- `COLLISION_READY`: worker queue, prepare and collision-publication times.
- `SCENE_READY`: loaded world objects and final scene publication elapsed
  time, mesh upload totals/worst object, NPC uploads, collisions and warmup flag.
- `UI_FIRST_FRAME`, `UI_FIRST_ART`, `UI_FIRST_ANIMATED`: loading presentation
  milestones; `LOADING_UI_CATALOG` and `SLOW_LOADING_UI_ASSETS` identify
  expensive loading-screen preparation calls.
- `UI_FRAMES`: ~every 72 submitted loading frames (and POST phase); app CPU
  frame time/cadence, estimated over-budget frames, GL work API budget,
  measured GPU time if supported. The over-budget count is an **estimate**,
  not a hardware compositor dropped-frame counter.
- `PRESENTATION_COMPLETE`, `PLAYABLE_FIRST_FRAME`: distinguish a finished
  loading panel from the actual first playable world frame.
- `ABORT`: failure reason, generation, current mesh/actor counts and the
  last known collision/CPU stages, **before cleanup**.
- `BLANK_WORLD`: every ~2 seconds when the world is not ready, no load
  is active, and the loading panel has already disappeared.

All timestamps use a monotonic process-local clock; compare `elapsedMs`
rather than absolute `tUs` across reboots. CPU, GPU API and XR frame-time
columns represent overlapping pipelines and **must not be summed** as though
they were all serial.

## Capture from a Meta Quest (Windows PowerShell)

1. Install a debug APK built from v218. Do not uninstall the existing app or
   wipe game saves.
2. Connect the headset over USB with developer mode and USB debugging accepted.
3. Launch FalloutQuest, reproduce the slow Megaton load, and optionally
   enter/leave a building to capture a second transition.
4. **Before restarting the application**, run:

```powershell
adb devices
adb exec-out run-as com.falloutquest.app cat files/loading-profile.log > loading-profile.log
adb logcat -d -s FQLoadProfile:I FalloutQuest:I "*:S" > falloutquest-logcat.txt
```

The first file is the compact stage profiler; the second provides surrounding
errors, warnings, GL diagnostics and unabridged existing loading-frame logs.
Share both files with the developer. The second command is also the fallback
if `run-as` is unavailable, e.g. a non-debuggable APK. In that case
`adb logcat -d -s FQLoadProfile:I "*:S" > loading-profile-logcat.txt`
captures the same profiler messages while they remain in the logcat ring buffer.

An app-private file is used intentionally so diagnostics do not depend on
Android shared-storage permission or the Fallout 3 installation location.
The logger performs no filesystem scan, file extraction, compression, or
frame-by-frame file I/O. It writes only stage changes, throttled progress,
the slowest bounded asset samples and occasional XR frame summaries.

## How to read a trace

Compare each `generation` separately.

- High `playerMs` or `placementsMs`: master/catalogue load or CELL
  metadata extraction is slow.
- High `meshesMs` or `worstMeshParseMs`: original NIF parsing/expanded
  vertex processing on the CPU worker is costly.
- High `texturePredecodeMs` or `worstTextureDecodeMs`: original DDS
  decode/cache preparation dominates; a 64 MiB speculative limit still applies.
- Many `SLOW_MESH` entries: GL texture/mesh upload and driver work may
  exceed the frame budget despite the outer 4 ms admission check.
- `PROGRESS` with `cpuReady=1`, unchanged `meshesUploaded`, and
  `budgetGpuReady=0`: GPU backlog is preventing new uploads.
- `PROGRESS` with finished mesh uploads but `collisionReady=0`:
  collision snapshot work or publication may be holding readiness.
- `SCENE_READY` but long delay before `PRESENTATION_COMPLETE`:
  loading UI, warmup, or publication presentation may be delaying the reveal.
- `ABORT` or `BLANK_WORLD`: inspect the failure reason and nearby
  original `SCENE LOAD FAILED` / EGL / XR / GL log lines. An empty tinted
  background is not a successful load.

A trace identifies suspect work and its timings; performance conclusions
still require at least one headset capture. Host CI cannot validate a Quest
GPU driver stall or actual XR compositor timing.
