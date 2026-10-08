# Resident local AI checkpoints after v182

Original ESM re-fetched from the user's Dropbox; canonical main baseline
96564e075635678df996c4800b1fdd3d8ec40cba. Reproducible audit:
`python tools/npc/audit_local_activities.py /path/to/Fallout3.esm`.

## Sandbox original-data audit

37 original resident placements have 90 Sandbox assignments. 54 assignments
set No Wandering. Type flags are exclusion flags: 1 No Eating, 2 No Sleeping,
4 No Conversation, 8 No Idle Markers, 16 No Furniture, 32 No Wandering.
The original assignments have 36 flags=0023, 35=0000, 12=002F, 4=0022,
1=0027, 1=0033, 1=0003. An absent type flag field defaults to zero.

* Moira service 00004153: centre MoiraServiceMarker 00076F52, radius 275,
  08:00–20:00, flags 0033. Idle markers allowed; chairs and wandering forbidden.
  Default 00004157: radius 1000 near 00003FDF, flags 0022.
* Gob 00003FA3: near 00003FA8, radius 300, flags 0023, unscheduled.
* Moriarty 00003F9C: same centre, radius 375, 13:00–16:00, actor local-variable
  condition 53. 000664A3 uses the same centre/radius at 04:00–08:00.
  Default 00003F9E: centre 0006A02C, radius 400, flags 0022.
* Doc Church 00004148: centre 0000414B, radius 4000, 09:00–21:00,
  flags 0023. Default 0000414F shares the area and flags.
* Common House default 000043DB: centre 000043B0, radius 2000, no exclusions.
* Lucas default 000BD075: editor location, radius 4000, no exclusions.

Placements include MoiraCounter/MoiraShelf/shopkeeper markers in Craterside,
GobSweep/MoriarityGobBartend/shelf markers in the saloon, doctor examine
markers in the clinic and observation/worship markers outside. The loader
retains original IDLM references, IDLF/IDLC/IDLT/IDLA, IDLE models/parent gates
and ANIO requirements. It excludes malformed/unknown/sequenced/do-once marker
programs, unavailable clips, unresolved CTDA gates, and animation-object props.
No broom, glass, cigarette or other prop is silently omitted from its animation.

Only supported adult, level, unscaled chair programs from v182 are used.
Ownership, original radius and reservation checks apply to every activity.
Both actual marker position and rotation come from REFR DATA; chairs use FRN.

Quest selection policy: deterministic actor/package/cycle diversity, previous
reference exclusion when alternatives exist, at most eight reachability probes,
12–24 seconds per chair activity, bounded marker timing, and a short pause
between selections. This is not Bethesda's energy/probability implementation.
Completed activities keep the package active. No-Wandering packages never
fall back to wandering. No eligible local activity yields bounded package
backoff and lower-priority authored selection. Existing interruption/exit/
root accumulation and ephemeral reservation rules remain the single owner.

Not implemented by this checkpoint: autonomous food consumption, Sandbox
sleep-time calculation, NPC conversations, animation-object rendering,
sequenced/do-once markers, linked Sandbox patrol centres, tilted/scaled/child
furniture, unresolved enable parents and cross-cell activity scheduling.
These limitations must prevent claims of full Sandbox package coverage.

Format sources: https://tes5edit.github.io/fopdoc/Fallout3/Records/PACK.html
and https://tes5edit.github.io/fopdoc/Fallout3/Records/IDLM.html.
Original GECK behavior documentation is mirrored at
https://geckwiki.com/index.php?title=Sandbox_Package.

Checkpoint 1 validation: 17/17 host player tests pass. Original-data execution
proves 25/90 activity assignments, 4/90 permitted wander-only assignments,
and 61/90 blocked/ineligible in the tested original resident/condition state.
Before this checkpoint, furniture/marker activity coverage was 0/90.
All 29 executable assignments are partial package support: the allowed
activity families listed above are not all implemented. Fully supported
original Sandbox packages: 0. The 61 are not claims of permanently false
conditions. Moira service and Doc Church marker execution are proven;
Gob/Moriarty's prop-dependent and other unresolved routines remain blocked.
Sleep remains 26/36 and seated Eat 8/33. Population simulations at 12:00,
20:00 and 01:00 retain independent per-actor state and reservations.

## Accompany original-data audit and adaptation

Harden 00004009 targets Maggie 00003B5C at distance 200, 09:00–16:00,
no conditions. Nathan 00019542 targets Manya 00003B58 at distance 210,
unscheduled, CTDA GetRandomPercent <= 33. Neither has PLDT or a script.
The documented Accompany behavior uses the target's intended destination,
unlike Follow's live-position destination. The implementation reads the
resident target's active route endpoint, falls back to its live root when
it is waiting, and checks death/unload through the existing target resolver.
Player destination intent and other target types are explicitly unavailable.

Quest adaptation clips the route endpoint to the original distance envelope
around the moving target; an actor outside that envelope first catches up.
Repath cadence is 0.75 seconds, destination change threshold 16 game units,
arrival tolerance 8 units. Original NAVM portals, blockers, door handling,
per-package failure backoff and normal conditions/schedules remain in charge.
GetRandomPercent is an explicitly supplied deterministic 0–99 sample stable
per actor/package/game minute. Only Accompany supplies it; callers without
an established random sample still report the condition as unavailable.
No saved schema or authored placement is changed.

