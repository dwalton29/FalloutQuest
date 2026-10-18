# Loading architecture checkpoint — v216

Baseline inspected: `7a762af` (v215), newer than the requested v214 baseline.
Version: `216 / 0.77.0-loading-frame-isolation`.

## What changes

The existing OpenXR/GL thread still owns the session and graphics context.
Once loading is visible, the logical frame bypasses gameplay clocks, scripts,
body/actor poses, interaction/UI preparation and time-of-day work. The loading
eye bypasses world rendering, the world depth swapchain, MSAA/HDR/post-processing
and gameplay HUDs. It uses physical headset poses for both stereo views.
Resident transaction/lifecycle pumping and necessary destination streaming remain
alive. Loading resources and scene work advance once before stereo acquisition,
rather than doing uploads inside the first eye. Animation samples remain shared
across both eyes.

The loading renderer is compiled before the legacy world GL override macros.
This matters: the old overrides could force depth testing back on and route a
loading framebuffer deletion through full world teardown. Loading now owns its
depth target and respects its own GL state restoration. Shutdown explicitly joins
its workers and releases its presentation resources while the context is alive.

There is no 30-frame/fade readiness gate on scene preparation. Completion still
requires a successful covered OpenXR submission before gameplay is revealed.
World shaders start after the first loading submission, overlapping CPU scene
preparation. Optional presentation readiness cannot hold a complete destination.

## Upload scheduling

A shared render-thread allowance covers scene transfers, presentation uploads,
detailed CELL steps, LOD steps, terrain streaming and deferred GPU destruction.
It adjusts between 0.5 and 3 ms using the runtime display period, prior application
elapsed time, cadence and supported asynchronous GPU timing. CPU/API elapsed time
is not treated as GPU completion. An unsignaled GL fence causes upload admission
to yield; the poll has zero timeout. No `glFinish` or blocking upload fence wait
was added. Once the GPU permits work, one bounded operation can make progress
even following an elapsed-budget overrun.

Main scene image misses decode on one bounded worker. Images beyond the former
64 MiB speculative decode cap no longer fall into synchronous DDS decoding on
the scene upload thread. Allocation, row transfer and mip generation are separate
steps. Rows transfer at approximately 256 KiB per step (at least one complete
row), preserving every RGBA byte. Static scene vertex expansion runs on the scene
CPU worker; the existing geometry capture/shared-buffer path transfers static
VBOs in 256 KiB pieces before publishing their shared geometry.

Loading artwork, slides, compass, overlay and exhibit use staged image transfers.
Presentation VBO and index data also transfer in pieces. The selected base artwork
has its own preparation result, so it can appear before slideshow/exhibit work.
Compass resources publish before the remaining enhancement uploads. Compass,
overlay, shaders and render targets remain reusable across transitions; per-load
art and exhibits still follow the original selection and decoding policy.

Image-transfer failure retains valid presentation fallback, while scene transfer
failure aborts the private destination. Cancellation requests are polled across
frames instead of joining an unfinished decoder from an XR frame. Shutdown joins
all jobs before their caches/context can be destroyed.

## Startup and scene dependencies

The loading catalogue skips unrelated homogeneous top-level ESM groups. It still
reads all original LSCR/WEAP/MISC records and uses the same selection functions.
The player ESM reader avoids seeking when already at the requested payload/header.
Catalogue validation now transfers the parsed catalogue through a private shared
owner and moves it to the output after the validation Player is destroyed. This
removes the full `Definitions()` copy, preserving inventory validation and all
catalogue identity/index construction. No mutable catalogue alias escapes.

Collision preparation begins as soon as the CPU scene packet and collision policy
are ready, before scene/NPC GPU uploads. Placement selection, original worldspace
bounds, generation/revision checks and the final atomic collision/visual commit
are retained. Admission cannot yield between consuming a collision token and the
visual commit. Workers are cancelled before private packets are discarded.

The exterior gate still requires the original 49 detailed warm-shell cells,
9 near LOD cells and the complete authored horizon. The 25-second warning remains
diagnostic only. Shared scheduling removes independent upload-budget stacking;
it does not shrink any window or readiness condition.

## Measurements actually obtained

These are host microbenchmarks against the supplied Fallout3.esm, with the same
unoptimized host compiler settings. They are **not** Quest launch measurements.
The original source variants were rebuilt for the catalogue comparison. The OS
file cache was warm; each catalogue sample used a fresh process. Some measurements
overlap host compilation, so ranges and comparable interleaved samples matter.

