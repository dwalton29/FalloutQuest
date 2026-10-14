# M1B2 — restore saved reference state during scene preparation

Started from M1B1 5f99428df64c8df1fa62c625bfc5c08461c1f5e2; replayed onto
newer canonical main fe798c0 (v209 legacy zero-health compatibility) after v209
and its compatibility follow-up arrived during publication. No v209 functionality or history is overwritten.
Version is now 209 / 0.72.0-persistent-player-stats (the incoming version).

## Implemented

Scene startup/transition workers capture an owned immutable reference snapshot
only after loading the player's save. Normal transitions and exterior CELL jobs
retain a shared const snapshot; no worker reads mutable live Player state.
The render-thread cache keys the projection by the world-state revision, so it
is not rebuilt per eye/frame or per CELL worker with an unchanged revision.

Interior/exterior selectors apply saved root state and global enable-parent
projection before base/model resolution. Previously discarded disabled models
can therefore enter preparation at their original transforms. Deleted records
cannot be resurrected by a snapshot. The same selected placements feed visual
and collision preparation; existing publication ordering is preserved. Interior
LIGH selection consumes the same snapshot, without changing light/shader maths.

Interaction eligibility, pickup, door access, container access and lockpicking
now query canonical enabled state rather than an immutable initial-disabled
bit. Disabled ACTIs/loose objects are excluded from interaction rays. Existing
ownership, keys, locks, item conditions and scripts remain separate policies.

## Validation

- All 27 updated player tests and all six world tests pass; NIF 12, physics 1,
  assets 3 and audio 2 pass (51 host CTest cases total).
- New production-loader fixtures cover enable/disable selection, deleted records,
  invalid parent state, exact transforms, collision inputs, interior selection,
  and independent concurrent snapshots over the same cached exterior index.
- The supplied master validates MS11DetonatorRef 0001E99F in Wasteland, at its
  original Tenpenny position and MODL/base. It is absent by default, selected with
  its original pose when enabled, retained in collision inputs, and absent again
  when disabled. This is placement/collision-input validation, not headset draw
  or bomb detonation validation.
- Player/runtime fixtures verify restored initially disabled ACTI targeting and
  permissions, disabled object exclusion, container/door/lockpick eligibility,
  snapshot persistence and legacy saves.
- Original master event/bomb/disarm, player catalogue, Lucas/MS11 acceptance,
  ownership/theft and lockpick/XP/Karma/save regressions all pass.
- Original actor census remains 619 registered; no actor count/draw-distance/
  graphics-quality/animation settings are reduced. Pickup eligibility excludes
  dormant references according to their authored parent graph.
- Scene collision publication, render dispatch, opaque path and shoulder source
  checks pass. Android CI is triggered by publication; record its result in the
  recovery log. No physical Quest or frame-time measurements are claimed.

## Deliberate boundary: live publication is still unfinished

Enable/Disable source handlers remain unavailable. This slice applies state at
scene load/stream-in; it does not yet guarantee a live mutation in an already
resident scene. Before command admission, M1B3 must:

1. Validate/prepare affected resident resources and only publish after the outer
   script transaction succeeds; dialogue previews must publish nothing.
2. Atomically coordinate render/interaction, reversible collision and physics,
   held objects and navigation. Collected-triangle deletion is not reversible
   enable state.
3. Reject or refresh stale CELL jobs/metadata/cache results by world-state
   revision; a snapshot-owning worker alone does not solve stale publication.
4. Coordinate actor registration/arrivals/AI and persistent ActorState. The
   current conditional-actor census policy is not an actor-enable implementation.
5. Complete loaded/unloaded/restart/failed-script tests and physical Quest checks.

Then continue cross-cell MoveTo/MoveToMarker, spawning and deletion. Stage 40
still rejects missing commands; no charge is consumed or quest advanced falsely.

## Additional original gate blockers found during integration

The existing Wasteland placement loader explicitly suppresses the gatehouse and
related gate references (the Q16.7 entrance guard). Preserve it until the M3/M6
authored trigger/animation integration is validated, then replace that old guard
as part of that work. ACRE/ACTI qualified variable ownership is also incomplete:
Pip-Boy referenceScripts currently indexes ACHR/NPC ownership only. Deputy Weld's
playerNear and gatehouse OpenNow cannot be treated as working simply because
trigger source can be parsed. These are recorded blockers, not newly introduced
special cases or claims that the Megaton entrance works.

## Concurrent v209 save-format integration

The v209 compatibility follow-up preserves v14 output for unchanged attributes.
Only characters with attribute overrides write v15, with required RFE1 followed
by PST1. Both forms are preserved here.
The previous publication lease prevented the older M1B2 tree from replacing it.
This checkpoint retains that format and SetPlayerAttributes, updates synthetic
fixtures to explicit legal character stats, and keeps original v14 corruption
positions intact for unchanged characters. Combined v15 world/stat tests locate
RFE1 before PST1 explicitly. It also
keeps post-parse resource validation for every supported version, including v1–3.
No production stats are invented to satisfy test fixtures.

Combined tests preserve SPECIAL/skills plus nonempty reference overrides in v15,
read original v14 saves both with and without RFE1, reject malformed records
atomically, and exercise legacy migrations and v1 resource bounds. The earlier
v209 CI failure in nine synthetic tests is addressed without weakening validation.