Original placed Harden and Maggie are in different resident cells, so that
assignment needs later cross-cell scheduling before it can run as placed.
Nathan and Manya are co-resident; selection still obeys its random gate.
Source: https://geckwiki.com/index.php?title=Accompany_Package.

Legacy original 20-byte CTDA conditions now retain their function, operands,
comparison and flags, with implicit subject run-on and no reference. The prior
28-byte-only decoder turned Nathan's real random gate into function zero.
Both short-layout decoding and malformed truncation are regression tested.

### Checkpoint 2 / v183 audit details

| PACK / EDID | Accompany actor REF / base | Target REF / base | Scene and eligibility |
| --- | --- | --- | --- |
| 00019542 / MegNathanAccompanyManya6x12 | Nathan 00003B57 / 00000A67 | Manya 00003B58 / 00000A68 | Both 00003A32; unscheduled despite EDID; legacy 20-byte CTDA 77, flags A0, value 33, operands zero: GetRandomPercent <= 33 |
| 00004009 / MegHardenPlayWithMaggie9x7 | Harden 00003B45 / 00000A73 | Maggie 00003B5C / 00000A66 | 09:00–16:00, no CTDA; authored placements in 00003A29 and 00003A33 |

Both have PKDT type 7, general flags 00400000 (Defensive Combat), zero
type flags, no PLDT/PLD2, PTDT specific-reference type 0, no second target,
no executable embedded script or event action, and no package IDLA idles.
Neither assignment requires linked-reference resolution or a quest result
script. Harden's higher-priority package has its own quest-variable gate;
normal priority selection is retained. Nathan's random sample is provided
only to Accompany; this checkpoint does not enable his unrelated random
Travel/Sandbox routines.

Verified GECK subset: target chooses the destination; Accompany knows it and
paths toward it, changing intent with the target, while trying to maintain
the configured distance. There is no authored fixed destination in these
records. Must Complete and Must Reach Location are invalid for Accompany.
Unlike Escort, it does not install a target's follow package or lead it to
an authored destination. Continue During Combat/Pretend In Combat do not
apply to Accompany; the existing combat owner interrupts it. No new combat
policy or animation owner is introduced.

This is continuous accompaniment: route arrival puts the actor in Waiting,
not Completed. A stationary target has its live root as intent; within the
authored distance, the actor stops and resumes when the target moves or
changes destination. Normal schedule/condition invalidation or target loss
ends local execution. COMPLETE diagnostics describe release/reselection,
not proof of a Bethesda success event. No exact Bethesda formation weighting,
side placement, acceleration, random sampling cadence, or arrival tolerance
is claimed. These are bounded Quest policies described above.

Only resident actor specific references with a positive distance are admitted.
Player destination intent, location variants, second targets, malformed/type
flags and procedure actions are diagnosed and skipped. Canonical target
cell/world transitions reject continuation immediately, permit lower-priority
selection, and identify the later XTEL dependency. There is no teleport,
unloaded simulation, saved route or new save schema. Existing retry timers
handle invalid/off-NAVM destinations and the existing door bridge handles
obstructions. Dialogue/combat return rebuilds intent through the same owner;
loading/teardown uses existing resident suspension/destruction.

Event diagnostics: BEGIN, TARGET, ROUTE, REPATH, INTERRUPT, COMPLETE and
UNSUPPORTED, with actor/package/target IDs. Interrupted/unsupported reasons
are once-only per actor/package/reason; routes log only on bounded rebuilds.

Sources: https://geckwiki.com/index.php/Accompany_Package,
https://geckwiki.com/index.php/Follow_Package,
https://geckwiki.com/index.php?title=Escort_Package, and
https://geckwiki.com/index.php?title=Combat_Package_Flags.

Checkpoint 2 validation: v183 / `0.49.0-npc-accompany`. All 17 player host
tests pass, including navigation, combat, dialogue, persistence, furniture,
Sandbox and door execution. Original ESM runs pass for player state, Pip-Boy
data, weapons, dialogue, dialogue bridge, package runtime, combat state, combat
runtime, doors and furniture/Accompany integration. The two original PACKs
are tested without record edits: Nathan in his authored shared cell, Harden
first rejected with the original separate-cell dependency, then executed with
a controlled canonical co-resident target on original NAVM. Thus coverage is
2/2 when supported local dependencies are satisfied, 1/2 as authored placements
currently resident; cross-cell continuation remains excluded.

Synthetic coverage includes two independent actors, static/stopped/reversing
targets, changing destination intent, falling behind, exhausted-route
displacement, invalid NAVM targets with fallback/retry, temporary door/wall
obstruction, target death/removal/cell change, actor death, dialogue/combat
return, original random CTDA, schedule/condition invalidation, loading/save
suspension and actual save/restore without saved routes. Existing assertions
are retained. Sleep 26/36, seated Eat 8/33 and Sandbox activities 25/90 are
unchanged. Original per-cell population runs at noon, evening and night retain
independent state and reservations. No physical Quest test was performed;
visual formation, child locomotion, physical door approach, grounding,
dialogue/combat return and save/scene restoration still need headset acceptance.
Android CI status and APK are reported separately with delivery.
