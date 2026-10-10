# NPC crowd and combat recovery — v200

## Behaviour

Live arrival batches retain their immutable furniture/IDLM asset scenes but join
one resident activity reservation pool when published. Actors cannot lease the
same original marker through separate batch-local pools. No original schedules,
markers, spawn positions or package flags change; Megaton settlers with
NoWandering still use their eligible authored activities.

A crowd-blocked approach yields locally as before. After two seconds without
16 game units of progress toward its endpoint, Sandbox and radius-based Wander
release the blocked approach and choose again. A blocked activity is excluded
for five seconds. Fixed authored travel destinations retain their intent.
Combat drops a stalled tactical route so its next decision can replan. These
thresholds are bounded Quest recovery policies, not recovered engine constants.

Combat movement has its own intent, separate from attack/reload state. Armed
repositioning can continue through weapon wind-up and original hit-key firing;
the original Attack animation overlays the moving actor's upper body, with its
sound events still processed. Reload remains stationary. Firing endpoints need
actual sight; occluded endpoints remain candidates for cover/escape only.

Cover approaches and source wait-to-fire holds survive temporary loss of sight,
then seek an exposed firing position. A source flee wait longer than the local
15-second search timeout is honoured. Actors that departed the loaded scene are
no longer targetable through their stale visual root.

Original CSTD dodge chance, left/right chance and eight directional/idle timers
are decoded. Ranged actors use these probabilities and durations to request
reachable lateral, forward or backward NAVM manoeuvres. Direction-to-endpoint
selection, deterministic actor-specific choices and the 0.1-second lower bound
are Quest adaptations; they do not reproduce Bethesda's combat solver.

## Verification

Production runtime regressions cover contested endpoint recovery, shared marker
leases across arrival batches, moving wind-up/hit events, cover without sight,
long flee waits, departed targets, directional endpoints and occluded firing
positions. Existing package, furniture, dialogue, inventory, animation, save and
combat tests remain in the host suite. An additional source check loads the
attached Fallout3.esm and verifies the actual default CSTD probabilities/timers.

## Remaining work and headset acceptance

Full original combat parity remains incomplete: CSAD advanced tactics, melee
block/power-attack decisions, unsupported weapon/unarmed animation families,
hearing/sneak/light detection, full-body cover/peek and essential unconscious
reactions remain separate work. NAVM routing still lacks dynamic congestion
costs, complete actor-clearance planning and advanced jump/swim rules. Unloaded
travel timing can still synchronise arrivals through an original narrow exit.

On Quest, check Common House exit and bomb-marker contention across multiple
arrival batches, opposing traffic, fixed travel destinations, furniture exits,
save/reload, moving firearm aim/hit/reload events, hidden cover and low-health
escape. Host regressions establish state/event behaviour; visual fluidity and
Fallout 3 combat feel require headset observation.
