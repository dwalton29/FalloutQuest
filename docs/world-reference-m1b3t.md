# M1B3t — resident pause-aware gameplay time

Continues canonical main 8266e32db0f8e9b504536d260e5b2aaf690310cf (v212).
M1B3s Android CI completed successfully (run 38906603237).

## Two clock domains

Skipping an update does not freeze its absolute deadlines. NPC package waits,
combat/furniture animation phases, door sequences and UI/weapon deadlines could
otherwise observe the entire reserved interval on their next call.

A small allocation-free render-thread clock adapter now maintains separate
steady-clock and OpenXR nanosecond timelines. Without a pause, the adapter keeps
the raw epoch and normal elapsed units. Acquisition samples the steady clock at
the reservation boundary and latches both domains. While the existing full frame
barrier is active, subsequent samples do not advance gameplay time. The first
unblocked sample establishes a baseline without catch-up; ordinary progression
then resumes. Repeated pauses accumulate correctly without modifying individual
deadline fields or canonical Player data.

Nonpositive or regressed raw samples establish safe baselines without rewinding
gameplay time; arithmetic saturates rather than overflowing. The clocks remain
process-lived across scene teardown, matching the lifetime of their consumers'
timestamp domains. They are not serialized and are not shared with asset workers.

The native loop samples before shouldRender/view validation, after workflow
progress. Acquisition latching also detects a reservation completed entirely
between submitted frames. Before input sampling, a latched pause rebases the
existing locomotion baseline and rearms the neutral-controls barrier, including
when no rendered blocked frame was available to do so.

## Consumers

OpenXR-derived gameplay time now feeds world-clock/script deltas, Pip-Boy
tracking/menu dwell, lockpicking, script messages, dialogue/barter input,
weapon/reload/recoil/projectile presentation, door/loot selection and notification
age. Notifications cannot prepare resources, advance queues or consume queued
sound delivery during the reservation.

Steady gameplay time feeds resident NPC simulation/package deadlines, actor
clip/furniture/combat-overlay/hit sampling, animation-start stamps, health-bar
reveal/fade, interior door sequences/fallback motion and loose-object physics/
held-motion/throw/shoulder sampling. Existing physics constants, throw/drop
mechanics, animation clips, route logic and persistence rules are unchanged.

XR wait/locate/input pose prediction/end-frame submission retain raw OpenXR
timestamps. Locomotion retains its existing raw frame-delta/reset policy.
Head/controller/body tracking remains live. Asset loading, GPU work budgets,
streaming diagnostics, frame cadence, rendering performance counters and audio
playback retain their existing real clocks; they must not stall with gameplay.
This does not introduce a general focus/loading pause system or suspend audio.
Existing focus-loss notification clearing and ordinary focus policies remain.

The clocks follow the complete existing frame barrier, not just CPU preparation.
Settled owners, cancellation while a worker exits and fatal recovery remain
paused; only complete release/reset permits the next gameplay baseline.

## Validation and boundary

All 19 world, 31 player and one physics CTest cases pass (51 core host cases).
The new test compiles the actual native clock bridge with controlled raw epochs
and an injected frame barrier. It covers normal progression, exact acquisition,
repeated/long pauses, all retained blocking phases, immediate between-frame
reservations, independent XR/steady epochs, first-resume baselines, clock
regression/invalid samples, saturation and real notification queue age.

The existing production actor-runtime test now uses the actual pause bridge.
A moving pose sampled through the real NPC runtime remains identical through a
123-second pause and its first resumed sample, then advances by the next normal
half-second. Both affected targets pass ASan/UBSan with LeakSanitizer disabled.

Native source guards cover acquisition, sampling before non-rendered frames,
input/locomotion rearming, affected consumers, notification exclusion and raw XR/
performance timestamps. The ordinary throw/drop byte-hash guard still matches
its historical implementation after normalizing only the clock call. Relevant
Pip-Boy, shoulder, body/skin, opaque, render-dispatch and notification-layering
checks pass. Verified-original ESM authored admission, load-door, Lucas/
nonresident/barter dialogue, merchant trading and MS11 reward/save regressions
pass; existing unsupported showmap diagnostics remain.

No device/OpenXR fault-injection or performance claim is made. Android CI runs
separately. No gameplay command yet dispatches the general resident workflow.
Complete dependency admission, remaining lifecycle/focus/teardown handling,
bounded dispatch and device validation remain open. Broader navigation/LOD,
actor/interactive and exterior mutations stay unadmitted. v212 is preserved.