| Host operation | Before | After | Evidence |
| --- | ---: | ---: | --- |
| Loading catalogue, cached-file repeats | 124–140 ms | 4.5–5.7 ms | 150 artwork records, 122 exhibits in both |
| Player catalogue, interleaved median | 10.301 s | 9.745 s | three samples per variant |
| Player catalogue process peak RSS, median | 245,480 KiB | 175,632 KiB | `wait4` process resource usage |

Interleaved catalogue samples (microseconds): before 10,672,260 / 10,301,113 /
9,870,853; after 9,745,475 / 9,496,942 / 10,026,998. A separate comparison returned
9.612 s versus 8.760 s medians. The smaller interleaved improvement is the reported
comparison rather than extrapolating an end-to-end startup percentage.

Both catalogue variants return 1,762 items and 573,610 references, with identical
fingerprints: catalogue `1577784072`, loot `1064499287`, world `1450618725`.

| Required device measurement | Status |
| --- | --- |
| First-ever cold startup / subsequent warm startup | Not measured: no Quest attached |
| Initial Megaton time to playable frame | Not measured |
| Interior / exterior transition time | Not measured |
| Loading frame cadence / missed deadlines / worst stall | Instrumented; device capture required |
| Peak whole-application decoded asset memory | Not measured; staged image/vertex byte counts are logged, not total app memory |

The application logs first submission, first selected artwork, first animated
frame, first successfully submitted playable frame, display-period-aware missed-deadline counts, worst application elapsed time,
submission duration, upload API elapsed time, asynchronous upload GPU samples,
worker scheduling delay, decode time and staged asset bytes. Application elapsed
time includes waits; it is not CPU execution time. Existing resolve/player/metadata/
CPU/collision/finalization/warmup timings remain. `tools/loading/analyze_log.py`
prints the ordered stage events from a real logcat capture.

## Validation and remaining limits

Host validation covers exact staged texture bytes, odd image sizes, full mip
completion, cancellation/restart, GL failure/recovery, GPU backlog admission,
nonblocking worker retirement, scene publication invariants, save/player/quest/NPC
regressions, asset indexing, collision, original ESM loading and native syntax.
The APK workflow also runs the loading ownership/source regressions.

Local results: 36/36 player tests, 21/21 world/loading tests, 3/3 asset tests,
1/1 collision test, 2/2 audio tests and 10 executed NIF tests passed. Three
optional NIF tests requiring additional installed assets were skipped. Thirty
rendering/loading source assertions and the state-cache, GPU-timer and render
policy binaries passed. Full runtime and world/terrain native syntax checks
passed using Khronos GL/OpenXR declarations and host Android adapters. Original
ESM player/save/debug integration also passed.

No Bethesda assets or save format were changed. No draw distance, texture
resolution, world geometry, NPC rig, collision coverage or gameplay script was
reduced. Save compatibility is exercised by the player tests; complete on-device
visual/audio/gameplay equivalence remains to be tested.

This checkpoint does **not** promise a hard 13.89 ms maximum. Texture/buffer storage
allocation, mip generation, shader compilation/linking, NPC skin/facial setup,
cube-map misses and the existing full terrain/environment scene publication can
still exceed one frame. They are explicitly identified by long-operation timing.
The full terrain initialization retains its existing original material/height
semantics rather than being replaced by an unverified approximation. Exterior
completion time therefore still requires device profiling.

A compositor quad was evaluated but not introduced: the current original exhibit
has true per-eye stereo depth, whereas replacing the projection with an untested
flat quad changes that presentation. All OpenXR calls remain on their established
thread. Device testing is needed before changing composition-layer policy.

No persistent derived cache was added. BSA archive objects already share their
process-wide indexed readers. The measured loading-catalogue scan is now tiny;
serializing the complete mutable/schema-sensitive player catalogue without a
proven invalidation benefit was deferred. Original quest/script/world data is
always parsed from the source ESM.

For device acceptance, capture identical v215/v216 runs for cold start, warm
restart, repeated Megaton/interior/Wasteland travel, focus loss/resume and scene
cancellation. Verify animation/audio in both eyes and collision before movement.
A long finalization event is a remaining bottleneck, not evidence that readiness
should be relaxed.
