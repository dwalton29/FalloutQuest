# M1B3e — empty collision worlds

Continues canonical main bff707b126122bb9853f855fc7ba72e65f82722f (v209).
This removes the empty-collision dependency of the resident resource coordinator.
It does not implement that coordinator or admit source Enable/Disable.

## Implemented

Collision readiness now describes an initialized, published world independently
of its triangle count. A reversible mutation can prepare an empty placement
selection, a non-solid-only selection, or a selection fully filtered by collected
references or current door poses. Assembly completion remains separate from
whether surfaces exist. Unprimed placements, wrong cache contexts, invalid
parameters and exterior safety caps still reject before publication. Existing
ordinary CELL-load preparation retains its empty-input/empty-assembly rejection.

Empty mutation exchanges and rollbacks use the existing allocation-free storage
exchange, publication revision, immutable captured inputs and stale-work guards.
An empty live world can prepare and exchange a subsequent nonempty candidate.
The derived player/loose-object broadphases, shape/weld data, interaction grids,
manifold invalidation and Pip-Boy local-map revision follow the same exchanges.
Shutdown/uninitialized controllers remain unavailable.

## Query behavior

- Player motion accepts the requested horizontal movement. Without ground it
  retains the existing player height policy; actual exterior LAND can provide
  the height. Empty interiors do not inherit exterior terrain. In-place removal
  does not run CELL-arrival safe-spawn rescue or reset its resolved flag.
- Loose-object collision accepts the integrator's requested position, including
  falling movement, and reports zero contacts, normals and candidates.
- Interaction occlusion reports no authored blocker.
- Actor ground queries report no surface, preserving the output argument, unless
  actual exterior LAND supplies one. Readiness does not promise ground exists.

These query paths do not invent a floor or add player gravity/fall-damage rules.
The existing player no-ground height behavior is a separate locomotion limitation;
this checkpoint is not a claim of original-game falling behavior.

## Validation

- The rebuilt production collision test covers interior/exterior empty selection,
  non-solid geometry, collected filtering and animation-nonsolid doors; unresolved
  and resolved safe-spawn states; player/actor LAND behavior; loose-object poses,
  occlusion, complete empty grids, re-enable/rollback, stale tokens, invalid empty
  context/units, legacy rejection and shutdown. Heap allocation is forbidden
  during empty exchanges, empty queries and rollback. Token ownership returns to
  the original registry size.
- Original Common House CELL 00004357: the supplied partial Megaton mesh archive
  provides 1,715 triangles from 335 input placements. Whole-world removal,
  queries and exact backing-buffer rollback pass without allocation. The existing
  authored REFR 000154DE four-triangle remove/restore case also passes. This is
  a partial asset fixture, not a complete CELL collision census.
- Physics, world and player suites: 36 host cases pass. Scene/epoch/NPC publication
  source checks pass. Player/world sources are unchanged by this checkpoint.
- Synthetic and original collision cases pass ASan/UBSan. LeakSanitizer remains
  disabled because it cannot inspect traced task threads in this runtime.
- Android CI and physical Quest validation remain separate. No performance
  measurement or headset result is claimed. Save schema and version stay v209.

## Remaining boundary

Resident visual/GPU preparation, interaction state, door poses, actor residency,
held/loose objects and navigation still need coordinated publication with the
M1B3d whole-script candidate. The coordinator must drain/isolate the serialized
asset lane, prepare all resources, revalidate scene/collision/Player stamps, and
exchange or roll back in one render-thread turn before simulation, saves, effect
delivery or rendering. No current script or render path calls the mutation API.
Enable/Disable, MoveTo, spawning and deletion remain unsupported.
