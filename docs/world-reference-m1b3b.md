# M1B3b — reversible collision publication

Baseline: canonical f6068e57afa335638246089ef0a765b8c9c89ff0 on main (v209).
This completes the collision-exchange dependency, not the resident mutation
coordinator or source Enable/Disable support. No version change.

## Implemented

- Render-thread collision publication revision. Legacy scene publications,
  shutdown/reinitialization, collected-reference changes and interior-door
  collision changes invalidate older mutation candidates and rollback tokens.
- Owned Fo3CollisionMutationState captures that revision, collected exclusions,
  and current interior-door transforms/nonsolid states. The serialized asset
  worker consumes this value and the candidate's preselected authored placements;
  it does not read Player, live triangles, or mutable door state.
- Mutation preparation builds authored triangles, applies current door poses and
  collected exclusions, and prepares exterior shapes/weld adjacency/player
  broadphase plus the loose-object/interaction spatial grid before publication.
- ExchangeFo3CollisionMutationSnapshot exchanges the prepared world with the live
  one without allocating or rebuilding derived data. The token retains the exact
  displaced triangle buffers, surface ownership and query structures. Calling
  Exchange again restores them; Discard finalizes either result and releases
  only the world held by the token.
- Exchange validates live revision, original transform context, interior/exterior
  mode, collected set and door poses before any live change. Stale candidates or
  rollbacks fail without replacing a newer world. Ordinary consuming publication
  cannot accidentally consume a mutation/rollback token, and ordinary tokens
  cannot be exchanged.
- Grounding initialization state survives an in-place exchange. Triangle pointer
  sentinels are rebound; contact manifolds are cleared; Pip-Boy local collision
  maps are invalidated on exchange/rollback and door/collected geometry changes.
- Legacy scene publication keeps its existing API and behavior. The new API is
  reserved for the future resident coordinator; no script calls it yet.

## Tests run

- Rebuilt production collision runtime via tests/physics/collision_tests.cpp.
  Tests exercise disable/re-enable selection from the original cache, exact
  rollback buffers, rotated and animation-nonsolid door preservation, collected
  exclusions, floor contact for loose objects, interaction occlusion, exterior
  derived structures, finalize/discard/double-use, stale candidate/rollback
  rejection after scene/door/collection changes, wrong-origin caches, missing
  prewarm, ordinary-token rejection, invalid inputs and exterior cap rejection.
- Heap allocation is deliberately made to fail during forward exchanges and
  rollbacks. Both interior/exterior operations complete successfully, including
  the original-data case. This does not claim all future scene systems have an
  allocation-free publication path.
- Original ESM Common House CELL 00004357 exports 335 authored collision-input
  placements. With the supplied partial Megaton mesh archive, 77 placements
  decode to 1,715 triangles. Missing archive models are absent from this fixture;
  this is not a complete collision census for the CELL. REFR 000154DE's four
  authored triangles are removed and restored, including the identical original
  backing buffer, with no allocations during exchange/rollback.
- Physics, world and player CTest suites: 34 passed, zero failed. Unchanged NIF,
  asset and audio suites were not rerun for this checkpoint.
- AddressSanitizer and UndefinedBehaviorSanitizer pass for synthetic and original
  collision cases. LeakSanitizer cannot inspect task threads in this environment;
  ASan runs use detect_leaks=0. Token-registry ownership/finalization is checked
  directly by the tests.
- Scene/epoch/NPC publication ordering source checks pass. Native Android build
  and physical Quest verification remain outstanding at local validation time.

## Remaining M1B3 boundary

Empty or fully filtered collision worlds remain rejected: existing player and
loose-object queries equate empty triangles with an unavailable controller.
Exterior-capped mutations also fail preflight rather than publishing partial
geometry. No safety cap, visible distance, render quality or authored geometry
has been reduced or replaced with invented collision.

The future coordinator must capture the scene publication stamp and collision
state together, use the matching candidate enable-parent projection, preflight
visuals, doors, actors, held objects and navigation, and exchange all affected
resources in one render-thread turn after the outer script transaction succeeds.
It must not run physics/AI between forward exchanges and rollback. Collision
revision checks alone do not make script previews, saves, resources or actor
registration transactional. Legacy collision policy/cache priming still belongs
to the serialized asset lane and must be isolated/drained by that coordinator.

No source Enable/Disable handler is admitted. MoveTo/spawning/deletion remain
later work. Preserve the publication guards in M1B3a and continue at the resident
outer-transaction coordinator, empty-world query semantics and actor/held-object/
navigation coordination. A normal transition reload is not this coordinator.

## Reproduce the optional original-data test

The data root must contain the supplied original master as `Fallout3.esm`.
The mesh root is the extracted `megaton-meshes` directory from the supplied zip.
No generated manifest or original binaries belong in Git.

```sh
g++ -std=c++17 -O1 -Iapp/src/main/cpp -Iapp/src/main/cpp/data \
  tools/physics/export_collision_fixture.cpp app/src/main/cpp/data/fo3-esm-reader.cpp \
  -lz -o /tmp/fq-export-collision
/tmp/fq-export-collision <data-root> 4357 /tmp/common-house.manifest
/tmp/fq-physics/collision_tests <mesh-root> /tmp/common-house.manifest 4357
```

The exporter uses the production interior loader and static-collision selector,
retaining authored identities, model paths, positions, rotations and scales.
