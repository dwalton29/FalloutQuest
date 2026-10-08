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
